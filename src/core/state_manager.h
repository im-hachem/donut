#pragma once

#include "memory.h"
#include "state.h"

#include <string>
#include <unordered_map>

namespace Donut
{
    class StateManager
    {
    public:
        ~StateManager() = default;
        auto shutdown() -> void;

        auto update(float delta_time) -> void;
        auto render() -> void;
        auto on_im_ui_render() -> void;
        auto on_event(Event& event) -> void;

        auto register_state(const std::string& state_name, Scope<State> state) -> void;
        auto switch_to_state(const std::string& state_name) -> void;

        auto get_current_state_name() const -> std::string { return m_current_state_name; }
        auto get_current_state() const -> State* { return m_current_state; }

        auto get_state(const std::string& state_name) -> State*;
        auto has_state(const std::string& state_name) const -> bool;

    private:
        auto create_states() -> void;
        auto destroy_states() -> void;

    private:
        State*      m_current_state      = nullptr;
        std::string m_current_state_name = "";

        std::unordered_map<std::string, Scope<State>> m_states;
    };
}
