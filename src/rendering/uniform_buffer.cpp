#include "uniform_buffer.h"

#include <glad/glad.h>

namespace Donut
{
    UniformBuffer::UniformBuffer(uint32_t size, uint32_t binding)
        : m_size(size), m_binding(binding)
    {
        glGenBuffers(1, &m_renderer_id);
        glBindBuffer(GL_UNIFORM_BUFFER, m_renderer_id);
        glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_renderer_id);
    }

    UniformBuffer::~UniformBuffer()
    {
        glDeleteBuffers(1, &m_renderer_id);
    }

    auto UniformBuffer::set_data(const void* data, uint32_t size, uint32_t offset) -> void
    {
        glBindBuffer(GL_UNIFORM_BUFFER, m_renderer_id);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    }

    auto UniformBuffer::bind(uint32_t binding) -> void
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_renderer_id);
    }

    auto UniformBuffer::create(uint32_t size, uint32_t binding) -> Ref<UniformBuffer>
    {
        return create_ref<UniformBuffer>(size, binding);
    }
};
