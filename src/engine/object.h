#pragma once

#include <glm/glm.hpp>

namespace Donut
{
    class Ray
    {
    public:
        Ray(glm::vec3 o, glm::vec3 d)
            : m_origin(o),
              m_direction(glm::normalize(d)) { }

    public:
        glm::vec3 m_direction;
        glm::vec3 m_origin;
    };

    class Material
    {
    public:
        Material() : m_color(1.0f, 1.0f, 1.0f), m_specular(0.5f), m_emission(0.0f) { }
        Material(glm::vec3 c, float s, float e)
            : m_color(c),
              m_specular(s),
              m_emission(e) { }

    public:
        glm::vec3 m_color;
        float     m_specular;
        float     m_emission;
    };

    class Object
    {
    public:
        Object() : m_centre(0.0f, 0.0f, 0.0f), m_radius(1.0f), m_material() { }
        Object(glm::vec3 c, float r, Material m)
            : m_centre(c),
              m_radius(r),
              m_material(m) { }

        auto intersect(Ray& ray, float& t) -> bool
        {
            glm::vec3 oc = ray.m_origin - m_centre;
            float a = glm::dot(ray.m_direction, ray.m_direction);
            float b = 2.0f * glm::dot(oc, ray.m_direction);
            float c = glm::dot(oc, oc) - m_radius * m_radius;
            float discriminant = static_cast<float>(b*b - 4*a*c);

            if (discriminant < 0)
                return false;

            float intercept = (-b - sqrt(discriminant)) / (2.0f*a);
            if (intercept < 0)
            {
                intercept = (-b + sqrt(discriminant)) / (2.0f*a);
                if (intercept < 0)
                    return false;
            }

            t = intercept;
            return true;
        }

        auto get_normal(glm::vec3& point) const -> glm::vec3
        {
            return glm::normalize(point - m_centre);
        }

    public:
        glm::vec3 m_centre;
        float     m_radius;
        Material  m_material;
    };
};
