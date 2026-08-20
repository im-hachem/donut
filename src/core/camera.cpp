#include "camera.h"

#include <glm/gtc/quaternion.hpp>
#include <GLFW/glfw3.h>

namespace Donut
{
    Camera::Camera(float fov, float aspect_ratio, float near_plane, float far_plane)
        : m_fov(fov), m_aspect_ratio(aspect_ratio), m_near_plane(near_plane), m_far_plane(far_plane)
    {
        recalculate_projection_matrix();
        recalculate_view_matrix();
    }

    auto Camera::set_projection(float fov, float aspect_ratio, float near_plane, float far_plane) -> void
    {
        m_fov = fov;
        m_aspect_ratio = aspect_ratio;
        m_near_plane = near_plane;
        m_far_plane = far_plane;
        recalculate_projection_matrix();
    }

    auto Camera::recalculate_projection_matrix() -> void
    {
        m_projection_matrix = glm::perspective(glm::radians(m_fov), m_aspect_ratio, m_near_plane, m_far_plane);
        m_view_projection_matrix = m_projection_matrix * m_view_matrix;
    }

    auto Camera::recalculate_view_matrix() -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            float pitch = glm::radians(m_rotation.x);
            float yaw   = glm::radians(m_rotation.y);
            float roll  = glm::radians(m_rotation.z);

            glm::vec3 direction;
            direction.x = cos(yaw) * cos(pitch);
            direction.y = sin(pitch);
            direction.z = sin(yaw) * cos(pitch);

            glm::vec3 world_up(0.0f, 1.0f, 0.0f);
            glm::vec3 front = glm::normalize(direction);
            glm::vec3 right = glm::normalize(glm::cross(front, world_up));
            glm::vec3 up    = glm::normalize(glm::cross(right, front));

            m_view_matrix            = glm::lookAt(m_position, m_position + front, up);
            m_view_projection_matrix = m_projection_matrix * m_view_matrix;
        }
        else if (m_camera_mode == CameraMode::Orbital)
        {
            glm::vec3 position = get_orbital_position();
            glm::vec3 target = m_orbital_target;
            glm::vec3 up(0.0f, 1.0f, 0.0f);

            m_view_matrix = glm::lookAt(position, target, up);
            m_view_projection_matrix = m_projection_matrix * m_view_matrix;
        }
    }

    auto Camera::get_forward_direction() const -> glm::vec3
    {
        float pitch = glm::radians(m_rotation.x);
        float yaw   = glm::radians(m_rotation.y);

        glm::vec3 direction;
        direction.x = cos(yaw) * cos(pitch);
        direction.y = sin(pitch);
        direction.z = sin(yaw) * cos(pitch);

        return glm::normalize(direction);
    }

    auto Camera::get_right_direction() const -> glm::vec3
    {
        glm::vec3 world_up(0.0f, 1.0f, 0.0f);
        return glm::normalize(glm::cross(get_forward_direction(), world_up));
    }

    auto Camera::get_up_direction() const -> glm::vec3
    {
        return glm::normalize(glm::cross(get_right_direction(), get_forward_direction()));
    }

    auto Camera::on_mouse_move(float x_offset, float y_offset, bool constrain_pitch) -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            x_offset *= m_mouse_sensitivity;
            y_offset *= m_mouse_sensitivity;

            m_rotation.y += x_offset;
            m_rotation.x += y_offset;

            if (constrain_pitch)
            {
                if (m_rotation.x > 89.0f)
                    m_rotation.x = 89.0f;
                if (m_rotation.x < -89.0f)
                    m_rotation.x = -89.0f;
            }

            recalculate_view_matrix();
        }
    }

    auto Camera::move_forward(float delta_time) -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            m_position += get_forward_direction() * m_movement_speed * delta_time;
            recalculate_view_matrix();
        }
    }

    auto Camera::move_backward(float delta_time) -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            m_position -= get_forward_direction() * m_movement_speed * delta_time;
            recalculate_view_matrix();
        }
    }

    auto Camera::move_right(float delta_time) -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            m_position += get_right_direction() * m_movement_speed * delta_time;
            recalculate_view_matrix();
        }
    }

    auto Camera::move_left(float delta_time) -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            m_position -= get_right_direction() * m_movement_speed * delta_time;
            recalculate_view_matrix();
        }
    }

    auto Camera::move_up(float delta_time) -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            glm::vec3 world_up(0.0f, 1.0f, 0.0f);
            m_position += world_up * m_movement_speed * delta_time;
            recalculate_view_matrix();
        }
    }

    auto Camera::move_down(float delta_time) -> void
    {
        if (m_camera_mode == CameraMode::FPS)
        {
            glm::vec3 world_up(0.0f, 1.0f, 0.0f);
            m_position -= world_up * m_movement_speed * delta_time;
            recalculate_view_matrix();
        }
    }

    auto Camera::get_orbital_position() const -> glm::vec3
    {
        float clamped_elevation = glm::clamp(m_elevation, 0.01f, float(std::numbers::pi) - 0.01f);
        return glm::vec3
        (
            m_orbital_radius * sin(clamped_elevation) * cos(m_azimuth),
            m_orbital_radius * cos(clamped_elevation),
            m_orbital_radius * sin(clamped_elevation) * sin(m_azimuth)
        );
    }

    auto Camera::update_orbital() -> void
    {
        m_orbital_target = glm::vec3(0.0f, 0.0f, 0.0f);
        if (m_dragging || m_panning)
            m_moving = true;
        else
            m_moving = false;
        recalculate_view_matrix();
    }

    auto Camera::process_orbital_mouse_move(double x, double y) -> void
    {
        if (m_dragging && !m_panning)
        {
            float dx = float(x - m_last_x_orbital);
            float dy = float(y - m_last_y_orbital);

            m_azimuth   += dx * m_orbital_speed;
            m_elevation -= dy * m_orbital_speed;
            m_elevation = glm::clamp(m_elevation, 0.01f, float(std::numbers::pi) - 0.01f);
        }

        m_last_x_orbital = x;
        m_last_y_orbital = y;
        update_orbital();
    }

    auto Camera::process_orbital_mouse_button(int button, int action, int mods) -> void
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (action == GLFW_PRESS)
            {
                m_dragging = true;
                m_panning = false;
            }
            else if (action == GLFW_RELEASE)
            {
                m_dragging = false;
                m_panning = false;
            }
        }
    }

    auto Camera::process_orbital_scroll(double x_offset, double y_offset) -> void
    {
        m_orbital_radius -= y_offset * m_zoom_speed;
        m_orbital_radius = glm::clamp(m_orbital_radius, m_orbital_min_radius, m_orbital_max_radius);
        update_orbital();
    }
}
