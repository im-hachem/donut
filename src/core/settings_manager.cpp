#include "settings_manager.h"
#include "log.h"
#include "theme_manager.h"
#include "rendering/renderer.h"

#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Donut
{
    Settings SettingsManager::s_settings;
    bool     SettingsManager::s_initialized = false;

    auto SettingsManager::initialize() -> void
    {
        if (s_initialized)
            return;
            
        load_settings();
        s_initialized = true;
        DONUT_INFO("Settings Manager initialized");
    }

    auto SettingsManager::shutdown() -> void
    {
        if (!s_initialized)
            return;
            
        save_settings();
        s_initialized = false;
        DONUT_INFO("Settings Manager shutdown");
    }

    auto SettingsManager::load_settings() -> void
    {
        std::string file_path = get_settings_file_path();
        
        try
        {
            if (std::filesystem::exists(file_path))
            {
                auto config = toml::parse(file_path);
                
                if (config.contains("simulation"))
                {
                    auto sim = config["simulation"];
                    s_settings.simulation.target_fps         = toml::find_or(sim, "target_fps",          60);
                    s_settings.simulation.compute_height     = toml::find_or(sim, "compute_height",      512);
                    s_settings.simulation.max_steps_moving    = toml::find_or(sim, "max_steps_moving",    30000);
                    s_settings.simulation.max_steps_static    = toml::find_or(sim, "max_steps_static",    15000);
                    s_settings.simulation.early_exit_distance = toml::find_or(sim, "early_exit_distance", 5e12f);
                    s_settings.simulation.gravity_enabled    = toml::find_or(sim, "gravity_enabled",     true);
                    s_settings.simulation.disk_thickness     = toml::find_or(sim, "disk_thickness",      0.1f);
                    s_settings.simulation.disk_density       = toml::find_or(sim, "disk_density",        0.1f);
                    s_settings.simulation.rotation_speed     = toml::find_or(sim, "rotation_speed",      1.0f);
                    s_settings.simulation.blur_strength      = toml::find_or(sim, "blur_strength",       2.0f);
                    s_settings.simulation.glow_intensity     = toml::find_or(sim, "glow_intensity",      0.1f);
                    
                    s_settings.simulation.target_fps         = std::max(30,    std::min(120,   s_settings.simulation.target_fps));
                    s_settings.simulation.compute_height     = std::max(64,    std::min(2048,  s_settings.simulation.compute_height));
                    s_settings.simulation.max_steps_moving    = std::max(1000,  std::min(60000, s_settings.simulation.max_steps_moving));
                    s_settings.simulation.max_steps_static    = std::max(1000,  std::min(30000, s_settings.simulation.max_steps_static));
                    s_settings.simulation.early_exit_distance = std::max(1e11f, std::min(1e13f, s_settings.simulation.early_exit_distance));
                    s_settings.simulation.disk_thickness     = std::max(0.01f, std::min(5.0f,  s_settings.simulation.disk_thickness));
                    s_settings.simulation.disk_density       = std::max(0.01f, std::min(5.0f,  s_settings.simulation.disk_density));
                    s_settings.simulation.rotation_speed     = std::max(0.0f,  std::min(5.0f,  s_settings.simulation.rotation_speed));
                    s_settings.simulation.blur_strength      = std::max(0.1f,  std::min(10.0f, s_settings.simulation.blur_strength));
                    s_settings.simulation.glow_intensity     = std::max(0.01f, std::min(5.0f,  s_settings.simulation.glow_intensity));
                }
                
                if (config.contains("graphics"))
                {
                    auto gfx = config["graphics"];
                    s_settings.graphics.render_api              = toml::find_or(gfx, "render_api",               std::string("OpenGL"));
                    s_settings.graphics.v_sync_enabled           = toml::find_or(gfx, "vsync_enabled",            true);
                    s_settings.graphics.show_fps                = toml::find_or(gfx, "show_fps",                 true);
                    s_settings.graphics.show_performance_metrics = toml::find_or(gfx, "show_performance_metrics", true);
                    s_settings.graphics.show_debug_info          = toml::find_or(gfx, "show_debug_info",          false);
                    s_settings.graphics.enable_anti_aliasing     = toml::find_or(gfx, "enable_anti_aliasing",     true);
                    s_settings.graphics.selected_theme          = toml::find_or(gfx, "selected_theme",           std::string("Dark"));
                    
                    if (s_settings.graphics.render_api != "OpenGL" && 
                        s_settings.graphics.render_api != "Vulkan")
                        s_settings.graphics.render_api = "OpenGL";
                    if (s_settings.graphics.selected_theme != "Dark" && 
                        s_settings.graphics.selected_theme != "Light" && 
                        s_settings.graphics.selected_theme != "Blue")
                        s_settings.graphics.selected_theme = "Dark";
                }
                
                DONUT_INFO("Settings loaded from {}", file_path);
            }
            else
            {
                load_default_settings();
                save_settings();
                DONUT_INFO("No settings file found, created default settings");
            }
        }
        catch (const std::exception& e)
        {
            DONUT_ERROR("Failed to load settings: {}", e.what());
            load_default_settings();
        }
    }

    auto SettingsManager::save_settings() -> void
    {
        std::string file_path = get_settings_file_path();
        
        try
        {
            std::filesystem::path path(file_path);
            std::filesystem::create_directories(path.parent_path());
            
            toml::value simulation = toml::table
            {
                {"target_fps",          s_settings.simulation.target_fps        },
                {"compute_height",      s_settings.simulation.compute_height    },
                {"max_steps_moving",    s_settings.simulation.max_steps_moving   },
                {"max_steps_static",    s_settings.simulation.max_steps_static   },
                {"early_exit_distance", s_settings.simulation.early_exit_distance},
                {"gravity_enabled",     s_settings.simulation.gravity_enabled   },
                {"disk_thickness",      s_settings.simulation.disk_thickness    },
                {"disk_density",        s_settings.simulation.disk_density      },
                {"rotation_speed",      s_settings.simulation.rotation_speed    },
                {"blur_strength",       s_settings.simulation.blur_strength     },
                {"glow_intensity",      s_settings.simulation.glow_intensity    }
            };
            
            toml::value graphics = toml::table
            {
                {"render_api",               s_settings.graphics.render_api             },
                {"vsync_enabled",            s_settings.graphics.v_sync_enabled          },
                {"show_fps",                 s_settings.graphics.show_fps               },
                {"show_performance_metrics", s_settings.graphics.show_performance_metrics},
                {"show_debug_info",          s_settings.graphics.show_debug_info         },
                {"enable_anti_aliasing",     s_settings.graphics.enable_anti_aliasing    },
                {"selected_theme",           s_settings.graphics.selected_theme         }
            };
            
            toml::value config = toml::table
            {
                {"simulation", simulation},
                {"graphics",   graphics}
            };
            
            std::ofstream file(file_path);
            file << config;
            file.close();
            
            DONUT_INFO("Settings saved to {}", file_path);
        }
        catch (const std::exception& e)
        {
            DONUT_ERROR("Failed to save settings: {}", e.what());
        }
    }

    auto SettingsManager::set_simulation_settings(const SimulationSettings& settings) -> void
    {
        s_settings.simulation = settings;
        save_settings();
    }

    auto SettingsManager::set_graphics_settings(const GraphicsSettings& settings) -> void
    {
        s_settings.graphics = settings;
        save_settings();
    }

    auto SettingsManager::get_settings_file_path() -> std::string
    {
        return "config/settings.toml";
    }

    auto SettingsManager::load_default_settings() -> void
    {
        s_settings.simulation.target_fps         = 60;
        s_settings.simulation.compute_height     = 512;
        s_settings.simulation.max_steps_moving    = 30000;
        s_settings.simulation.max_steps_static    = 15000;
        s_settings.simulation.early_exit_distance = 5e12f;
        s_settings.simulation.gravity_enabled    = true;
        s_settings.simulation.disk_thickness     = 0.1f;
        s_settings.simulation.disk_density       = 0.1f;
        s_settings.simulation.rotation_speed     = 1.0f;
        s_settings.simulation.blur_strength      = 2.0f;
        s_settings.simulation.glow_intensity     = 0.1f;
        
        s_settings.graphics.render_api              = "OpenGL";
        s_settings.graphics.v_sync_enabled           = true;
        s_settings.graphics.show_fps                = true;
        s_settings.graphics.show_performance_metrics = true;
        s_settings.graphics.show_debug_info          = false;
        s_settings.graphics.enable_anti_aliasing     = true;
        s_settings.graphics.selected_theme          = "Dark";
    }
}
