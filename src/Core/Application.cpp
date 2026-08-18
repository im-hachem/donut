#include "Application.h"

#include "Rendering/Renderer.h"
#include "SettingsManager.h"

#include "States/SimulationState.h"
#include "States/ConfigState.h"
#include "States/WorldBuilderState.h"

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>

#ifdef __APPLE__
#include "Platform/Metal/MetalContext.h"
#endif

namespace Donut
{
    Application* Application::s_Instance = nullptr;

    Application::Application(const std::string& name, int width, int height)
        : m_Running(true), m_Minimized(false)
    {
        s_Instance = this;

        m_Window = CreateScope<Window>(name, width, height);
        m_Window->SetEventCallback([this](Event& event) 
        {
            OnEvent(event);
        });

        OnInit();
    }

    Application::~Application()
    {
        OnShutdown();
        s_Instance = nullptr;
    }

    void Application::Run()
    {
        while (m_Running)
        {
            if (!m_Minimized)
            {
                OnUpdate();
                OnRender();
            }

            m_Window->OnUpdate();
        }
    }

    void Application::Close()
    {
        m_Running = false;
    }

    void Application::OnEvent(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>([this, &event](WindowCloseEvent& e) 
        {
            m_Running = false;
            event.Handled = true;
            return true;
        });
        
        dispatcher.Dispatch<WindowResizeEvent>([this, &event](WindowResizeEvent& e) 
        {
            if (e.GetWidth() == 0 || e.GetHeight() == 0)
                m_Minimized = true;
            else
                m_Minimized = false;
            
            Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
            m_Engine->SetWindowDimensions(e.GetWidth(), e.GetHeight());
            event.Handled = true;
            return true;
        });
        
        dispatcher.Dispatch<KeyPressedEvent>([this, &event](KeyPressedEvent& e) 
        {
            if (e.GetKeyCode() == GLFW_KEY_1)
            {
                m_StateManager->SwitchToState("Config");
                event.Handled = true;
                return true;
            }
            else if (e.GetKeyCode() == GLFW_KEY_2)
            {
                m_StateManager->SwitchToState("Simulation");
                event.Handled = true;
                return true;
            }
            else if (e.GetKeyCode() == GLFW_KEY_3)
            {
                m_StateManager->SwitchToState("WorldBuilder");
                event.Handled = true;
                return true;
            }
            
            return true;
        });
        
        m_StateManager->OnEvent(event);
    }

    void Application::OnInit()
    {
        Logger::Init();

#ifdef __APPLE__
        MetalProbe();           // Phase 1: native Metal device available?
        MetalComputeSelfTest(); // Phase 1: full compute dispatch + read-back works?
#endif
        
        SettingsManager::Initialize();
        
        const auto& settings = SettingsManager::GetSettingsConst();
        RendererAPI::API api = (settings.graphics.renderAPI == "Vulkan") ? RendererAPI::API::Vulkan : RendererAPI::API::OpenGL;
        RendererAPI::SetAPI(api);
        
        Renderer::Init();
        m_Window->InitImGui();
        
        Renderer::OnWindowResize(1280, 720);
        RenderCommand::SetFaceCulling(false);

        m_Engine = CreateScope<Engine>();
        m_Engine->SetWindowDimensions(1280, 720);
        
        m_StateManager = CreateScope<StateManager>();
        m_StateManager->RegisterState("Config",       CreateScope<ConfigState>());
        m_StateManager->RegisterState("Simulation",   CreateScope<SimulationState>());
        m_StateManager->RegisterState("WorldBuilder", CreateScope<WorldBuilderState>());
        m_StateManager->SwitchToState("Config");
    }

    void Application::OnShutdown() 
    { 
        if (m_StateManager)
            m_StateManager->Shutdown();
            
        Renderer::Shutdown();
        SettingsManager::Shutdown();
        Logger::Shutdown();
    }

    void Application::OnUpdate()
    {
        float currentFrame = (float)glfwGetTime();
        m_DeltaTime = currentFrame - m_LastFrame;
        m_LastFrame = currentFrame;

        m_StateManager->Update(m_DeltaTime);
    }

    void Application::OnRender()
    {
        m_StateManager->Render();
        m_Window->BeginImGuiFrame();
        
        ImGuizmo::BeginFrame();
        ImGuizmo::SetOrthographic(false);
        
        SetupDockingLayout();
        m_StateManager->OnImUIRender();
        m_Window->EndImGuiFrame();
    }
    
    void Application::SetupDockingLayout()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
    }
};
