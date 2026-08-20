#pragma once

#include <string>
#include <toml.hpp>

namespace Donut
{
    struct SimulationSettings
    {
        int   target_fps          = 60;
        int   compute_height      = 512;
        int   max_steps_moving    = 30000;
        int   max_steps_static    = 15000;
        float early_exit_distance = 5e12f;
        bool  gravity_enabled     = true;
        float disk_thickness      = 0.1f;
        float disk_density        = 0.1f;
        float rotation_speed      = 1.0f;
        float blur_strength       = 2.0f;
        float glow_intensity      = 0.1f;
    };

    struct GraphicsSettings
    {
        std::string render_api               = "OpenGL";
        bool        v_sync_enabled           = true;
        bool        show_fps                 = true;
        bool        show_performance_metrics = true;
        bool        show_debug_info          = false;
        bool        enable_anti_aliasing     = true;
        std::string selected_theme           = "Dark";
    };

    struct Settings
    {
        SimulationSettings simulation;
        GraphicsSettings   graphics;
    };

    class SettingsManager
    {
    public:
        static auto initialize() -> void;
        static auto shutdown() -> void;

        static auto load_settings() -> void;
        static auto save_settings() -> void;

        static auto get_settings() -> Settings&            { return s_settings; }
        static auto get_settings_const() -> const Settings& { return s_settings; }

        static auto set_simulation_settings(const SimulationSettings& settings) -> void;
        static auto set_graphics_settings(const GraphicsSettings& settings) -> void;

        static auto get_target_fps() -> int                { return s_settings.simulation.target_fps;            }
        static auto get_compute_height() -> int            { return s_settings.simulation.compute_height;        }
        static auto get_max_steps_moving() -> int          { return s_settings.simulation.max_steps_moving;      }
        static auto get_max_steps_static() -> int          { return s_settings.simulation.max_steps_static;      }
        static auto get_early_exit_distance() -> float     { return s_settings.simulation.early_exit_distance;   }
        static auto get_gravity_enabled() -> bool          { return s_settings.simulation.gravity_enabled;       }
        static auto get_disk_thickness() -> float          { return s_settings.simulation.disk_thickness;        }
        static auto get_disk_density() -> float            { return s_settings.simulation.disk_density;          }
        static auto get_rotation_speed() -> float          { return s_settings.simulation.rotation_speed;        }
        static auto get_blur_strength() -> float           { return s_settings.simulation.blur_strength;         }
        static auto get_glow_intensity() -> float          { return s_settings.simulation.glow_intensity;        }
        static auto get_render_api() -> std::string        { return s_settings.graphics.render_api;              }
        static auto get_v_sync_enabled() -> bool           { return s_settings.graphics.v_sync_enabled;          }
        static auto get_show_fps() -> bool                 { return s_settings.graphics.show_fps;                }
        static auto get_show_performance_metrics() -> bool { return s_settings.graphics.show_performance_metrics; }
        static auto get_show_debug_info() -> bool          { return s_settings.graphics.show_debug_info;         }
        static auto get_enable_anti_aliasing() -> bool     { return s_settings.graphics.enable_anti_aliasing;    }
        static auto get_selected_theme() -> std::string    { return s_settings.graphics.selected_theme;          }

    private:
        static auto get_settings_file_path() -> std::string;
        static auto load_default_settings() -> void;

    private:
        static Settings s_settings;
        static bool     s_initialized;
    };
}
