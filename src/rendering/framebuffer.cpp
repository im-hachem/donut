#include "framebuffer.h"
#include "renderer.h"

#include "platform/opengl/opengl_framebuffer.h"

namespace Donut
{
    auto Framebuffer::create(const FramebufferSpecification& spec) -> Ref<Framebuffer>
    {
        switch (Renderer::get_api())
        {
            case RendererAPI::API::OpenGL:  return create_ref<OpenGLFramebuffer>(spec);
        }

        return nullptr;
    }
};
