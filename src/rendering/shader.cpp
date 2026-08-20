#include "shader.h"
#include "renderer.h"

#include "platform/opengl/opengl_shader.h"
#include "platform/vulkan/vulkan_shader.h"

namespace Donut
{
    auto Shader::create(const std::string& filepath) -> Shader*
    {
        switch (Renderer::get_api())
        {
            case RendererAPI::API::OpenGL:
                return new OpenGLShader(filepath);
            case RendererAPI::API::Vulkan:
                return new VulkanShader(filepath);
            default:
                return nullptr;
        }
    }

    auto Shader::create(const std::string& name, const std::string& vertex_src, const std::string& fragment_src) -> Shader*
    {
        switch (Renderer::get_api()) 
        {
            case RendererAPI::API::OpenGL:
                return new OpenGLShader(name, vertex_src, fragment_src);
            case RendererAPI::API::Vulkan:
                return new VulkanShader(name, vertex_src, fragment_src);
            default:
                return nullptr;
        }
    }

    auto Shader::create_compute(const std::string& name, const std::string& compute_src) -> Shader*
    {
        switch (Renderer::get_api()) 
        {
            case RendererAPI::API::OpenGL:
                return new OpenGLShader(name, compute_src);
            case RendererAPI::API::Vulkan:
                return new VulkanShader(name, compute_src);
            default:
                return nullptr;
        }
    }

    auto ShaderLibrary::add(const Ref<Shader>& shader) -> void
    {
        auto& name = shader->get_name();
        add(name, shader);
    }

    auto ShaderLibrary::add(const std::string& name, const Ref<Shader>& shader) -> void
    {
        m_shaders[name] = shader;
    }

    auto ShaderLibrary::load(const std::string& filepath) -> Ref<Shader>
    {
        auto shader = Ref<Shader>(Shader::create(filepath));
        add(shader);
        return shader;
    }

    auto ShaderLibrary::load(const std::string& name, const std::string& filepath) -> Ref<Shader>
    {
        auto shader = Ref<Shader>(Shader::create(filepath));
        add(name, shader);
        return shader;
    }

    auto ShaderLibrary::Get(const std::string& name) -> Ref<Shader>
    {
        if (exists(name))
            return m_shaders[name];
        return nullptr;
    }

    auto ShaderLibrary::exists(const std::string& name) const -> bool
    {
        return m_shaders.find(name) != m_shaders.end();
    }
};
