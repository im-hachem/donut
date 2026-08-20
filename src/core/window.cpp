#include <glad/glad.h>
#include "window.h"
#include "theme_manager.h"
#include "rendering/renderer.h"   // RendererAPI::get_api()

#include <cstdint>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace Donut
{
    static bool     s_glfw_initialized = false;
    static uint32_t s_glfw_window_count = 0;

    Window::Window(const std::string& title, int width, int height)
        : m_title(title), m_width(width),
          m_height(height), m_is_closed(false)
    {
        init();
    }

    Window::~Window()
    {
        shutdown();
    }

    auto Window::init() -> void
    {
        DONUT_INFO("Initializing window: ", m_title, " (", m_width, "x", m_height, ")");

        if (!s_glfw_initialized)
        {
            int success = glfwInit();
            if (!success)
            {
                DONUT_ERROR("Could not initialize GLFW!");
                return;
            }

            s_glfw_initialized = true;
            DONUT_INFO("GLFW initialized successfully");
        }

        if (RendererAPI::get_api() == RendererAPI::API::Vulkan)
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

        m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
        if (!m_window)
        {
            DONUT_ERROR("Could not create GLFW window!");
            glfwTerminate();
            return;
        }

        DONUT_INFO("GLFW window created successfully");

        if (RendererAPI::get_api() != RendererAPI::API::Vulkan)
            glfwMakeContextCurrent(m_window);
        glfwSetWindowUserPointer(m_window, this);

        glfwSetErrorCallback(glfw_error_callback);
        glfwSetWindowCloseCallback(m_window, glfw_window_close_callback);
        glfwSetWindowSizeCallback(m_window,  glfw_window_size_callback);
        glfwSetWindowFocusCallback(m_window, glfw_window_focus_callback);
        glfwSetWindowPosCallback(m_window,   glfw_window_pos_callback);
        glfwSetKeyCallback(m_window,         glfw_key_callback);
        glfwSetCharCallback(m_window,        glfw_char_callback);
        glfwSetMouseButtonCallback(m_window, glfw_mouse_button_callback);
        glfwSetScrollCallback(m_window,      glfw_mouse_scroll_callback);
        glfwSetCursorPosCallback(m_window,   glfw_cursor_pos_callback);

        s_glfw_window_count++;
    }

    auto Window::shutdown() -> void
    {
        DONUT_INFO("Shutting down window: ", m_title);

        shutdown_im_gui();

        glfwDestroyWindow(m_window);
        s_glfw_window_count--;

        if (s_glfw_window_count == 0)
        {
            glfwTerminate();
            s_glfw_initialized = false;
            DONUT_INFO("GLFW terminated (no more windows)");
        }
    }

    auto Window::on_update() const -> void
    {
        glfwPollEvents();
        if (RendererAPI::get_api() != RendererAPI::API::Vulkan)
            glfwSwapBuffers(m_window); // Vulkan presents via the swapchain instead
    }

    auto Window::should_close() const -> bool
    {
        return glfwWindowShouldClose(m_window) || m_is_closed;
    }

    auto Window::set_cursor_locked(bool locked) -> void
    {
        m_cursor_locked = locked;
        glfwSetInputMode(m_window, GLFW_CURSOR, locked ?
                         GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }

    auto Window::set_cursor_visible(bool visible) -> void
    {
        m_cursor_visible = visible;
        glfwSetInputMode(m_window, GLFW_CURSOR, visible ?
                         GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }

    auto Window::init_im_gui() -> void
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

        setup_im_gui_fonts();
        ThemeManager::set_theme(Theme::Dark);

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
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

    auto Window::setup_im_gui_fonts() -> void
    {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        m_main_font = io.Fonts->AddFontFromFileTTF("assets/fonts/inter/static/Inter_18pt-Regular.ttf", 16.0f);
        if (!m_main_font)
        {
            DONUT_WARN("Failed to load Inter font, falling back to default");
            m_main_font = io.Fonts->AddFontDefault();
        }
        else
            DONUT_INFO("Successfully loaded Inter font");

        m_large_font = io.Fonts->AddFontFromFileTTF("assets/fonts/inter/static/Inter_18pt-Bold.ttf", 20.0f);
        if (!m_large_font)
            m_large_font = m_main_font;

        m_small_font = io.Fonts->AddFontFromFileTTF("assets/fonts/inter/static/Inter_18pt-Light.ttf", 12.0f);
        if (!m_small_font)
            m_small_font = m_main_font;

        io.FontDefault = m_main_font;
        DONUT_INFO("ImGUI fonts loaded successfully");
    }

    auto Window::shutdown_im_gui() -> void
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        DONUT_INFO("ImGUI shutdown");
    }

    auto Window::begin_im_gui_frame() -> void
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    auto Window::end_im_gui_frame() -> void
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

    auto Window::glfw_error_callback(int error, const char* description) -> void
    {
        DONUT_ERROR("GLFW Error ({}): {}", error, description ? description : "");
    }

    auto Window::glfw_window_close_callback(GLFWwindow* window) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        WindowCloseEvent event;
        win->m_event_handler.on_event(event);
    }

    auto Window::glfw_window_size_callback(GLFWwindow* window, int width, int height) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        win->m_width = width;
        win->m_height = height;

        WindowResizeEvent event(width, height);
        win->m_event_handler.on_event(event);
    }

    auto Window::glfw_window_focus_callback(GLFWwindow* window, int focused) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

        if (focused)
        {
            WindowFocusEvent event;
            win->m_event_handler.on_event(event);
        }
        else
        {
            WindowLostFocusEvent event;
            win->m_event_handler.on_event(event);
        }
    }

    auto Window::glfw_window_pos_callback(GLFWwindow* window, int xpos, int ypos) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        WindowMovedEvent event(xpos, ypos);
        win->m_event_handler.on_event(event);
    }

    auto Window::glfw_key_callback(GLFWwindow* window, int key, int scancode,
                                   int action, int mods) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

        switch (action)
        {
            case GLFW_PRESS:
            {
                KeyPressedEvent event(key, false);
                win->m_event_handler.on_event(event);
            } break;
            case GLFW_RELEASE:
            {
                KeyReleasedEvent event(key);
                win->m_event_handler.on_event(event);
            } break;
            case GLFW_REPEAT:
            {
                KeyPressedEvent event(key, true);
                win->m_event_handler.on_event(event);
            } break;
        }
    }

    auto Window::glfw_char_callback(GLFWwindow* window, unsigned int keycode) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        KeyTypedEvent event(keycode);
        win->m_event_handler.on_event(event);
    }

    auto Window::glfw_mouse_button_callback(GLFWwindow* window, int button,
                                            int action, int mods) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

        switch (action)
        {
            case GLFW_PRESS:
            {
                MouseButtonPressedEvent event(button);
                win->m_event_handler.on_event(event);
            } break;
            case GLFW_RELEASE:
            {
                MouseButtonReleasedEvent event(button);
                win->m_event_handler.on_event(event);
            } break;
        }
    }

    auto Window::glfw_mouse_scroll_callback(GLFWwindow* window, double x_offset, double y_offset) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        MouseScrolledEvent event((float)x_offset, (float)y_offset);
        win->m_event_handler.on_event(event);
    }

    auto Window::glfw_cursor_pos_callback(GLFWwindow* window, double x_pos, double y_pos) -> void
    {
        Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
        MouseMovedEvent event((float)x_pos, (float)y_pos);
        win->m_event_handler.on_event(event);
    }
}
