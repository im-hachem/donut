#pragma once

#include "core/log.h"
#include "rendering/texture.h"

#include <string>
#include <unordered_map>
#include <memory>

namespace Donut
{
    class HDRIManager
    {
    public:
        static auto get() -> HDRIManager&
        {
            static HDRIManager instance;
            return instance;
        }

        auto load_hdri(const std::string& path) -> Ref<CubemapTexture>;
        auto get_current_hdri() const -> Ref<CubemapTexture> { return m_current_hdri; }

        auto set_current_hdri(const std::string& path) -> void;
        auto get_available_hdri() const -> const std::vector<std::string>& { return m_available_hdri; }
        auto get_hdri_name(const std::string& path) const -> std::string;
        auto clear_cache() -> void;

    private:
         HDRIManager() = default;
        ~HDRIManager() = default;

        HDRIManager(const HDRIManager&)            = delete;
        HDRIManager& operator=(const HDRIManager&) = delete;

        std::unordered_map<std::string, Ref<CubemapTexture>> m_hdri_cache;

        Ref<CubemapTexture> m_current_hdri;

        std::vector<std::string> m_available_hdri =
        {
            "assets/hdri/HDR_blue_nebulae-1.hdr",
            "assets/hdri/HDR_subdued_blue_nebulae.hdr",
            "assets/hdri/HDR_subdued_multi_nebulae.hdr",
            "assets/hdri/night_sky.hdr"
        };
    };
};
