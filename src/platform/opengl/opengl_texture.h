#pragma once

#include "rendering/texture.h"
#include "core/log.h"

#include <glad/glad.h>

namespace Donut
{
    class OpenGLTexture2D 
        : public Texture2D
    {
    public:
        OpenGLTexture2D(uint32_t width, uint32_t height);
        OpenGLTexture2D(const std::string& path);
        virtual ~OpenGLTexture2D();

        virtual auto get_width() const -> uint32_t override{ return m_width; }
        virtual auto get_height() const -> uint32_t override{ return m_height; }
        virtual auto get_renderer_id() const -> uint32_t override{ return m_renderer_id; }

        virtual auto set_data(void* data, uint32_t size) -> void override;
        virtual auto bind(uint32_t slot = 0) const -> void override;
        virtual auto bind_as_image(uint32_t slot = 0, bool read_only = false) const -> void override;

        virtual bool operator==(const Texture& other) const override
        {
            return m_renderer_id == other.get_renderer_id();
        }

    private:
        std::string m_path;
        uint32_t    m_width, m_height;
        uint32_t    m_renderer_id;
        GLenum      m_internal_format, m_data_format;
    };

    class OpenGLCubemapTexture 
        : public CubemapTexture
    {
    public:
        OpenGLCubemapTexture(uint32_t width, uint32_t height);
        OpenGLCubemapTexture(const std::string& path);
        virtual ~OpenGLCubemapTexture();

        virtual auto get_width() const -> uint32_t override{ return m_width; }
        virtual auto get_height() const -> uint32_t override{ return m_height; }
        virtual auto get_renderer_id() const -> uint32_t override{ return m_renderer_id; }

        virtual auto set_data(void* data, uint32_t size) -> void override;
        virtual auto bind(uint32_t slot = 0) const -> void override;
        virtual auto bind_as_image(uint32_t slot = 0, bool read_only = false) const -> void override;

        virtual bool operator==(const Texture& other) const override
        {
            return m_renderer_id == other.get_renderer_id();
        }

    private:
        void LoadHDRI(const std::string& path);
        auto convert_equirectangular_to_cubemap(float* hdr_data, int width, int height) -> void;

        std::string m_path;
        uint32_t    m_width, m_height;
        uint32_t    m_renderer_id;
        GLenum      m_internal_format, m_data_format;
    };
}
