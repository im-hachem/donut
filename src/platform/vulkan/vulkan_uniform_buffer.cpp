#include "vulkan_uniform_buffer.h"

namespace Donut
{
    VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size, uint32_t binding)
        : m_size(size), m_binding(binding)
    {
        // TODO: Implement Vulkan uniform buffer
    }

    VulkanUniformBuffer::~VulkanUniformBuffer()
    {
        // TODO: Implement Vulkan uniform buffer cleanup
    }

    auto VulkanUniformBuffer::set_data(const void* data, uint32_t size, uint32_t offset) -> void
    {
        // TODO: Implement Vulkan uniform buffer data setting
    }

    auto VulkanUniformBuffer::bind(uint32_t binding) -> void
    {
        // TODO: Implement Vulkan uniform buffer binding
    }
};
