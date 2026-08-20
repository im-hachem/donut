#pragma once

#include <vector>
#include <limits>

#include "object.h"

namespace Donut
{
    class Scene
    {
    public:
        Scene()
            : m_light_pos(5.0f, 5.0f, 5.0f) { }

        auto trace(Ray& ray) -> glm::vec3
        {
            float closest = std::numeric_limits<float>::infinity();
            const Object* hit_obj = nullptr;

            for (auto& obj : objs)
            {
                float t;

                if (obj.intersect(ray, t))
                    if (t < closest)
                    {
                        closest = t;
                        hit_obj = &obj;
                    }
            }

            if (hit_obj)
            {
                glm::vec3 hit_point = ray.m_origin + ray.m_direction * closest;
                glm::vec3 normal = hit_obj->get_normal(hit_point);
                glm::vec3 light_dir = glm::normalize(m_light_pos - hit_point);

                float diff = std::max(glm::dot(normal, light_dir), 0.0f);

                Ray shadow_ray(hit_point + normal * 0.001f, light_dir);
                bool in_shadow = false;

                for (auto& obj : objs)
                {
                    float t;

                    if (obj.intersect(shadow_ray, t))
                    {
                        in_shadow = true;
                        break;
                    }
                }

                glm::vec3 color = hit_obj->m_material.m_color;
                float ambient = 0.1f;

                if (in_shadow)
                    return color * ambient;
                return color * (ambient + diff * 0.9f);
            }

            return glm::vec3(0.0f, 0.0f, 0.1f);
        }

    public:
        std::vector<Object> objs;
        glm::vec3 m_light_pos;
    };
};
