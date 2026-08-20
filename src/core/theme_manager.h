#pragma once

#include <imgui.h>

namespace Donut
{
    enum class Theme
    {
        Dark = 0,
        Light = 1,
        Blue = 2
    };

    class ThemeManager
    {
    public:
        static auto set_theme(Theme theme) -> void;
        static auto get_current_theme() -> Theme { return s_current_theme; }

        static auto apply_dark_theme() -> void;
        static auto apply_light_theme() -> void;
        static auto apply_blue_theme() -> void;

    private:
        static Theme s_current_theme;
    };
}
