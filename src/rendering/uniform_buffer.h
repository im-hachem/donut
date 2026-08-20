#pragma once

#include "core/memory.h"
#include <cstdint>

namespace Donut
{
    class UniformBuffer
    {
    public:
        UniformBuffer(uint32_t size, uint32_t binding);
        ~UniformBuffer();

        auto set_data(const void* data, uint32_t size, uint32_t offset = 0) -> void;
        auto bind(uint32_t binding) -> void;

        static auto create(uint32_t size, uint32_t binding) -> Ref<UniformBuffer>;

    private:
        uint32_t m_renderer_id = 0;
        uint32_t m_size    = 0;
        uint32_t m_binding = 0;
    };
};
