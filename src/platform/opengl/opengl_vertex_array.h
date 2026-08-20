#pragma once

#include "core/memory.h"

#include "rendering/vertex_array.h"
#include "rendering/vertex_buffer.h"
#include "rendering/index_buffer.h"

#include <vector>

namespace Donut
{
    class OpenGLVertexArray 
        : public VertexArray 
        {
    public:
        OpenGLVertexArray();
        virtual ~OpenGLVertexArray();

        virtual auto bind() const -> void override;
        virtual auto unbind() const -> void override;

        virtual auto add_vertex_buffer(const Ref<VertexBuffer>& vertex_buffer) -> void override;
        virtual auto set_index_buffer(const Ref<IndexBuffer>& index_buffer) -> void override;

        virtual const std::vector<Ref<VertexBuffer>>& get_vertex_buffers() const override 
        { 
            return m_vertex_buffers; 
        }
        
        virtual const Ref<IndexBuffer>& get_index_buffer() const override 
        { 
            return m_index_buffer; 
        }
    private:
        uint32_t m_renderer_id;
        uint32_t m_vertex_buffer_index = 0;
        std::vector<Ref<VertexBuffer>> m_vertex_buffers;
        Ref<IndexBuffer>               m_index_buffer;
    };
};
