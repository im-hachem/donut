#pragma once

#include "state_manager.h"
#include "memory.h"
#include "window.h"
#include "event.h"
#include "log.h"

#include "engine/engine.h"

namespace Donut
{
    class VulkanRenderer; // live-window Vulkan backend (platform/vulkan)

    class Application
    {
    public:
        Application(const std::string& name = "Donut",
                    int width = 1280, int height = 720);
        ~Application();

        auto run() -> void;
        auto close() -> void;

        auto get_window() -> Window&             { return *m_window;        }
        auto get_state_manager() -> StateManager& { return *m_state_manager; }
        auto get_engine() -> Engine&             { return *m_engine;        }
        static auto get() -> Application&         { return *s_instance;      }

    private:
        auto on_init() -> void;
        auto on_shutdown() -> void;
        auto on_update() -> void;
        auto on_render() -> void;
        auto on_event(Event& event) -> void;
        auto setup_docking_layout() -> void;
        auto build_vulkan_ui() -> void; // ImGui UI built each frame on the Vulkan path

    private:
        Scope<StateManager> m_state_manager;
        Scope<Window> m_window;
        Scope<Engine> m_engine;
        Scope<VulkanRenderer> m_vulkan_renderer; // non-null only when the Vulkan API is selected

        bool m_running;
        bool m_minimized;

        float m_delta_time = 0.0f;
        float m_last_frame = 0.0f;

        static Application* s_instance;
    };
}
