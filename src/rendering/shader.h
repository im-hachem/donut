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
        virtual ~Shader() = default;

        virtual auto bind() const -> void = 0;
        virtual auto unbind() const -> void = 0;

        virtual auto set_int(     const std::string& name, int value) -> void = 0;
        virtual auto set_int_array(const std::string& name, int* values, uint32_t count) -> void = 0;
        virtual auto set_float(   const std::string& name, float value) -> void = 0;
        virtual auto set_float2(  const std::string& name, const glm::vec2& value) -> void = 0;
        virtual auto set_float3(  const std::string& name, const glm::vec3& value) -> void = 0;
        virtual auto set_float4(  const std::string& name, const glm::vec4& value) -> void = 0;
        virtual auto set_mat4(    const std::string& name, const glm::mat4& value) -> void = 0;

        virtual auto dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) -> void = 0;
        virtual auto dispatch_indirect(uint32_t offset = 0) -> void = 0;
        virtual auto memory_barrier(uint32_t barriers) -> void = 0;

        virtual auto get_name() const -> const std::string& = 0;
        virtual auto get_renderer_id() const -> uint32_t = 0;

        static auto create(const std::string& filepath) -> Shader*;
        static auto create(const std::string& name, const std::string& vertex_src, const std::string& fragment_src) -> Shader*;
        static auto create_compute(const std::string& name, const std::string& compute_src) -> Shader*;
    };

    class ShaderLibrary
    {
    public:
        auto add(const Ref<Shader>& shader) -> void;
        auto add(const std::string& name, const Ref<Shader>& shader) -> void;
        auto load(const std::string& filepath) -> Ref<Shader>;
        auto load(const std::string& name, const std::string& filepath) -> Ref<Shader>;

        Ref<Shader> Get(const std::string& name);

        auto exists(const std::string& name) const -> bool;
    private:
        std::unordered_map<std::string, Ref<Shader>> m_shaders;
    };
};
