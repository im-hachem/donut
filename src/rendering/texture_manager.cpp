#include "texture_manager.h"
#include "renderer.h"

#include <glad/glad.h>

namespace Donut
{
    auto TextureManager::create_texture(uint32_t width, uint32_t height) -> Ref<Texture2D>
    {
        return create_ref<Texture2D>(width, height);
    }

    auto TextureManager::bind_texture(uint32_t texture_id, uint32_t slot) -> void
    {
        RenderCommand::bind_texture(texture_id, slot);
    }

    auto TextureManager::bind_image_texture(uint32_t texture_id, uint32_t slot, bool read_only) -> void
    {
        RenderCommand::bind_image_texture(texture_id, slot, read_only);
    }

    auto TextureManager::set_texture_data(uint32_t texture_id, void* data, uint32_t width, uint32_t height) -> void
    {
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    auto TextureManager::resize_texture(uint32_t texture_id, uint32_t width, uint32_t height) -> void
    {
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }
};
