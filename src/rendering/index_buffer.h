#pragma once

#include <cstdint>

namespace Donut
{
    class IndexBuffer
    {
    public:
        IndexBuffer(const uint32_t* indices, uint32_t count);
        ~IndexBuffer();

        auto bind() const -> void;
        auto unbind() const -> void;
        auto get_count() const -> uint32_t { return m_count; }

        static auto create(const uint32_t* indices, uint32_t count) -> IndexBuffer*;

    private:
        uint32_t m_renderer_id = 0;
        uint32_t m_count = 0;
    };
};
