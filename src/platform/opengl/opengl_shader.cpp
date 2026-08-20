#include "opengl_shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>

namespace Donut
{
    static uint32_t ShaderTypeFromString(const std::string& type)
    {
        if (type == "vertex")
            return GL_VERTEX_SHADER;
        if (type == "fragment" || type == "pixel")
            return GL_FRAGMENT_SHADER;
        if (type == "compute")
            return GL_COMPUTE_SHADER;
        return 0;
    }

    // Shaders are authored in Slang and compiled to assets/shaders/generated/
    // <name>.glsl by Tools/compile-shaders.sh. Given a legacy ".../<name>.glsl"
    // path, prefer that generated file when present; otherwise fall back to the
    // hand-written GLSL (e.g. shaders not yet ported to Slang).
    static std::string ResolveShaderPath(const std::string& filepath)
    {
        size_t slash = filepath.find_last_of("/\\");
        std::string dir  = (slash == std::string::npos) ? std::string() : filepath.substr(0, slash + 1);
        std::string file = (slash == std::string::npos) ? filepath : filepath.substr(slash + 1);
        size_t dot = file.rfind('.');
        std::string base = (dot == std::string::npos) ? file : file.substr(0, dot);

        std::string generated = dir + "generated/" + base + ".glsl";
        std::ifstream test(generated);
        if (test.good())
            return generated;
        return filepath;
    }

    OpenGLShader::OpenGLShader(const std::string& filepath)
    {
        std::string resolved = ResolveShaderPath(filepath);
        m_is_slang = (resolved != filepath);
        std::string source = read_file(resolved);
        auto shader_sources = pre_process(source);
        compile(shader_sources);

        auto last_slash = filepath.find_last_of("/\\");
        last_slash = last_slash == std::string::npos ? 0 : last_slash + 1;
        auto last_dot = filepath.rfind('.');
        auto count = last_dot == std::string::npos ? filepath.size() - last_slash : last_dot - last_slash;
        m_name = filepath.substr(last_slash, count);
    }

    OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertex_src, const std::string& fragment_src)
        : m_name(name)
    {
        std::unordered_map<uint32_t, std::string> sources;
        sources[GL_VERTEX_SHADER] = vertex_src;
        sources[GL_FRAGMENT_SHADER] = fragment_src;
        compile(sources);
    }

    OpenGLShader::OpenGLShader(const std::string& name, const std::string& compute_src)
        : m_name(name)
    {
        std::unordered_map<uint32_t, std::string> sources;
        sources[GL_COMPUTE_SHADER] = compute_src;
        compile(sources);
    }

    OpenGLShader::~OpenGLShader()
    {
        glDeleteProgram(m_renderer_id);
    }

    auto OpenGLShader::read_file(const std::string& filepath) -> std::string
    {
        std::string result;
        std::ifstream in(filepath, std::ios::in |
                                   std::ios::binary);

        if (in)
        {
            in.seekg(0, std::ios::end);
            size_t size = in.tellg();
            if (size != -1)
            {
                result.resize(size);
                in.seekg(0, std::ios::beg);
                in.read(&result[0], size);
            }
        }
        return result;
    }

    auto OpenGLShader::pre_process(const std::string& source) -> std::unordered_map<uint32_t, std::string>
    {
        std::unordered_map<uint32_t, std::string> shader_sources;

        const char* type_token = "#type";
        size_t type_token_length = strlen(type_token);
        size_t pos = source.find(type_token, 0);

        while (pos != std::string::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            size_t begin = pos + type_token_length + 1;
            std::string type = source.substr(begin, eol - begin);

            size_t next_line_pos = source.find_first_not_of("\r\n", eol);
            pos = source.find(type_token, next_line_pos);
            shader_sources[ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(next_line_pos) :
            source.substr(next_line_pos, pos - next_line_pos);
        }

        return shader_sources;
    }

    auto OpenGLShader::compile(const std::unordered_map<uint32_t, std::string>& shader_sources) -> void
    {
        uint32_t program = glCreateProgram();
        std::vector<uint32_t> glShaderIDs(shader_sources.size());
        for (auto& kv : shader_sources)
        {
            uint32_t type = kv.first;
            const std::string& source = kv.second;

            uint32_t shader = glCreateShader(type);
            const char* source_c_str = source.c_str();
            glShaderSource(shader, 1, &source_c_str, 0);
            glCompileShader(shader);

            int is_compiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
            if (is_compiled == GL_FALSE)
            {
                int max_length = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);
                std::vector<char> info_log(max_length);
                glGetShaderInfoLog(shader, max_length, &max_length, &info_log[0]);
                glDeleteShader(shader);
                for (auto id : glShaderIDs)
                    glDeleteShader(id);
                glDeleteProgram(program);
                m_renderer_id = 0;
                // info_log.data() is null when the driver returns an empty log
                // (e.g. macOS rejecting a compute shader); streaming a null
                // char* into std::cout calls strlen(NULL) and crashes.
                const char* log = info_log.empty() ? "" : info_log.data();
                std::cout << "Shader compilation failure!" << std::endl << log << std::endl;
                return;
            }
            glAttachShader(program, shader);
            glShaderIDs.push_back(shader);
        }

        m_renderer_id = program;
        glLinkProgram(m_renderer_id);

        int is_linked = 0;
        glGetProgramiv(m_renderer_id, GL_LINK_STATUS, (int*)&is_linked);
        if (is_linked == GL_FALSE)
        {
            int max_length = 0;
            glGetProgramiv(m_renderer_id, GL_INFO_LOG_LENGTH, &max_length);
            std::vector<char> info_log(max_length);
            glGetProgramInfoLog(m_renderer_id, max_length, &max_length, &info_log[0]);
            glDeleteProgram(m_renderer_id);
            for (auto id : glShaderIDs)
                glDeleteShader(id);
            m_renderer_id = 0;
            const char* log = info_log.empty() ? "" : info_log.data();
            std::cout << "Shader link failure!" << std::endl << log << std::endl;
            return;
        }

        for (auto id : glShaderIDs)
        {
            glDetachShader(m_renderer_id, id);
            glDeleteShader(id);
        }
    }

    auto OpenGLShader::bind() const -> void
    {
        glUseProgram(m_renderer_id);
    }

    auto OpenGLShader::unbind() const -> void
    {
        glUseProgram(0);
    }

    auto OpenGLShader::set_int(const std::string& name, int value) -> void
    {
        upload_uniform_int(name, value);
    }

    auto OpenGLShader::set_int_array(const std::string& name, int* values, uint32_t count) -> void
    {
        upload_uniform_int_array(name, values, count);
    }

    auto OpenGLShader::set_float(const std::string& name, float value) -> void
    {
        upload_uniform_float(name, value);
    }

    auto OpenGLShader::set_float2(const std::string& name, const glm::vec2& value) -> void
    {
        upload_uniform_float2(name, value);
    }

    auto OpenGLShader::set_float3(const std::string& name, const glm::vec3& value) -> void
    {
        upload_uniform_float3(name, value);
    }

    auto OpenGLShader::set_float4(const std::string& name, const glm::vec4& value) -> void
    {
        upload_uniform_float4(name, value);
    }

    auto OpenGLShader::set_mat4(const std::string& name, const glm::mat4& value) -> void
    {
        upload_uniform_mat4(name, value);
    }

    auto OpenGLShader::upload_uniform_int(const std::string& name, int value) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniform1i(location, value);
    }

    auto OpenGLShader::upload_uniform_int_array(const std::string& name, int* values, uint32_t count) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniform1iv(location, count, values);
    }

    auto OpenGLShader::upload_uniform_float(const std::string& name, float value) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniform1f(location, value);
    }

    auto OpenGLShader::upload_uniform_float2(const std::string& name, const glm::vec2& value) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniform2f(location, value.x, value.y);
    }

    auto OpenGLShader::upload_uniform_float3(const std::string& name, const glm::vec3& value) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniform3f(location, value.x, value.y, value.z);
    }

    auto OpenGLShader::upload_uniform_float4(const std::string& name, const glm::vec4& value) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }

    auto OpenGLShader::upload_uniform_mat3(const std::string& name, const glm::mat3& matrix) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniformMatrix3fv(location, 1, m_is_slang ? GL_TRUE : GL_FALSE, glm::value_ptr(matrix));
    }

    auto OpenGLShader::upload_uniform_mat4(const std::string& name, const glm::mat4& matrix) -> void
    {
        int location = glGetUniformLocation(m_renderer_id, name.c_str());
        glUniformMatrix4fv(location, 1, m_is_slang ? GL_TRUE : GL_FALSE, glm::value_ptr(matrix));
    }

    auto OpenGLShader::dispatch(uint32_t x, uint32_t y, uint32_t z) -> void
    {
        // Compute shaders require OpenGL 4.3+. On drivers that cap out earlier
        // (e.g. macOS, which is frozen at 4.1) glDispatchCompute is never
        // loaded and the pointer is null. Guard so we no-op instead of crash.
        if (m_renderer_id == 0 || glDispatchCompute == nullptr)
            return;
        glDispatchCompute(x, y, z);
    }

    auto OpenGLShader::dispatch_indirect(uint32_t offset) -> void
    {
        if (m_renderer_id == 0 || glDispatchComputeIndirect == nullptr)
            return;
        glDispatchComputeIndirect(offset);
    }

    auto OpenGLShader::memory_barrier(uint32_t barriers) -> void
    {
        if (glMemoryBarrier == nullptr)
            return;
        glMemoryBarrier(barriers);
    }
};
