#pragma once

#include "core/memory.h"
#include "vertex_array.h"
#include "shader.h"
#include "framebuffer.h"

#include <glm/glm.hpp>

namespace Donut
{
    class RendererAPI
    {
    public:
        enum class API
        {
            None   = 0,
            OpenGL = 1,
            Vulkan = 2,
        };

    public:
        virtual ~RendererAPI() = default;

        virtual auto init() -> void                                                = 0;
        virtual auto set_viewport(uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height) -> void          = 0;
        virtual auto set_clear_color(const glm::vec4& color) -> void                = 0;
        virtual auto clear() -> void                                               = 0;
        virtual auto enable_depth_test() -> void                                   = 0;
        virtual auto disable_depth_test() -> void                                  = 0;
        virtual auto set_face_culling(bool enabled) -> void                        = 0;
        virtual auto enable_blending() -> void                                     = 0;
        virtual auto disable_blending() -> void                                    = 0;

        virtual auto draw_indexed(const Ref<VertexArray>& vertex_array,
                                  uint32_t index_count = 0) -> void                 = 0;

        virtual auto draw_arrays(uint32_t vertex_count, uint32_t first = 0) -> void = 0;
        virtual auto draw_lines(const Ref<VertexArray>& vertex_array,
                                uint32_t index_count = 0) -> void                   = 0;
        virtual auto bind_texture(uint32_t texture_id, uint32_t slot = 0) -> void   = 0;
        virtual auto bind_image_texture(uint32_t texture_id,
                                        uint32_t slot = 0,
                                        bool read_only = false) -> void             = 0;
        virtual auto read_pixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                 uint32_t format, uint32_t type, void* pixels) -> void = 0;

        inline static auto get_api() -> API         { return s_api; }
        inline static auto set_api(API api) -> void { s_api = api;  }
        static auto create() -> Scope<RendererAPI>;

    private:
        static API s_api;
    };

    class RenderCommand
    {
    public:
        inline static auto init() -> void
        {
            s_renderer_api->init();
        }

        inline static auto set_viewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) -> void
        {
            s_renderer_api->set_viewport(x, y, width, height);
        }

        inline static auto set_clear_color(const glm::vec4& color) -> void
        {
            s_renderer_api->set_clear_color(color);
        }

        inline static auto clear() -> void
        {
            s_renderer_api->clear();
        }

        inline static auto enable_depth_test() -> void
        {
            s_renderer_api->enable_depth_test();
        }

        inline static auto disable_depth_test() -> void
        {
            s_renderer_api->disable_depth_test();
        }

        inline static auto set_face_culling(bool enabled) -> void
        {
            s_renderer_api->set_face_culling(enabled);
        }

        inline static auto enable_blending() -> void
        {
            s_renderer_api->enable_blending();
        }

        inline static auto disable_blending() -> void
        {
            s_renderer_api->disable_blending();
        }

        inline static auto draw_indexed(const Ref<VertexArray>& vertex_array, uint32_t index_count = 0) -> void
        {
            s_renderer_api->draw_indexed(vertex_array, index_count);
        }

        inline static auto draw_arrays(uint32_t vertex_count, uint32_t first = 0) -> void
        {
            s_renderer_api->draw_arrays(vertex_count, first);
        }

        inline static auto draw_lines(const Ref<VertexArray>& vertex_array, uint32_t index_count = 0) -> void
        {
            s_renderer_api->draw_lines(vertex_array, index_count);
        }

        inline static auto bind_texture(uint32_t texture_id, uint32_t slot = 0) -> void
        {
            s_renderer_api->bind_texture(texture_id, slot);
        }

        inline static auto bind_image_texture(uint32_t texture_id, uint32_t slot = 0, bool read_only = false) -> void
        {
            s_renderer_api->bind_image_texture(texture_id, slot, read_only);
        }

        inline static auto read_pixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                       uint32_t format, uint32_t type, void* pixels) -> void
        {
            s_renderer_api->read_pixels(x, y, width, height, format, type, pixels);
        }

    private:
        static Scope<RendererAPI> s_renderer_api;
    };

    class Renderer
    {
    public:
        static auto init() -> void;
        static auto shutdown() -> void;

        static auto on_window_resize(uint32_t width, uint32_t height) -> void;

        static auto submit(const Ref<Shader>& shader,
                           const Ref<VertexArray>& vertex_array,
                           const glm::mat4& transform,
                           const glm::mat4& view_projection) -> void;

        static auto set_clear_color(const glm::vec4& color) -> void;
        static auto clear() -> void;

        inline static auto get_api() -> RendererAPI::API { return RendererAPI::get_api(); }
    };
};
