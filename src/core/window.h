#pragma once

#include "event.h"
#include "log.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <string>

namespace Donut
{
    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        Window(const std::string& title, int width, int height);
        ~Window();

        auto should_close() const -> bool;
        auto on_update() const -> void;

        auto set_event_callback(const EventCallbackFn& callback) -> void
        {
            m_event_handler.set_event_callback(callback);
        }

        auto get_event_handler() -> EventHandler& { return m_event_handler; }

        auto get_width() const -> unsigned int  { return m_width;  }
        auto get_height() const -> unsigned int { return m_height; }

        auto get_native_window() const -> void* { return m_window; }

        auto set_cursor_locked(bool locked) -> void;
        auto set_cursor_visible(bool visible) -> void;
        auto is_cursor_locked() const -> bool  { return m_cursor_locked;  }
        auto is_cursor_visible() const -> bool { return m_cursor_visible; }

        auto init_im_gui() -> void;
        auto begin_im_gui_frame() -> void;
        auto end_im_gui_frame() -> void;

    private:
        auto init() -> void;
        auto shutdown() -> void;

        auto setup_im_gui_fonts() -> void;
        auto shutdown_im_gui() -> void;

        static auto glfw_error_callback(int error, const char* description) -> void;
        static auto glfw_window_close_callback(GLFWwindow* window) -> void;
        static auto glfw_window_size_callback(GLFWwindow* window, int width, int height) -> void;
        static auto glfw_window_focus_callback(GLFWwindow* window, int focused) -> void;
        static auto glfw_window_pos_callback(GLFWwindow* window, int xpos, int ypos) -> void;
        static auto glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) -> void;
        static auto glfw_char_callback(GLFWwindow* window, unsigned int keycode) -> void;
        static auto glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) -> void;
        static auto glfw_mouse_scroll_callback(GLFWwindow* window, double x_offset, double y_offset) -> void;
        static auto glfw_cursor_pos_callback(GLFWwindow* window, double x_pos, double y_pos) -> void;

    private:
        GLFWwindow*  m_window;
        EventHandler m_event_handler;

        std::string  m_title;
        unsigned int m_width;
        unsigned int m_height;
        bool m_is_closed;
        bool m_cursor_locked  = false;
        bool m_cursor_visible = true;

        ImFont* m_main_font = nullptr;
        ImFont* m_small_font = nullptr;
        ImFont* m_large_font = nullptr;
    };
}
