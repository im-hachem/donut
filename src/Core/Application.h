#pragma once

#include "StateManager.h"
#include "Memory.h"
#include "Window.h"
#include "Event.h"
#include "Log.h"

#include "Engine/Engine.h"

namespace Donut
{
    class VulkanRenderer; // live-window Vulkan backend (Platform/Vulkan)

    class Application
    {
    public:
        Application(const std::string& name = "Donut", 
                    int width = 1280, int height = 720);
        ~Application();

        void Run();
        void Close();

        Window& GetWindow()             { return *m_Window;       }
        StateManager& GetStateManager() { return *m_StateManager; }
        Engine& GetEngine()             { return *m_Engine;       }
        static Application& Get()       { return *s_Instance;     }
    private:
        void OnInit();
        void OnShutdown();
        void OnUpdate();
        void OnRender();
        void OnEvent(Event& event);
        void SetupDockingLayout();
        void BuildVulkanUI(); // ImGui UI built each frame on the Vulkan path
    private:
        Scope<StateManager> m_StateManager;
        Scope<Window> m_Window;
        Scope<Engine> m_Engine;
        Scope<VulkanRenderer> m_VulkanRenderer; // non-null only when the Vulkan API is selected

        bool m_Running;
        bool m_Minimized;
        
        float m_DeltaTime = 0.0f;
        float m_LastFrame = 0.0f;

        static Application* s_Instance;
    };
}