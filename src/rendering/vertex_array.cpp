#include <glad/glad.h>

#include "vertex_array.h"

namespace Donut
{
    VertexArray::VertexArray()
    {
        // glCreateVertexArrays is 4.5 DSA; macOS caps at 4.1. glGenVertexArrays
        // reserves the name and the VAO is created on first bind (done below).
        glGenVertexArrays(1, &m_renderer_id);
    }

    VertexArray::~VertexArray()
    {
        glDeleteVertexArrays(1, &m_renderer_id);
    }

    auto VertexArray::bind() const -> void
    {
        glBindVertexArray(m_renderer_id);
    }

    auto VertexArray::unbind() const -> void
    {
        glBindVertexArray(0);
    }

    auto VertexArray::add_vertex_buffer(const Ref<VertexBuffer>& vertex_buffer) -> void
    {
        glBindVertexArray(m_renderer_id);
        vertex_buffer->bind();

        const auto& layout = vertex_buffer->get_layout();
        for (const auto& element : layout.get_elements())
        {
            glEnableVertexAttribArray(m_vertex_buffer_index);
            glVertexAttribPointer(m_vertex_buffer_index,
                element.count,
                element.type,
                element.normalized ? GL_TRUE : GL_FALSE,
                layout.get_stride(),
                reinterpret_cast<const   void*>(static_cast<uintptr_t>(element.offset)));
            m_vertex_buffer_index++;
        }

        m_vertex_buffers.push_back(vertex_buffer);
    }

    auto VertexArray::set_index_buffer(const Ref<IndexBuffer>& index_buffer) -> void
    {
        glBindVertexArray(m_renderer_id);
        index_buffer->bind();
        m_index_buffer = index_buffer;
    }

    auto VertexArray::create() -> VertexArray*
    {
        return new VertexArray();
    }
};
