#pragma once

#include "core/memory.h"
#include "texture.h"

#include <vector>

namespace Donut
{
    enum class FramebufferTextureFormat
    {
        None = 0,

        RGBA8,
        RED_INTEGER,

        DEPTH24STENCIL8,

        Depth = DEPTH24STENCIL8
    };

    struct FramebufferTextureSpecification
    {
        FramebufferTextureSpecification() = default;
        FramebufferTextureSpecification(FramebufferTextureFormat format)
            : texture_format(format) {}

        FramebufferTextureFormat texture_format = FramebufferTextureFormat::None;
    };

    struct FramebufferAttachmentSpecification
    {
        FramebufferAttachmentSpecification() = default;
        FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
            : attachments(attachments) {}

        std::vector<FramebufferTextureSpecification> attachments;
    };

    struct FramebufferSpecification
    {
        uint32_t Width = 0, Height = 0;
        FramebufferAttachmentSpecification attachments;
        uint32_t Samples = 1;

        bool SwapChainTarget = false;
    };

    class Framebuffer
    {
    public:
        Framebuffer(const FramebufferSpecification& spec);
        ~Framebuffer();

        auto bind() -> void;
        auto unbind() -> void;

        auto resize(uint32_t width, uint32_t height) -> void;
        auto read_pixel(uint32_t attachment_index, int x, int y) -> int;

        auto clear_attachment(uint32_t attachment_index, int value) -> void;
        auto get_color_attachment_renderer_id(uint32_t index = 0) const -> uint32_t { return m_color_attachments[index]; }

        auto get_specification() const -> const FramebufferSpecification& { return m_specification; }

        static auto create(const FramebufferSpecification& spec) -> Ref<Framebuffer>;

    private:
        auto invalidate() -> void;

        uint32_t m_renderer_id = 0;
        FramebufferSpecification m_specification;

        std::vector<FramebufferTextureSpecification> m_color_attachment_specifications;
        FramebufferTextureSpecification m_depth_attachment_specification = FramebufferTextureFormat::None;

        std::vector<uint32_t> m_color_attachments;
        uint32_t m_depth_attachment = 0;
    };
};
