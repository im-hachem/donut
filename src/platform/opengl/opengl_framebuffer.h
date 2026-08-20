#pragma once

#include "rendering/framebuffer.h"

namespace Donut
{
    class OpenGLFramebuffer : public Framebuffer
    {
    public:
        OpenGLFramebuffer(const FramebufferSpecification& spec);
        virtual ~OpenGLFramebuffer();

        auto invalidate() -> void;

        virtual auto bind() -> void override;
        virtual auto unbind() -> void override;

        virtual auto resize(uint32_t width, uint32_t height) -> void override;
        virtual auto read_pixel(uint32_t attachment_index, int x, int y) -> int override;

        virtual auto clear_attachment(uint32_t attachment_index, int value) -> void override;
        virtual auto get_color_attachment_renderer_id(uint32_t index = 0) const -> uint32_t override{ return m_color_attachments[index]; }

        virtual auto get_specification() const -> const FramebufferSpecification& override{ return m_specification; }
    private:
        uint32_t m_renderer_id = 0;
        FramebufferSpecification m_specification;

        std::vector<FramebufferTextureSpecification> m_color_attachment_specifications;
        FramebufferTextureSpecification m_depth_attachment_specification = FramebufferTextureFormat::None;

        std::vector<uint32_t> m_color_attachments;
        uint32_t m_depth_attachment = 0;
    };
};
