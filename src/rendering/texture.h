#pragma once

#include "core/memory.h"

#include <string>
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
        virtual void bind_as_image(uint32_t slot = 0, 
                                 bool read_only = false) const = 0;

        virtual bool operator==(const Texture& other)   const = 0;
    };

    class Texture2D
        : public Texture
    {
    public:
        static auto create(uint32_t width, uint32_t height) -> Ref<Texture2D>;
        static auto create(const std::string& path) -> Ref<Texture2D>;
    };

    class CubemapTexture
        : public Texture
    {
    public:
        static auto create(uint32_t width, uint32_t height) -> Ref<CubemapTexture>;
        static auto create_from_hdri(const std::string& path) -> Ref<CubemapTexture>;
    };
};