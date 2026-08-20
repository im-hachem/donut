#include "opengl_texture.h"

#include "rendering/shader.h"

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
        : m_width(width), m_height(height)
    {
        m_internal_format = GL_RGBA8;
        m_data_format     = GL_RGBA;

        glGenTextures(1, &m_renderer_id);
        glBindTexture(GL_TEXTURE_2D, m_renderer_id);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_width, m_height, 0, m_data_format, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
        : m_path(path)
    {
        m_width          = 1;
        m_height         = 1;
        m_internal_format = GL_RGBA8;
        m_data_format     = GL_RGBA;

        glGenTextures(1, &m_renderer_id);
        glBindTexture(GL_TEXTURE_2D, m_renderer_id);
        glTexImage2D(GL_TEXTURE_2D, 0, m_internal_format, m_width, m_height, 0, m_data_format, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        uint32_t white_pixel = 0xFFFFFFFF;
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_data_format, GL_UNSIGNED_BYTE, &white_pixel);

        DONUT_INFO("Created default texture (stb_image not available for loading: ", path, ")");
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        glDeleteTextures(1, &m_renderer_id);
    }

    auto OpenGLTexture2D::set_data(void* data, uint32_t size) -> void
    {
        uint32_t bpp = m_data_format == GL_RGBA ? 4 : 3;
        if (size != m_width * m_height * bpp)
        {
            DONUT_ERROR("Data must be entire texture!");
            return;
        }

        glBindTexture(GL_TEXTURE_2D, m_renderer_id);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_data_format, GL_UNSIGNED_BYTE, data);
    }

    auto OpenGLTexture2D::bind(uint32_t slot) const -> void
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_renderer_id);
    }

    auto OpenGLTexture2D::bind_as_image(uint32_t slot, bool read_only) const -> void
    {
        // Image load/store is OpenGL 4.2 and unavailable on macOS. Guard the
        // function pointer so this degrades to a no-op instead of crashing.
        if (glBindImageTexture == nullptr)
            return;
        GLenum access = read_only ? GL_READ_ONLY : GL_WRITE_ONLY;
        glBindImageTexture(slot, m_renderer_id, 0, GL_FALSE, 0, access, m_internal_format);
    }

    OpenGLCubemapTexture::OpenGLCubemapTexture(uint32_t width, uint32_t height)
        : m_width(width), m_height(height)
    {
        m_internal_format = GL_RGBA16F;
        m_data_format     = GL_RGBA;

        glGenTextures(1, &m_renderer_id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_renderer_id);
        for (uint32_t i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_internal_format, m_width, m_height, 0, m_data_format, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    OpenGLCubemapTexture::OpenGLCubemapTexture(const std::string& path)
        : m_path(path)
    {
        m_width          = 1024;
        m_height         = 1024;
        m_internal_format = GL_RGBA16F;
        m_data_format     = GL_RGBA;

        glGenTextures(1, &m_renderer_id);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_renderer_id);
        for (uint32_t i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_internal_format, m_width, m_height, 0, m_data_format, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        LoadHDRI(path);
    }

    OpenGLCubemapTexture::~OpenGLCubemapTexture()
    {
        glDeleteTextures(1, &m_renderer_id);
    }

    auto OpenGLCubemapTexture::LoadHDRI(const std::string& path) -> void
    {
        stbi_set_flip_vertically_on_load(true);
        int width, height, channels;
        float* hdr_data = stbi_loadf(path.c_str(), &width, &height, &channels, 3);

        if (!hdr_data)
        {
            DONUT_ERROR("Failed to load HDRI: {}", path);
            float default_sky[6 * 4] =
            {
                0.5f, 0.7f, 1.0f, 1.0f,  // Right
                0.5f, 0.7f, 1.0f, 1.0f,  // Left
                0.5f, 0.7f, 1.0f, 1.0f,  // Top
                0.5f, 0.7f, 1.0f, 1.0f,  // Bottom
                0.5f, 0.7f, 1.0f, 1.0f,  // Front
                0.5f, 0.7f, 1.0f, 1.0f   // Back
            };

            glBindTexture(GL_TEXTURE_CUBE_MAP, m_renderer_id);
            for (int i = 0; i < 6; ++i)
                glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, 1, 1, GL_RGBA, GL_FLOAT, &default_sky[i * 4]);
            return;
        }

        convert_equirectangular_to_cubemap(hdr_data, width, height);
        stbi_image_free(hdr_data);

        DONUT_INFO("Successfully loaded HDRI: {} ({}x{})", path, width, height);
    }

    auto OpenGLCubemapTexture::convert_equirectangular_to_cubemap(float* hdr_data, int width, int height) -> void
    {
        uint32_t capture_fbo, capture_rbo;
        glGenFramebuffers(1, &capture_fbo);
        glGenRenderbuffers(1, &capture_rbo);

        glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
        glBindRenderbuffer(GL_RENDERBUFFER, capture_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capture_rbo);

        uint32_t hdr_texture;
        glGenTextures(1, &hdr_texture);
        glBindTexture(GL_TEXTURE_2D, hdr_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, hdr_data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        auto equirect_shader = Shader::create("assets/shaders/EquirectToCubemap.glsl");
        if (!equirect_shader)
        {
            DONUT_ERROR("Failed to create equirectangular to cubemap shader");
            return;
        }

        uint32_t shader_program = equirect_shader->get_renderer_id();

        float vertices[] =
        {
            -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f
        };

        uint32_t cube_vao, cube_vbo;
        glGenVertexArrays(1, &cube_vao);
        glGenBuffers(1, &cube_vbo);
        glBindVertexArray(cube_vao);
        glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glm::mat4 capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 capture_views[] =
        {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
        };

        glUseProgram(shader_program);
        glUniform1i(glGetUniformLocation(shader_program, "u_EquirectangularMap"), 0);
        // EquirectToCubemap is authored in Slang (row-major); transpose glm's
        // column-major matrices on upload (GL_TRUE) to match.
        glUniformMatrix4fv(glGetUniformLocation(shader_program, "u_Projection"), 1, GL_TRUE, &capture_projection[0][0]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdr_texture);

        glViewport(0, 0, m_width, m_height);
        glBindFramebuffer(GL_FRAMEBUFFER, capture_fbo);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glUniformMatrix4fv(glGetUniformLocation(shader_program, "u_View"), 1, GL_TRUE, &capture_views[i][0][0]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_renderer_id, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBindVertexArray(cube_vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDeleteVertexArrays(1, &cube_vao);
        glDeleteBuffers(1, &cube_vbo);
        glDeleteTextures(1, &hdr_texture);
        glDeleteFramebuffers(1, &capture_fbo);
        glDeleteRenderbuffers(1, &capture_rbo);
    }

    auto OpenGLCubemapTexture::set_data(void* data, uint32_t size) -> void
    {
        DONUT_WARN("set_data not implemented for cubemaps");
    }

    auto OpenGLCubemapTexture::bind(uint32_t slot) const -> void
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_renderer_id);
    }

    auto OpenGLCubemapTexture::bind_as_image(uint32_t slot, bool read_only) const -> void
    {
        if (glBindImageTexture == nullptr)
            return;
        GLenum access = read_only ? GL_READ_ONLY : GL_WRITE_ONLY;
        glBindImageTexture(slot, m_renderer_id, 0, GL_TRUE, 0, access, m_internal_format);
    }
};
