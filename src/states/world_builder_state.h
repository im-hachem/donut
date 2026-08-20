#pragma once

#include "core/camera.h"
#include "core/state.h"
#include "core/event.h"
#include "core/log.h"

#include "engine/scene.h"
#include "engine/object.h"

#include "rendering/renderer.h"
#include "rendering/shader.h"
#include "rendering/vertex_array.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <vector>
#include <memory>

namespace Donut
{
    class WorldBuilderState
        : public State
    {
    public:
        ~WorldBuilderState() = default;
        
        auto on_enter() -> void override;
        auto on_exit() -> void override;
        auto on_update(float delta_time) -> void override;
        auto on_render() -> void override;
        auto on_im_ui_render() -> void override;
        auto on_event(Event& event) -> void override;
    private:
        auto add_sphere() -> void;
        auto remove_selected_object() -> void;
        auto clear_scene() -> void;
        auto save_scene() -> void;
        auto load_scene() -> void;
        auto render_scene() -> void;
        auto initialize_sphere_geometry() -> void;
        auto initialize_grid_geometry() -> void;
        auto render_grid() -> void;
    private:
        Scene  m_scene;
        Camera m_camera;
        bool   m_initialized = false;
        
        Ref<Shader>      m_sphere_shader;
        Ref<VertexArray> m_sphere_vao;
        Ref<CubemapTexture> m_hdri_environment;
        
        Ref<Shader>      m_skybox_shader;
        Ref<VertexArray> m_skybox_vao;
        
        Ref<Shader>      m_grid_shader;
        Ref<VertexArray> m_grid_vao;
        
        glm::vec3 m_new_object_position = glm::vec3(0.0f, 0.0f, 0.0f);
        float     m_new_object_radius   = 1.0f;
        glm::vec3 m_new_object_color    = glm::vec3(1.0f, 1.0f, 1.0f);
        float     m_new_object_specular = 0.5f;
        float     m_new_object_emission = 0.0f;
        
        int       m_selected_object_index = -1;
        bool      m_camera_dragging      = false;
        glm::vec2 m_last_mouse_pos        = glm::vec2(0.0f, 0.0f);
        
        ImGuizmo::OPERATION m_gizmo_operation = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE      m_gizmo_mode      = ImGuizmo::LOCAL;
        
        bool m_show_object_list      = true;
        bool m_show_object_creator   = true;
        bool m_show_scene_info       = true;
        bool m_show_gizmo_controls   = true;
        bool m_show_outline_controls = true;
        bool m_show_grid            = true;

        glm::vec3 m_outline_color = glm::vec3(1.0f, 1.0f, 1.0f);
        float     m_outline_width = 0.25f;
        
        glm::vec3 m_grid_color = glm::vec3(0.5f, 0.5f, 0.5f);
        float     m_grid_alpha = 0.5f;
        float     m_grid_size  = 50.0f;
        
        Object m_black_hole;
        bool   m_black_hole_initialized = false;
        
        auto set_hdri_environment(Ref<CubemapTexture> hdri) -> void { m_hdri_environment = hdri; }
        auto get_hdri_environment() const -> Ref<CubemapTexture> { return m_hdri_environment; }
        
        auto initialize_skybox_geometry() -> void;
        auto render_skybox() -> void;
    };
};
