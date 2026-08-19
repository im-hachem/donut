#pragma once

#include <glm/glm.hpp>
#include <functional>

// Live-window Vulkan backend: owns the instance, surface, device, swapchain,
// render pass, framebuffers and per-frame synchronization, and drives the
// acquire -> record -> submit -> present loop. Pure-C++ header (no vulkan.h /
// glfw leak); the GLFW window is passed as an opaque handle.
namespace Donut
{
    // Must be called BEFORE glfwInit() when the Vulkan API is selected: points
    // GLFW at the loader the app links against (GLFW's own dlopen fails on
    // macOS/Homebrew) and configures the MoltenVK ICD / layer paths.
    void VulkanPrepareGLFW();

    class VulkanRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer();

        // glfwWindow must be a GLFW window created with GLFW_NO_API.
        bool Init(void* glfwWindow, int width, int height);
        void Shutdown();

        // Creates the ImGui context + Vulkan/GLFW backends. Call after Init().
        bool InitImGui();

        // Renders + presents one frame: clears to clearColor, then (if InitImGui
        // ran) opens an ImGui frame, invokes buildUI to populate it, and draws it.
        void DrawFrame(const glm::vec4& clearColor, const std::function<void()>& buildUI = {});

        void OnResize(int width, int height);

    private:
        struct Impl;
        Impl* m_Impl = nullptr;
    };
}
