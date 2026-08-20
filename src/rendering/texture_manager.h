#pragma once

#include "core/memory.h"
#include "rendering/texture.h"
#include <cstdint>

namespace Donut
{
    class TextureManager
    {
    public:
        static auto create_texture(uint32_t width, uint32_t height) -> Ref<Texture2D>;
        
        static auto bind_texture(uint32_t texture_id, uint32_t slot = 0) -> void;
        static auto bind_image_texture(uint32_t texture_id, uint32_t slot = 0, bool read_only = false) -> void;
        static auto set_texture_data(uint32_t texture_id, void* data, uint32_t width, uint32_t height) -> void;
        static auto resize_texture(uint32_t texture_id, uint32_t width, uint32_t height) -> void;
    };
};
