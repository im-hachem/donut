#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <string>

// Live-window Vulkan backend: owns the instance, surface, device, swapchain,
// render pass, framebuffers and per-frame synchronization, and drives the
// acquire -> record -> submit -> present loop. Pure-C++ header (no vulkan.h /
// glfw leak); the GLFW window is passed as an opaque handle.
namespace Donut
{
    // Must be called BEFORE glfwInit() when the Vulkan API is selected: points
    // GLFW at the loader the app links against (GLFW's own dlopen fails on
    // macOS/Homebrew) and configures the MoltenVK ICD / layer paths.
    auto vulkan_prepare_glfw() -> void;

    class VulkanRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer();

        // glfwWindow must be a GLFW window created with GLFW_NO_API.
        auto init(void* glfwWindow, int width, int height) -> bool;
        auto shutdown() -> void;

        // Creates the ImGui context + Vulkan/GLFW backends. Call after init().
        auto init_im_gui() -> bool;

        // Renders + presents one frame: clears to clearColor, then (if init_im_gui
        // ran) opens an ImGui frame, invokes buildUI to populate it, and draws it.
        auto draw_frame(const glm::vec4& clearColor, const std::function<void()>& buildUI = {}) -> void;

        auto on_resize(int width, int height) -> void;

        // Rebuilds the starfield environment cubemap from another equirect .hdr
        // at runtime (device-idle teardown + rebuild + descriptor rewrite).
        // No-op if path is already the active HDRI.
        auto set_hdri(const std::string& path) -> void;

        // Filesystem path of the HDRI currently backing the cubemap.
        auto current_hdri() const -> const std::string&;

    private:
        struct Impl;
        Impl* m_impl = nullptr;
    };
}
