#include "uniform_buffer.h"
#include "renderer.h"

#include "platform/opengl/opengl_uniform_buffer.h"
#include "platform/vulkan/vulkan_uniform_buffer.h"

namespace Donut
{
    auto UniformBuffer::create(uint32_t size, uint32_t binding) -> Ref<UniformBuffer>
    {
        switch (Renderer::get_api())
        {
        case RendererAPI::API::OpenGL:
            return create_ref<OpenGLUniformBuffer>(size, binding);
        case RendererAPI::API::Vulkan:
            return create_ref<VulkanUniformBuffer>(size, binding);
        case RendererAPI::API::None:
            return nullptr;
        default:
            return nullptr;
        }
    }
};
