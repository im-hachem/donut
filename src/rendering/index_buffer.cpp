#include "index_buffer.h"
#include "renderer.h"

#include "platform/opengl/opengl_index_buffer.h"
#include "platform/vulkan/vulkan_index_buffer.h"

namespace Donut 
{
    auto IndexBuffer::create(const uint32_t* indices, uint32_t count) -> IndexBuffer*
    {
        switch (Renderer::get_api()) 
        {
            case RendererAPI::API::OpenGL:
                return new OpenGLIndexBuffer(indices, count);
            case RendererAPI::API::Vulkan:
                return new VulkanIndexBuffer((uint32_t*)indices, count);
            default:
                return nullptr;
        }
    }
};
