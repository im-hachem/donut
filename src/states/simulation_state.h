#pragma once

#include "core/state.h"
#include "core/event.h"
#include "core/log.h"
#include "engine/engine.h"
#include <vector>

namespace Donut
{
    class SimulationState
        : public State
    {
    public:
        ~SimulationState() = default;
        
        auto on_enter() -> void override;
        auto on_exit() -> void override;
        auto on_update(float delta_time) -> void override;
        auto on_render() -> void override;
        auto on_im_ui_render() -> void override;
        auto on_event(Event& event) -> void override;
    
    private:
        bool m_initialized = false;
    };
};
