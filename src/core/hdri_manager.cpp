#include "hdri_manager.h"
#include <filesystem>

namespace Donut
{
    auto HDRIManager::load_hdri(const std::string& path) -> Ref<CubemapTexture>
    {
        auto it = m_hdri_cache.find(path);
        if (it != m_hdri_cache.end())
        {
            DONUT_INFO("HDRI already cached: {}", path);
            return it->second;
        }

        DONUT_INFO("Loading HDRI: {}", path);
        auto hdri = CubemapTexture::create_from_hdri(path);

        if (hdri)
        {
            m_hdri_cache[path] = hdri;
            DONUT_INFO("Successfully loaded and cached HDRI: {}", path);
        }
        else
            DONUT_ERROR("Failed to load HDRI: {}", path);

        return hdri;
    }

    auto HDRIManager::set_current_hdri(const std::string& path) -> void
    {
        auto hdri = load_hdri(path);
        if (hdri)
        {
            m_current_hdri = hdri;
            DONUT_INFO("Set current HDRI to: {}", path);
        }
        else
            DONUT_ERROR("Failed to set current HDRI: {}", path);
    }

    auto HDRIManager::get_hdri_name(const std::string& path) const -> std::string
    {
        std::filesystem::path file_path(path);
        std::string filename = file_path.stem().string();
        std::string name = filename;

        for (size_t i = 0; i < name.length(); ++i)
        {
            if (name[i] == '_')
                name[i] = ' ';
            else if (i == 0 || name[i-1] == ' ')
                name[i] = std::toupper(name[i]);
        }

        return name;
    }

    auto HDRIManager::clear_cache() -> void
    {
        m_hdri_cache.clear();
        m_current_hdri.reset();
        DONUT_INFO("HDRI cache cleared");
    }
};
