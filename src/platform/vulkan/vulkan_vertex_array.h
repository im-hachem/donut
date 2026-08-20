#pragma once

#include "rendering/vertex_array.h"
#include "core/memory.h"

namespace Donut
{
	class VulkanVertexArray 
		: public VertexArray
	{
	public:
		VulkanVertexArray();
		virtual ~VulkanVertexArray();

		virtual auto bind() const -> void override;
		virtual auto unbind() const -> void override;

		virtual auto add_vertex_buffer(const Ref<VertexBuffer>& vertex_buffer) -> void override;
		virtual auto set_index_buffer(const Ref<IndexBuffer>& index_buffer) -> void override;

		virtual auto get_vertex_buffers() const -> const std::vector<Ref<VertexBuffer>>& { return m_vertex_buffers; }
		virtual auto get_index_buffer() const -> const Ref<IndexBuffer>& { return m_index_buffer;   }
	private:
		uint32_t                                   m_renderer_id;
		std::vector<Ref<VertexBuffer>> m_vertex_buffers;
		Ref<IndexBuffer>               m_index_buffer;
	};
};
