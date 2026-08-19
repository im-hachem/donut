#include <glad/glad.h>
#include "Window.h"
#include "ThemeManager.h"
#include "Rendering/Renderer.h"   // RendererAPI::GetAPI()

#include <cstdint>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace Donut
{
    static bool     s_GLFWInitialized = false;
    static uint32_t s_GLFWWindowCount = 0;

    Window::Window(const std::string& title, int width, int height)
        : m_Title(title), m_Width(width), 
          m_Height(height), m_IsClosed(false)
    {
        Init();
    }

    Window::~Window()
    {
        Shutdown();
    }

    void Window::Init()
    {
        DONUT_INFO("Initializing window: ", m_Title, " (", m_Width, "x", m_Height, ")");
        
        if (!s_GLFWInitialized)
        {
            int success = glfwInit();
            if (!success)
            {
                DONUT_ERROR("Could not initialize GLFW!");
                return;
            }

            s_GLFWInitialized = true;
            DONUT_INFO("GLFW initialized successfully");
        }

        if (RendererAPI::GetAPI() == RendererAPI::API::Vulkan)
        {
            // Vulkan manages presentation itself; GLFW must not create a GL context.
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        else
        {
#ifdef __APPLE__
            // macOS only exposes OpenGL up to 4.1 Core Profile, and requires a
            // forward-compatible core-profile context for any modern (>= 3.3)
            // shader to compile. Without these hints GLFW hands back a legacy
            // 2.1 context and every GLSL shader in the project fails to build.
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
        }

        m_Window = glfwCreateWindow(m_Width, m_Height, m_Title.c_str(), nullptr, nullptr);
        if (!m_Window)
        {
            DONUT_ERROR("Could not create GLFW window!");
            glfwTerminate();
            return;
        }
        
        DONUT_INFO("GLFW window created successfully");

        if (RendererAPI::GetAPI() != RendererAPI::API::Vulkan)
            glfwMakeContextCurrent(m_Window);
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetErrorCallback(GLFWErrorCallback);
        glfwSetWindowCloseCallback(m_Window, GLFWWindowCloseCallback);
        glfwSetWindowSizeCallback(m_Window,  GLFWWindowSizeCallback);
        glfwSetWindowFocusCallback(m_Window, GLFWWindowFocusCallback);
        glfwSetWindowPosCallback(m_Window,   GLFWWindowPosCallback);
        glfwSetKeyCallback(m_Window,         GLFWKeyCallback);
        glfwSetCharCallback(m_Window,        GLFWCharCallback);
        glfwSetMouseButtonCallback(m_Window, GLFWMouseButtonCallback);
        glfwSetScrollCallback(m_Window,      GLFWMouseScrollCallback);
        glfwSetCursorPosCallback(m_Window,   GLFWCursorPosCallback);

        s_GLFWWindowCount++;
    }

    void Window::Shutdown()
    {
        DONUT_INFO("Shutting down window: ", m_Title);
        
        ShutdownImGui();
        
        glfwDestroyWindow(m_Window);
        s_GLFWWindowCount--;

        if (s_GLFWWindowCount == 0)
        {
            glfwTerminate();
            s_GLFWInitialized = false;
            DONUT_INFO("GLFW terminated (no more windows)");
        }
    }

    void Window::OnUpdate() const
    {
        glfwPollEvents();
        if (RendererAPI::GetAPI() != RendererAPI::API::Vulkan)
            glfwSwapBuffers(m_Window); // Vulkan presents via the swapchain instead
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window) || m_IsClosed;
    }

    void Window::SetCursorLocked(bool locked)
    {
        m_CursorLocked = locked;
        glfwSetInputMode(m_Window, GLFW_CURSOR, locked ? 
                         GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    void Window::SetCursorVisible(bool visible)
    {
        m_CursorVisible = visible;
        glfwSetInputMode(m_Window, GLFW_CURSOR, visible ? 
                         GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }

    void Window::InitImGui()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#ifndef __APPLE__
        // Multi-viewport (dragging ImGui panels out as separate OS windows)
        // relies on a populated platform-monitor list and is unreliable on
        // macOS, where it intermittently asserts (Monitors.Size > 0) and
        // aborts. Docking stays enabled; panels just remain inside the window.
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif

        SetupImGuiFonts();
        ThemeManager::SetTheme(Theme::Dark);

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
#ifdef __APPLE__
        // macOS uses a core-profile context, which rejects the legacy
        // "#version 130" GLSL the ImGui backend defaults to. 150 is the
        // minimum core-profile GLSL that macOS's OpenGL 4.1 accepts.
        ImGui_ImplOpenGL3_Init("#version 150");
#else
        ImGui_ImplOpenGL3_Init("#version 130");
#endif
        
        DONUT_INFO("ImGUI initialized successfully");
    }

    void Window::SetupImGuiFonts()
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        
        m_MainFont = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Inter/static/Inter_18pt-Regular.ttf", 16.0f);
        if (!m_MainFont)
        {
            DONUT_WARN("Failed to load Inter font, falling back to default");
            m_MainFont = io.Fonts->AddFontDefault();
        }
        else
            DONUT_INFO("Successfully loaded Inter font");
        
        m_LargeFont = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Inter/static/Inter_18pt-Bold.ttf", 20.0f);
        if (!m_LargeFont)
            m_LargeFont = m_MainFont;
        
        m_SmallFont = io.Fonts->AddFontFromFileTTF("Assets/Fonts/Inter/static/Inter_18pt-Light.ttf", 12.0f);
        if (!m_SmallFont)
            m_SmallFont = m_MainFont;
        
        io.FontDefault = m_MainFont;
        DONUT_INFO("ImGUI fonts loaded successfully");
    }

    void Window::ShutdownImGui()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        DONUT_INFO("ImGUI shutdown");
    }

    void Window::BeginImGuiFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Window::EndImGuiFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void Window::GLFWErrorCallback(int error, const char* description)
    {
        DONUT_ERROR("GLFW Error ({}): {}", error, description ? description : "");
    }

    void Window::GLFWWindowCloseCallback(GLFWwindow* window)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        WindowCloseEvent event;
        win->m_EventHandler.OnEvent(event);
    }

    void Window::GLFWWindowSizeCallback(GLFWwindow* window, int width, int height)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        win->m_Width = width;
        win->m_Height = height;
        
        WindowResizeEvent event(width, height);
        win->m_EventHandler.OnEvent(event);
    }

    void Window::GLFWWindowFocusCallback(GLFWwindow* window, int focused)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        
        if (focused)
        {
            WindowFocusEvent event;
            win->m_EventHandler.OnEvent(event);
        }
        else
        {
            WindowLostFocusEvent event;
            win->m_EventHandler.OnEvent(event);
        }
    }

    void Window::GLFWWindowPosCallback(GLFWwindow* window, int xpos, int ypos)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        WindowMovedEvent event(xpos, ypos);
        win->m_EventHandler.OnEvent(event);
    }

    void Window::GLFWKeyCallback(GLFWwindow* window, int key, int scancode, 
                                 int action, int mods)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        
        switch (action)
        {
            case GLFW_PRESS:
            {
                KeyPressedEvent event(key, false);
                win->m_EventHandler.OnEvent(event);
            } break;
            case GLFW_RELEASE:
            {
                KeyReleasedEvent event(key);
                win->m_EventHandler.OnEvent(event);
            } break;
            case GLFW_REPEAT:
            {
                KeyPressedEvent event(key, true);
                win->m_EventHandler.OnEvent(event);
            } break;
        }
    }

    void Window::GLFWCharCallback(GLFWwindow* window, unsigned int keycode)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        KeyTypedEvent event(keycode);
        win->m_EventHandler.OnEvent(event);
    }

    void Window::GLFWMouseButtonCallback(GLFWwindow* window, int button, 
                                         int action, int mods)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        
        switch (action)
        {
            case GLFW_PRESS:
            {
                MouseButtonPressedEvent event(button);
                win->m_EventHandler.OnEvent(event);
            } break;
            case GLFW_RELEASE:
            {
                MouseButtonReleasedEvent event(button);
                win->m_EventHandler.OnEvent(event);
            } break;
        }
    }

    void Window::GLFWMouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        MouseScrolledEvent event((float)xOffset, (float)yOffset);
        win->m_EventHandler.OnEvent(event);
    }

    void Window::GLFWCursorPosCallback(GLFWwindow* window, double xPos, double yPos)
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        MouseMovedEvent event((float)xPos, (float)yPos);
        win->m_EventHandler.OnEvent(event);
    }
}