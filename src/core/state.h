#pragma once

#include "event.h"

namespace Donut
{
    class State
    {
    public:
        virtual ~State() = default;

        virtual auto on_enter() -> void                  = 0;
        virtual auto on_exit() -> void                   = 0;
        virtual auto on_update(float delta_time) -> void = 0;
        virtual auto on_render() -> void                 = 0;
        virtual auto on_im_ui_render() -> void           = 0;
        virtual auto on_event(Event& event) -> void      = 0;
    };
};
