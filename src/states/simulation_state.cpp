#include "simulation_state.h"
#include "rendering/renderer.h"
#include "core/application.h"
#include "core/hdri_manager.h"
#include "core/window.h"
#include "core/event.h"
#include "core/settings_manager.h"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Donut
{
    auto SimulationState::on_enter() -> void
    {
        DONUT_INFO("Entering Simulation State");
        
        const auto& settings = SettingsManager::get_settings_const();
        auto& engine = Application::get().get_engine();
        
        engine.set_target_fps(settings.simulation.target_fps);
        engine.set_compute_height(settings.simulation.compute_height);
        engine.set_max_steps_moving(settings.simulation.max_steps_moving);
        engine.set_max_steps_static(settings.simulation.max_steps_static);
        engine.set_early_exit_distance(settings.simulation.early_exit_distance);
        engine.get_gravity() = settings.simulation.gravity_enabled;
        
        engine.set_disk_thickness(settings.simulation.disk_thickness);
        engine.set_disk_density(settings.simulation.disk_density);
        engine.set_rotation_speed(settings.simulation.rotation_speed);
        engine.set_blur_strength(settings.simulation.blur_strength);
        engine.set_glow_intensity(settings.simulation.glow_intensity);
        
        engine.update_compute_dimensions();
        m_initialized = true;
    }
    
    auto SimulationState::on_exit() -> void
    {
        DONUT_INFO("Exiting Simulation State");
    }
    
    auto SimulationState::on_update(float delta_time) -> void
    {
        auto& engine = Application::get().get_engine();
        engine.update_performance(delta_time);
        engine.update_window_dimensions();
        engine.update_physics(delta_time);

        if (engine.get_camera().is_dragging())
        {
            GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            engine.get_camera().process_orbital_mouse_move(xpos, ypos);
        }
    }
    
    auto SimulationState::on_render() -> void
    {
        auto& engine = Application::get().get_engine();
        RenderCommand::set_clear_color(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        RenderCommand::clear();
        RenderCommand::set_viewport(0, 0, static_cast<uint32_t>(engine.get_width()), static_cast<uint32_t>(engine.get_height()));

        engine.dispatch_compute(engine.get_camera());
        engine.draw_blur_pass();
    }
    
    auto SimulationState::on_event(Event& event) -> void
    {
        auto& engine = Application::get().get_engine();
        
        if (event.get_event_type() == EventType::MouseButtonPressed)
        {
            MouseButtonPressedEvent& e = (MouseButtonPressedEvent&)event;
            int button = e.get_mouse_button();
            
            GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
            Camera& camera = engine.get_camera();
            double last_x = camera.get_last_x();
            double last_y = camera.get_last_y();
            glfwGetCursorPos(window, &last_x, &last_y);
            
            camera.process_orbital_mouse_button(button, GLFW_PRESS, 0);
        }
        else if (event.get_event_type() == EventType::MouseButtonReleased)
        {
            MouseButtonReleasedEvent& e = (MouseButtonReleasedEvent&)event;
            int button = e.get_mouse_button();
            engine.get_camera().process_orbital_mouse_button(button, GLFW_RELEASE, 0);
        }
        
        if (event.get_event_type() == EventType::MouseScrolled)
        {
            MouseScrolledEvent& e = (MouseScrolledEvent&)event;
            engine.get_camera().process_orbital_scroll(e.get_x_offset(), e.get_y_offset());
        }
        
        if (event.get_event_type() == EventType::KeyPressed)
        {
            KeyPressedEvent& e = (KeyPressedEvent&)event;
            if (e.get_key_code() == GLFW_KEY_G)
            {
                engine.get_gravity() = !engine.get_gravity();
                DONUT_INFO("Gravity turned {}", engine.get_gravity() ? "ON" : "OFF");
            }
        }
    }
    
    auto SimulationState::on_im_ui_render() -> void
    {
        auto& engine = Application::get().get_engine();
        
        ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 420, 20), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("Simulation Controls", nullptr, ImGuiWindowFlags_NoCollapse);
        
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Black Hole Simulation");
        ImGui::SameLine();
        if (ImGui::Button("Back to Config"))
            Application::get().get_state_manager().switch_to_state("Config");
        ImGui::SameLine();
        if (ImGui::Button("Save Settings"))
        {
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.target_fps = engine.get_target_fps();
            settings.compute_height = engine.get_compute_height();
            settings.max_steps_moving = engine.get_max_steps_moving();
            settings.max_steps_static = engine.get_max_steps_static();
            settings.early_exit_distance = engine.get_early_exit_distance();
            settings.gravity_enabled = engine.get_gravity();
            SettingsManager::set_simulation_settings(settings);
            DONUT_INFO("Simulation settings saved manually");
        }

        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Performance");
        ImGui::Separator();
        
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Engine FPS: %.1f", engine.get_current_fps());
        
        int target_fps = engine.get_target_fps();
        if (ImGui::SliderInt("Target FPS", &target_fps, 30, 120))
        {
            engine.set_target_fps(target_fps);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.target_fps = target_fps;
            SettingsManager::set_simulation_settings(settings);
        }
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Simulation Info");
        ImGui::Separator();
        
        ImGui::Text("Resolution: %dx%d", engine.get_width(), engine.get_height());
        ImGui::Text("Compute Resolution: %dx%d", engine.get_compute_width(), engine.get_compute_height());
        ImGui::Text("Objects: %zu", engine.get_objects().size());
        
        if (ImGui::Button("Print Object Info"))
            engine.print_object_info();
        
        int compute_height = engine.get_compute_height();
        if (ImGui::SliderInt("Compute Height", &compute_height, 64, 2048))
        {
            engine.set_compute_height(compute_height);
            engine.update_compute_dimensions();
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.compute_height = compute_height;
            SettingsManager::set_simulation_settings(settings);
        }
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Compute Width: %d (auto-calculated)", engine.get_compute_width());
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Quality Settings");
        ImGui::Separator();
        
        int max_steps_moving = engine.get_max_steps_moving();
        if (ImGui::SliderInt("Max Steps (Moving)", &max_steps_moving, 1000, 60000))
        {
            engine.set_max_steps_moving(max_steps_moving);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.max_steps_moving = max_steps_moving;
            SettingsManager::set_simulation_settings(settings);
        }
        int max_steps_static = engine.get_max_steps_static();
        if (ImGui::SliderInt("Max Steps (Static)", &max_steps_static, 1000, 30000))
        {
            engine.set_max_steps_static(max_steps_static);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.max_steps_static = max_steps_static;
            SettingsManager::set_simulation_settings(settings);
        }
        float early_exit_distance = engine.get_early_exit_distance();
        if (ImGui::SliderFloat("Early Exit Distance", &early_exit_distance, 1e11f, 1e13f, "%.2e"))
        {
            engine.set_early_exit_distance(early_exit_distance);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.early_exit_distance = early_exit_distance;
            SettingsManager::set_simulation_settings(settings);
        }
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Physics");
        ImGui::Separator();
        
        bool& gravity = engine.get_gravity();
        if (ImGui::Checkbox("Gravity Enabled", &gravity))
        {
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.gravity_enabled = gravity;
            SettingsManager::set_simulation_settings(settings);
        }
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Accretion Disk");
        ImGui::Separator();
        
        float disk_thickness = engine.get_disk_thickness();
        if (ImGui::SliderFloat("Cloud Thickness", &disk_thickness, 0.1f, 2.0f, "%.2f"))
        {
            engine.set_disk_thickness(disk_thickness);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.disk_thickness = disk_thickness;
            SettingsManager::set_simulation_settings(settings);
        }
        ImGui::TextDisabled("Thickness relative to Schwarzschild radius");
        
        float disk_density = engine.get_disk_density();
        if (ImGui::SliderFloat("Cloud Density", &disk_density, 0.1f, 3.0f, "%.2f"))
        {
            engine.set_disk_density(disk_density);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.disk_density = disk_density;
            SettingsManager::set_simulation_settings(settings);
        }
        ImGui::TextDisabled("Overall density multiplier");
        
        float rotation_speed = engine.get_rotation_speed();
        if (ImGui::SliderFloat("Rotation Speed", &rotation_speed, 0.0f, 3.0f, "%.2f"))
        {
            engine.set_rotation_speed(rotation_speed);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.rotation_speed = rotation_speed;
            SettingsManager::set_simulation_settings(settings);
        }
        ImGui::TextDisabled("Rotation speed multiplier (0 = no rotation)");
        
        float blur_strength = engine.get_blur_strength();
        if (ImGui::SliderFloat("Blur Strength", &blur_strength, 0.5f, 5.0f, "%.2f"))
        {
            engine.set_blur_strength(blur_strength);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.blur_strength = blur_strength;
            SettingsManager::set_simulation_settings(settings);
        }
        ImGui::TextDisabled("Blur radius for glow effect");
        
        float glow_intensity = engine.get_glow_intensity();
        if (ImGui::SliderFloat("Glow Intensity", &glow_intensity, 0.1f, 3.0f, "%.2f"))
        {
            engine.set_glow_intensity(glow_intensity);
            SimulationSettings settings = SettingsManager::get_settings_const().simulation;
            settings.glow_intensity = glow_intensity;
            SettingsManager::set_simulation_settings(settings);
        }
        ImGui::TextDisabled("Intensity of the glow effect");
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "HDRI Environment");
        ImGui::Separator();
        
        static int selected_hdri = 0;
        ImGui::PushID("SimulationHDRI");
        auto& hdri_manager = HDRIManager::get();
        const auto& available_hdri = hdri_manager.get_available_hdri();
        
        static std::vector<std::string> hdri_option_names;
        static std::vector<const char*> hdri_options;
        
        if (hdri_option_names.size() != available_hdri.size())
        {
            hdri_option_names.clear();
            hdri_options.clear();
            
            for (const auto& path : available_hdri)
            {
                hdri_option_names.push_back(hdri_manager.get_hdri_name(path));
                hdri_options.push_back(hdri_option_names.back().c_str());
            }
        }

        if (ImGui::Combo("HDRI Environment", &selected_hdri, hdri_options.data(), static_cast<int>(hdri_options.size())))
        {
            auto& hdri_manager = HDRIManager::get();
            hdri_manager.set_current_hdri(available_hdri[selected_hdri]);
            engine.set_hdri_environment(hdri_manager.get_current_hdri());
        }
        ImGui::TextDisabled("HDRI provides background and lighting for the simulation");
        ImGui::PopID();
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Camera");
        ImGui::Separator();
        
        ImGui::Text("Position: (%.2e, %.2e, %.2e)", 
                   engine.get_camera().get_orbital_position().x, 
                   engine.get_camera().get_orbital_position().y, 
                   engine.get_camera().get_orbital_position().z);
        ImGui::Text("Radius: %.2e", engine.get_camera().get_orbital_radius());
        ImGui::Text("Azimuth: %.2f", engine.get_camera().get_azimuth());
        ImGui::Text("Elevation: %.2f", engine.get_camera().get_elevation());
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Export");
        ImGui::Separator();
        
        if (ImGui::Button("Export Frame (1080p)", ImVec2(-1, 30)))
        {
            auto& engine = Application::get().get_engine();
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << "frame_1080p_" << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S") << ".png";
            engine.export_high_res_frame(ss.str(), 1920, 1080);
        }
        
        if (ImGui::Button("Export High-Res Frame (4K)", ImVec2(-1, 30)))
        {
            auto& engine = Application::get().get_engine();
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << "high_res_frame_4k_" << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S") << ".png";
            engine.export_high_res_frame(ss.str(), 4096, 3072);
        }
        
        if (ImGui::Button("Export High-Res Frame (8K)", ImVec2(-1, 30)))
        {
            auto& engine = Application::get().get_engine();
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << "high_res_frame_8k_" << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S") << ".png";
            engine.export_high_res_frame(ss.str(), 8192, 6144);
        }
        
        if (ImGui::Button("Export Ultra High-Res Frame (16K)", ImVec2(-1, 30)))
        {
            auto& engine = Application::get().get_engine();
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << "high_res_frame_16k_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".png";
            engine.export_high_res_frame(ss.str(), 16384, 12288);
        }
        
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Controls");
        ImGui::Separator();
        
        ImGui::Text("Left Mouse: Orbit camera");
        ImGui::Text("Scroll: Zoom in/out");
        ImGui::Text("G: Toggle gravity");
        ImGui::Text("Right Mouse: Enable gravity (hold)");
        
        ImGui::End();
    }
};
