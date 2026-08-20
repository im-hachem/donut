#pragma once

#include "rendering/vertex_buffer.h"

namespace Donut
{
	class VulkanVertexBuffer
        : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(uint32_t size);
		VulkanVertexBuffer(float* vertices, uint32_t size);
		virtual ~VulkanVertexBuffer();

		virtual auto bind() const -> void override;
		virtual auto unbind() const -> void override;

		virtual auto set_data(const void* data, uint32_t size) -> void override;

		virtual auto get_layout() const -> const VertexBufferLayout& override{ return m_layout;  }
		virtual auto set_layout(const VertexBufferLayout& layout) -> void override{ m_layout = layout; }
	private:
		uint32_t           m_renderer_id;
		VertexBufferLayout m_layout;
	};
};
