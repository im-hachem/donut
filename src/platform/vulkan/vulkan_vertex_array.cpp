#include "vulkan_vertex_array.h"

namespace Donut
{
	VulkanVertexArray::VulkanVertexArray()
	{
		// TODO(Hachem): Implement Vulkan vertex array creation
	}

	VulkanVertexArray::~VulkanVertexArray()
	{
		// TODO(Hachem): Implement Vulkan vertex array cleanup
	}

	auto VulkanVertexArray::bind() const -> void
	{
		// TODO(Hachem): Implement Vulkan vertex array binding
	}

	auto VulkanVertexArray::unbind() const -> void
	{
		// TODO(Hachem): Implement Vulkan vertex array unbinding
	}

	auto VulkanVertexArray::add_vertex_buffer(const Ref<VertexBuffer>& vertex_buffer) -> void
	{
		// TODO(Hachem): Implement Vulkan vertex buffer addition
		m_vertex_buffers.push_back(vertex_buffer);
	}

	auto VulkanVertexArray::set_index_buffer(const Ref<IndexBuffer>& index_buffer) -> void
	{
		// TODO(Hachem): Implement Vulkan index buffer setting
		m_index_buffer = index_buffer;
	}
};
