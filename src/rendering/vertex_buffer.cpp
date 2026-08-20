#include "vertex_buffer.h"

#include <glad/glad.h>

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

    VertexBuffer::VertexBuffer(const void* data, uint32_t size)
    {
        glGenBuffers(1, &m_renderer_id); // glCreateBuffers is 4.5 DSA; unavailable on macOS 4.1
        glBindBuffer(GL_ARRAY_BUFFER, m_renderer_id);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }

    VertexBuffer::~VertexBuffer()
    {
        glDeleteBuffers(1, &m_renderer_id);
    }

    auto VertexBuffer::bind() const -> void
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_renderer_id);
    }

    auto VertexBuffer::unbind() const -> void
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    auto VertexBuffer::set_data(const void* data, uint32_t size) -> void
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_renderer_id);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }

    auto VertexBuffer::create(const void* data, uint32_t size) -> VertexBuffer*
    {
        return new VertexBuffer(data, size);
    }
};
