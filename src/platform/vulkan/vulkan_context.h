#pragma once

// Pure-C++ interface to the Vulkan backend (no vulkan.h leaks into the rest of
// the engine; the implementation lives in VulkanContext.cpp). On macOS Vulkan
// runs through MoltenVK (Vulkan -> Metal).
namespace Donut
{
    class VulkanContext
    {
    public:
        VulkanContext();
        ~VulkanContext();

        // Creates the instance, picks a physical device, and creates the logical
        // device + graphics queue. Returns false (and logs) on failure.
        auto init() -> bool;
        auto shutdown() -> void;

        // Phase 1 verification: renders a known clear colour into an offscreen
        // image and reads it back, confirming instance -> device -> render pass
        // -> command buffer -> submit -> read-back all work end to end.
        auto self_test_clear() -> bool;

        // Phase 2/3 verification: builds a graphics pipeline from Slang-compiled
        // SPIR-V and draws a full-screen gradient triangle into the offscreen
        // image, confirming the SPIR-V -> pipeline -> draw path works.
        auto self_test_triangle() -> bool;

        // B-3: renders the geodesic (black hole) fragment shader through Vulkan
        // into an offscreen image and writes it to pngPath. Exercises UBOs,
        // descriptor sets, a cubemap sampler and the geodesic pipeline.
        auto render_geodesic(const char* pngPath) -> bool;

    private:
        struct Impl;
        Impl* m_impl = nullptr;
    };

    // Convenience one-shot: init() + self_test_clear() + shutdown(). Logs results.
    auto vulkan_self_test() -> bool;
}
