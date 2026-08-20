#include "theme_manager.h"

namespace Donut
{
    Theme ThemeManager::s_current_theme = Theme::Dark;

    auto ThemeManager::set_theme(Theme theme) -> void
    {
        s_current_theme = theme;
        
        switch (theme)
        {
            case Theme::Dark:
                apply_dark_theme();
                break;
            case Theme::Light:
                apply_light_theme();
                break;
            case Theme::Blue:
                apply_blue_theme();
                break;
        }
    }

    auto ThemeManager::apply_dark_theme() -> void
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.06f, 0.06f, 0.06f, 0.95f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
        colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.25f, 0.54f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.12f, 0.95f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.18f, 0.18f, 0.95f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 0.95f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.60f, 0.60f, 0.60f, 0.95f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.60f, 0.60f, 0.60f, 0.95f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.70f, 0.70f, 0.70f, 0.95f);
        colors[ImGuiCol_Button]                 = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.20f, 0.20f, 0.20f, 0.95f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.25f, 0.25f, 0.25f, 0.95f);
        colors[ImGuiCol_Header]                 = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.20f, 0.20f, 0.20f, 0.95f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.25f, 0.25f, 0.25f, 0.95f);
        colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.43f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.60f, 0.60f, 0.60f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.08f, 0.08f, 0.08f, 0.83f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.15f, 0.15f, 0.15f, 0.83f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.08f, 0.08f, 0.08f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_DockingPreview]         = ImVec4(0.60f, 0.60f, 0.60f, 0.70f);
        colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.60f, 0.60f, 0.60f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        style.WindowPadding     = ImVec2(8, 8);
        style.FramePadding      = ImVec2(5, 2);
        style.CellPadding       = ImVec2(6, 4);
        style.ItemSpacing       = ImVec2(6, 4);
        style.ItemInnerSpacing  = ImVec2(4, 4);
        style.IndentSpacing     = 21;
        style.ScrollbarSize     = 14;
        style.GrabMinSize       = 10;

        style.WindowBorderSize  = 1;
        style.ChildBorderSize   = 1;
        style.PopupBorderSize   = 1;
        style.FrameBorderSize   = 0;
        style.TabBorderSize     = 0;

        style.WindowRounding    = 8.0f;
        style.ChildRounding     = 6.0f;
        style.FrameRounding     = 4.0f;
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding      = 4.0f;
        style.TabRounding       = 4.0f;

        style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
        style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        style.DisplaySafeAreaPadding = ImVec2(3, 3);
        
        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6f;
    }

    auto ThemeManager::apply_light_theme() -> void
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text]                   = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.95f, 0.95f, 0.95f, 0.95f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.92f, 0.92f, 0.92f, 0.95f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.95f, 0.95f, 0.95f, 0.95f);
        colors[ImGuiCol_Border]                 = ImVec4(0.70f, 0.70f, 0.70f, 0.54f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.85f, 0.85f, 0.85f, 0.95f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.80f, 0.80f, 0.80f, 0.95f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.75f, 0.75f, 0.75f, 0.95f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.95f, 0.95f, 0.95f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.90f, 0.90f, 0.90f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.20f, 0.60f, 0.90f, 0.95f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.20f, 0.60f, 0.90f, 0.95f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.15f, 0.50f, 0.80f, 0.95f);
        colors[ImGuiCol_Button]                 = ImVec4(0.80f, 0.80f, 0.80f, 0.95f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.75f, 0.75f, 0.75f, 0.95f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.70f, 0.70f, 0.70f, 0.95f);
        colors[ImGuiCol_Header]                 = ImVec4(0.85f, 0.85f, 0.85f, 0.95f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.80f, 0.80f, 0.80f, 0.95f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.75f, 0.75f, 0.75f, 0.95f);
        colors[ImGuiCol_Separator]              = ImVec4(0.70f, 0.70f, 0.70f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.60f, 0.60f, 0.60f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.20f, 0.60f, 0.90f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.85f, 0.85f, 0.85f, 0.83f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.80f, 0.80f, 0.80f, 0.83f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.90f, 0.90f, 0.90f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
        colors[ImGuiCol_DockingPreview]         = ImVec4(0.20f, 0.60f, 0.90f, 0.70f);
        colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.90f, 0.30f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.70f, 0.50f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.80f, 0.40f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.20f, 0.60f, 0.90f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(0.10f, 0.50f, 0.80f, 1.00f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.20f, 0.60f, 0.90f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.10f, 0.10f, 0.10f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);

        style.WindowPadding     = ImVec2(8, 8);
        style.FramePadding      = ImVec2(5, 2);
        style.CellPadding       = ImVec2(6, 4);
        style.ItemSpacing       = ImVec2(6, 4);
        style.ItemInnerSpacing  = ImVec2(4, 4);
        style.IndentSpacing     = 21;
        style.ScrollbarSize     = 14;
        style.GrabMinSize       = 10;

        style.WindowBorderSize  = 1;
        style.ChildBorderSize   = 1;
        style.PopupBorderSize   = 1;
        style.FrameBorderSize   = 0;
        style.TabBorderSize     = 0;

        style.WindowRounding    = 8.0f;
        style.ChildRounding     = 6.0f;
        style.FrameRounding     = 4.0f;
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding      = 4.0f;
        style.TabRounding       = 4.0f;

        style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
        style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        style.DisplaySafeAreaPadding = ImVec2(3, 3);
        
        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6f;
    }

    auto ThemeManager::apply_blue_theme() -> void
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.05f, 0.10f, 0.20f, 0.95f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.03f, 0.08f, 0.18f, 0.95f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.05f, 0.10f, 0.20f, 0.95f);
        colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.30f, 0.50f, 0.54f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.08f, 0.15f, 0.30f, 0.95f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.12f, 0.20f, 0.40f, 0.95f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.15f, 0.25f, 0.45f, 0.95f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.05f, 0.10f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.05f, 0.10f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.08f, 0.15f, 0.25f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.05f, 0.10f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.40f, 0.60f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.50f, 0.70f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.60f, 0.80f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.20f, 0.60f, 1.00f, 0.95f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.20f, 0.60f, 1.00f, 0.95f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.30f, 0.70f, 1.00f, 0.95f);
        colors[ImGuiCol_Button]                 = ImVec4(0.10f, 0.20f, 0.40f, 0.95f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.15f, 0.25f, 0.45f, 0.95f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.30f, 0.50f, 0.95f);
        colors[ImGuiCol_Header]                 = ImVec4(0.10f, 0.20f, 0.40f, 0.95f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.15f, 0.25f, 0.45f, 0.95f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.20f, 0.30f, 0.50f, 0.95f);
        colors[ImGuiCol_Separator]              = ImVec4(0.30f, 0.40f, 0.60f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.40f, 0.50f, 0.70f, 1.00f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.50f, 0.60f, 0.80f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.30f, 0.40f, 0.60f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.20f, 0.60f, 1.00f, 0.95f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.08f, 0.15f, 0.25f, 0.83f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.12f, 0.20f, 0.35f, 0.83f);
        colors[ImGuiCol_TabActive]              = ImVec4(0.15f, 0.25f, 0.40f, 1.00f);
        colors[ImGuiCol_TabUnfocused]           = ImVec4(0.05f, 0.10f, 0.20f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.08f, 0.15f, 0.25f, 1.00f);
        colors[ImGuiCol_DockingPreview]         = ImVec4(0.20f, 0.60f, 1.00f, 0.70f);
        colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.05f, 0.10f, 0.20f, 1.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.60f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.60f, 0.40f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.30f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.80f, 0.40f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.20f, 0.60f, 1.00f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(0.10f, 0.50f, 0.90f, 1.00f);
        colors[ImGuiCol_NavHighlight]           = ImVec4(0.20f, 0.60f, 1.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

        style.WindowPadding     = ImVec2(8, 8);
        style.FramePadding      = ImVec2(5, 2);
        style.CellPadding       = ImVec2(6, 4);
        style.ItemSpacing       = ImVec2(6, 4);
        style.ItemInnerSpacing  = ImVec2(4, 4);
        style.IndentSpacing     = 21;
        style.ScrollbarSize     = 14;
        style.GrabMinSize       = 10;

        style.WindowBorderSize  = 1;
        style.ChildBorderSize   = 1;
        style.PopupBorderSize   = 1;
        style.FrameBorderSize   = 0;
        style.TabBorderSize     = 0;

        style.WindowRounding    = 8.0f;
        style.ChildRounding     = 6.0f;
        style.FrameRounding     = 4.0f;
        style.PopupRounding     = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding      = 4.0f;
        style.TabRounding       = 4.0f;

        style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
        style.ButtonTextAlign   = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        style.DisplaySafeAreaPadding = ImVec2(3, 3);
        
        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6f;
    }
}
