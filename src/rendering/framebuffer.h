#pragma once

#include "core/memory.h"
#include "texture.h"

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
        virtual ~Framebuffer() = default;

        virtual auto bind() -> void = 0;
        virtual auto unbind() -> void = 0;

        virtual auto resize(uint32_t width, uint32_t height) -> void = 0;
        virtual auto read_pixel(uint32_t attachment_index, int x, int y) -> int = 0;

        virtual auto clear_attachment(uint32_t attachment_index, int value) -> void = 0;
        virtual auto get_color_attachment_renderer_id(uint32_t index = 0) const -> uint32_t = 0;

        virtual auto get_specification() const -> const FramebufferSpecification& = 0;

        static auto create(const FramebufferSpecification& spec) -> Ref<Framebuffer>;
    };
};
