#pragma once

#include <cstdint>
#include <vector>

namespace Donut
{
    struct VertexBufferElement
    {
        uint32_t type;
        uint32_t count;
        uint8_t normalized;
        uint32_t offset;

        static auto get_size_of_type(uint32_t type) -> uint32_t;
    };

    class VertexBufferLayout
    {
    public:
        VertexBufferLayout() = default;
        ~VertexBufferLayout() = default;

        template<typename T>
        auto push(uint32_t count) -> void
        {
            static_assert(false);
        }

        template<>
        void push<float>(uint32_t count)
        {
            m_elements.push_back({ 0x1406, count, 0, m_stride }); // GL_FLOAT
            m_stride += count * VertexBufferElement::get_size_of_type(0x1406);
        }

        template<>
        void push<uint32_t>(uint32_t count)
        {
            m_elements.push_back({ 0x1405, count, 0, m_stride }); // GL_UNSIGNED_INT
            m_stride += count * VertexBufferElement::get_size_of_type(0x1405);
        }

        template<>
        void push<uint8_t>(uint32_t count)
        {
            m_elements.push_back({ 0x1401, count, 1, m_stride }); // GL_UNSIGNED_BYTE
            m_stride += count * VertexBufferElement::get_size_of_type(0x1401);
        }

        inline auto get_elements() const -> const std::vector<VertexBufferElement>& { return m_elements; }
        inline auto get_stride() const -> uint32_t { return m_stride; }

    private:
        std::vector<VertexBufferElement> m_elements;
        uint32_t m_stride = 0;
    };

    class VertexBuffer
    {
    public:
        VertexBuffer(const void* data, uint32_t size);
        ~VertexBuffer();

        auto bind() const -> void;
        auto unbind() const -> void;
        auto set_data(const void* data, uint32_t size) -> void;

        auto get_layout() const -> const VertexBufferLayout& { return m_layout; }
        auto set_layout(const VertexBufferLayout& layout) -> void { m_layout = layout; }

        static auto create(const void* data, uint32_t size) -> VertexBuffer*;

    private:
        uint32_t           m_renderer_id = 0;
        VertexBufferLayout m_layout;
    };
};
