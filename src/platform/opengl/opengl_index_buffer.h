#pragma once

#include "rendering/index_buffer.h"

namespace Donut
{
    class OpenGLIndexBuffer
        : public IndexBuffer 
    {
    public:
        OpenGLIndexBuffer(const uint32_t* indices, uint32_t count);
        virtual ~OpenGLIndexBuffer();

        virtual auto bind() const -> void override;
        virtual auto unbind() const -> void override;
        virtual auto get_count() const -> uint32_t override{ return m_count; }

    private:
        uint32_t m_renderer_id;
        uint32_t m_count;
    };
};
