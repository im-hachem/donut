#pragma once

#include "core/memory.h"

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

#define SHADER_STORAGE_BARRIER_BIT    0x00002000
#define UNIFORM_BARRIER_BIT           0x00000004
#define TEXTURE_FETCH_BARRIER_BIT     0x00000008
#define IMAGE_ACCESS_BARRIER_BIT      0x00000020

namespace Donut
{
    class Shader
    {
    public:
        Shader(const std::string& filepath);
        Shader(const std::string& name, const std::string& vertex_src, const std::string& fragment_src);
        Shader(const std::string& name, const std::string& compute_src);
        ~Shader();

        auto bind() const -> void;
        auto unbind() const -> void;

        auto set_int(     const std::string& name, int value) -> void;
        auto set_int_array(const std::string& name, int* values, uint32_t count) -> void;
        auto set_float(   const std::string& name, float value) -> void;
        auto set_float2(  const std::string& name, const glm::vec2& value) -> void;
        auto set_float3(  const std::string& name, const glm::vec3& value) -> void;
        auto set_float4(  const std::string& name, const glm::vec4& value) -> void;
        auto set_mat4(    const std::string& name, const glm::mat4& value) -> void;

        auto dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) -> void;
        auto dispatch_indirect(uint32_t offset = 0) -> void;
        auto memory_barrier(uint32_t barriers) -> void;

        auto get_name() const -> const std::string& { return m_name; }
        auto get_renderer_id() const -> uint32_t { return m_renderer_id; }

        auto upload_uniform_int(     const std::string& name, int value) -> void;
        auto upload_uniform_int_array(const std::string& name, int* values, uint32_t count) -> void;
        auto upload_uniform_float(   const std::string& name, float value) -> void;
        auto upload_uniform_float2(  const std::string& name, const glm::vec2& value) -> void;
        auto upload_uniform_float3(  const std::string& name, const glm::vec3& value) -> void;
        auto upload_uniform_float4(  const std::string& name, const glm::vec4& value) -> void;
        auto upload_uniform_mat3(    const std::string& name, const glm::mat3& matrix) -> void;
        auto upload_uniform_mat4(    const std::string& name, const glm::mat4& matrix) -> void;

        static auto create(const std::string& filepath) -> Shader*;
        static auto create(const std::string& name, const std::string& vertex_src, const std::string& fragment_src) -> Shader*;
        static auto create_compute(const std::string& name, const std::string& compute_src) -> Shader*;

    private:
        auto read_file(const std::string& filepath) -> std::string;
        auto pre_process(const std::string& source) -> std::unordered_map<uint32_t, std::string>;
        auto compile(const std::unordered_map<uint32_t, std::string>& shader_sources) -> void;

        uint32_t    m_renderer_id = 0;
        std::string m_name;
        // True when loaded from a Slang-compiled GLSL. Slang expects row-major
        // matrix data, so matrix uniforms are transposed on upload (glm is
        // column-major) to keep all matrix math correct.
        bool        m_is_slang = false;
    };

    class ShaderLibrary
    {
    public:
        auto add(const Ref<Shader>& shader) -> void;
        auto add(const std::string& name, const Ref<Shader>& shader) -> void;
        auto load(const std::string& filepath) -> Ref<Shader>;
        auto load(const std::string& name, const std::string& filepath) -> Ref<Shader>;

        auto Get(const std::string& name) -> Ref<Shader>;

        auto exists(const std::string& name) const -> bool;
    private:
        std::unordered_map<std::string, Ref<Shader>> m_shaders;
    };
};
