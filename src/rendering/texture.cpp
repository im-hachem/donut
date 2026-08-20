#include "texture.h"
#include "renderer.h"

#include "platform/opengl/opengl_texture.h"
#include "platform/vulkan/vulkan_texture.h"

namespace Donut
{
    auto Texture2D::create(uint32_t width, uint32_t height) -> Ref<Texture2D>
    {
        switch (Renderer::get_api())
        {
        case RendererAPI::API::OpenGL:
            return create_ref<OpenGLTexture2D>(width, height);
        case RendererAPI::API::Vulkan:
            return create_ref<VulkanTexture2D>(width, height);
        case RendererAPI::API::None:
            return nullptr;
        default:
            return nullptr;
        }
    }

    auto Texture2D::create(const std::string& path) -> Ref<Texture2D>
    {
        switch (Renderer::get_api())
        {
        case RendererAPI::API::OpenGL:
            return create_ref<OpenGLTexture2D>(path);
        case RendererAPI::API::Vulkan:
            return create_ref<VulkanTexture2D>(path);
        case RendererAPI::API::None:
            return nullptr;
        default:
            return nullptr;
        }
    }

    auto CubemapTexture::create(uint32_t width, uint32_t height) -> Ref<CubemapTexture>
    {
        switch (Renderer::get_api())
        {
        case RendererAPI::API::OpenGL:
            return create_ref<OpenGLCubemapTexture>(width, height);
        case RendererAPI::API::Vulkan:
            return create_ref<VulkanCubemapTexture>(width, height);
        case RendererAPI::API::None:
            return nullptr;
        default:
            return nullptr;
        }
    }

    auto CubemapTexture::create_from_hdri(const std::string& path) -> Ref<CubemapTexture>
    {
        switch (Renderer::get_api())
        {
        case RendererAPI::API::OpenGL:
            return create_ref<OpenGLCubemapTexture>(path);
        case RendererAPI::API::Vulkan:
            return create_ref<VulkanCubemapTexture>(path);
        case RendererAPI::API::None:
            return nullptr;
        default:
            return nullptr;
        }
    }
};