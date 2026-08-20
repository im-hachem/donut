#include <iostream>
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "engine.h"
#include "core/log.h"
#include "core/hdri_manager.h"
#include "rendering/vertex_buffer.h"
#include "rendering/index_buffer.h"

namespace Donut
{
    Engine::Engine()
        : m_sag_a(glm::vec3(0.0f, 0.0f, 0.0f), 8.54e36f)
    {
        m_width = 1280;
        m_height = 720;

        m_camera.set_camera_mode(CameraMode::Orbital);
        m_camera.set_orbital_radius(1e11);
        m_camera.set_orbital_limits(1e9, 1e13);
        m_camera.set_orbital_speed(0.01f);
        m_camera.set_zoom_speed(1e10f);

        m_objects =
        {
            { glm::vec4(0.00f, 0.00f, 0.00f, m_sag_a.m_rs), glm::vec4(0, 0, 0, 1), static_cast<float>(m_sag_a.m_mass) }
        };

        // The geodesic ray tracer used to be a compute shader; it is now a
        // fullscreen vertex+fragment pass (see dispatch_compute) so it runs on
        // macOS OpenGL 4.1, which has no compute shaders.
        m_compute_program = Ref<Shader>(Shader::create("assets/shaders/Geodesic.glsl"));
        m_shader_program  = Ref<Shader>(Shader::create("assets/shaders/TexturedQuad.glsl"));
        m_blur_shader     = Ref<Shader>(Shader::create("assets/shaders/Blur.glsl"));

        auto& hdri_manager = HDRIManager::get();
        m_hdri_environment = hdri_manager.get_current_hdri();
        if (!m_hdri_environment)
        {
            hdri_manager.set_current_hdri("assets/hdri/HDR_blue_nebulae-1.hdr");
            m_hdri_environment = hdri_manager.get_current_hdri();
            if (!m_hdri_environment)
                DONUT_WARN("Failed to load default HDRI, using fallback");
        }

        m_camera_ubo = UniformBuffer::create(128, 1);
        m_disk_ubo   = UniformBuffer::create(sizeof(float) * 5, 2);

        uint32_t obj_ubo_size = sizeof(int) + 3 * sizeof(float)
            + 16 * (sizeof(glm::vec4) + sizeof(glm::vec4))
            + 16 * sizeof(float);
        m_objects_ubo = UniformBuffer::create(obj_ubo_size, 3);

        m_simulation_ubo = UniformBuffer::create(sizeof(int) * 2 + sizeof(float) * 2, 4);

        auto result = QuadVAO();
        m_quad_vao = result.first;
        m_texture = result.second;

        // GLSL 4.10 forbids explicit binding qualifiers on uniform blocks, so
        // associate the geodesic shader's blocks with their UBO binding points
        // from the host side instead.
        if (m_compute_program)
        {
            uint32_t prog = m_compute_program->get_renderer_id();
            struct { const char* name; uint32_t point; } blocks[] =
            {
                { "Camera", 1 }, { "Disk", 2 }, { "Objects", 3 }, { "Simulation", 4 }
            };
            for (const auto& b : blocks)
            {
                uint32_t idx = glGetUniformBlockIndex(prog, b.name);
                if (idx != GL_INVALID_INDEX)
                    glUniformBlockBinding(prog, idx, b.point);
            }
        }

        glGenFramebuffers(1, &m_geodesic_fbo);
    }

    auto Engine::update_window_dimensions() -> void
    {
        update_compute_dimensions();
    }

    auto Engine::set_window_dimensions(int width, int height) -> void
    {
        int old_width = m_width;
        int old_height = m_height;
        int old_compute_height = m_compute_height;

        m_width = width;
        m_height = height;

        if (old_width         != m_width  ||
            old_height        != m_height ||
            old_compute_height != m_compute_height)
            update_compute_dimensions();
    }

    auto Engine::update_performance(float delta_time) -> void
    {
        if (delta_time > 0.0f)
            m_current_fps = 1.0f / delta_time;
    }

    auto Engine::update_compute_dimensions() -> void
    {
        m_texture = Texture2D::create(get_compute_width(), m_compute_height);
    }

    auto Engine::draw_full_screen_quad() -> void
    {
        RenderCommand::set_viewport(0, 0, m_width, m_height);

        m_shader_program->bind();
        m_quad_vao->bind();

        m_texture->bind(0);
        m_shader_program->set_int("u_ScreenTexture", 0);

        RenderCommand::disable_depth_test();
        RenderCommand::draw_arrays(6);
        RenderCommand::enable_depth_test();
    }

    auto Engine::draw_blur_pass() -> void
    {
        RenderCommand::set_viewport(0, 0, m_width, m_height);

        m_blur_shader->bind();
        m_quad_vao->bind();

        m_texture->bind(0);
        m_blur_shader->set_int("u_ScreenTexture", 0);
        m_blur_shader->set_float2("u_Resolution", glm::vec2(m_width, m_height));
        m_blur_shader->set_float("u_BlurStrength", m_blur_strength);
        m_blur_shader->set_float("u_GlowIntensity", m_glow_intensity);

        RenderCommand::disable_depth_test();
        RenderCommand::draw_arrays(6);
        RenderCommand::enable_depth_test();
    }

    auto Engine::draw_geodesic_pass(int cw, int ch) -> void
    {
        m_quad_vao->bind();
        RenderCommand::disable_depth_test();

#ifdef __APPLE__
        // macOS aborts any GPU submission that runs longer than a couple of
        // seconds ("GPU Hang"). The geodesic ray-marcher can far exceed that in
        // a single fullscreen draw, so render it in scissored tiles and flush
        // after each, keeping every submission short enough to survive the
        // watchdog. Compute-capable platforms draw it in one pass.
        // Largest tile that keeps a tile's worst-case work (tile^2 * step_cap)
        // inside the safe watchdog zone measured on this GPU (~25M pixel-steps
        // per submission); bigger tiles mean fewer glFinish stalls.
        const int tile = 64;
        glEnable(GL_SCISSOR_TEST);
        for (int y = 0; y < ch; y += tile)
        {
            int th = std::min(tile, ch - y);
            for (int x = 0; x < cw; x += tile)
            {
                int tw = std::min(tile, cw - x);
                glScissor(x, y, tw, th);
                RenderCommand::draw_arrays(6);
                glFinish();
            }
        }
        glDisable(GL_SCISSOR_TEST);
#else
        RenderCommand::draw_arrays(6);
#endif

        RenderCommand::enable_depth_test();
    }

    auto Engine::dispatch_compute(const Camera& cam) -> void
    {
        auto& hdri_manager = HDRIManager::get();
        m_hdri_environment = hdri_manager.get_current_hdri();

        int cw = get_compute_width();
        int ch = m_compute_height;

        // Render the geodesic pass into m_texture through an FBO. This replaces
        // the old compute dispatch + image_store path, which relied on OpenGL
        // 4.3 compute and 4.2 image load/store that macOS does not provide.
        glBindFramebuffer(GL_FRAMEBUFFER, m_geodesic_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture->get_renderer_id(), 0);
        glViewport(0, 0, cw, ch);

        m_compute_program->bind();
        upload_camera_ubo(cam);
        upload_disk_ubo();
        upload_objects_ubo(m_objects);
        upload_simulation_ubo();
        m_compute_program->set_float2("u_Resolution", glm::vec2(static_cast<float>(cw), static_cast<float>(ch)));

        if (m_hdri_environment)
        {
            m_hdri_environment->bind(5);
            m_compute_program->set_int("u_HDRIEnvironment", 5);
        }

        draw_geodesic_pass(cw, ch);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    auto Engine::upload_camera_ubo(const Camera& cam) -> void
    {
        struct UBOData
        {
            glm::vec3 pos;     float _pad0;
            glm::vec3 right;   float _pad1;
            glm::vec3 up;      float _pad2;
            glm::vec3 forward; float _pad3;
            float tan_half_fov;
            float aspect;
            bool  moving;
            int   _pad4;
        } data;

        glm::vec3 fwd   = glm::normalize(cam.get_orbital_target() - cam.get_orbital_position());
        glm::vec3 up    = glm::vec3(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(fwd, up));
        up = glm::cross(right, fwd);

        data.pos        = cam.get_orbital_position();
        data.right      = right;
        data.up         = up;
        data.forward    = fwd;
        data.tan_half_fov = static_cast<float>(tan(glm::radians(60.0f * 0.5f)));
        data.aspect     = static_cast<float>(get_compute_width()) / static_cast<float>(m_compute_height);
        data.moving     = cam.is_dragging() || cam.is_panning();

        m_camera_ubo->set_data(&data, sizeof(UBOData));
        m_camera_ubo->bind(1);
    }

    auto Engine::upload_objects_ubo(const std::vector<ObjectData>& objs) -> void
    {
        struct UBOData
        {
            int num_objects;
            float _pad0, _pad1, _pad2;
            glm::vec4 pos_radius[16];
            glm::vec4 color[16];
            float mass[16];
        } data;

        size_t count = std::min(objs.size(), size_t(16));
        data.num_objects = static_cast<int>(count);

        for (size_t i = 0; i < count; ++i)
        {
            data.pos_radius[i] = objs[i].m_pos_radius;
            data.color[i] = objs[i].m_color;
            data.mass[i] = objs[i].m_mass;
        }

        m_objects_ubo->set_data(&data, sizeof(data));
        m_objects_ubo->bind(3);
    }

    auto Engine::upload_disk_ubo() -> void
    {
        float r1 = static_cast<float>(m_sag_a.m_rs * 2.2);
        float r2 = static_cast<float>(m_sag_a.m_rs * 5.2);
        float num = 2.0f;
        float thickness = static_cast<float>(m_sag_a.m_rs * m_disk_thickness);
        float disk_data[5] = { r1, r2, num, thickness, m_disk_density };

        m_disk_ubo->set_data(disk_data, sizeof(disk_data));
        m_disk_ubo->bind(2);
    }

    auto Engine::upload_simulation_ubo() -> void
    {
        struct UBOData
        {
            int max_steps_moving;
            int max_steps_static;
            float early_exit_distance;
            float time;
        } data;

        data.max_steps_moving    = m_max_steps_moving;
        data.max_steps_static    = m_max_steps_static;
        data.early_exit_distance = m_early_exit_distance;
        data.time              = static_cast<float>(glfwGetTime()) * m_rotation_speed;

#ifdef __APPLE__
        // macOS has no compute shaders, so the geodesic pass runs as a tiled
        // fragment shader under the OS GPU watchdog. The stock step counts
        // (up to 30000) make a single tile exceed the watchdog and hang the
        // GPU, so cap them here. Windows/Linux keep the full step count.
        // While the camera moves, render cheaply so interaction stays smooth;
        // when it settles, spend more steps for a cleaner image. Both stay well
        // under the per-tile GPU-watchdog budget (see draw_geodesic_pass).
        data.max_steps_moving = std::min(data.max_steps_moving, 4000);
        data.max_steps_static = std::min(data.max_steps_static, 6000);
#endif

        m_simulation_ubo->set_data(&data, sizeof(data));
        m_simulation_ubo->bind(4);
    }

    auto Engine::update_physics(float delta_time) -> void
    {
        for (auto& obj : m_objects)
        {
            for (auto& obj2 : m_objects)
            {
                if (&obj == &obj2) continue;
                float dx = obj2.m_pos_radius.x - obj.m_pos_radius.x;
                float dy = obj2.m_pos_radius.y - obj.m_pos_radius.y;
                float dz = obj2.m_pos_radius.z - obj.m_pos_radius.z;
                float distance = sqrt(dx * dx + dy * dy + dz * dz);
                if (distance > 0)
                {
                    std::vector<double> direction = {dx / distance, dy / distance, dz / distance};
                    double Gforce = (G * obj.m_mass * obj2.m_mass) / (distance * distance);
                    double acc1 = Gforce / obj.m_mass;
                    std::vector<double> acc = {direction[0] * acc1, direction[1] * acc1, direction[2] * acc1};

                    if (m_gravity)
                    {
                        obj.m_velocity.x += static_cast<float>(acc[0]);
                        obj.m_velocity.y += static_cast<float>(acc[1]);
                        obj.m_velocity.z += static_cast<float>(acc[2]);

                        obj.m_pos_radius.x += static_cast<float>(obj.m_velocity.x);
                        obj.m_pos_radius.y += static_cast<float>(obj.m_velocity.y);
                        obj.m_pos_radius.z += static_cast<float>(obj.m_velocity.z);
                    }
                }
            }
        }
    }

    auto Engine::render_scene() -> void
    {
        RenderCommand::clear();
        m_shader_program->bind();
        m_quad_vao->bind();
        m_texture->bind(0);
        RenderCommand::draw_arrays(6);
    }

    auto Engine::create_compute_program(const char* path) -> Ref<Shader>
    {
        std::ifstream in(path);
        if(!in.is_open())
        {
            std::cerr << "Failed to open compute shader: " << path << "\n";
            return nullptr;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        std::string src_str = ss.str();
        return Ref<Shader>(Shader::create_compute("ComputeShader", src_str));
    }

    auto Engine::QuadVAO() -> std::pair<Ref<VertexArray>, Ref<Texture2D>>
    {
        float quad_vertices[] =
        {
            // Positions   // TexCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        auto vertex_buffer = Ref<VertexBuffer>(VertexBuffer::create(quad_vertices, static_cast<uint32_t>(sizeof(quad_vertices))));
        VertexBufferLayout layout;
        layout.push<float>(2); // Position (x, y)
        layout.push<float>(2); // TexCoord (u, v)
        vertex_buffer->set_layout(layout);

        auto vertex_array = Ref<VertexArray>(VertexArray::create());
        vertex_array->add_vertex_buffer(vertex_buffer);
        auto texture = Texture2D::create(get_compute_width(), m_compute_height);

        return { vertex_array, texture };
    }

    auto Engine::load_objects_from_scene(const std::vector<Donut::Object>& objects) -> void
    {
        m_objects.clear();
        m_objects.push_back(
        {
            glm::vec4(0.00f, 0.00f, 0.00f, m_sag_a.m_rs),
            glm::vec4(0, 0, 0, 1),
            static_cast<float>(m_sag_a.m_mass)
        });

        for (const auto& obj : objects)
        {
            ObjectData engine_obj;

            float scale_factor = 1e10f;
            engine_obj.m_pos_radius = glm::vec4
            (
                obj.m_centre.x * scale_factor,
                obj.m_centre.y * scale_factor,
                obj.m_centre.z * scale_factor,
                obj.m_radius * scale_factor
            );

            engine_obj.m_color = glm::vec4(obj.m_material.m_color, 1.0f);

            float volume  = (4.0f / 3.0f) * 3.14159f * engine_obj.m_pos_radius.w * engine_obj.m_pos_radius.w * engine_obj.m_pos_radius.w;
            float density = 1e12f;
            engine_obj.m_mass     = volume * density;
            engine_obj.m_velocity = glm::vec3(0.0f, 0.0f, 0.0f);

            m_objects.push_back(engine_obj);
        }

        DONUT_INFO("Loaded {} objects from WorldBuilder scene (scaled up by {})", objects.size(), 1e10f);
        print_object_info();
    }

    auto Engine::print_object_info() const -> void
    {
        DONUT_INFO("=== Object Information ===");
        DONUT_INFO("Total objects: {}", m_objects.size());

        for (size_t i = 0; i < m_objects.size(); ++i)
        {
            const auto& obj = m_objects[i];
            DONUT_INFO("Object {}: Pos=({}, {}, {}), Radius={}, Mass={}, Color=({}, {}, {})",
                i,
                obj.m_pos_radius.x, obj.m_pos_radius.y, obj.m_pos_radius.z,
                obj.m_pos_radius.w,
                obj.m_mass,
                obj.m_color.x, obj.m_color.y, obj.m_color.z
            );
        }
        DONUT_INFO("Camera position: ({}, {}, {})",
            m_camera.get_orbital_position().x,
            m_camera.get_orbital_position().y,
            m_camera.get_orbital_position().z
        );
        DONUT_INFO("Camera radius: {}", m_camera.get_orbital_radius());
        DONUT_INFO("========================");
    }

    auto Engine::export_high_res_frame(const std::string& filename, int width, int height) -> void
    {
        DONUT_INFO("Exporting high-resolution frame: {}x{} to {}", width, height, filename);

        if (width <= 0 || height <= 0)
        {
            DONUT_ERROR("Invalid dimensions for export: {}x{}", width, height);
            return;
        }

        if (filename.empty())
        {
            DONUT_ERROR("Invalid filename for export");
            return;
        }

        int original_width = m_width;
        int original_height = m_height;
        int original_compute_height = m_compute_height;

        m_width = width;
        m_height = height;
        m_compute_height = height;

        int compute_height = height;
        int compute_width = (width * compute_height) / height;

        if (compute_width <= 0 || compute_height <= 0)
        {
            DONUT_ERROR("Invalid compute dimensions: {}x{}", compute_width, compute_height);
            return;
        }

        FramebufferSpecification fb_spec;
        fb_spec.Width = width;
        fb_spec.Height = height;
        fb_spec.attachments = { FramebufferTextureFormat::RGBA8 };

        auto high_res_framebuffer = Framebuffer::create(fb_spec);
        if (!high_res_framebuffer)
        {
            DONUT_ERROR("Failed to create high-resolution framebuffer");
            return;
        }

        auto high_res_texture = Texture2D::create(compute_width, compute_height);
        if (!high_res_texture)
        {
            DONUT_ERROR("Failed to create high-resolution texture");
            return;
        }

        // Render the geodesic pass into high_res_texture through the geodesic FBO.
        glBindFramebuffer(GL_FRAMEBUFFER, m_geodesic_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, high_res_texture->get_renderer_id(), 0);
        glViewport(0, 0, compute_width, compute_height);

        m_compute_program->bind();

        struct UBOData
        {
            glm::vec3 pos;     float _pad0;
            glm::vec3 right;   float _pad1;
            glm::vec3 up;      float _pad2;
            glm::vec3 forward; float _pad3;
            float tan_half_fov;
            float aspect;
            bool  moving;
            int   _pad4;
        } data;

        glm::vec3 fwd   = glm::normalize(m_camera.get_orbital_target() - m_camera.get_orbital_position());
        glm::vec3 up    = glm::vec3(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(fwd, up));
        up = glm::cross(right, fwd);

        data.pos        = m_camera.get_orbital_position();
        data.right      = right;
        data.up         = up;
        data.forward    = fwd;
        data.tan_half_fov = static_cast<float>(tan(glm::radians(60.0f * 0.5f)));
        data.aspect     = static_cast<float>(compute_width) / static_cast<float>(compute_height);
        data.moving     = m_camera.is_dragging() || m_camera.is_panning();

        m_camera_ubo->set_data(&data, sizeof(UBOData));
        m_camera_ubo->bind(1);

        upload_disk_ubo();
        upload_objects_ubo(m_objects);
        upload_simulation_ubo();
        m_compute_program->set_float2("u_Resolution", glm::vec2(static_cast<float>(compute_width), static_cast<float>(compute_height)));

        if (m_hdri_environment)
        {
            m_hdri_environment->bind(5);
            m_compute_program->set_int("u_HDRIEnvironment", 5);
        }

        draw_geodesic_pass(compute_width, compute_height);

        // Display the rendered frame into the high-res framebuffer for read-back.
        high_res_framebuffer->bind();
        RenderCommand::set_viewport(0, 0, width, height);
        RenderCommand::clear();

        m_shader_program->bind();
        m_quad_vao->bind();

        high_res_texture->bind(0);
        m_shader_program->set_int("u_ScreenTexture", 0);

        RenderCommand::disable_depth_test();
        RenderCommand::draw_arrays(6);
        RenderCommand::enable_depth_test();

        std::vector<unsigned char> pixels(width * height * 4);
        DONUT_INFO("Reading {} pixels from framebuffer...", width * height);
        RenderCommand::read_pixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        DONUT_INFO("Flipping image vertically...");
        std::vector<unsigned char> flipped_pixels(width * height * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int src_index = (y * width + x) * 4;
                int dst_index = ((height - 1 - y) * width + x) * 4;
                flipped_pixels[dst_index + 0] = pixels[src_index + 0]; // R
                flipped_pixels[dst_index + 1] = pixels[src_index + 1]; // G
                flipped_pixels[dst_index + 2] = pixels[src_index + 2]; // B
                flipped_pixels[dst_index + 3] = pixels[src_index + 3]; // A
            }
        }

        DONUT_INFO("Saving PNG file: {}...", filename);
        int result = stbi_write_png(filename.c_str(), width, height, 4, flipped_pixels.data(), width * 4);

        if (result)
            DONUT_INFO("Successfully exported high-resolution frame to: {}", filename);
        else
            DONUT_ERROR("Failed to export high-resolution frame to: {}", filename);

        high_res_framebuffer->unbind();

        m_width         = original_width;
        m_height        = original_height;
        m_compute_height = original_compute_height;

        RenderCommand::set_viewport(0, 0, original_width, original_height);
    }

}
