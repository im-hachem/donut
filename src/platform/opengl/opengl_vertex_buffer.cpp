#include "opengl_vertex_buffer.h"
#include "rendering/vertex_buffer.h"

#include <glad/glad.h>

namespace Donut 
{
    OpenGLVertexBuffer::OpenGLVertexBuffer(const void* data, uint32_t size)
    {
        glGenBuffers(1, &m_renderer_id); // glCreateBuffers is 4.5 DSA; unavailable on macOS 4.1
        glBindBuffer(GL_ARRAY_BUFFER, m_renderer_id);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer()
    {
        glDeleteBuffers(1, &m_renderer_id);
    }

    auto OpenGLVertexBuffer::bind() const -> void
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_renderer_id);
    }

    auto OpenGLVertexBuffer::unbind() const -> void
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    auto OpenGLVertexBuffer::set_data(const void* data, uint32_t size) -> void
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_renderer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }
};
