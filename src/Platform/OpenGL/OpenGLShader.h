#pragma once

#include "Rendering/Shader.h"

#include <unordered_map>
#include <glm/glm.hpp>

namespace Donut 
{
    class OpenGLShader 
        : public Shader 
    {
    public:
        OpenGLShader(const std::string& filepath);
        OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
        OpenGLShader(const std::string& name, const std::string& computeSrc);
        virtual ~OpenGLShader();

        virtual void Bind()   const override;
        virtual void Unbind() const override;

        virtual void SetInt(     const std::string& name, int value)                   override;
        virtual void SetIntArray(const std::string& name, int* values, uint32_t count) override;
        virtual void SetFloat(   const std::string& name, float value)                 override;
        virtual void SetFloat2(  const std::string& name, const glm::vec2& value)      override;
        virtual void SetFloat3(  const std::string& name, const glm::vec3& value)      override;
        virtual void SetFloat4(  const std::string& name, const glm::vec4& value)      override;
        virtual void SetMat4(    const std::string& name, const glm::mat4& value)      override;

        virtual void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1) override;
        virtual void DispatchIndirect(uint32_t offset = 0)                override;
        virtual void MemoryBarrier(uint32_t barriers)                     override;

        virtual const std::string& GetName() const override { return m_Name; }
        virtual uint32_t GetRendererID() const override { return m_RendererID; }

        void UploadUniformInt(     const std::string& name, int value);
        void UploadUniformIntArray(const std::string& name, int* values, uint32_t count);
        void UploadUniformFloat(   const std::string& name, float value);
        void UploadUniformFloat2(  const std::string& name, const glm::vec2& value);
        void UploadUniformFloat3(  const std::string& name, const glm::vec3& value);
        void UploadUniformFloat4(  const std::string& name, const glm::vec4& value);
        void UploadUniformMat3(    const std::string& name, const glm::mat3& matrix);
        void UploadUniformMat4(    const std::string& name, const glm::mat4& matrix);

    private:
        std::string ReadFile(const std::string& filepath);
        std::unordered_map<uint32_t, std::string> PreProcess(const std::string& source);
        void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);
    private:
        uint32_t    m_RendererID = 0;
        std::string m_Name;
        // True when loaded from a Slang-compiled GLSL. Slang expects row-major
        // matrix data, so matrix uniforms are transposed on upload (glm is
        // column-major) to keep all matrix math correct.
        bool        m_IsSlang = false;
    };
};
