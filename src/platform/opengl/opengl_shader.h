#pragma once

#include "rendering/shader.h"

#include <unordered_map>
#include <glm/glm.hpp>

namespace Donut 
{
    class OpenGLShader 
        : public Shader 
    {
    public:
        OpenGLShader(const std::string& filepath);
        OpenGLShader(const std::string& name, const std::string& vertex_src, const std::string& fragment_src);
        OpenGLShader(const std::string& name, const std::string& compute_src);
        virtual ~OpenGLShader();

        virtual auto bind() const -> void override;
        virtual auto unbind() const -> void override;

        virtual auto set_int(     const std::string& name, int value) -> void override;
        virtual auto set_int_array(const std::string& name, int* values, uint32_t count) -> void override;
        virtual auto set_float(   const std::string& name, float value) -> void override;
        virtual auto set_float2(  const std::string& name, const glm::vec2& value) -> void override;
        virtual auto set_float3(  const std::string& name, const glm::vec3& value) -> void override;
        virtual auto set_float4(  const std::string& name, const glm::vec4& value) -> void override;
        virtual auto set_mat4(    const std::string& name, const glm::mat4& value) -> void override;

        virtual auto dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) -> void override;
        virtual auto dispatch_indirect(uint32_t offset = 0) -> void override;
        virtual auto memory_barrier(uint32_t barriers) -> void override;

        virtual auto get_name() const -> const std::string& override{ return m_name; }
        virtual auto get_renderer_id() const -> uint32_t override{ return m_renderer_id; }

        auto upload_uniform_int(     const std::string& name, int value) -> void;
        auto upload_uniform_int_array(const std::string& name, int* values, uint32_t count) -> void;
        auto upload_uniform_float(   const std::string& name, float value) -> void;
        auto upload_uniform_float2(  const std::string& name, const glm::vec2& value) -> void;
        auto upload_uniform_float3(  const std::string& name, const glm::vec3& value) -> void;
        auto upload_uniform_float4(  const std::string& name, const glm::vec4& value) -> void;
        auto upload_uniform_mat3(    const std::string& name, const glm::mat3& matrix) -> void;
        auto upload_uniform_mat4(    const std::string& name, const glm::mat4& matrix) -> void;

    private:
        auto read_file(const std::string& filepath) -> std::string;
        auto pre_process(const std::string& source) -> std::unordered_map<uint32_t, std::string>;
        auto compile(const std::unordered_map<uint32_t, std::string>& shader_sources) -> void;
    private:
        uint32_t    m_renderer_id = 0;
        std::string m_name;
        // True when loaded from a Slang-compiled GLSL. Slang expects row-major
        // matrix data, so matrix uniforms are transposed on upload (glm is
        // column-major) to keep all matrix math correct.
        bool        m_is_slang = false;
    };
};
