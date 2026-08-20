#pragma once

#include "core/state.h"
#include "core/event.h"
#include "core/log.h"
#include "rendering/renderer.h"

namespace Donut
{
    class ConfigState
        : public State
    {
    public:
        ~ConfigState() = default;
        
        auto on_enter() -> void override;
        auto on_exit() -> void override;
        auto on_update(float delta_time) -> void override;
        auto on_render() -> void override;
        auto on_im_ui_render() -> void override;
        auto on_event(Event& event) -> void override;
        
    private:
        auto apply_settings() -> void;
        auto reset_to_defaults() -> void;
        
    private:
        RendererAPI::API m_selected_api = RendererAPI::API::OpenGL;
        bool m_show_restart_message = false;
        float m_restart_message_timer = 0.0f;
        
        int   m_target_fps         = 60;
        int   m_compute_height     = 512;
        int   m_max_steps_moving    = 30000;
        int   m_max_steps_static    = 15000;
        float m_early_exit_distance = 5e12f;
        bool  m_gravity_enabled    = true;
        
        bool m_v_sync_enabled = true;
        bool m_show_fps = true;
        bool m_show_performance_metrics = true;
        bool m_show_debug_info = false;
        bool m_enable_anti_aliasing = true;
        
        int m_selected_theme = 0; // 0=Dark, 1=Light, 2=Blue
    };
};