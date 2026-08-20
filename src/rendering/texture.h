#pragma once

#include "core/memory.h"

#include <string>
#include <cstdint>
#include <glm/glm.hpp>

namespace Donut
{
    class Texture
    {
    public:
        virtual ~Texture() = default;

        virtual auto get_width() const -> uint32_t = 0;
        virtual auto get_height() const -> uint32_t = 0;
        virtual auto get_renderer_id() const -> uint32_t = 0;

        virtual auto set_data(void* data, uint32_t size) -> void = 0;
        virtual auto bind(uint32_t slot = 0) const -> void = 0;
        virtual auto bind_as_image(uint32_t slot = 0, bool read_only = false) const -> void = 0;

        virtual auto operator==(const Texture& other) const -> bool = 0;
    };

    class Texture2D : public Texture
    {
    public:
        Texture2D(uint32_t width, uint32_t height);
        Texture2D(const std::string& path);
        ~Texture2D();

        auto get_width() const -> uint32_t override { return m_width; }
        auto get_height() const -> uint32_t override { return m_height; }
        auto get_renderer_id() const -> uint32_t override { return m_renderer_id; }

        auto set_data(void* data, uint32_t size) -> void override;
        auto bind(uint32_t slot = 0) const -> void override;
        auto bind_as_image(uint32_t slot = 0, bool read_only = false) const -> void override;

        auto operator==(const Texture& other) const -> bool override { return m_renderer_id == other.get_renderer_id(); }

        static auto create(uint32_t width, uint32_t height) -> Ref<Texture2D>;
        static auto create(const std::string& path) -> Ref<Texture2D>;

    private:
        std::string m_path;
        uint32_t    m_width = 0, m_height = 0;
        uint32_t    m_renderer_id = 0;
        uint32_t    m_internal_format = 0, m_data_format = 0; // GLenum values
    };

    class CubemapTexture : public Texture
    {
    public:
        CubemapTexture(uint32_t width, uint32_t height);
        CubemapTexture(const std::string& path);
        ~CubemapTexture();

        auto get_width() const -> uint32_t override { return m_width; }
        auto get_height() const -> uint32_t override { return m_height; }
        auto get_renderer_id() const -> uint32_t override { return m_renderer_id; }

        auto set_data(void* data, uint32_t size) -> void override;
        auto bind(uint32_t slot = 0) const -> void override;
        auto bind_as_image(uint32_t slot = 0, bool read_only = false) const -> void override;

        auto operator==(const Texture& other) const -> bool override { return m_renderer_id == other.get_renderer_id(); }

        static auto create(uint32_t width, uint32_t height) -> Ref<CubemapTexture>;
        static auto create_from_hdri(const std::string& path) -> Ref<CubemapTexture>;

    private:
        auto LoadHDRI(const std::string& path) -> void;
        auto convert_equirectangular_to_cubemap(float* hdr_data, int width, int height) -> void;

        std::string m_path;
        uint32_t    m_width = 0, m_height = 0;
        uint32_t    m_renderer_id = 0;
        uint32_t    m_internal_format = 0, m_data_format = 0; // GLenum values
    };
};
