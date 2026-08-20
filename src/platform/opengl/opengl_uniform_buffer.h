#pragma once

#include "rendering/uniform_buffer.h"
#include <glad/glad.h>

namespace Donut
{
    class OpenGLUniformBuffer : public UniformBuffer
    {
    public:
        OpenGLUniformBuffer(uint32_t size, uint32_t binding);
        virtual ~OpenGLUniformBuffer();

        virtual auto set_data(const void* data, uint32_t size, uint32_t offset = 0) -> void override;
        virtual auto bind(uint32_t binding) -> void override;
    private:
        uint32_t m_renderer_id = 0;
        uint32_t m_size       = 0;
        uint32_t m_binding    = 0;
    };
};
