#include "vertex_array.h"
#include "renderer.h"

#include "platform/opengl/opengl_vertex_array.h"
#include "platform/vulkan/vulkan_vertex_array.h"

namespace Donut
{
    auto VertexBufferElement::get_size_of_type(uint32_t type) -> uint32_t
    {
        switch (type) 
        {
            case 0x1406: return 4; // GL_FLOAT
            case 0x1405: return 4; // GL_UNSIGNED_INT
            case 0x1401: return 1; // GL_UNSIGNED_BYTE
            default: return 0;
        }
    }

    auto VertexArray::create() -> VertexArray*
    {
        switch (Renderer::get_api()) 
        {
            case RendererAPI::API::OpenGL:
                return new OpenGLVertexArray();
            case RendererAPI::API::Vulkan:
                return new VulkanVertexArray();
            default:
                return nullptr;
        }
    }
};
