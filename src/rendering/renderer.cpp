#include "renderer.h"

#include "platform/opengl/opengl_renderer_api.h"
#include "platform/vulkan/vulkan_renderer_api.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Donut 
{
    auto RendererAPI::create() -> Scope<RendererAPI>
     {
        switch (s_api)
        {
            case API::OpenGL:
                return create_scope<OpenGLRendererAPI>();
            case API::Vulkan:
                return create_scope<VulkanRendererAPI>();
            default:
                return nullptr;
        }
    }

    RendererAPI::API RendererAPI::s_api = RendererAPI::API::OpenGL;

    auto Renderer::init() -> void
    {
        RenderCommand::init();
        RenderCommand::enable_depth_test();
    }

    auto Renderer::shutdown() -> void
    {
    }

    auto Renderer::on_window_resize(uint32_t width, uint32_t height) -> void
    {
        RenderCommand::set_viewport(0, 0, width, height);
    }

    auto Renderer::submit(const Ref<Shader>& shader,
                          const Ref<VertexArray>& vertex_array,
                          const glm::mat4& transform,
                          const glm::mat4& view_projection) -> void
    {
        shader->bind();
        shader->set_mat4("u_ViewProjection", view_projection);
        shader->set_mat4("u_Transform", transform);

        vertex_array->bind();
        RenderCommand::draw_indexed(vertex_array);
    }

    Scope<RendererAPI> RenderCommand::s_renderer_api = RendererAPI::create();

    auto Renderer::set_clear_color(const glm::vec4& color) -> void
    {
        RenderCommand::set_clear_color(color);
    }

    auto Renderer::clear() -> void
    {
        RenderCommand::clear();
    }
};
