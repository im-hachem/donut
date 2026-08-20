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
        virtual ~VertexArray() = default;

        virtual auto bind() const -> void = 0;
        virtual auto unbind() const -> void = 0;

        virtual auto add_vertex_buffer(const Ref<VertexBuffer>& vertex_buffer) -> void = 0;
        virtual auto set_index_buffer(const Ref<IndexBuffer>& index_buffer) -> void = 0;

        virtual auto get_vertex_buffers() const -> const std::vector<Ref<VertexBuffer>>& = 0;
        virtual auto get_index_buffer() const -> const Ref<IndexBuffer>& = 0;

        static auto create() -> VertexArray*;
    };
};
