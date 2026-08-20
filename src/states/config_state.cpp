#include "config_state.h"
#include "rendering/renderer.h"
#include "core/application.h"
#include "core/theme_manager.h"
#include "core/settings_manager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glad/glad.h>

namespace Donut
{
    auto ConfigState::on_enter() -> void
    {
        DONUT_INFO("Entering Config State");
        
        const auto& settings = SettingsManager::get_settings_const();
        
        m_selected_api            = (settings.graphics.render_api == "Vulkan") ? RendererAPI::API::Vulkan : RendererAPI::API::OpenGL;
        m_selected_theme          = (settings.graphics.selected_theme == "Light") ? 1 : 
                                   (settings.graphics.selected_theme == "Blue") ? 2 : 0;
        m_target_fps              = settings.simulation.target_fps;
        m_compute_height          = settings.simulation.compute_height;
        m_max_steps_moving         = settings.simulation.max_steps_moving;
        m_max_steps_static         = settings.simulation.max_steps_static;
        m_early_exit_distance      = settings.simulation.early_exit_distance;
        m_gravity_enabled         = settings.simulation.gravity_enabled;
        m_v_sync_enabled           = settings.graphics.v_sync_enabled;
        m_show_fps                = settings.graphics.show_fps;
        m_show_performance_metrics = settings.graphics.show_performance_metrics;
        m_show_debug_info          = settings.graphics.show_debug_info;
        m_enable_anti_aliasing     = settings.graphics.enable_anti_aliasing;
    }
    
    auto ConfigState::on_exit() -> void
    {
        DONUT_INFO("Exiting Config State");
    }
    
    auto ConfigState::on_update(float delta_time) -> void
    {
        if (m_show_restart_message)
        {
            m_restart_message_timer += delta_time;
            if (m_restart_message_timer > 3.0f)
            {
                m_show_restart_message = false;
                m_restart_message_timer = 0.0f;
            }
        }
    }
    
    auto ConfigState::on_render() -> void
    {
        Renderer::set_clear_color({ 0.1f, 0.1f, 0.1f, 1.0f });
        Renderer::clear();
    }
    
    auto ConfigState::on_event(Event& event) -> void
    {
    }
    
    auto ConfigState::on_im_ui_render() -> void
    {
        ImGui::SetNextWindowSize(ImVec2(900, 700), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), 
                               ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        
        ImGui::Begin("Donut Configuration", nullptr, ImGuiWindowFlags_NoCollapse);
        
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Donut Configuration");
        ImGui::PopFont();
        ImGui::Separator();
        
        ImGui::Columns(2, "ConfigColumns", true);
        
        float available_height = ImGui::GetWindowHeight() - 140;
        
        ImGui::BeginChild("GraphicsSettings", ImVec2(0, available_height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Graphics Settings");
        ImGui::Separator();
        
        ImGui::Text("Render API:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(requires restart)");
        
        const char* api_names[] = { "OpenGL", "Vulkan" };
        static int current_api = (int)m_selected_api - 1; 
        
        if (ImGui::BeginCombo("##RenderAPI", api_names[current_api]))
        {
            for (int i = 0; i < IM_ARRAYSIZE(api_names); i++)
            {
                const bool is_selected = (current_api == i);
                if (ImGui::Selectable(api_names[i], is_selected))
                {
                    current_api = i;
                    m_selected_api = (RendererAPI::API)(i + 1);
                    m_show_restart_message = true;
                    m_restart_message_timer = 0.0f;
                }
                
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            
            ImGui::EndCombo();
        }
        
        ImGui::Text("Current API: ");
        ImGui::SameLine();
        const char* current_api_name = (Renderer::get_api() == RendererAPI::API::OpenGL) ? "OpenGL" : "Vulkan";
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), current_api_name);
        
        ImGui::Spacing();
        
        ImGui::Checkbox("Enable VSync", &m_v_sync_enabled);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(recommended)");
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Display");
        ImGui::Separator();
        
        ImGui::Text("Window Size: 1280x720");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Fullscreen: Not implemented yet");
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "System Info");
        ImGui::Separator();
        
        ImGui::Text("OpenGL Version: %s", glGetString(GL_VERSION));
        ImGui::Text("GPU: %s", glGetString(GL_RENDERER));
        ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Performance");
        ImGui::Separator();
        
        ImGui::SliderInt("Target FPS", &m_target_fps, 30, 120, "%d FPS");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Target frame rate for the simulation");
            
        ImGui::Checkbox("Show FPS Counter", &m_show_fps);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Display current FPS in the simulation");
            
        ImGui::Checkbox("Show Performance Metrics", &m_show_performance_metrics);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Show detailed performance information");
            
        ImGui::Checkbox("Show Debug Info", &m_show_debug_info);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Display debug information and statistics");
            
        ImGui::Checkbox("Enable Anti-Aliasing", &m_enable_anti_aliasing);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enable anti-aliasing for smoother rendering");
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Theme");
        ImGui::Separator();
        
        const char* theme_names[] = { "Dark", "Light", "Blue" };
        if (ImGui::Combo("UI Theme", &m_selected_theme, theme_names, IM_ARRAYSIZE(theme_names)))
            ThemeManager::set_theme(static_cast<Theme>(m_selected_theme));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Choose the application theme");
        
        ImGui::EndChild();
        
        ImGui::NextColumn();
        
        ImGui::BeginChild("SimulationSettings", ImVec2(0, available_height), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Simulation Settings");
        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Quality");
        ImGui::Separator();
        
        ImGui::SliderInt("Compute Height", &m_compute_height, 64, 2048, "%d px");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Resolution of the compute shader. Higher values give better quality but lower performance.");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Higher = better quality, lower performance");
        
        ImGui::SliderInt("Max Steps (Moving)", &m_max_steps_moving, 1000, 60000, "%d");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Maximum ray marching steps when camera is moving");
            
        ImGui::SliderInt("Max Steps (Static)", &m_max_steps_static, 1000, 30000, "%d");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Maximum ray marching steps when camera is stationary");
        
        ImGui::SliderFloat("Early Exit Distance", &m_early_exit_distance, 1e11f, 1e13f, "%.2e");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Distance at which ray marching stops to improve performance");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Distance at which ray marching stops");
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Physics");
        ImGui::Separator();
        
        ImGui::Checkbox("Enable Gravity", &m_gravity_enabled);
        
        ImGui::EndChild();
        
        ImGui::Columns(1);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_width = (ImGui::GetWindowWidth() - 120) / 5.0f;
        
        if (ImGui::Button("World Builder", ImVec2(button_width, 35)))
        {
            apply_settings();
            Application::get().get_state_manager().switch_to_state("WorldBuilder");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults", ImVec2(button_width, 35)))
            reset_to_defaults();
        
        ImGui::SameLine();
        if (ImGui::Button("Apply Settings", ImVec2(button_width, 35)))
            apply_settings();
        
        ImGui::SameLine();
        if (ImGui::Button("Save Settings", ImVec2(button_width, 35)))
        {
            apply_settings();
            DONUT_INFO("Settings saved manually");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Exit", ImVec2(button_width, 35)))
            Application::get().close();
        
        if (m_show_restart_message)
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            ImGui::TextWrapped("Warning: Render API changed! Please restart the application for changes to take effect.");
            ImGui::PopStyleColor();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Donut Engine v1.0.0 | Black Hole Simulation");
        
        ImGui::End();
    }
    
    auto ConfigState::apply_settings() -> void
    {
        SimulationSettings sim_settings;
        sim_settings.target_fps = m_target_fps;
        sim_settings.compute_height = m_compute_height;
        sim_settings.max_steps_moving = m_max_steps_moving;
        sim_settings.max_steps_static = m_max_steps_static;
        sim_settings.early_exit_distance = m_early_exit_distance;
        sim_settings.gravity_enabled = m_gravity_enabled;
        
        GraphicsSettings gfx_settings;
        gfx_settings.render_api = (m_selected_api == RendererAPI::API::Vulkan) ? "Vulkan" : "OpenGL";
        gfx_settings.v_sync_enabled = m_v_sync_enabled;
        gfx_settings.show_fps = m_show_fps;
        gfx_settings.show_performance_metrics = m_show_performance_metrics;
        gfx_settings.show_debug_info = m_show_debug_info;
        gfx_settings.enable_anti_aliasing = m_enable_anti_aliasing;
        gfx_settings.selected_theme = (m_selected_theme == 1) ? "Light" : 
                                    (m_selected_theme == 2) ? "Blue"  : "Dark";
        
        SettingsManager::set_simulation_settings(sim_settings);
        SettingsManager::set_graphics_settings(gfx_settings);
        
        RendererAPI::set_api(m_selected_api);
        DONUT_INFO("Settings applied and saved");
    }
    
    auto ConfigState::reset_to_defaults() -> void
    {
        m_selected_api            = RendererAPI::API::OpenGL;
        m_target_fps              = 60;
        m_compute_height          = 512;
        m_max_steps_moving         = 30000;
        m_max_steps_static         = 15000;
        m_early_exit_distance      = 5e12f;
        m_gravity_enabled         = true;
        m_v_sync_enabled           = true;
        m_show_fps                = true;
        m_show_performance_metrics = true;
        m_show_debug_info          = false;
        m_enable_anti_aliasing     = true;
        m_selected_theme          = 0;
        
        apply_settings();
        DONUT_INFO("Settings reset to defaults and saved");
    }
};
