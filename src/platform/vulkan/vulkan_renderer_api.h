#pragma once

#include "core/memory.h"
#include "rendering/renderer.h"

namespace Donut
{
    class VulkanRendererAPI 
        : public RendererAPI
    {
    public:
        virtual auto init() -> void override;
        virtual void set_viewport(uint32_t x,     uint32_t y, 
                                 uint32_t width, uint32_t height) override;
        virtual auto set_clear_color(const glm::vec4& color) -> void override;
        virtual auto clear() -> void override;
        virtual auto enable_depth_test() -> void override;
        virtual auto disable_depth_test() -> void override;
        virtual auto set_face_culling(bool enabled) -> void override;
        virtual auto enable_blending() -> void override;
        virtual auto disable_blending() -> void override;

        virtual void draw_indexed(const Ref<VertexArray>& vertex_array, 
                                 uint32_t index_count = 0)         override;
        
        virtual void draw_arrays(uint32_t vertex_count, 
                                uint32_t first = 0)               override;
        virtual void draw_lines(const Ref<VertexArray>& vertex_array, 
                               uint32_t index_count = 0)           override;
        virtual void bind_texture(uint32_t texture_id, 
                                 uint32_t slot = 0)               override;
        virtual void bind_image_texture(uint32_t texture_id, 
                                      uint32_t slot = 0, 
                                      bool read_only = false)      override;
        virtual void read_pixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
                                uint32_t format, uint32_t type, void* pixels) override;
    };
};
