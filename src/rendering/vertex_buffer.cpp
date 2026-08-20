#include "vertex_buffer.h"
#include "renderer.h"

#include "platform/opengl/opengl_vertex_buffer.h"
#include "platform/vulkan/vulkan_vertex_buffer.h"

namespace Donut
{
    auto VertexBuffer::create(const void* data, uint32_t size) -> VertexBuffer*
    {
        switch (Renderer::get_api()) 
        {
            case RendererAPI::API::OpenGL:
                return new OpenGLVertexBuffer(data, size);
            case RendererAPI::API::Vulkan:
                return new VulkanVertexBuffer((float*)data, size);
            default:
                return nullptr;
        }
    }
};
