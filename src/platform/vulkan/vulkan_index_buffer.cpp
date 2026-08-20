#include "vulkan_index_buffer.h"

namespace Donut
{
	VulkanIndexBuffer::VulkanIndexBuffer(uint32_t* indices, uint32_t count)
		: m_count(count)
	{
		// TODO(Hachem): Implement Vulkan index buffer creation
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		// TODO(Hachem): Implement Vulkan index buffer cleanup
	}

	auto VulkanIndexBuffer::bind() const -> void
	{
		// TODO(Hachem): Implement Vulkan index buffer binding
	}

	auto VulkanIndexBuffer::unbind() const -> void
	{
		// TODO(Hachem): Implement Vulkan index buffer unbinding
	}
};
