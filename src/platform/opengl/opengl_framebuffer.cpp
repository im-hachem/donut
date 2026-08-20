#include "opengl_framebuffer.h"
#include "core/log.h"

#include <glad/glad.h>

namespace Donut
{
    namespace Utils
    {
        static GLenum TextureTarget(bool multisampled)
        {
            return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        }

        static void bind_texture(bool multisampled, uint32_t id)
        {
            glBindTexture(TextureTarget(multisampled), id);
        }

        static void CreateTextures(bool multisampled, uint32_t* out_id, uint32_t count)
        {
            // glCreateTextures is 4.5 DSA; macOS caps at 4.1. Callers bind each
            // texture (with the correct target) before use.
            glGenTextures(count, out_id);
        }

        static void AttachColorTexture(uint32_t id, int samples, GLenum internal_format, GLenum format, uint32_t width, uint32_t height, int index)
        {
            bool multisampled = samples > 1;
            if (multisampled)
            {
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internal_format, width, height, GL_FALSE);
            }
            else
            {
                // Integer color formats require an integer pixel type even when
                // data is null, or macOS's strict core profile rejects the call.
                GLenum type = (format == GL_RED_INTEGER) ? GL_INT : GL_UNSIGNED_BYTE;
                glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
        }

        static void AttachDepthTexture(uint32_t id, int samples, GLenum format, GLenum attachment_type, uint32_t width, uint32_t height)
        {
            bool multisampled = samples > 1;
            if (multisampled)
            {
                glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
            }
            else
            {
                // glTexStorage2D is 4.2; use mutable storage for macOS (4.1).
                GLenum depth_format = (format == GL_DEPTH24_STENCIL8) ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT;
                GLenum depth_type   = (format == GL_DEPTH24_STENCIL8) ? GL_UNSIGNED_INT_24_8 : GL_FLOAT;
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, depth_format, depth_type, nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_type, TextureTarget(multisampled), id, 0);
        }

        static bool IsDepthFormat(FramebufferTextureFormat format)
        {
            switch (format)
            {
                case FramebufferTextureFormat::DEPTH24STENCIL8:  return true;
            }
            return false;
        }

        static GLenum DonutFBTextureFormatToGL(FramebufferTextureFormat format)
        {
            switch (format)
            {
                case FramebufferTextureFormat::RGBA8:       return GL_RGBA8;
                case FramebufferTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
            }

            return 0;
        }
    }

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
        : m_specification(spec)
    {
        for (auto spec : m_specification.attachments.attachments)
        {
            if (!Utils::IsDepthFormat(spec.texture_format))
                m_color_attachment_specifications.emplace_back(spec);
            else
                m_depth_attachment_specification = spec;
        }

        invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        glDeleteFramebuffers(1, &m_renderer_id);
        glDeleteTextures(static_cast<GLsizei>(m_color_attachments.size()), m_color_attachments.data());
        glDeleteTextures(1, &m_depth_attachment);
    }

    auto OpenGLFramebuffer::invalidate() -> void
    {
        if (m_renderer_id)
        {
            glDeleteFramebuffers(1, &m_renderer_id);
            glDeleteTextures(static_cast<GLsizei>(m_color_attachments.size()), m_color_attachments.data());
            glDeleteTextures(1, &m_depth_attachment);

            m_color_attachments.clear();
            m_depth_attachment = 0;
        }

        glGenFramebuffers(1, &m_renderer_id); // glCreateFramebuffers is 4.5 DSA; unavailable on macOS 4.1
        glBindFramebuffer(GL_FRAMEBUFFER, m_renderer_id);

        bool multisample = m_specification.Samples > 1;

        if (m_color_attachment_specifications.size())
        {
            m_color_attachments.resize(m_color_attachment_specifications.size());
            Utils::CreateTextures(multisample, m_color_attachments.data(), static_cast<uint32_t>(m_color_attachments.size()));

            for (size_t i = 0; i < m_color_attachments.size(); i++)
            {
                Utils::bind_texture(multisample, m_color_attachments[i]);
                switch (m_color_attachment_specifications[i].texture_format)
                {
                    case FramebufferTextureFormat::RGBA8:
                        Utils::AttachColorTexture(m_color_attachments[i], m_specification.Samples, GL_RGBA8, GL_RGBA, m_specification.Width, m_specification.Height, static_cast<int>(i));
                        break;
                    case FramebufferTextureFormat::RED_INTEGER:
                        Utils::AttachColorTexture(m_color_attachments[i], m_specification.Samples, GL_R32I, GL_RED_INTEGER, m_specification.Width, m_specification.Height, static_cast<int>(i));
                        break;
                }
            }
        }

        if (m_depth_attachment_specification.texture_format != FramebufferTextureFormat::None)
        {
            Utils::CreateTextures(multisample, &m_depth_attachment, 1);
            Utils::bind_texture(multisample, m_depth_attachment);
            switch (m_depth_attachment_specification.texture_format)
            {
                case FramebufferTextureFormat::DEPTH24STENCIL8:
                    Utils::AttachDepthTexture(m_depth_attachment, m_specification.Samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_specification.Width, m_specification.Height);
                    break;
            }
        }

        if (m_color_attachments.size() > 1)
        {
            GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(static_cast<GLsizei>(m_color_attachments.size()), buffers);
        }
        else if (m_color_attachments.empty())
            glDrawBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            switch (status)
            {
                case GL_FRAMEBUFFER_UNDEFINED:
                    DONUT_ERROR("Framebuffer is undefined");
                    break;
                case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                    DONUT_ERROR("Framebuffer has incomplete attachment");
                    break;
                case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                    DONUT_ERROR("Framebuffer is missing attachment");
                    break;
                case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                    DONUT_ERROR("Framebuffer has incomplete draw buffer");
                    break;
                case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                    DONUT_ERROR("Framebuffer has incomplete read buffer");
                    break;
                case GL_FRAMEBUFFER_UNSUPPORTED:
                    DONUT_ERROR("Framebuffer format is unsupported");
                    break;
                case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
                    DONUT_ERROR("Framebuffer has incomplete multisample");
                    break;
                case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
                    DONUT_ERROR("Framebuffer has incomplete layer targets");
                    break;
                default:
                    DONUT_ERROR("Framebuffer is incomplete (unknown error: {})", status);
                    break;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    auto OpenGLFramebuffer::bind() -> void
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_renderer_id);
        glViewport(0, 0, m_specification.Width, m_specification.Height);
    }

    auto OpenGLFramebuffer::unbind() -> void
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    auto OpenGLFramebuffer::resize(uint32_t width, uint32_t height) -> void
    {
        m_specification.Width = width;
        m_specification.Height = height;

        invalidate();
    }

    auto OpenGLFramebuffer::read_pixel(uint32_t attachment_index, int x, int y) -> int
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_index);
        int pixel_data;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixel_data);
        return pixel_data;
    }

    auto OpenGLFramebuffer::clear_attachment(uint32_t attachment_index, int value) -> void
    {
        // glClearTexImage is 4.4 and unavailable on macOS. clear the integer
        // attachment by binding this framebuffer and clearing its draw buffer.
        glBindFramebuffer(GL_FRAMEBUFFER, m_renderer_id);
        glClearBufferiv(GL_COLOR, static_cast<GLint>(attachment_index), &value);
    }
};
