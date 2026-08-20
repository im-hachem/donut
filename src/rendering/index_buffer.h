#pragma once

#include <cstdint>

namespace Donut
{
    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        virtual auto bind() const -> void = 0;
        virtual auto unbind() const -> void = 0;
        virtual auto get_count() const -> uint32_t = 0;

        static auto create(const uint32_t* indices, uint32_t count) -> IndexBuffer*;
    };
};
