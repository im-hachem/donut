#include "opengl_index_buffer.h"
#include <glad/glad.h>

namespace Donut
{
    OpenGLIndexBuffer::OpenGLIndexBuffer(const uint32_t* indices, uint32_t count)
        : m_count(count)
    {
        glGenBuffers(1, &m_renderer_id); // glCreateBuffers is 4.5 DSA; unavailable on macOS 4.1
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_renderer_id);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer() 
    {
        glDeleteBuffers(1, &m_renderer_id);
    }

    auto OpenGLIndexBuffer::bind() const -> void
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_renderer_id);
    }

    auto OpenGLIndexBuffer::unbind() const -> void
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
};
