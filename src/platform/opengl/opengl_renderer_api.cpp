#include "opengl_renderer_api.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Donut
{
    auto OpenGLRendererAPI::init() -> void
    {
        if (!glfwGetCurrentContext())
        {
            DONUT_ERROR("No OpenGL context is current! Cannot initialize GLAD.");
            return;
        }
        
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            DONUT_ERROR("Failed to initialize GLAD!");
            return;
        }
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    auto OpenGLRendererAPI::set_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) -> void
    {
        glViewport(x, y, width, height);
    }

    auto OpenGLRendererAPI::set_clear_color(const glm::vec4& color) -> void
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    auto OpenGLRendererAPI::clear() -> void
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    auto OpenGLRendererAPI::enable_depth_test() -> void
    {
        glEnable(GL_DEPTH_TEST);
    }

    auto OpenGLRendererAPI::disable_depth_test() -> void
    {
        glDisable(GL_DEPTH_TEST);
    }

    auto OpenGLRendererAPI::set_face_culling(bool enabled) -> void
    {
        if (enabled)
        {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
        }
        else
            glDisable(GL_CULL_FACE);
    }

    auto OpenGLRendererAPI::enable_blending() -> void
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    auto OpenGLRendererAPI::disable_blending() -> void
    {
        glDisable(GL_BLEND);
    }

    auto OpenGLRendererAPI::draw_indexed(const Ref<VertexArray>& vertex_array, uint32_t index_count) -> void
    {
        uint32_t count = index_count ? index_count : vertex_array->get_index_buffer()->get_count();
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    auto OpenGLRendererAPI::draw_arrays(uint32_t vertex_count, uint32_t first) -> void
    {
        glDrawArrays(GL_TRIANGLES, first, vertex_count);
    }

    auto OpenGLRendererAPI::draw_lines(const Ref<VertexArray>& vertex_array, uint32_t index_count) -> void
    {
        uint32_t count = index_count ? index_count : vertex_array->get_index_buffer()->get_count();
        glDrawElements(GL_LINES, count, GL_UNSIGNED_INT, nullptr);
    }

    auto OpenGLRendererAPI::bind_texture(uint32_t texture_id, uint32_t slot) -> void
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture_id);
    }

    auto OpenGLRendererAPI::bind_image_texture(uint32_t texture_id, uint32_t slot, bool read_only) -> void
    {
        // Image load/store is OpenGL 4.2; the pointer is null on macOS (4.1).
        if (glBindImageTexture == nullptr)
            return;
        glBindImageTexture(slot, texture_id, 0, GL_FALSE, 0,
                          read_only ? GL_READ_ONLY : GL_WRITE_ONLY, GL_RGBA8);
    }

    auto OpenGLRendererAPI::read_pixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
                                       uint32_t format, uint32_t type, void* pixels) -> void
    {
        glReadPixels(x, y, width, height, format, type, pixels);
    }
};
