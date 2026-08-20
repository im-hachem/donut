#pragma once

#include "rendering/uniform_buffer.h"

namespace Donut
{
    class VulkanUniformBuffer : public UniformBuffer
    {
    public:
        VulkanUniformBuffer(uint32_t size, uint32_t binding);
        virtual ~VulkanUniformBuffer();

        virtual auto set_data(const void* data, uint32_t size, uint32_t offset = 0) -> void override;
        virtual auto bind(uint32_t binding) -> void override;
    private:
        uint32_t m_size;
        uint32_t m_binding;
    };
};
