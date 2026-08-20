#pragma once

#include "core/memory.h"
#include <cstdint>

namespace Donut
{
    class UniformBuffer
    {
    public:
        virtual ~UniformBuffer() = default;

        virtual auto set_data(const void* data, uint32_t size, uint32_t offset = 0) -> void = 0;
        virtual auto bind(uint32_t binding) -> void = 0;

        static auto create(uint32_t size, uint32_t binding) -> Ref<UniformBuffer>;
    };
};
