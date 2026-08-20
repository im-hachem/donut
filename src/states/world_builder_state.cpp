#include "world_builder_state.h"

#include "core/application.h"
#include "core/window.h"
#include "core/hdri_manager.h"

#include "rendering/renderer.h"
#include "rendering/shader.h"
#include "rendering/vertex_array.h"
#include "rendering/vertex_buffer.h"
#include "rendering/index_buffer.h"
#include "rendering/texture.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <nlohmann/json.hpp>

#include <numbers>
#include <fstream>
#include <sstream>
#include <limits>
#include <vector>

namespace Donut
{
    auto WorldBuilderState::on_enter() -> void
    {
        DONUT_INFO("Entering World Builder State");
        
        ImGuizmo::Enable(true);

        m_camera.set_camera_mode(CameraMode::Orbital);
        m_camera.set_orbital_target(glm::vec3(0.0f, 0.0f, 0.0f));
        m_camera.set_orbital_radius(15.0f);
        m_camera.set_orbital_limits(2.0f, 200.0f);
        m_camera.set_orbital_speed(0.01f);
        m_camera.set_zoom_speed(2.0f);
        m_camera.set_azimuth(0.0f);
        m_camera.set_elevation(static_cast<float>(std::numbers::pi) / 3.0f);
        m_camera.update_orbital();
        
        m_sphere_shader = Ref<Shader>(Shader::create("assets/shaders/Sphere.glsl"));
        m_skybox_shader = Ref<Shader>(Shader::create("assets/shaders/Skybox.glsl"));
        m_grid_shader   = Ref<Shader>(Shader::create("assets/shaders/Grid.glsl"));
        
        if (!m_sphere_shader)
            DONUT_ERROR("Failed to create sphere shader");
        if (!m_skybox_shader)
            DONUT_ERROR("Failed to create skybox shader");
        if (!m_grid_shader)
            DONUT_ERROR("Failed to create grid shader");
        
        initialize_sphere_geometry();
        initialize_skybox_geometry();
        initialize_grid_geometry();
        
        auto& hdri_manager = HDRIManager::get();
        m_hdri_environment = hdri_manager.get_current_hdri();
        if (!m_hdri_environment)
        {
            hdri_manager.set_current_hdri("assets/hdri/HDR_blue_nebulae-1.hdr");
            m_hdri_environment = hdri_manager.get_current_hdri();
            if (!m_hdri_environment)
                DONUT_WARN("Failed to load default HDRI for WorldBuilder, using fallback");
        }
        
        Material black_hole_material(glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, 0.0f);
        m_black_hole = Object(glm::vec3(0.0f, 0.0f, 0.0f), 2.0f, black_hole_material);
        m_black_hole_initialized = true;
        
        // Set default grid size
        m_grid_size = 10.0f;
        
        m_initialized = true;
    }
    
    auto WorldBuilderState::on_exit() -> void
    {
        DONUT_INFO("Exiting World Builder State");
    }
    
    auto WorldBuilderState::on_update(float delta_time) -> void
    {
        if (m_camera_dragging && 
            !ImGuizmo::IsUsing())
        {
            GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            
            glm::vec2 current_mouse_pos(xpos, ypos);
            glm::vec2 delta = current_mouse_pos - m_last_mouse_pos;
            
            float sensitivity    = 0.005f;
            float azimuth_delta   = delta.x * sensitivity;
            float elevation_delta = -delta.y * sensitivity;
            
            float new_azimuth   = m_camera.get_azimuth() + azimuth_delta;
            float new_elevation = m_camera.get_elevation() + elevation_delta;
            
            new_elevation = glm::clamp(new_elevation, 0.01f, static_cast<float>(std::numbers::pi) - 0.01f);
            
            m_camera.set_azimuth(new_azimuth);
            m_camera.set_elevation(new_elevation);
            m_camera.update_orbital();
            
            m_last_mouse_pos = current_mouse_pos;
        }
    }
    
    auto WorldBuilderState::on_render() -> void
    {
        auto& hdri_manager = HDRIManager::get();
        m_hdri_environment = hdri_manager.get_current_hdri();
        
        RenderCommand::set_clear_color(glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
        RenderCommand::clear();
        
        if (m_hdri_environment)
            render_skybox();
        
        if (m_show_grid)
            render_grid();
        
        render_scene();
    }
    
    auto WorldBuilderState::on_event(Event& event) -> void
    {
        EventDispatcher dispatcher(event);
        
        dispatcher.dispatch<MouseButtonPressedEvent>([this](MouseButtonPressedEvent& e) 
        {
            if (e.get_mouse_button() == GLFW_MOUSE_BUTTON_LEFT)
            {
                if (ImGuizmo::IsUsing() || 
                    ImGuizmo::IsOver())
                    return false;
                
                GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                
                float ndc_x = (2.0f * static_cast<float>(xpos)) / static_cast<float>(width) - 1.0f;
                float ndc_y = 1.0f - (2.0f * static_cast<float>(ypos)) / static_cast<float>(height);
                
                glm::vec4 rayStart_NDC(ndc_x, ndc_y, -1.0f, 1.0f);
                glm::vec4 rayEnd_NDC(ndc_x, ndc_y, 0.0f, 1.0f);
                
                glm::mat4 inv_vp          = glm::inverse(m_camera.get_projection_matrix() * m_camera.get_view_matrix());
                glm::vec4 rayStart_World = inv_vp * rayStart_NDC;
                glm::vec4 rayEnd_World   = inv_vp * rayEnd_NDC;
                
                rayStart_World /= rayStart_World.w;
                rayEnd_World   /= rayEnd_World.w;
                
                glm::vec3 ray_dir    = glm::normalize(glm::vec3(rayEnd_World - rayStart_World));
                glm::vec3 ray_origin = glm::vec3(rayStart_World);
                
                float closest_distance  = std::numeric_limits<float>::max();
                int closest_object_index = -1;
                
                if (m_black_hole_initialized)
                {
                    glm::vec3 oc       = ray_origin - m_black_hole.m_centre;
                    float a            = glm::dot(ray_dir, ray_dir);
                    float b            = 2.0f * glm::dot(oc, ray_dir);
                    float c            = glm::dot(oc, oc) - m_black_hole.m_radius * m_black_hole.m_radius;
                    float discriminant = b * b - 4 * a * c;
                    
                    if (discriminant > 0)
                    {
                        float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
                        float t2 = (-b + sqrt(discriminant)) / (2.0f * a);
                        
                        if (t1 > 0 && t1 < closest_distance)
                        {
                            closest_distance = t1;
                            closest_object_index = -2;
                        }
                        else if (t2 > 0 && t2 < closest_distance)
                        {
                            closest_distance = t2;
                            closest_object_index = -2;
                        }
                    }
                }
                
                for (size_t i = 0; i < m_scene.objs.size(); ++i)
                {
                    const Object& obj = m_scene.objs[i];
                    
                    glm::vec3 oc       = ray_origin - obj.m_centre;
                    float a            = glm::dot(ray_dir, ray_dir);
                    float b            = 2.0f * glm::dot(oc, ray_dir);
                    float c            = glm::dot(oc, oc) - obj.m_radius * obj.m_radius;
                    float discriminant = b * b - 4 * a * c;
                    
                    if (discriminant > 0)
                    {
                        float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
                        float t2 = (-b + sqrt(discriminant)) / (2.0f * a);
                        
                        if (t1 > 0 && t1 < closest_distance)
                        {
                            closest_distance = t1;
                            closest_object_index = static_cast<int>(i);
                        }
                        else if (t2 > 0 && t2 < closest_distance)
                        {
                            closest_distance = t2;
                            closest_object_index = static_cast<int>(i);
                        }
                    }
                }
                
                if (closest_object_index >= 0)
                {
                    m_selected_object_index = closest_object_index;
                    DONUT_INFO("Selected object {}", closest_object_index);
                    return true;
                }
                else if (closest_object_index == -2)
                {
                    m_selected_object_index = -1;
                    DONUT_INFO("Black hole clicked (not selectable)");
                    return true;
                }
                else
                {
                    m_selected_object_index = -1;
                    m_camera_dragging      = true;
                    m_last_mouse_pos        = glm::vec2(xpos, ypos);
                }
                
                return true;
            }
            return false;
        });
        
        dispatcher.dispatch<MouseButtonReleasedEvent>([this](MouseButtonReleasedEvent& e) 
        {
            if (e.get_mouse_button() == GLFW_MOUSE_BUTTON_LEFT)
            {
                if (!ImGuizmo::IsUsing())
                    m_camera_dragging = false;
                return true;
            }

            return false;
        });
        
        dispatcher.dispatch<WindowResizeEvent>([this](WindowResizeEvent& e)
        {
            int new_width = e.get_width();
            int new_height = e.get_height();
            if (new_width > 0 && new_height > 0)
            {
                float aspect = static_cast<float>(new_width) / static_cast<float>(new_height);
                m_camera.set_projection(45.0f, aspect, 0.1f, 1000.0f);
            }
            return false;
        });
        
        dispatcher.dispatch<MouseScrolledEvent>([this](MouseScrolledEvent& e) 
        {
            float zoom_speed = 0.1f;
            float zoom_delta = e.get_y_offset() * zoom_speed;
            
            double current_radius = m_camera.get_orbital_radius();
            double new_radius = current_radius - zoom_delta * current_radius * 0.1f;
            
            double min_radius = 2.0f;
            double max_radius = 200.0f;
            new_radius        = glm::clamp(new_radius, min_radius, max_radius);
            
            m_camera.set_orbital_radius(new_radius);
            m_camera.update_orbital();

            return true;
        });
        
        dispatcher.dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) 
        {
            if (m_selected_object_index >= 0 && 
                m_selected_object_index < static_cast<int>(m_scene.objs.size()))
            {
                switch (e.get_key_code())
                {
                    case GLFW_KEY_M:
                        m_gizmo_operation = ImGuizmo::TRANSLATE;
                        return true;
                    case GLFW_KEY_R:
                        m_gizmo_operation = ImGuizmo::ROTATE;
                        return true;
                    case GLFW_KEY_S:
                        m_gizmo_operation = ImGuizmo::SCALE;
                        return true;
                }
            }
            return false;
        });
    }
    
    auto WorldBuilderState::on_im_ui_render() -> void
    {
        GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
     
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGuizmo::SetRect(vp->Pos.x, vp->Pos.y, vp->Size.x, vp->Size.y);
        
        ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        
        ImGui::Begin("World Builder", nullptr, ImGuiWindowFlags_NoCollapse);
        
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "World Builder");
        ImGui::PopFont();
        ImGui::Separator();
        
                 if (ImGui::CollapsingHeader("Scene Info"))
         {
            ImGui::Text("Objects in scene: %zu/16 (+ 1 black hole)", m_scene.objs.size());
            
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Camera Info:");
            
            glm::vec3 camera_pos = m_camera.get_orbital_position();
            glm::vec3 target = m_camera.get_orbital_target();
            
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", camera_pos.x, camera_pos.y, camera_pos.z);
            ImGui::Text("Target: (%.2f, %.2f, %.2f)", target.x, target.y, target.z);
            ImGui::Text("Distance: %.2f", m_camera.get_orbital_radius());
            ImGui::Text("Azimuth: %.1f°", glm::degrees(m_camera.get_azimuth()));
            ImGui::Text("Elevation: %.1f°", glm::degrees(m_camera.get_elevation()));
            
            ImGui::Separator();
            
            if (ImGui::Button("Reset Camera"))
            {
                m_camera.set_orbital_radius(15.0f);
                m_camera.set_azimuth(0.0f);
                m_camera.set_elevation(static_cast<float>(std::numbers::pi) / 3.0f);
                m_camera.set_orbital_target(glm::vec3(0.0f, 0.0f, 0.0f));
                m_camera.update_orbital();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Clear Scene"))
                clear_scene();
            
            ImGui::SameLine();
            if (ImGui::Button("Focus on Objects"))
            {
                if (!m_scene.objs.empty())
                {
                    glm::vec3 center(0.0f);
                    for (const auto& obj : m_scene.objs)
                        center += obj.m_centre;
                    center /= static_cast<float>(m_scene.objs.size());
                    m_camera.set_orbital_target(center);
                    m_camera.update_orbital();
                }
            }
        }
        
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("create Object"))
        {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "New Sphere");
            ImGui::Separator();
            
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Objects: %zu/16", m_scene.objs.size());
            
            ImGui::Text("Position:");
            ImGui::DragFloat3("##Position", &m_new_object_position.x, 0.1f);
            
            ImGui::Text("Radius:");
            ImGui::DragFloat("##Radius", &m_new_object_radius, 0.1f, 0.1f, 10.0f);
            
            ImGui::Text("Color:");
            ImGui::ColorEdit3("##Color", &m_new_object_color.x);
            
            ImGui::Text("Specular:");
            ImGui::SliderFloat("##Specular", &m_new_object_specular, 0.0f, 1.0f);
            
            ImGui::Text("Emission:");
            ImGui::SliderFloat("##Emission", &m_new_object_emission, 0.0f, 1.0f);
            
            ImGui::Spacing();
            
            if (m_scene.objs.size() >= 16)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
                ImGui::Button("add Sphere (Limit Reached)", ImVec2(ImGui::GetWindowWidth() - 20, 30));
                ImGui::PopStyleVar();
            }
            else
            {
                if (ImGui::Button("add Sphere", ImVec2(ImGui::GetWindowWidth() - 20, 30)))
                    add_sphere();
            }
        }
        
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Scene Objects"))
        {
            if (m_black_hole_initialized)
            {
                ImGui::PushID(-1);
                
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Black Hole (Center)");
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Position: (0.00, 0.00, 0.00)");
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Radius: %.2f", m_black_hole.m_radius);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Color: Black (with white outline)");
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Cannot be removed or modified");
                
                ImGui::PopID();
                ImGui::Separator();
            }
            
            if (m_scene.objs.empty())
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No additional objects in scene");
            else
            {
                for (size_t i = 0; i < m_scene.objs.size(); ++i)
                {
                    Object& obj = m_scene.objs[i];
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    bool is_selected = (m_selected_object_index == static_cast<int>(i));
                    if (ImGui::Selectable(("Sphere " + std::to_string(i)).c_str(), is_selected))
                        m_selected_object_index = static_cast<int>(i);
                    
                    if (is_selected)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Remove"))
                        {
                            remove_selected_object();
                        }
                        
                        ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
                                   obj.m_centre.x, obj.m_centre.y, obj.m_centre.z);
                        ImGui::Text("Radius: %.2f", obj.m_radius);
                        ImGui::Text("Color: (%.2f, %.2f, %.2f)", 
                                   obj.m_material.m_color.x, obj.m_material.m_color.y, obj.m_material.m_color.z);
                    }
                    
                    ImGui::PopID();
                }
            }
        }

        ImGui::Spacing();
         
        if (ImGui::CollapsingHeader("Gizmo Controls"))
        {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Gizmo Operation:");
            
            if (ImGui::RadioButton("Translate", m_gizmo_operation == ImGuizmo::TRANSLATE))
                m_gizmo_operation = ImGuizmo::TRANSLATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate", m_gizmo_operation == ImGuizmo::ROTATE))
                m_gizmo_operation = ImGuizmo::ROTATE;
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale", m_gizmo_operation == ImGuizmo::SCALE))
                m_gizmo_operation = ImGuizmo::SCALE;
            
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Hotkeys:");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "M = Move, R = Rotate, S = Scale");
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Selection Outline"))
        {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Outline Settings:");

            ImGui::Text("Outline Color:");
            ImGui::ColorEdit3("##OutlineColor", &m_outline_color.x);

            ImGui::Text("Outline Width:");
            ImGui::SliderFloat("##OutlineWidth", &m_outline_width, 0.01f, 0.9f, "%.2f");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "White rim around sphere silhouette");
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Grid Settings"))
        {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "Grid Settings:");

            ImGui::Text("Show Grid:");
            ImGui::SameLine();
            ImGui::Checkbox("##ShowGrid", &m_show_grid);

            ImGui::Text("Grid Color:");
            ImGui::ColorEdit3("##GridColor", &m_grid_color.x);

            ImGui::Text("Grid Alpha:");
            ImGui::SliderFloat("##GridAlpha", &m_grid_alpha, 0.0f, 1.0f, "%.2f");

            ImGui::Text("Grid Size:");
            ImGui::SliderFloat("##GridSize", &m_grid_size, 1.0f, 500.0f, "%.1f");

            ImGui::Spacing();
            if (ImGui::Button("Regenerate Grid", ImVec2(ImGui::GetWindowWidth() - 20, 25)))
                initialize_grid_geometry();

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Reference grid for spatial orientation");
        }

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("HDRI Environment", nullptr))
        {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "HDRI Settings:");

            static int selected_hdri = 0;
            ImGui::PushID("WorldBuilderHDRI");
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
                m_hdri_environment = hdri_manager.get_current_hdri();
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "HDRI provides background skybox and lighting for the scene");
            ImGui::PopID();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float button_width = (ImGui::GetWindowWidth() - 30) / 2.0f;
        
        if (ImGui::Button("Save Scene", ImVec2(button_width, 30)))
            save_scene();
        
        ImGui::SameLine();
        if (ImGui::Button("load Scene", ImVec2(button_width, 30)))
            load_scene();
        
        ImGui::Spacing();
        
        if (ImGui::Button("Start Simulation", ImVec2(ImGui::GetWindowWidth() - 20, 30)))
        {
            auto& engine = Application::get().get_engine();
            engine.load_objects_from_scene(m_scene.objs);
            Application::get().get_state_manager().switch_to_state("Simulation");
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("Back to Config", ImVec2(ImGui::GetWindowWidth() - 20, 30)))
            Application::get().get_state_manager().switch_to_state("Config");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Camera: Left mouse = rotate, Scroll = zoom");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Gizmo: M=Move, R=Rotate, S=Scale");
        
        ImGui::End();
        
        if (m_selected_object_index >= 0 && 
            m_selected_object_index < static_cast<int>(m_scene.objs.size()) &&
            m_selected_object_index != -2)
        {
            glm::mat4 view       = m_camera.get_view_matrix();
            glm::mat4 projection = m_camera.get_projection_matrix();
            
            Object& selected_obj = m_scene.objs[m_selected_object_index];
            
            glm::mat4 object_transform = glm::mat4(1.0f);
                      object_transform = glm::translate(object_transform, selected_obj.m_centre);
                      object_transform = glm::scale(object_transform, glm::vec3(selected_obj.m_radius));
            
            DONUT_INFO("Object position: ({}, {}, {})", selected_obj.m_centre.x, selected_obj.m_centre.y, selected_obj.m_centre.z);
            glm::vec3 camera_pos = m_camera.get_orbital_position();
            DONUT_INFO("Camera position: ({}, {}, {})", camera_pos.x, camera_pos.y, camera_pos.z);
            
            glm::vec3 view_translation = glm::vec3(view[3][0], view[3][1], view[3][2]);
            DONUT_INFO("View translation: ({}, {}, {})", view_translation.x, view_translation.y, view_translation.z);
            
            ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
            if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), 
                                   m_gizmo_operation, m_gizmo_mode, 
                                   glm::value_ptr(object_transform), 
                                   nullptr, nullptr))
            {
                glm::vec3 new_position = glm::vec3(object_transform[3][0], object_transform[3][1], object_transform[3][2]);
                selected_obj.m_centre = new_position;
                
                glm::vec3 scale = glm::vec3
                (
                    glm::length(glm::vec3(object_transform[0])),
                    glm::length(glm::vec3(object_transform[1])),
                    glm::length(glm::vec3(object_transform[2]))
                );
                selected_obj.m_radius = (scale.x + scale.y + scale.z) / 3.0f;
                
                DONUT_INFO("Matrix [3]: ({}, {}, {}, {})", object_transform[3][0], object_transform[3][1], object_transform[3][2], object_transform[3][3]);
                DONUT_INFO("New position: ({}, {}, {})", new_position.x, new_position.y, new_position.z);
            }
        }
    }
    
    auto WorldBuilderState::add_sphere() -> void
    {
        if (m_scene.objs.size() >= 16)
        {
            DONUT_WARN("Cannot add more objects. Maximum of 16 objects reached.");
            return;
        }
        
        Material material(m_new_object_color, m_new_object_specular, m_new_object_emission);
        Object   sphere(m_new_object_position, m_new_object_radius, material);
        m_scene.objs.push_back(sphere);
        
        m_selected_object_index = static_cast<int>(m_scene.objs.size() - 1);
        
        m_new_object_position = glm::vec3(0.0f, 0.0f, 0.0f);
        m_new_object_radius   = 1.0f;
        m_new_object_color    = glm::vec3(1.0f, 1.0f, 1.0f);
        m_new_object_specular = 0.5f;
        m_new_object_emission = 0.0f;
        
        DONUT_INFO("Added sphere to scene and selected it");
    }
    
    auto WorldBuilderState::remove_selected_object() -> void
    {
        if (m_selected_object_index >= 0 && m_selected_object_index < static_cast<int>(m_scene.objs.size()))
        {
            m_scene.objs.erase(m_scene.objs.begin() + m_selected_object_index);
            m_selected_object_index = -1;
            DONUT_INFO("Removed object from scene");
        }
    }
    
    auto WorldBuilderState::clear_scene() -> void
    {
        m_scene.objs.clear();
        m_selected_object_index = -1;
        DONUT_INFO("Cleared scene (black hole remains at center)");
    }
    
    auto WorldBuilderState::save_scene() -> void
    {
        nlohmann::json scene_data;
        scene_data["objects"] = nlohmann::json::array();

        for (const auto& obj : m_scene.objs)
        {
            nlohmann::json sphere_data;
            sphere_data["position"] = 
            { 
                obj.m_centre.x, 
                obj.m_centre.y, 
                obj.m_centre.z 
            };

            sphere_data["color"] = 
            {
                obj.m_material.m_color.x, 
                obj.m_material.m_color.y, 
                obj.m_material.m_color.z
            };

            sphere_data["radius"]  = obj.m_radius;
            sphere_data["specular"] = obj.m_material.m_specular;
            sphere_data["emission"] = obj.m_material.m_emission;
            scene_data["objects"].push_back(sphere_data);
        }

        std::ofstream file("Scene.json");
        if (file.is_open())
        {
            file << scene_data.dump(4);
            file.close();
            DONUT_INFO("Scene saved to Scene.json (black hole always present at center)");
        }
        else
            DONUT_ERROR("Failed to save scene");
    }
    
    auto WorldBuilderState::load_scene() -> void
    {
        std::ifstream file("Scene.json");
        if (file.is_open())
        {
            m_scene.objs.clear();
            m_selected_object_index = -1;
            
            nlohmann::json scene_data;
            file >> scene_data;

            if (scene_data.contains("objects"))
            {
                for (const auto& sphere_data : scene_data["objects"])
                {
                    glm::vec3 position;
                    glm::vec3 color;
                    float radius;
                    float specular;
                    float emission;
                    
                    auto pos_array = sphere_data["position"];
                    position.x = pos_array[0].get<float>();
                    position.y = pos_array[1].get<float>();
                    position.z = pos_array[2].get<float>();
                    
                    auto color_array = sphere_data["color"];
                    color.x = color_array[0].get<float>();
                    color.y = color_array[1].get<float>();
                    color.z = color_array[2].get<float>();
                    
                    radius   = sphere_data["radius"].get<float>();
                    specular = sphere_data["specular"].get<float>();
                    emission = sphere_data["emission"].get<float>();
                    
                    Material material(color, specular, emission);
                    Object   sphere(position, radius, material);
                    m_scene.objs.push_back(sphere);
                }
            }
            
            file.close();
            DONUT_INFO("Scene loaded from Scene.json (black hole remains at center)");
        }
        else
            DONUT_ERROR("Failed to load scene");
    }
    
    auto WorldBuilderState::initialize_sphere_geometry() -> void
    {
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        
        const int segments = 32;
        const int rings = 16;
        
        for (int ring = 0; ring <= rings; ++ring)
        {
            float phi = static_cast<float>(std::numbers::pi) * ring / rings;
            float sin_phi = sin(phi);
            float cos_phi = cos(phi);
            
            for (int segment = 0; segment <= segments; ++segment)
            {
                float theta = 2.0f * static_cast<float>(std::numbers::pi) * segment / segments;
                float sin_theta = sin(theta);
                float cos_theta = cos(theta);
                
                float x = cos_theta * sin_phi;
                float y = cos_phi;
                float z = sin_theta * sin_phi;
                
                float nx = x;
                float ny = y;
                float nz = z;
                
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
                vertices.push_back(nx);
                vertices.push_back(ny);
                vertices.push_back(nz);
            }
        }
        
        for (int ring = 0; ring < rings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                uint32_t first = ring * (segments + 1) + segment;
                uint32_t second = first + segments + 1;
                
                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);
                
                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }
        
        auto vertex_buffer = Ref<VertexBuffer>(VertexBuffer::create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float))));
        VertexBufferLayout layout;
        layout.push<float>(3);
        layout.push<float>(3);
        vertex_buffer->set_layout(layout);
        
        auto index_buffer = Ref<IndexBuffer>(IndexBuffer::create(indices.data(), static_cast<uint32_t>(indices.size())));
        
        m_sphere_vao = Ref<VertexArray>(VertexArray::create());
        m_sphere_vao->add_vertex_buffer(vertex_buffer);
        m_sphere_vao->set_index_buffer(index_buffer);
    }
    
    auto WorldBuilderState::render_scene() -> void
    {
        GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        RenderCommand::set_viewport(0, 0, width, height);
        RenderCommand::enable_depth_test();
        
        glm::mat4 view           = m_camera.get_view_matrix();
        glm::mat4 projection     = m_camera.get_projection_matrix();
        glm::mat4 view_projection = projection * view;
        
        glm::vec3 light_pos  = m_scene.m_light_pos;
        glm::vec3 camera_pos = m_camera.get_orbital_position();
        
        m_sphere_shader->bind();
        m_sphere_shader->set_mat4("u_ViewProjection", view_projection);
        m_sphere_shader->set_float3("u_LightPos",  light_pos);
        m_sphere_shader->set_float3("u_CameraPos", camera_pos);

        m_sphere_shader->set_float3("u_OutlineColor", m_outline_color);
        m_sphere_shader->set_float("u_OutlineWidth",  m_outline_width);
        
        if (m_hdri_environment)
        {
            m_hdri_environment->bind(1);
            m_sphere_shader->set_int("u_HDRIEnvironment", 1);
        }
        
        if (m_black_hole_initialized)
        {
            glm::mat4 black_hole_transform = glm::translate(glm::mat4(1.0f), m_black_hole.m_centre);
                      black_hole_transform = glm::scale(black_hole_transform, glm::vec3(m_black_hole.m_radius));
            
            m_sphere_shader->set_mat4("u_Transform", black_hole_transform);
            m_sphere_shader->set_int("u_IsSelected", 1);
            m_sphere_shader->set_float3("u_Color", m_black_hole.m_material.m_color);
            m_sphere_shader->set_float("u_Emission", m_black_hole.m_material.m_emission);
            m_sphere_shader->set_float("u_Specular", m_black_hole.m_material.m_specular);
            m_sphere_shader->set_float("u_OutlineWidth", 0.3f);
            
            m_sphere_vao->bind();
            RenderCommand::draw_indexed(m_sphere_vao);
            
            m_sphere_shader->set_float("u_OutlineWidth", m_outline_width);
        }
        
        for (size_t i = 0; i < m_scene.objs.size(); ++i)
        {
            const auto& obj = m_scene.objs[i];
            
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), obj.m_centre);
                      transform = glm::scale(transform, glm::vec3(obj.m_radius));
            
            m_sphere_shader->set_mat4("u_Transform", transform);
            
            bool is_selected = (m_selected_object_index == static_cast<int>(i));
            m_sphere_shader->set_int("u_IsSelected", is_selected ? 1 : 0);
            if (is_selected)
            {
                glm::vec3 highlight_color = obj.m_material.m_color * 1.5f;
                highlight_color = glm::clamp(highlight_color, 0.0f, 1.0f);
                m_sphere_shader->set_float3("u_Color", highlight_color);
                m_sphere_shader->set_float("u_Emission", 0.2f);
            }
            else
            {
                m_sphere_shader->set_float3("u_Color", obj.m_material.m_color);
                m_sphere_shader->set_float("u_Emission", obj.m_material.m_emission);
            }
            
            m_sphere_shader->set_float("u_Specular", obj.m_material.m_specular);
            
            m_sphere_vao->bind();
            RenderCommand::draw_indexed(m_sphere_vao);
        }
        
        RenderCommand::disable_depth_test();
    }
    

    
    auto WorldBuilderState::initialize_skybox_geometry() -> void
    {
        float skybox_vertices[] = 
        {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        auto vertex_buffer = Ref<VertexBuffer>(VertexBuffer::create(skybox_vertices, sizeof(skybox_vertices)));
        VertexBufferLayout layout;
        layout.push<float>(3);
        vertex_buffer->set_layout(layout);

        m_skybox_vao = Ref<VertexArray>(VertexArray::create());
        m_skybox_vao->add_vertex_buffer(vertex_buffer);
    }
    
    auto WorldBuilderState::render_skybox() -> void
    {
        if (!m_skybox_shader || !m_skybox_vao || !m_hdri_environment)
            return;
        
        GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        RenderCommand::set_viewport(0, 0, width, height);
        RenderCommand::disable_depth_test();
        
        glm::mat4 view = m_camera.get_view_matrix();
        glm::mat4 projection = m_camera.get_projection_matrix();
        
        view = glm::mat4(glm::mat3(view));
        
        m_skybox_shader->bind();
        m_skybox_shader->set_mat4("u_View", view);
        m_skybox_shader->set_mat4("u_Projection", projection);
        
        m_hdri_environment->bind(0);
        m_skybox_shader->set_int("u_Skybox", 0);
        
        m_skybox_vao->bind();
        RenderCommand::draw_arrays(36);
        
        RenderCommand::enable_depth_test();
    }
    
    auto WorldBuilderState::initialize_grid_geometry() -> void
    {
        if (m_grid_vao)
        {
            m_grid_vao->unbind();
            m_grid_vao.reset();
        }
        
        const float grid_size = 50.0f;
        const int grid_lines = 101;
        const float half_size = grid_size * 0.5f;
        const float step = grid_size / (grid_lines - 1);
        
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        
        for (int i = 0; i < grid_lines; ++i)
        {
            float z = -half_size + i * step;
            
            vertices.push_back(-half_size);
            vertices.push_back(0.0f);
            vertices.push_back(z);
            
            vertices.push_back(half_size);
            vertices.push_back(0.0f);
            vertices.push_back(z);
            
            uint32_t base_index = static_cast<uint32_t>(vertices.size() / 3) - 2;
            indices.push_back(base_index);
            indices.push_back(base_index + 1);
        }
        
        for (int i = 0; i < grid_lines; ++i)
        {
            float x = -half_size + i * step;
            
            vertices.push_back(x);
            vertices.push_back(0.0f);
            vertices.push_back(-half_size);
            
            vertices.push_back(x);
            vertices.push_back(0.0f);
            vertices.push_back(half_size);
            
            uint32_t base_index = static_cast<uint32_t>(vertices.size() / 3) - 2;
            indices.push_back(base_index);
            indices.push_back(base_index + 1);
        }
        
        auto vertex_buffer = Ref<VertexBuffer>(VertexBuffer::create(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(float))));
        VertexBufferLayout layout;
        layout.push<float>(3);
        vertex_buffer->set_layout(layout);
        
        auto index_buffer = Ref<IndexBuffer>(IndexBuffer::create(indices.data(), static_cast<uint32_t>(indices.size())));
        
        m_grid_vao = Ref<VertexArray>(VertexArray::create());
        m_grid_vao->add_vertex_buffer(vertex_buffer);
        m_grid_vao->set_index_buffer(index_buffer);
        
        m_grid_vao->unbind();
    }
    
    auto WorldBuilderState::render_grid() -> void
    {
        if (!m_grid_shader || !m_grid_vao)
            return;
        
        GLFWwindow* window = static_cast<GLFWwindow*>(Application::get().get_window().get_native_window());
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        RenderCommand::set_viewport(0, 0, width, height);
        RenderCommand::enable_depth_test();
        RenderCommand::enable_blending();
        
        glm::mat4 view = m_camera.get_view_matrix();
        glm::mat4 projection = m_camera.get_projection_matrix();
        glm::mat4 view_projection = projection * view;
        glm::mat4 transform = glm::mat4(1.0f);
        
        glm::vec3 camera_pos = m_camera.get_orbital_position();
        
        m_grid_shader->bind();
        m_grid_shader->set_mat4("u_ViewProjection", view_projection);
        m_grid_shader->set_mat4("u_Transform", transform);
        m_grid_shader->set_float3("u_GridColor", m_grid_color);
        m_grid_shader->set_float("u_GridAlpha", m_grid_alpha);
        m_grid_shader->set_float("u_GridSize", m_grid_size);
        m_grid_shader->set_float3("u_CameraPos", camera_pos);
        
        m_grid_vao->bind();
        RenderCommand::draw_lines(m_grid_vao);
        
        // Clean up state to prevent conflicts with scene rendering
        m_grid_vao->unbind();
        m_grid_shader->unbind();
        RenderCommand::disable_blending();
        
        // Ensure depth test is still enabled for scene rendering
        RenderCommand::enable_depth_test();
    }
};
