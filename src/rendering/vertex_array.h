#pragma once

#include "core/memory.h"

#include "vertex_buffer.h"
#include "index_buffer.h"

#include <vector>

namespace Donut
{
    class VertexArray
    {
    public:
        VertexArray();
        ~VertexArray();

        auto bind() const -> void;
        auto unbind() const -> void;

        auto add_vertex_buffer(const Ref<VertexBuffer>& vertex_buffer) -> void;
        auto set_index_buffer(const Ref<IndexBuffer>& index_buffer) -> void;

        auto get_vertex_buffers() const -> const std::vector<Ref<VertexBuffer>>& { return m_vertex_buffers; }
        auto get_index_buffer() const -> const Ref<IndexBuffer>& { return m_index_buffer; }

        static auto create() -> VertexArray*;

    private:
        uint32_t m_renderer_id = 0;
        uint32_t m_vertex_buffer_index = 0;
        std::vector<Ref<VertexBuffer>> m_vertex_buffers;
        Ref<IndexBuffer>               m_index_buffer;
    };
};
