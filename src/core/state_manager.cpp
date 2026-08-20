#include "state_manager.h"
#include "log.h"

namespace Donut
{
    auto StateManager::shutdown() -> void
    {
        if (m_current_state)
            m_current_state->on_exit();

        destroy_states();
        DONUT_INFO("StateManager shutdown");
    }

    auto StateManager::destroy_states() -> void
    {
        m_states.clear();
        m_current_state = nullptr;
        m_current_state_name = "";
    }

    auto StateManager::register_state(const std::string& state_name, Scope<State> state) -> void
    {
        if (m_states.find(state_name) != m_states.end())
            DONUT_WARN("State '{}' already exists, overwriting", state_name);
        m_states[state_name] = std::move(state);
        DONUT_INFO("Registered state: {}", state_name);
    }

    auto StateManager::switch_to_state(const std::string& state_name) -> void
    {
        auto it = m_states.find(state_name);
        if (it == m_states.end())
        {
            DONUT_ERROR("Attempted to switch to unknown state: {}", state_name);
            return;
        }

        State* new_state = it->second.get();
        if (new_state == m_current_state)
            return;

        if (m_current_state)
            m_current_state->on_exit();

        m_current_state = new_state;
        m_current_state_name = state_name;
        m_current_state->on_enter();
        DONUT_INFO("Switched to state: {}", state_name);
    }

    auto StateManager::get_state(const std::string& state_name) -> State*
    {
        auto it = m_states.find(state_name);
        if (it != m_states.end())
            return it->second.get();
        return nullptr;
    }

    auto StateManager::has_state(const std::string& state_name) const -> bool
    {
        return m_states.find(state_name) != m_states.end();
    }

    auto StateManager::update(float delta_time) -> void
    {
        if (m_current_state)
            m_current_state->on_update(delta_time);
    }

    auto StateManager::render() -> void
    {
        if (m_current_state)
            m_current_state->on_render();
    }

    auto StateManager::on_im_ui_render() -> void
    {
        if (m_current_state)
            m_current_state->on_im_ui_render();
    }

    auto StateManager::on_event(Event& event) -> void
    {
        if (m_current_state)
            m_current_state->on_event(event);
    }
}
