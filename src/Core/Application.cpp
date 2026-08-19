#include "Application.h"

#include "Rendering/Renderer.h"
#include "SettingsManager.h"

#include "States/SimulationState.h"
#include "States/ConfigState.h"
#include "States/WorldBuilderState.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>

#include "Platform/Vulkan/VulkanRenderer.h"

#ifdef __APPLE__
#include "Platform/Metal/MetalContext.h"
#include "Platform/Vulkan/VulkanContext.h"
#endif

namespace Donut
{
    Application* Application::s_Instance = nullptr;

    Application::Application(const std::string& name, int width, int height)
        : m_Running(true), m_Minimized(false)
    {
        s_Instance = this;

        // Select the render API before the window is created: the window is
        // built differently for Vulkan (GLFW_NO_API) than for OpenGL.
        Logger::Init();
        SettingsManager::Initialize();
        {
            const auto& settings = SettingsManager::GetSettingsConst();
            RendererAPI::SetAPI(settings.graphics.renderAPI == "Vulkan"
                                ? RendererAPI::API::Vulkan : RendererAPI::API::OpenGL);
        }

        if (RendererAPI::GetAPI() == RendererAPI::API::Vulkan)
            VulkanPrepareGLFW(); // must precede glfwInit() inside the Window ctor

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
            if (m_VulkanRenderer)
            {
                glfwPollEvents(); // before the ImGui frame so input is current
                if (!m_Minimized)
                    m_VulkanRenderer->DrawFrame(glm::vec4(0.05f, 0.06f, 0.10f, 1.0f),
                                                [this] { BuildVulkanUI(); });
            }
            else
            {
                if (!m_Minimized)
                {
                    OnUpdate();
                    OnRender();
                }
                m_Window->OnUpdate(); // polls events + swaps buffers (OpenGL)
            }
        }
    }

    void Application::BuildVulkanUI()
    {
        // Placeholder UI proving the ImGui Vulkan backend renders. The real
        // application UI (states' OnImUIRender) moves here once the scene is
        // ported to Vulkan (B-3).
        ImGui::Begin("Donut - Vulkan backend");
        ImGui::Text("Geodesic black hole through Vulkan (MoltenVK).");
        ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
        ImGui::Separator();
        ImGui::TextWrapped("Drag to orbit, scroll to zoom. HDRI starfield is live; "
                           "next is porting the simulation UI to Vulkan.");
        ImGui::End();
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

            if (m_VulkanRenderer)
            {
                m_VulkanRenderer->OnResize(e.GetWidth(), e.GetHeight());
            }
            else
            {
                Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
                if (m_Engine) m_Engine->SetWindowDimensions(e.GetWidth(), e.GetHeight());
            }
            event.Handled = true;
            return true;
        });
        
        dispatcher.Dispatch<KeyPressedEvent>([this, &event](KeyPressedEvent& e)
        {
            if (!m_StateManager)
                return true;
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

        if (m_StateManager)
            m_StateManager->OnEvent(event);
    }

    void Application::OnInit()
    {
        // (Logger, settings and API selection happen in the constructor.)

        if (RendererAPI::GetAPI() == RendererAPI::API::Vulkan)
        {
            int w = 0, h = 0;
            glfwGetFramebufferSize((GLFWwindow*)m_Window->GetNativeWindow(), &w, &h);
            m_VulkanRenderer = CreateScope<VulkanRenderer>();
            if (!m_VulkanRenderer->Init(m_Window->GetNativeWindow(), w, h))
                DONUT_ERROR("Vulkan renderer initialization failed");
            else
                m_VulkanRenderer->InitImGui();
            // Scene rendering hooks in here in the B-3 phase; the Vulkan path now
            // clears, presents, and draws the ImGui UI.
            return;
        }

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
        if (m_VulkanRenderer)
        {
            m_VulkanRenderer->Shutdown();
            m_VulkanRenderer.reset();
        }
        else
        {
            if (m_StateManager)
                m_StateManager->Shutdown();
            Renderer::Shutdown();
        }
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
