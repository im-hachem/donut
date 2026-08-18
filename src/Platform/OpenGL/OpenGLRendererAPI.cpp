#include "OpenGLRendererAPI.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Donut
{
    void OpenGLRendererAPI::Init()
    {
        if (!glfwGetCurrentContext())
        {
            DONUT_ERROR("No OpenGL context is current! Cannot initialize GLAD.");
            return;
        }
        
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            DONUT_ERROR("Failed to initialize GLAD!");
            return;
        }
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }

    void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::EnableDepthTest()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::DisableDepthTest()
    {
        glDisable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::SetFaceCulling(bool enabled)
    {
        if (enabled)
        {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
        }
        else
            glDisable(GL_CULL_FACE);
    }

    void OpenGLRendererAPI::EnableBlending()
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void OpenGLRendererAPI::DisableBlending()
    {
        glDisable(GL_BLEND);
    }

    void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
    {
        uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void OpenGLRendererAPI::DrawArrays(uint32_t vertexCount, uint32_t first)
    {
        glDrawArrays(GL_TRIANGLES, first, vertexCount);
    }

    void OpenGLRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
    {
        uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_LINES, count, GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::BindTexture(uint32_t textureID, uint32_t slot)
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }

    void OpenGLRendererAPI::BindImageTexture(uint32_t textureID, uint32_t slot, bool readOnly)
    {
        // Image load/store is OpenGL 4.2; the pointer is null on macOS (4.1).
        if (glBindImageTexture == nullptr)
            return;
        glBindImageTexture(slot, textureID, 0, GL_FALSE, 0,
                          readOnly ? GL_READ_ONLY : GL_WRITE_ONLY, GL_RGBA8);
    }

    void OpenGLRendererAPI::ReadPixels(uint32_t x, uint32_t y, uint32_t width, uint32_t height, 
                                       uint32_t format, uint32_t type, void* pixels)
    {
        glReadPixels(x, y, width, height, format, type, pixels);
    }
};
