#include "OpenGLTexture.h"

#include "Rendering/Shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// NOTE: This file targets OpenGL 4.1 (the maximum macOS exposes). It uses the
// classic bind-based texture API rather than 4.5 Direct State Access
// (glCreateTextures / glTextureStorage2D / glTextureParameteri / glBindTextureUnit),
// none of which exist on macOS.

namespace Donut
{
    OpenGLTexture2D::OpenGLTexture2D(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height)
    {
        m_InternalFormat = GL_RGBA8;
        m_DataFormat     = GL_RGBA;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
        : m_Path(path)
    {
        m_Width          = 1;
        m_Height         = 1;
        m_InternalFormat = GL_RGBA8;
        m_DataFormat     = GL_RGBA;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        uint32_t whitePixel = 0xFFFFFFFF;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, &whitePixel);

        DONUT_INFO("Created default texture (stb_image not available for loading: ", path, ")");
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::SetData(void* data, uint32_t size)
    {
        uint32_t bpp = m_DataFormat == GL_RGBA ? 4 : 3;
        if (size != m_Width * m_Height * bpp)
        {
            DONUT_ERROR("Data must be entire texture!");
            return;
        }

        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
    }

    void OpenGLTexture2D::Bind(uint32_t slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

    void OpenGLTexture2D::BindAsImage(uint32_t slot, bool readOnly) const
    {
        // Image load/store is OpenGL 4.2 and unavailable on macOS. Guard the
        // function pointer so this degrades to a no-op instead of crashing.
        if (glBindImageTexture == nullptr)
            return;
        GLenum access = readOnly ? GL_READ_ONLY : GL_WRITE_ONLY;
        glBindImageTexture(slot, m_RendererID, 0, GL_FALSE, 0, access, m_InternalFormat);
    }

    OpenGLCubemapTexture::OpenGLCubemapTexture(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height)
    {
        m_InternalFormat = GL_RGBA16F;
        m_DataFormat     = GL_RGBA;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
        for (uint32_t i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    OpenGLCubemapTexture::OpenGLCubemapTexture(const std::string& path)
        : m_Path(path)
    {
        m_Width          = 1024;
        m_Height         = 1024;
        m_InternalFormat = GL_RGBA16F;
        m_DataFormat     = GL_RGBA;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
        for (uint32_t i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        LoadHDRI(path);
    }

    OpenGLCubemapTexture::~OpenGLCubemapTexture()
    {
        glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLCubemapTexture::LoadHDRI(const std::string& path)
    {
        stbi_set_flip_vertically_on_load(true);
        int width, height, channels;
        float* hdrData = stbi_loadf(path.c_str(), &width, &height, &channels, 3);

        if (!hdrData)
        {
            DONUT_ERROR("Failed to load HDRI: {}", path);
            float defaultSky[6 * 4] =
            {
                0.5f, 0.7f, 1.0f, 1.0f,  // Right
                0.5f, 0.7f, 1.0f, 1.0f,  // Left
                0.5f, 0.7f, 1.0f, 1.0f,  // Top
                0.5f, 0.7f, 1.0f, 1.0f,  // Bottom
                0.5f, 0.7f, 1.0f, 1.0f,  // Front
                0.5f, 0.7f, 1.0f, 1.0f   // Back
            };

            glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
            for (int i = 0; i < 6; ++i)
                glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, &defaultSky[i * 4]);
            return;
        }

        ConvertEquirectangularToCubemap(hdrData, width, height);
        stbi_image_free(hdrData);

        DONUT_INFO("Successfully loaded HDRI: {} ({}x{})", path, width, height);
    }

    void OpenGLCubemapTexture::ConvertEquirectangularToCubemap(float* hdrData, int width, int height)
    {
        uint32_t captureFBO, captureRBO;
        glGenFramebuffers(1, &captureFBO);
        glGenRenderbuffers(1, &captureRBO);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_Width, m_Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

        uint32_t hdrTexture;
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, hdrData);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        auto equirectShader = Shader::Create("Assets/Shaders/EquirectToCubemap.glsl");
        if (!equirectShader)
        {
            DONUT_ERROR("Failed to create equirectangular to cubemap shader");
            return;
        }

        uint32_t shaderProgram = equirectShader->GetRendererID();

        float vertices[] =
        {
            -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f
        };

        uint32_t cubeVAO, cubeVBO;
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] =
        {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

        glUseProgram(shaderProgram);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_EquirectangularMap"), 0);
        // EquirectToCubemap is authored in Slang (row-major); transpose glm's
        // column-major matrices on upload (GL_TRUE) to match.
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_Projection"), 1, GL_TRUE, &captureProjection[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);

        glViewport(0, 0, m_Width, m_Height);
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_View"), 1, GL_TRUE, &captureViews[i][0][0]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_RendererID, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBindVertexArray(cubeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDeleteVertexArrays(1, &cubeVAO);
        glDeleteBuffers(1, &cubeVBO);
        glDeleteTextures(1, &hdrTexture);
        glDeleteFramebuffers(1, &captureFBO);
        glDeleteRenderbuffers(1, &captureRBO);
    }

    void OpenGLCubemapTexture::SetData(void* data, uint32_t size)
    {
        DONUT_WARN("SetData not implemented for cubemaps");
    }

    void OpenGLCubemapTexture::Bind(uint32_t slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
    }

    void OpenGLCubemapTexture::BindAsImage(uint32_t slot, bool readOnly) const
    {
        if (glBindImageTexture == nullptr)
            return;
        GLenum access = readOnly ? GL_READ_ONLY : GL_WRITE_ONLY;
        glBindImageTexture(slot, m_RendererID, 0, GL_TRUE, 0, access, m_InternalFormat);
    }
};
