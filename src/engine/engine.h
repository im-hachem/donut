#pragma once

#include <vector>
#include <numbers>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>

#include "core/camera.h"
#include "object.h"

#include "rendering/renderer.h"
#include "rendering/shader.h"
#include "rendering/vertex_array.h"
#include "rendering/texture.h"
#include "rendering/uniform_buffer.h"
#include "rendering/texture_manager.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Donut
{
    const double c = 299792458.0;
    const double G = 6.67430e-11;

    struct BlackHole
    {
        glm::vec3 m_position;
        double m_mass;
        double m_radius;
        double m_rs;

        BlackHole(glm::vec3 pos, float mass)
            : m_position(pos), m_mass(mass)
        {
            m_rs = 2.0 * G * m_mass / (c * c);
        }

        bool intercept(float px, float py, float pz) const
        {
            double dx = double(px) - double(m_position.x);
            double dy = double(py) - double(m_position.y);
            double dz = double(pz) - double(m_position.z);
            double dist2 = dx * dx + dy * dy + dz * dz;
            return dist2 < m_rs * m_rs;
        }
    };

    struct ObjectData
    {
        glm::vec4 m_pos_radius;
        glm::vec4 m_color;
        float     m_mass;
        glm::vec3 m_velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    };

    class Engine
    {
    public:
        Engine();
        ~Engine() = default;

        auto draw_full_screen_quad() -> void;
        auto draw_blur_pass() -> void;
        auto dispatch_compute(const Camera& cam) -> void;
        auto upload_camera_ubo(const Camera& cam) -> void;
        auto upload_objects_ubo(const std::vector<ObjectData>& objs) -> void;
        auto upload_disk_ubo() -> void;
        auto upload_simulation_ubo() -> void;
        auto render_scene() -> void;
        auto update_physics(float delta_time) -> void;
        auto update_window_dimensions() -> void;
        auto set_window_dimensions(int width, int height) -> void;

        auto get_width() const -> int { return m_width;  }
        auto get_height() const -> int { return m_height; }

        auto get_objects() -> std::vector<ObjectData>& { return m_objects; }
        auto get_sag_a() -> BlackHole& { return m_sag_a;    }
        auto get_camera() -> Camera& { return m_camera;  }
        auto get_gravity() -> bool& { return m_gravity; }

        auto update_performance(float delta_time) -> void;
        auto set_target_fps(int fps) -> void { m_target_fps = fps;                             }
        auto get_target_fps() const -> int { return m_target_fps;                            }
        auto get_current_fps() const -> float { return m_current_fps;                           }
        void  set_compute_height(int height)
        {
#ifdef __APPLE__
            // Without compute shaders the geodesic pass runs as a tiled
            // fragment shader (see draw_geodesic_pass), so very high working
            // resolutions make each frame take many seconds. Cap it on macOS.
            if (height > 256) height = 256;
#endif
            m_compute_height = height;
        }
        auto get_compute_height() const -> int { return m_compute_height;                        }
        auto get_compute_width() const -> int { return (m_width * m_compute_height) / m_height; }
        auto update_compute_dimensions() -> void;

        auto get_max_steps_moving() const -> int { return m_max_steps_moving; }
        auto get_max_steps_static() const -> int { return m_max_steps_static; }
        auto get_early_exit_distance() const -> float { return m_early_exit_distance; }
        auto set_max_steps_moving(int steps) -> void { m_max_steps_moving = steps; }
        auto set_max_steps_static(int steps) -> void { m_max_steps_static = steps; }
        auto set_early_exit_distance(float distance) -> void { m_early_exit_distance = distance; }

        auto get_disk_thickness() const -> float { return m_disk_thickness;      }
        auto set_disk_thickness(float thickness) -> void { m_disk_thickness = thickness; }

        auto get_disk_density() const -> float { return m_disk_density;    }
        auto set_disk_density(float density) -> void { m_disk_density = density; }

        auto get_rotation_speed() const -> float { return m_rotation_speed;  }
        auto set_rotation_speed(float speed) -> void { m_rotation_speed = speed; }

        auto get_blur_strength() const -> float { return m_blur_strength;     }
        auto set_blur_strength(float strength) -> void { m_blur_strength = strength; }

        auto get_glow_intensity() const -> float { return m_glow_intensity;      }
        auto set_glow_intensity(float intensity) -> void { m_glow_intensity = intensity; }

        auto load_objects_from_scene(const std::vector<Donut::Object>& objects) -> void;
        auto export_high_res_frame(const std::string& filename, int width = 4096, int height = 3072) -> void;
        auto print_object_info() const -> void;

        auto set_hdri_environment(Ref<CubemapTexture> hdri) -> void { m_hdri_environment = hdri; }
        auto get_hdri_environment() const -> Ref<CubemapTexture> { return m_hdri_environment; }
    private:
        auto create_compute_program(const char* path) -> Ref<Shader>;
        std::pair<Ref<VertexArray>, Ref<Texture2D>> QuadVAO();

        // Draws the bound geodesic shader over a cw x ch target. On macOS this
        // is split into scissored tiles (with a flush each) so no single GPU
        // submission trips the OS watchdog; elsewhere it is one fast draw.
        auto draw_geodesic_pass(int cw, int ch) -> void;
    private:
        Ref<VertexArray>   m_quad_vao;
        Ref<Texture2D>     m_texture;
        Ref<CubemapTexture> m_hdri_environment;
        Ref<Shader>        m_shader_program;
        Ref<Shader>        m_compute_program;
        Ref<Shader>        m_blur_shader;
        Ref<UniformBuffer> m_camera_ubo;
        Ref<UniformBuffer> m_disk_ubo;
        Ref<UniformBuffer> m_objects_ubo;
        Ref<UniformBuffer> m_simulation_ubo;

        // FBO used to render the geodesic pass into m_texture. The geodesic
        // shader is a fragment shader (macOS has no compute), so it draws a
        // fullscreen quad into this framebuffer instead of dispatching compute.
        uint32_t m_geodesic_fbo = 0;

        int   m_width;
        int   m_height;
        float m_width_f = 100.0f*1e10f;
        float m_height_f = 75.0f*1e10f;

        int   m_target_fps     = 60;
        float m_current_fps    = 60.0f;
        float m_last_frame_time = 0.0f;
        int   m_compute_height = 150;

        std::vector<ObjectData> m_objects;
        BlackHole               m_sag_a;
        Camera                  m_camera;
        bool                    m_gravity = false;

        int   m_max_steps_moving    = 60000;
        int   m_max_steps_static    = 30000;
        float m_early_exit_distance = 5.0e11f;

        float m_disk_thickness = 0.1f;
        float m_disk_density   = 0.1f;
        float m_rotation_speed = 1.0f;
        float m_blur_strength  = 2.0f;
        float m_glow_intensity = 0.1f;
    };
};
