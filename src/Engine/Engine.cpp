#include <iostream>
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "Engine.h"
#include "Core/Log.h"
#include "Core/HDRIManager.h"
#include "Rendering/VertexBuffer.h"
#include "Rendering/IndexBuffer.h"

namespace Donut
{
    Engine::Engine()
        : m_SagA(glm::vec3(0.0f, 0.0f, 0.0f), 8.54e36f)
    {
        m_Width = 1280;
        m_Height = 720;
        
        m_Camera.SetCameraMode(CameraMode::Orbital);
        m_Camera.SetOrbitalRadius(1e11);
        m_Camera.SetOrbitalLimits(1e9, 1e13);
        m_Camera.SetOrbitalSpeed(0.01f);
        m_Camera.SetZoomSpeed(1e10f);
        
        m_Objects = 
        {
            { glm::vec4(0.00f, 0.00f, 0.00f, m_SagA.m_Rs), glm::vec4(0, 0, 0, 1), static_cast<float>(m_SagA.m_Mass) }
        };

        // The geodesic ray tracer used to be a compute shader; it is now a
        // fullscreen vertex+fragment pass (see DispatchCompute) so it runs on
        // macOS OpenGL 4.1, which has no compute shaders.
        m_ComputeProgram = Ref<Shader>(Shader::Create("Assets/Shaders/Geodesic.glsl"));
        m_ShaderProgram  = Ref<Shader>(Shader::Create("Assets/Shaders/TexturedQuad.glsl"));
        m_BlurShader     = Ref<Shader>(Shader::Create("Assets/Shaders/Blur.glsl"));
        
        auto& hdriManager = HDRIManager::Get();
        m_HDRIEnvironment = hdriManager.GetCurrentHDRI();
        if (!m_HDRIEnvironment)
        {
            hdriManager.SetCurrentHDRI("Assets/HDRI/HDR_blue_nebulae-1.hdr");
            m_HDRIEnvironment = hdriManager.GetCurrentHDRI();
            if (!m_HDRIEnvironment)
                DONUT_WARN("Failed to load default HDRI, using fallback");
        }
        
        m_CameraUBO = UniformBuffer::Create(128, 1);
        m_DiskUBO   = UniformBuffer::Create(sizeof(float) * 5, 2);
        
        uint32_t objUBOSize = sizeof(int) + 3 * sizeof(float)
            + 16 * (sizeof(glm::vec4) + sizeof(glm::vec4))
            + 16 * sizeof(float);
        m_ObjectsUBO = UniformBuffer::Create(objUBOSize, 3);
        
        m_SimulationUBO = UniformBuffer::Create(sizeof(int) * 2 + sizeof(float) * 2, 4);

        auto result = QuadVAO();
        m_QuadVAO = result.first;
        m_Texture = result.second;

        // GLSL 4.10 forbids explicit binding qualifiers on uniform blocks, so
        // associate the geodesic shader's blocks with their UBO binding points
        // from the host side instead.
        if (m_ComputeProgram)
        {
            uint32_t prog = m_ComputeProgram->GetRendererID();
            struct { const char* name; uint32_t point; } blocks[] =
            {
                { "Camera", 1 }, { "Disk", 2 }, { "Objects", 3 }, { "Simulation", 4 }
            };
            for (const auto& b : blocks)
            {
                uint32_t idx = glGetUniformBlockIndex(prog, b.name);
                if (idx != GL_INVALID_INDEX)
                    glUniformBlockBinding(prog, idx, b.point);
            }
        }

        glGenFramebuffers(1, &m_GeodesicFBO);
    }

    void Engine::UpdateWindowDimensions()
    {
        UpdateComputeDimensions();
    }
    
    void Engine::SetWindowDimensions(int width, int height)
    {
        int oldWidth = m_Width;
        int oldHeight = m_Height;
        int oldComputeHeight = m_ComputeHeight;
        
        m_Width = width;
        m_Height = height;
        
        if (oldWidth         != m_Width  || 
            oldHeight        != m_Height || 
            oldComputeHeight != m_ComputeHeight)
            UpdateComputeDimensions();
    }

    void Engine::UpdatePerformance(float deltaTime)
    {
        if (deltaTime > 0.0f)
            m_CurrentFPS = 1.0f / deltaTime;
    }

    void Engine::UpdateComputeDimensions()
    {
        m_Texture = Texture2D::Create(GetComputeWidth(), m_ComputeHeight);
    }

    void Engine::DrawFullScreenQuad()
    {
        RenderCommand::SetViewport(0, 0, m_Width, m_Height);
        
        m_ShaderProgram->Bind();
        m_QuadVAO->Bind();

        m_Texture->Bind(0);
        m_ShaderProgram->SetInt("u_ScreenTexture", 0);

        RenderCommand::DisableDepthTest();
        RenderCommand::DrawArrays(6);
        RenderCommand::EnableDepthTest();
    }

    void Engine::DrawBlurPass()
    {
        RenderCommand::SetViewport(0, 0, m_Width, m_Height);
        
        m_BlurShader->Bind();
        m_QuadVAO->Bind();

        m_Texture->Bind(0);
        m_BlurShader->SetInt("u_ScreenTexture", 0);
        m_BlurShader->SetFloat2("u_Resolution", glm::vec2(m_Width, m_Height));
        m_BlurShader->SetFloat("u_BlurStrength", m_BlurStrength);
        m_BlurShader->SetFloat("u_GlowIntensity", m_GlowIntensity);

        RenderCommand::DisableDepthTest();
        RenderCommand::DrawArrays(6);
        RenderCommand::EnableDepthTest();
    }

    void Engine::DrawGeodesicPass(int cw, int ch)
    {
        m_QuadVAO->Bind();
        RenderCommand::DisableDepthTest();

#ifdef __APPLE__
        // macOS aborts any GPU submission that runs longer than a couple of
        // seconds ("GPU Hang"). The geodesic ray-marcher can far exceed that in
        // a single fullscreen draw, so render it in scissored tiles and flush
        // after each, keeping every submission short enough to survive the
        // watchdog. Compute-capable platforms draw it in one pass.
        const int tile = 24;
        glEnable(GL_SCISSOR_TEST);
        for (int y = 0; y < ch; y += tile)
        {
            int th = std::min(tile, ch - y);
            for (int x = 0; x < cw; x += tile)
            {
                int tw = std::min(tile, cw - x);
                glScissor(x, y, tw, th);
                RenderCommand::DrawArrays(6);
                glFinish();
            }
        }
        glDisable(GL_SCISSOR_TEST);
#else
        RenderCommand::DrawArrays(6);
#endif

        RenderCommand::EnableDepthTest();
    }

    void Engine::DispatchCompute(const Camera& cam)
    {
        auto& hdriManager = HDRIManager::Get();
        m_HDRIEnvironment = hdriManager.GetCurrentHDRI();

        int cw = GetComputeWidth();
        int ch = m_ComputeHeight;

        // Render the geodesic pass into m_Texture through an FBO. This replaces
        // the old compute dispatch + imageStore path, which relied on OpenGL
        // 4.3 compute and 4.2 image load/store that macOS does not provide.
        glBindFramebuffer(GL_FRAMEBUFFER, m_GeodesicFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture->GetRendererID(), 0);
        glViewport(0, 0, cw, ch);

        m_ComputeProgram->Bind();
        UploadCameraUBO(cam);
        UploadDiskUBO();
        UploadObjectsUBO(m_Objects);
        UploadSimulationUBO();
        m_ComputeProgram->SetFloat2("u_Resolution", glm::vec2(static_cast<float>(cw), static_cast<float>(ch)));

        if (m_HDRIEnvironment)
        {
            m_HDRIEnvironment->Bind(5);
            m_ComputeProgram->SetInt("u_HDRIEnvironment", 5);
        }

        DrawGeodesicPass(cw, ch);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Engine::UploadCameraUBO(const Camera& cam)
    {
        struct UBOData
        {
            glm::vec3 pos;     float _pad0;
            glm::vec3 right;   float _pad1;
            glm::vec3 up;      float _pad2;
            glm::vec3 forward; float _pad3;
            float tanHalfFov;
            float aspect;
            bool  moving;
            int   _pad4;
        } data;

        glm::vec3 fwd   = glm::normalize(cam.GetOrbitalTarget() - cam.GetOrbitalPosition());
        glm::vec3 up    = glm::vec3(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(fwd, up));
        up = glm::cross(right, fwd);

        data.pos        = cam.GetOrbitalPosition();
        data.right      = right;
        data.up         = up;
        data.forward    = fwd;
        data.tanHalfFov = static_cast<float>(tan(glm::radians(60.0f * 0.5f)));
        data.aspect     = static_cast<float>(GetComputeWidth()) / static_cast<float>(m_ComputeHeight);
        data.moving     = cam.IsDragging() || cam.IsPanning();

        m_CameraUBO->SetData(&data, sizeof(UBOData));
        m_CameraUBO->Bind(1);
    }

    void Engine::UploadObjectsUBO(const std::vector<ObjectData>& objs)
    {
        struct UBOData
        {
            int numObjects;
            float _pad0, _pad1, _pad2;
            glm::vec4 posRadius[16];
            glm::vec4 color[16];
            float mass[16];
        } data;

        size_t count = std::min(objs.size(), size_t(16));
        data.numObjects = static_cast<int>(count);

        for (size_t i = 0; i < count; ++i)
        {
            data.posRadius[i] = objs[i].m_PosRadius;
            data.color[i] = objs[i].m_Color;
            data.mass[i] = objs[i].m_Mass;
        }

        m_ObjectsUBO->SetData(&data, sizeof(data));
        m_ObjectsUBO->Bind(3);
    }

    void Engine::UploadDiskUBO()
    {
        float r1 = static_cast<float>(m_SagA.m_Rs * 2.2);
        float r2 = static_cast<float>(m_SagA.m_Rs * 5.2);
        float num = 2.0f;
        float thickness = static_cast<float>(m_SagA.m_Rs * m_DiskThickness);
        float diskData[5] = { r1, r2, num, thickness, m_DiskDensity };

        m_DiskUBO->SetData(diskData, sizeof(diskData));
        m_DiskUBO->Bind(2);
    }

    void Engine::UploadSimulationUBO()
    {
        struct UBOData
        {
            int maxStepsMoving;
            int maxStepsStatic;
            float earlyExitDistance;
            float time;
        } data;

        data.maxStepsMoving    = m_MaxStepsMoving;
        data.maxStepsStatic    = m_MaxStepsStatic;
        data.earlyExitDistance = m_EarlyExitDistance;
        data.time              = static_cast<float>(glfwGetTime()) * m_RotationSpeed;

#ifdef __APPLE__
        // macOS has no compute shaders, so the geodesic pass runs as a tiled
        // fragment shader under the OS GPU watchdog. The stock step counts
        // (up to 30000) make a single tile exceed the watchdog and hang the
        // GPU, so cap them here. Windows/Linux keep the full step count.
        data.maxStepsMoving = std::min(data.maxStepsMoving, 10000);
        data.maxStepsStatic = std::min(data.maxStepsStatic, 10000);
#endif

        m_SimulationUBO->SetData(&data, sizeof(data));
        m_SimulationUBO->Bind(4);
    }

    void Engine::UpdatePhysics(float deltaTime)
    {
        for (auto& obj : m_Objects)
        {
            for (auto& obj2 : m_Objects)
            {
                if (&obj == &obj2) continue;
                float dx = obj2.m_PosRadius.x - obj.m_PosRadius.x;
                float dy = obj2.m_PosRadius.y - obj.m_PosRadius.y;
                float dz = obj2.m_PosRadius.z - obj.m_PosRadius.z;
                float distance = sqrt(dx * dx + dy * dy + dz * dz);
                if (distance > 0)
                {
                    std::vector<double> direction = {dx / distance, dy / distance, dz / distance};
                    double Gforce = (G * obj.m_Mass * obj2.m_Mass) / (distance * distance);
                    double acc1 = Gforce / obj.m_Mass;
                    std::vector<double> acc = {direction[0] * acc1, direction[1] * acc1, direction[2] * acc1};
                    
                    if (m_Gravity)
                    {
                        obj.m_Velocity.x += static_cast<float>(acc[0]);
                        obj.m_Velocity.y += static_cast<float>(acc[1]);
                        obj.m_Velocity.z += static_cast<float>(acc[2]);

                        obj.m_PosRadius.x += static_cast<float>(obj.m_Velocity.x);
                        obj.m_PosRadius.y += static_cast<float>(obj.m_Velocity.y);
                        obj.m_PosRadius.z += static_cast<float>(obj.m_Velocity.z);
                    }
                }
            }
        }
    }

    void Engine::RenderScene()
    {
        RenderCommand::Clear();
        m_ShaderProgram->Bind();
        m_QuadVAO->Bind();
        m_Texture->Bind(0);
        RenderCommand::DrawArrays(6);
    }

    Ref<Shader> Engine::CreateComputeProgram(const char* path)
    {
        std::ifstream in(path);
        if(!in.is_open())
        {
            std::cerr << "Failed to open compute shader: " << path << "\n";
            return nullptr;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        std::string srcStr = ss.str();
        return Ref<Shader>(Shader::CreateCompute("ComputeShader", srcStr));
    }

    std::pair<Ref<VertexArray>, Ref<Texture2D>> Engine::QuadVAO()
    {
        float quadVertices[] = 
        {
            // Positions   // TexCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        auto vertexBuffer = Ref<VertexBuffer>(VertexBuffer::Create(quadVertices, static_cast<uint32_t>(sizeof(quadVertices))));
        VertexBufferLayout layout;
        layout.Push<float>(2); // Position (x, y)
        layout.Push<float>(2); // TexCoord (u, v)
        vertexBuffer->SetLayout(layout);

        auto vertexArray = Ref<VertexArray>(VertexArray::Create());
        vertexArray->AddVertexBuffer(vertexBuffer);
        auto texture = Texture2D::Create(GetComputeWidth(), m_ComputeHeight);
        
        return { vertexArray, texture };
    }
    
    void Engine::LoadObjectsFromScene(const std::vector<Donut::Object>& objects)
    {
        m_Objects.clear();
        m_Objects.push_back(
        { 
            glm::vec4(0.00f, 0.00f, 0.00f, m_SagA.m_Rs), 
            glm::vec4(0, 0, 0, 1), 
            static_cast<float>(m_SagA.m_Mass) 
        });
        
        for (const auto& obj : objects)
        {
            ObjectData engineObj;
            
            float scaleFactor = 1e10f;
            engineObj.m_PosRadius = glm::vec4
            (
                obj.m_Centre.x * scaleFactor,
                obj.m_Centre.y * scaleFactor,
                obj.m_Centre.z * scaleFactor,
                obj.m_Radius * scaleFactor
            );
            
            engineObj.m_Color = glm::vec4(obj.m_Material.m_Color, 1.0f);
            
            float volume  = (4.0f / 3.0f) * 3.14159f * engineObj.m_PosRadius.w * engineObj.m_PosRadius.w * engineObj.m_PosRadius.w;
            float density = 1e12f;
            engineObj.m_Mass     = volume * density;
            engineObj.m_Velocity = glm::vec3(0.0f, 0.0f, 0.0f);
            
            m_Objects.push_back(engineObj);
        }
        
        DONUT_INFO("Loaded {} objects from WorldBuilder scene (scaled up by {})", objects.size(), 1e10f);
        PrintObjectInfo();
    }
    
    void Engine::PrintObjectInfo() const
    {
        DONUT_INFO("=== Object Information ===");
        DONUT_INFO("Total objects: {}", m_Objects.size());
        
        for (size_t i = 0; i < m_Objects.size(); ++i)
        {
            const auto& obj = m_Objects[i];
            DONUT_INFO("Object {}: Pos=({}, {}, {}), Radius={}, Mass={}, Color=({}, {}, {})", 
                i,
                obj.m_PosRadius.x, obj.m_PosRadius.y, obj.m_PosRadius.z,
                obj.m_PosRadius.w,
                obj.m_Mass,
                obj.m_Color.x, obj.m_Color.y, obj.m_Color.z
            );
        }
        DONUT_INFO("Camera position: ({}, {}, {})", 
            m_Camera.GetOrbitalPosition().x,
            m_Camera.GetOrbitalPosition().y,
            m_Camera.GetOrbitalPosition().z
        );
        DONUT_INFO("Camera radius: {}", m_Camera.GetOrbitalRadius());
        DONUT_INFO("========================");
    }
    
    void Engine::ExportHighResFrame(const std::string& filename, int width, int height)
    {
        DONUT_INFO("Exporting high-resolution frame: {}x{} to {}", width, height, filename);
        
        if (width <= 0 || height <= 0)
        {
            DONUT_ERROR("Invalid dimensions for export: {}x{}", width, height);
            return;
        }
        
        if (filename.empty())
        {
            DONUT_ERROR("Invalid filename for export");
            return;
        }
        
        int originalWidth = m_Width;
        int originalHeight = m_Height;
        int originalComputeHeight = m_ComputeHeight;
        
        m_Width = width;
        m_Height = height;
        m_ComputeHeight = height;
        
        int computeHeight = height;
        int computeWidth = (width * computeHeight) / height;
        
        if (computeWidth <= 0 || computeHeight <= 0)
        {
            DONUT_ERROR("Invalid compute dimensions: {}x{}", computeWidth, computeHeight);
            return;
        }
        
        FramebufferSpecification fbSpec;
        fbSpec.Width = width;
        fbSpec.Height = height;
        fbSpec.Attachments = { FramebufferTextureFormat::RGBA8 };
        
        auto highResFramebuffer = Framebuffer::Create(fbSpec);
        if (!highResFramebuffer)
        {
            DONUT_ERROR("Failed to create high-resolution framebuffer");
            return;
        }
        
        auto highResTexture = Texture2D::Create(computeWidth, computeHeight);
        if (!highResTexture)
        {
            DONUT_ERROR("Failed to create high-resolution texture");
            return;
        }

        // Render the geodesic pass into highResTexture through the geodesic FBO.
        glBindFramebuffer(GL_FRAMEBUFFER, m_GeodesicFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, highResTexture->GetRendererID(), 0);
        glViewport(0, 0, computeWidth, computeHeight);

        m_ComputeProgram->Bind();

        struct UBOData
        {
            glm::vec3 pos;     float _pad0;
            glm::vec3 right;   float _pad1;
            glm::vec3 up;      float _pad2;
            glm::vec3 forward; float _pad3;
            float tanHalfFov;
            float aspect;
            bool  moving;
            int   _pad4;
        } data;

        glm::vec3 fwd   = glm::normalize(m_Camera.GetOrbitalTarget() - m_Camera.GetOrbitalPosition());
        glm::vec3 up    = glm::vec3(0, 1, 0);
        glm::vec3 right = glm::normalize(glm::cross(fwd, up));
        up = glm::cross(right, fwd);

        data.pos        = m_Camera.GetOrbitalPosition();
        data.right      = right;
        data.up         = up;
        data.forward    = fwd;
        data.tanHalfFov = static_cast<float>(tan(glm::radians(60.0f * 0.5f)));
        data.aspect     = static_cast<float>(computeWidth) / static_cast<float>(computeHeight);
        data.moving     = m_Camera.IsDragging() || m_Camera.IsPanning();

        m_CameraUBO->SetData(&data, sizeof(UBOData));
        m_CameraUBO->Bind(1);

        UploadDiskUBO();
        UploadObjectsUBO(m_Objects);
        UploadSimulationUBO();
        m_ComputeProgram->SetFloat2("u_Resolution", glm::vec2(static_cast<float>(computeWidth), static_cast<float>(computeHeight)));

        if (m_HDRIEnvironment)
        {
            m_HDRIEnvironment->Bind(5);
            m_ComputeProgram->SetInt("u_HDRIEnvironment", 5);
        }

        DrawGeodesicPass(computeWidth, computeHeight);

        // Display the rendered frame into the high-res framebuffer for read-back.
        highResFramebuffer->Bind();
        RenderCommand::SetViewport(0, 0, width, height);
        RenderCommand::Clear();

        m_ShaderProgram->Bind();
        m_QuadVAO->Bind();
        
        highResTexture->Bind(0);
        m_ShaderProgram->SetInt("u_ScreenTexture", 0);
        
        RenderCommand::DisableDepthTest();
        RenderCommand::DrawArrays(6);
        RenderCommand::EnableDepthTest();
        
        std::vector<unsigned char> pixels(width * height * 4);
        DONUT_INFO("Reading {} pixels from framebuffer...", width * height);
        RenderCommand::ReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        
        DONUT_INFO("Flipping image vertically...");
        std::vector<unsigned char> flippedPixels(width * height * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int srcIndex = (y * width + x) * 4;
                int dstIndex = ((height - 1 - y) * width + x) * 4;
                flippedPixels[dstIndex + 0] = pixels[srcIndex + 0]; // R
                flippedPixels[dstIndex + 1] = pixels[srcIndex + 1]; // G
                flippedPixels[dstIndex + 2] = pixels[srcIndex + 2]; // B
                flippedPixels[dstIndex + 3] = pixels[srcIndex + 3]; // A
            }
        }
        
        DONUT_INFO("Saving PNG file: {}...", filename);
        int result = stbi_write_png(filename.c_str(), width, height, 4, flippedPixels.data(), width * 4);
        
        if (result)
            DONUT_INFO("Successfully exported high-resolution frame to: {}", filename);
        else
            DONUT_ERROR("Failed to export high-resolution frame to: {}", filename);
        
        highResFramebuffer->Unbind();
        
        m_Width         = originalWidth;
        m_Height        = originalHeight;
        m_ComputeHeight = originalComputeHeight;
        
        RenderCommand::SetViewport(0, 0, originalWidth, originalHeight);
    }
    
}
