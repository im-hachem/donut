#pragma once

#include <vector>
#include <numbers>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cmath>

#include "Core/Camera.h"
#include "Object.h"

#include "Rendering/Renderer.h"
#include "Rendering/Shader.h"
#include "Rendering/VertexArray.h"
#include "Rendering/Texture.h"
#include "Rendering/UniformBuffer.h"
#include "Rendering/TextureManager.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Donut
{
    const double c = 299792458.0;
    const double G = 6.67430e-11;

    struct BlackHole
    {
        glm::vec3 m_Position;
        double m_Mass;
        double m_Radius;
        double m_Rs;

        BlackHole(glm::vec3 pos, float mass)
            : m_Position(pos), m_Mass(mass)
        {
            m_Rs = 2.0 * G * m_Mass / (c * c);
        }

        bool Intercept(float px, float py, float pz) const
        {
            double dx = double(px) - double(m_Position.x);
            double dy = double(py) - double(m_Position.y);
            double dz = double(pz) - double(m_Position.z);
            double dist2 = dx * dx + dy * dy + dz * dz;
            return dist2 < m_Rs * m_Rs;
        }
    };

    struct ObjectData
    {
        glm::vec4 m_PosRadius;
        glm::vec4 m_Color;
        float     m_Mass;
        glm::vec3 m_Velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    };

    class Engine
    {
    public:
        Engine();
        ~Engine() = default;

        void DrawFullScreenQuad();
        void DrawBlurPass();
        void DispatchCompute(const Camera& cam);
        void UploadCameraUBO(const Camera& cam);
        void UploadObjectsUBO(const std::vector<ObjectData>& objs);
        void UploadDiskUBO();
        void UploadSimulationUBO();
        void RenderScene();
        void UpdatePhysics(float deltaTime);
        void UpdateWindowDimensions();
        void SetWindowDimensions(int width, int height);

        int GetWidth()  const { return m_Width;  }
        int GetHeight() const { return m_Height; }

        std::vector<ObjectData>& GetObjects() { return m_Objects; }
        BlackHole&               GetSagA()    { return m_SagA;    }
        Camera&                  GetCamera()  { return m_Camera;  }
        bool&                    GetGravity() { return m_Gravity; }

        void  UpdatePerformance(float deltaTime);
        void  SetTargetFPS(int fps)        { m_TargetFPS = fps;                             }
        int   GetTargetFPS()         const { return m_TargetFPS;                            }
        float GetCurrentFPS()        const { return m_CurrentFPS;                           }
        void  SetComputeHeight(int height)
        {
#ifdef __APPLE__
            // Without compute shaders the geodesic pass runs as a tiled
            // fragment shader (see DrawGeodesicPass), so very high working
            // resolutions make each frame take many seconds. Cap it on macOS.
            if (height > 256) height = 256;
#endif
            m_ComputeHeight = height;
        }
        int   GetComputeHeight()     const { return m_ComputeHeight;                        }
        int   GetComputeWidth()      const { return (m_Width * m_ComputeHeight) / m_Height; }
        void  UpdateComputeDimensions();

        int   GetMaxStepsMoving()    const { return m_MaxStepsMoving; }
        int   GetMaxStepsStatic()    const { return m_MaxStepsStatic; }
        float GetEarlyExitDistance() const { return m_EarlyExitDistance; }
        void  SetMaxStepsMoving(int steps) { m_MaxStepsMoving = steps; }
        void  SetMaxStepsStatic(int steps) { m_MaxStepsStatic = steps; }
        void  SetEarlyExitDistance(float distance) { m_EarlyExitDistance = distance; }

        float GetDiskThickness()         const  { return m_DiskThickness;      }
        void  SetDiskThickness(float thickness) { m_DiskThickness = thickness; }

        float GetDiskDensity()            const { return m_DiskDensity;    }
        void  SetDiskDensity(float density)     { m_DiskDensity = density; }

        float GetRotationSpeed()          const { return m_RotationSpeed;  }
        void  SetRotationSpeed(float speed)     { m_RotationSpeed = speed; }

        float GetBlurStrength()           const { return m_BlurStrength;     }
        void  SetBlurStrength(float strength)   { m_BlurStrength = strength; }

        float GetGlowIntensity()          const { return m_GlowIntensity;      }
        void  SetGlowIntensity(float intensity) { m_GlowIntensity = intensity; }

        void LoadObjectsFromScene(const std::vector<Donut::Object>& objects);
        void ExportHighResFrame(const std::string& filename, int width = 4096, int height = 3072);
        void PrintObjectInfo() const;

        void SetHDRIEnvironment(Ref<CubemapTexture> hdri) { m_HDRIEnvironment = hdri; }
        Ref<CubemapTexture> GetHDRIEnvironment()    const { return m_HDRIEnvironment; }
    private:
        Ref<Shader> CreateComputeProgram(const char* path);
        std::pair<Ref<VertexArray>, Ref<Texture2D>> QuadVAO();

        // Draws the bound geodesic shader over a cw x ch target. On macOS this
        // is split into scissored tiles (with a flush each) so no single GPU
        // submission trips the OS watchdog; elsewhere it is one fast draw.
        void DrawGeodesicPass(int cw, int ch);
    private:
        Ref<VertexArray>   m_QuadVAO;
        Ref<Texture2D>     m_Texture;
        Ref<CubemapTexture> m_HDRIEnvironment;
        Ref<Shader>        m_ShaderProgram;
        Ref<Shader>        m_ComputeProgram;
        Ref<Shader>        m_BlurShader;
        Ref<UniformBuffer> m_CameraUBO;
        Ref<UniformBuffer> m_DiskUBO;
        Ref<UniformBuffer> m_ObjectsUBO;
        Ref<UniformBuffer> m_SimulationUBO;

        // FBO used to render the geodesic pass into m_Texture. The geodesic
        // shader is a fragment shader (macOS has no compute), so it draws a
        // fullscreen quad into this framebuffer instead of dispatching compute.
        uint32_t m_GeodesicFBO = 0;

        int   m_Width;
        int   m_Height;
        float m_Width_f = 100.0f*1e10f;
        float m_Height_f = 75.0f*1e10f;

        int   m_TargetFPS     = 60;
        float m_CurrentFPS    = 60.0f;
        float m_LastFrameTime = 0.0f;
        int   m_ComputeHeight = 150;

        std::vector<ObjectData> m_Objects;
        BlackHole               m_SagA;
        Camera                  m_Camera;
        bool                    m_Gravity = false;

        int   m_MaxStepsMoving    = 60000;
        int   m_MaxStepsStatic    = 30000;
        float m_EarlyExitDistance = 5.0e11f;

        float m_DiskThickness = 0.1f;
        float m_DiskDensity   = 0.1f;
        float m_RotationSpeed = 1.0f;
        float m_BlurStrength  = 2.0f;
        float m_GlowIntensity = 0.1f;
    };
};
