#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>

namespace Donut
{
    class Event;
    class EventDispatcher;

    enum class EventType
    {
        None = 0,

        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,

        KeyPressed,
        KeyReleased,
        KeyTyped,

        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled
    };

    enum EventCategory
    {
        None                     = 0,
        EventCategoryApplication = 1 << 0,
        EventCategoryInput       = 1 << 1,
        EventCategoryKeyboard    = 1 << 2,
        EventCategoryMouse       = 1 << 3,
        EventCategoryMouseButton = 1 << 4
    };

    class Event
    {
    public:
        virtual ~Event() = default;

        virtual auto get_event_type() const -> EventType = 0;
        virtual auto get_name() const -> const char* = 0;
        virtual auto get_category_flags() const -> int = 0;
        virtual auto to_string() const -> std::string { return get_name(); }

        auto is_in_category(EventCategory category) -> bool { return get_category_flags() & category; }

        bool handled = false;
    };

    #define EVENT_CLASS_TYPE(type)                                                                    \
        static  auto get_static_type() -> EventType               { return EventType::type; }         \
        virtual auto get_event_type() const -> EventType override  { return get_static_type(); }       \
        virtual auto get_name() const -> const char* override      { return #type; }

    #define APPLICATION_EVENT_CLASS_TYPE(type) \
        EVENT_CLASS_TYPE(type)                 \
        virtual auto get_category_flags() const -> int override { return EventCategoryApplication; }

    #define MOUSE_EVENT_CLASS_TYPE(type) \
        EVENT_CLASS_TYPE(type)           \
        virtual auto get_category_flags() const -> int override { return EventCategoryMouse | \
                                                                         EventCategoryInput; }

    #define MOUSE_BUTTON_EVENT_CLASS_TYPE(type) \
        EVENT_CLASS_TYPE(type)                  \
        virtual auto get_category_flags() const -> int override { return EventCategoryMouseButton | \
                                                                         EventCategoryMouse       | \
                                                                         EventCategoryInput; }

    #define KEYBOARD_EVENT_CLASS_TYPE(type) \
        EVENT_CLASS_TYPE(type)              \
        virtual auto get_category_flags() const -> int override { return EventCategoryKeyboard | \
                                                                         EventCategoryInput; }

    class WindowResizeEvent
        : public Event
    {
    public:
        WindowResizeEvent(unsigned int width, unsigned int height)
            : m_width(width), m_height(height) { }

        auto get_width() const -> unsigned int { return m_width; }
        auto get_height() const -> unsigned int { return m_height; }

        auto to_string() const -> std::string override
        {
            return "WindowResizeEvent: " + std::to_string(m_width) + ", " + std::to_string(m_height);
        }

        APPLICATION_EVENT_CLASS_TYPE(WindowResize)

    private:
        unsigned int m_width, m_height;
    };

    class WindowCloseEvent
        : public Event
    {
    public:
        WindowCloseEvent() = default;
        APPLICATION_EVENT_CLASS_TYPE(WindowClose)
    };

    class WindowFocusEvent
        : public Event
    {
    public:
        WindowFocusEvent() = default;
        APPLICATION_EVENT_CLASS_TYPE(WindowFocus)
    };

    class WindowLostFocusEvent
        : public Event
    {
    public:
        WindowLostFocusEvent() = default;
        APPLICATION_EVENT_CLASS_TYPE(WindowLostFocus)
    };

    class WindowMovedEvent
        : public Event
    {
    public:
        WindowMovedEvent(int x, int y)
            : m_x(x), m_y(y) { }

        auto get_x() const -> int { return m_x; }
        auto get_y() const -> int { return m_y; }

        auto to_string() const -> std::string override
        {
            return "WindowMovedEvent: " + std::to_string(m_x) + ", " + std::to_string(m_y);
        }

        APPLICATION_EVENT_CLASS_TYPE(WindowMoved)

    private:
        int m_x, m_y;
    };

    class KeyEvent
        : public Event
    {
    public:
        auto get_key_code() const -> int { return m_key_code; }
        virtual auto get_category_flags() const -> int override { return EventCategoryKeyboard |
                                                                         EventCategoryInput; }

    protected:
        KeyEvent(int keycode)
            : m_key_code(keycode) { }
        int m_key_code;
    };

    class KeyPressedEvent
        : public KeyEvent
    {
    public:
        KeyPressedEvent(int keycode, bool is_repeat = false)
            : KeyEvent(keycode), m_is_repeat(is_repeat) { }

        auto is_repeat() const -> bool { return m_is_repeat; }

        auto to_string() const -> std::string override
        {
            return "KeyPressedEvent: " + std::to_string(m_key_code) +
                   " (repeat = " + std::to_string(m_is_repeat) + ")";
        }

        KEYBOARD_EVENT_CLASS_TYPE(KeyPressed)

    private:
        bool m_is_repeat;
    };

    class KeyReleasedEvent
        : public KeyEvent
    {
    public:
        KeyReleasedEvent(int keycode)
            : KeyEvent(keycode) { }

        auto to_string() const -> std::string override
        {
            return "KeyReleasedEvent: " + std::to_string(m_key_code);
        }

        KEYBOARD_EVENT_CLASS_TYPE(KeyReleased)
    };

    class KeyTypedEvent
        : public KeyEvent
    {
    public:
        KeyTypedEvent(int keycode)
            : KeyEvent(keycode) { }

        auto to_string() const -> std::string override
        {
            return "KeyTypedEvent: " + std::to_string(m_key_code);
        }

        KEYBOARD_EVENT_CLASS_TYPE(KeyTyped)
    };

    class MouseMovedEvent
        : public Event
    {
    public:
        MouseMovedEvent(float x, float y)
            : m_mouse_x(x), m_mouse_y(y) { }

        auto get_x() const -> float { return m_mouse_x; }
        auto get_y() const -> float { return m_mouse_y; }

        auto to_string() const -> std::string override
        {
            return "MouseMovedEvent: " + std::to_string(m_mouse_x) + ", " + std::to_string(m_mouse_y);
        }

        MOUSE_EVENT_CLASS_TYPE(MouseMoved)

    private:
        float m_mouse_x, m_mouse_y;
    };

    class MouseScrolledEvent
        : public Event
    {
    public:
        MouseScrolledEvent(float x_offset, float y_offset)
            : m_x_offset(x_offset), m_y_offset(y_offset) { }

        auto get_x_offset() const -> float { return m_x_offset; }
        auto get_y_offset() const -> float { return m_y_offset; }

        auto to_string() const -> std::string override
        {
            return "MouseScrolledEvent: " + std::to_string(m_x_offset) +
                   ", " + std::to_string(m_y_offset);
        }

        MOUSE_EVENT_CLASS_TYPE(MouseScrolled)

    private:
        float m_x_offset, m_y_offset;
    };

    class MouseButtonEvent
        : public Event
    {
    public:
        auto get_mouse_button() const -> int { return m_button; }
        virtual auto get_category_flags() const -> int override { return EventCategoryMouseButton |
                                                                         EventCategoryMouse       |
                                                                         EventCategoryInput; }

    protected:
        MouseButtonEvent(int button)
            : m_button(button) { }
        int m_button;
    };

    class MouseButtonPressedEvent
        : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(int button)
            : MouseButtonEvent(button) { }

        auto to_string() const -> std::string override
        {
            return "MouseButtonPressedEvent: " + std::to_string(m_button);
        }

        MOUSE_BUTTON_EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    class MouseButtonReleasedEvent
        : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(int button)
            : MouseButtonEvent(button) { }

        auto to_string() const -> std::string override
        {
            return "MouseButtonReleasedEvent: " + std::to_string(m_button);
        }

        MOUSE_BUTTON_EVENT_CLASS_TYPE(MouseButtonReleased)
    };

    class EventDispatcher
    {
    public:
        EventDispatcher(Event& event)
            : m_event(event) { }

        template<typename T, typename F>
        auto dispatch(const F& func) -> bool
        {
            if (m_event.get_event_type() == T::get_static_type())
            {
                m_event.handled = func(static_cast<T&>(m_event));
                return true;
            }
            return false;
        }

    private:
        Event& m_event;
    };

    class EventHandler
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        EventHandler() = default;
        ~EventHandler() = default;

        auto set_event_callback(const EventCallbackFn& callback) -> void
        {
            m_event_callback = callback;
        }

        auto on_event(Event& event) -> void
        {
            if (m_event_callback)
                m_event_callback(event);
        }

        template<typename T>
        auto bind_event(const std::function<void(T&)>& callback) -> void
        {
            m_event_callback = [callback](Event& event)
            {
                if (event.get_event_type() == T::get_static_type())
                    callback(static_cast<T&>(event));
            };
        }

    private:
        EventCallbackFn m_event_callback;
    };

    #undef EVENT_CLASS_TYPE
    #undef APPLICATION_EVENT_CLASS_TYPE
    #undef MOUSE_EVENT_CLASS_TYPE
    #undef MOUSE_BUTTON_EVENT_CLASS_TYPE
    #undef KEYBOARD_EVENT_CLASS_TYPE
}
