#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <numbers>

namespace Donut
{
    enum class CameraMode
    {
        FPS,
        Orbital
    };

    class Camera
    {
    public:
        Camera(float fov = 45.0f, float aspect_ratio = 16.0f / 9.0f,
               float near_plane = 0.1f, float far_plane = 100.0f);
        ~Camera() = default;

        auto set_position(const glm::vec3& position) -> void { m_position = position; recalculate_view_matrix(); }
        auto set_rotation(const glm::vec3& rotation) -> void { m_rotation = rotation; recalculate_view_matrix(); }

        auto get_position() const -> const glm::vec3& { return m_position; }
        auto get_rotation() const -> const glm::vec3& { return m_rotation; }

        auto get_forward_direction() const -> glm::vec3;
        auto get_right_direction()   const -> glm::vec3;
        auto get_up_direction()      const -> glm::vec3;

        auto get_projection_matrix()      const -> const glm::mat4& { return m_projection_matrix; }
        auto get_view_matrix()            const -> const glm::mat4& { return m_view_matrix; }
        auto get_view_projection_matrix() const -> const glm::mat4& { return m_view_projection_matrix; }

        auto set_projection(float fov, float aspect_ratio, float near_plane, float far_plane) -> void;
        auto on_mouse_move(float x_offset, float y_offset, bool constrain_pitch = true) -> void;

        auto move_forward(float delta_time) -> void;
        auto move_backward(float delta_time) -> void;
        auto move_right(float delta_time) -> void;
        auto move_left(float delta_time) -> void;
        auto move_up(float delta_time) -> void;
        auto move_down(float delta_time) -> void;

        auto set_mouse_sensitivity(float sensitivity) -> void { m_mouse_sensitivity = sensitivity; }
        auto get_mouse_sensitivity() const -> float { return m_mouse_sensitivity; }

        auto set_movement_speed(float speed) -> void { m_movement_speed = speed; }
        auto get_movement_speed() const -> float { return m_movement_speed; }

        auto set_orbital_target(const glm::vec3& target) -> void { m_orbital_target = target; }
        auto get_orbital_target() const -> const glm::vec3& { return m_orbital_target; }

        auto set_orbital_radius(double radius) -> void { m_orbital_radius = radius; }
        auto get_orbital_radius() const -> double { return m_orbital_radius; }

        auto set_orbital_limits(double min_radius, double max_radius) -> void
        {
            m_orbital_min_radius = min_radius;
            m_orbital_max_radius = max_radius;
        }

        auto set_orbital_speed(float speed) -> void { m_orbital_speed = speed; }
        auto get_orbital_speed() const -> float { return m_orbital_speed; }

        auto set_zoom_speed(double speed) -> void { m_zoom_speed = speed; }
        auto get_zoom_speed() const -> double { return m_zoom_speed; }

        auto set_azimuth(float azimuth) -> void { m_azimuth = azimuth; }
        auto get_azimuth() const -> float { return m_azimuth; }

        auto set_elevation(float elevation) -> void { m_elevation = elevation; }
        auto get_elevation() const -> float { return m_elevation; }

        auto get_orbital_position() const -> glm::vec3;
        auto update_orbital() -> void;
        auto process_orbital_mouse_move(double x, double y) -> void;
        auto process_orbital_mouse_button(int button, int action, int mods) -> void;
        auto process_orbital_scroll(double x_offset, double y_offset) -> void;

        auto set_camera_mode(CameraMode mode) -> void { m_camera_mode = mode; }
        auto get_camera_mode() const -> CameraMode { return m_camera_mode; }

        auto is_dragging() const -> bool { return m_dragging; }
        auto is_panning()  const -> bool { return m_panning; }
        auto is_moving()   const -> bool { return m_moving; }
        auto get_last_x()  const -> double { return m_last_x; }
        auto get_last_y()  const -> double { return m_last_y; }

    private:
        auto recalculate_view_matrix() -> void;
        auto recalculate_projection_matrix() -> void;

    private:
        CameraMode m_camera_mode = CameraMode::FPS;

        glm::mat4 m_projection_matrix;
        glm::mat4 m_view_matrix;
        glm::mat4 m_view_projection_matrix;

        glm::vec3 m_position = { 0.0f, 0.0f, 3.0f };
        glm::vec3 m_rotation = { 0.0f, 0.0f, 0.0f };

        float m_fov          = 45.0f;
        float m_aspect_ratio = 16.0f / 9.0f;
        float m_near_plane   = 0.1f;
        float m_far_plane    = 100.0f;

        float m_mouse_sensitivity = 0.1f;
        float m_movement_speed    = 5.0f;
        bool  m_first_mouse       = true;
        float m_last_x            = 0.0f;
        float m_last_y            = 0.0f;

        glm::vec3 m_orbital_target     = glm::vec3(0.0f, 0.0f, 0.0f);
        double    m_orbital_radius     = 6.34194e10;
        double    m_orbital_min_radius = 1e10;
        double    m_orbital_max_radius = 1e12;
        float     m_azimuth            = 0.0f;
        float     m_elevation          = static_cast<float>(std::numbers::pi) / 2.0f;
        float     m_orbital_speed      = 0.01f;
        double    m_zoom_speed         = 25e9f;
        bool      m_dragging           = false;
        bool      m_panning            = false;
        bool      m_moving             = false;
        double    m_last_x_orbital     = 0.0;
        double    m_last_y_orbital     = 0.0;
    };
}
