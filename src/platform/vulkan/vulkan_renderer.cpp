#include "vulkan_renderer.h"
#include "core/log.h"
#include "core/camera.h"
#include "core/settings_manager.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "stb_image.h"

#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>

namespace Donut
{
    #define VK_CHECK(expr)                                             \
        do {                                                           \
            VkResult _r = (expr);                                      \
            if (_r != VK_SUCCESS) {                                    \
                DONUT_ERROR("Vulkan: {} failed ({})", #expr, (int)_r); \
                return false;                                          \
            }                                                          \
        } while (0)

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    auto vulkan_prepare_glfw() -> void
    {
#ifdef __APPLE__
        if (!getenv("VK_ICD_FILENAMES"))
            setenv("VK_ICD_FILENAMES", "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json", 0);
        if (!getenv("VK_LAYER_PATH"))
            setenv("VK_LAYER_PATH", "/opt/homebrew/share/vulkan/explicit_layer.d", 0);
        if (!getenv("DYLD_LIBRARY_PATH"))
            setenv("DYLD_LIBRARY_PATH", "/opt/homebrew/lib", 0);
#endif
        // GLFW dlopen's the Vulkan loader by bare name, which fails on
        // mac_os/Homebrew; hand it the loader entry point we already link against.
        glfwInitVulkanLoader(vkGetInstanceProcAddr);
    }

    struct VulkanRenderer::Impl
    {
        GLFWwindow* window = nullptr;
        int width = 0, height = 0;
        bool framebuffer_resized = false;

        VkInstance       instance = VK_NULL_HANDLE;
        VkSurfaceKHR     surface  = VK_NULL_HANDLE;
        VkPhysicalDevice physical = VK_NULL_HANDLE;
        VkDevice         device   = VK_NULL_HANDLE;
        uint32_t         graphics_family = 0, present_family = 0;
        VkQueue          graphics_queue = VK_NULL_HANDLE, present_queue = VK_NULL_HANDLE;

        VkSwapchainKHR   swapchain = VK_NULL_HANDLE;
        VkFormat         swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
        VkExtent2D       swapchain_extent{};
        std::vector<VkImage>       images;
        std::vector<VkImageView>   image_views;
        VkRenderPass               render_pass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> framebuffers;

        VkCommandPool                command_pool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> command_buffers;   // MAX_FRAMES_IN_FLIGHT

        std::vector<VkSemaphore> image_available;        // per frame in flight
        std::vector<VkSemaphore> render_finished;        // per swapchain image
        std::vector<VkFence>     in_flight;              // per frame in flight
        std::vector<VkFence>     images_in_flight;        // per swapchain image
        uint32_t current_frame = 0;

        VkDescriptorPool imgui_pool = VK_NULL_HANDLE;
        bool imgui_init = false;

        VkPhysicalDeviceMemoryProperties mem_props{};

        // The geodesic is rendered every frame into an offscreen image, then
        // upscaled onto the swapchain by the present pass. Progressive resolution:
        // low-res while the camera moves (keeps dragging responsive and each draw
        // well under the Metal GPU watchdog) and high-res once it settles (resolves
        // the photon ring cleanly). Both are 16:9 so the camera aspect is shared.
        static constexpr uint32_t GEO_LO_W = 480, GEO_LO_H = 270;
        static constexpr uint32_t GEO_HI_W = 960, GEO_HI_H = 540;
        struct GeoTarget
        {
            uint32_t        w = 0, h = 0;
            VkImage         image = VK_NULL_HANDLE;
            VkDeviceMemory  mem   = VK_NULL_HANDLE;
            VkImageView     view  = VK_NULL_HANDLE;
            VkFramebuffer   fb    = VK_NULL_HANDLE;
            VkDescriptorSet present_set = VK_NULL_HANDLE;  // present pass samples this target
        };
        GeoTarget     geo_lo, geo_hi;
        VkRenderPass  geo_render_pass = VK_NULL_HANDLE;    // shared (resolution-independent)
        VkBuffer cam_buf = VK_NULL_HANDLE, disk_buf = VK_NULL_HANDLE, obj_buf = VK_NULL_HANDLE, sim_buf = VK_NULL_HANDLE;
        VkDeviceMemory cam_mem = VK_NULL_HANDLE, disk_mem = VK_NULL_HANDLE, obj_mem = VK_NULL_HANDLE, sim_mem = VK_NULL_HANDLE;
        void* cam_mapped = nullptr;
        void* sim_mapped = nullptr;
        Camera camera{ 60.0f, (float)GEO_HI_W / (float)GEO_HI_H, 0.1f, 100.0f };
        bool left_was_down = false;
        double last_frame_time = 0.0;               // for free-fly dt
        double fps_last_x = 0.0, fps_last_y = 0.0;  // cursor tracking for free-fly look
        bool user_moving = false;                   // camera moved/looked this frame (drives step count)
        VkImage cube_image = VK_NULL_HANDLE; VkDeviceMemory cube_mem = VK_NULL_HANDLE;
        VkImageView cube_view = VK_NULL_HANDLE; VkSampler cube_sampler = VK_NULL_HANDLE;
        std::string hdri_path;  // path backing the current cubemap (UI display + no-op switch guard)
        VkDescriptorSetLayout geo_set_layout = VK_NULL_HANDLE;
        VkDescriptorPool geo_pool = VK_NULL_HANDLE;
        VkDescriptorSet geo_set = VK_NULL_HANDLE;
        VkPipelineLayout geo_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline geo_pipeline = VK_NULL_HANDLE;
        VkBuffer quad_vb = VK_NULL_HANDLE; VkDeviceMemory quad_vb_mem = VK_NULL_HANDLE;
        double start_time = 0.0;
        VkFence geo_in_use = VK_NULL_HANDLE;  // previous frame's fence; guards the shared geodesic image

        // Present pass: samples the geodesic image with a full-screen textured
        // quad, drawn into the swapchain render pass just before the ImGui UI.
        VkSampler present_sampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout present_set_layout = VK_NULL_HANDLE;
        VkDescriptorPool present_pool = VK_NULL_HANDLE;
        VkPipelineLayout present_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline present_pipeline = VK_NULL_HANDLE;

        // World-builder scene view (grid now; sphere/skybox next). Normal-scale
        // orbital camera; geometry drawn straight into the swapchain render pass.
        bool scene_mode = false;
        Camera scene_camera{ 45.0f, (float)GEO_HI_W / (float)GEO_HI_H, 0.1f, 1000.0f };
        VkBuffer grid_vb = VK_NULL_HANDLE; VkDeviceMemory grid_vb_mem = VK_NULL_HANDLE;
        uint32_t grid_vertex_count = 0;
        VkBuffer grid_ubo = VK_NULL_HANDLE; VkDeviceMemory grid_ubo_mem = VK_NULL_HANDLE;
        void* grid_ubo_mapped = nullptr;
        VkDescriptorSetLayout grid_set_layout = VK_NULL_HANDLE;
        VkDescriptorPool grid_pool = VK_NULL_HANDLE;
        VkDescriptorSet grid_set = VK_NULL_HANDLE;
        VkPipelineLayout grid_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline grid_pipeline = VK_NULL_HANDLE;

        // Scene color+depth pass into the swapchain (the grid is coplanar, but the
        // sphere needs real depth). Depth image + framebuffers track the swapchain.
        VkImage scene_depth_image = VK_NULL_HANDLE; VkDeviceMemory scene_depth_mem = VK_NULL_HANDLE;
        VkImageView scene_depth_view = VK_NULL_HANDLE;
        VkRenderPass scene_render_pass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> scene_framebuffers;

        // Lit sphere (Sphere.slang): pos+normal indexed mesh; samples the HDRI
        // cubemap for ambient at binding 1 (shares the geodesic cube).
        VkBuffer sphere_vb = VK_NULL_HANDLE; VkDeviceMemory sphere_vb_mem = VK_NULL_HANDLE;
        VkBuffer sphere_ib = VK_NULL_HANDLE; VkDeviceMemory sphere_ib_mem = VK_NULL_HANDLE;
        uint32_t sphere_index_count = 0;
        static constexpr uint32_t MAX_SCENE_OBJECTS = 64;
        std::vector<SceneObject> scene_objects{ SceneObject{} };  // one default sphere
        int selected_object = 0;
        VkDeviceSize sphere_ubo_stride = 0;  // aligned per-object UBO slot (dynamic offset)
        VkBuffer sphere_ubo = VK_NULL_HANDLE; VkDeviceMemory sphere_ubo_mem = VK_NULL_HANDLE;
        void* sphere_ubo_mapped = nullptr;
        VkDescriptorSetLayout sphere_set_layout = VK_NULL_HANDLE;
        VkDescriptorPool sphere_pool = VK_NULL_HANDLE;
        VkDescriptorSet sphere_set = VK_NULL_HANDLE;
        VkPipelineLayout sphere_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline sphere_pipeline = VK_NULL_HANDLE;

        // Skybox (Skybox.slang): a cube sampling the HDRI cubemap at the far plane
        // (pos.xyww -> depth 1), drawn first as the scene background.
        VkBuffer skybox_vb = VK_NULL_HANDLE; VkDeviceMemory skybox_vb_mem = VK_NULL_HANDLE;
        VkBuffer skybox_ubo = VK_NULL_HANDLE; VkDeviceMemory skybox_ubo_mem = VK_NULL_HANDLE;
        void* skybox_ubo_mapped = nullptr;
        VkDescriptorSetLayout skybox_set_layout = VK_NULL_HANDLE;
        VkDescriptorPool skybox_pool = VK_NULL_HANDLE;
        VkDescriptorSet skybox_set = VK_NULL_HANDLE;
        VkPipelineLayout skybox_pipeline_layout = VK_NULL_HANDLE;
        VkPipeline skybox_pipeline = VK_NULL_HANDLE;

        auto find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags) const -> uint32_t;
        auto create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) const -> bool;
        static auto load_spirv(const std::string& path) -> std::vector<uint32_t>;
        auto create_shader_module(const std::string& path, VkShaderModule& out) const -> bool;

        auto create_instance() -> bool;
        auto pick_physical_and_device() -> bool;
        auto create_swapchain() -> bool;
        auto create_image_views() -> bool;
        auto create_render_pass() -> bool;
        auto create_framebuffers() -> bool;
        auto create_command_buffers() -> bool;
        auto create_sync_objects() -> bool;
        auto create_geodesic_resources() -> bool;
        auto create_geo_target(GeoTarget& target, uint32_t w, uint32_t h) -> bool;
        auto create_hdri_cubemap(const char* path) -> bool;
        auto rebuild_hdri_cubemap(const char* path) -> void;
        auto create_present_resources() -> bool;
        auto create_scene_resources() -> bool;
        auto create_scene_targets() -> bool;
        auto destroy_scene_targets() -> void;
        auto process_input() -> void;
        auto update_geodesic_uniforms() -> void;
        auto update_scene_uniforms() -> void;
        auto destroy_geodesic_resources() -> void;
        auto recreate_swapchain() -> bool;
        auto cleanup_swapchain() -> void;
        auto record_command_buffer(VkCommandBuffer cmd, uint32_t image_index, const glm::vec4& clear, ImDrawData* draw_data) -> bool;
    };

    auto VulkanRenderer::Impl::create_instance() -> bool
    {
#ifdef __APPLE__
        if (!getenv("VK_ICD_FILENAMES"))
            setenv("VK_ICD_FILENAMES", "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json", 0);
        if (!getenv("VK_LAYER_PATH"))
            setenv("VK_LAYER_PATH", "/opt/homebrew/share/vulkan/explicit_layer.d", 0);
        // The Homebrew validation-layer manifest names the dylib without a path;
        // let dlopen find it in the Homebrew lib dir.
        if (!getenv("DYLD_LIBRARY_PATH"))
            setenv("DYLD_LIBRARY_PATH", "/opt/homebrew/lib", 0);
#endif
        VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        app.pApplicationName = "Donut";
        app.apiVersion       = VK_API_VERSION_1_2;

        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        if (!glfwExts) { DONUT_ERROR("Vulkan: GLFW reports no surface support"); return false; }
        std::vector<const char*> exts;
        for (uint32_t i = 0; i < glfwExtCount; ++i) exts.push_back(glfwExts[i]);
        exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        exts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        std::vector<const char*> layers;
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> avail(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, avail.data());
        for (const auto& l : avail)
            if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0)
                layers.push_back("VK_LAYER_KHRONOS_validation");

        VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        ici.flags                   = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        ici.pApplicationInfo        = &app;
        ici.enabledExtensionCount   = (uint32_t)exts.size();
        ici.ppEnabledExtensionNames = exts.data();
        ici.enabledLayerCount       = (uint32_t)layers.size();
        ici.ppEnabledLayerNames     = layers.data();

        VkResult r = vkCreateInstance(&ici, nullptr, &instance);
        if (r != VK_SUCCESS && !layers.empty())
        {
            // The validation layer failed to load (its dylib isn't on the loader
            // search path); it is optional, so retry without it.
            DONUT_WARN("Vulkan: validation layer unavailable, continuing without it");
            layers.clear();
            ici.enabledLayerCount = 0;
            ici.ppEnabledLayerNames = nullptr;
            r = vkCreateInstance(&ici, nullptr, &instance);
        }
        if (r != VK_SUCCESS) { DONUT_ERROR("Vulkan: vkCreateInstance failed ({})", (int)r); return false; }

        VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));
        DONUT_INFO("Vulkan: instance + surface created (validation {})", layers.empty() ? "off" : "on");
        return true;
    }

    auto VulkanRenderer::Impl::pick_physical_and_device() -> bool
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) { DONUT_ERROR("Vulkan: no physical devices"); return false; }
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());
        physical = devices[0];

        uint32_t q_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &q_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfams(q_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &q_count, qfams.data());
        bool found_g = false, found_p = false;
        for (uint32_t i = 0; i < q_count; ++i)
        {
            if (!found_g && (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { graphics_family = i; found_g = true; }
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &present);
            if (!found_p && present) { present_family = i; found_p = true; }
        }
        if (!found_g || !found_p) { DONUT_ERROR("Vulkan: no graphics/present queue"); return false; }

        std::vector<const char*> dev_exts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        uint32_t dev_ext_count = 0;
        vkEnumerateDeviceExtensionProperties(physical, nullptr, &dev_ext_count, nullptr);
        std::vector<VkExtensionProperties> dev_ext_props(dev_ext_count);
        vkEnumerateDeviceExtensionProperties(physical, nullptr, &dev_ext_count, dev_ext_props.data());
        for (const auto& e : dev_ext_props)
            if (std::strcmp(e.extensionName, "VK_KHR_portability_subset") == 0)
                dev_exts.push_back("VK_KHR_portability_subset");

        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> qcis;
        uint32_t families[2] = { graphics_family, present_family };
        for (uint32_t i = 0; i < (graphics_family == present_family ? 1u : 2u); ++i)
        {
            VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
            qci.queueFamilyIndex = families[i];
            qci.queueCount = 1;
            qci.pQueuePriorities = &priority;
            qcis.push_back(qci);
        }
        VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        dci.queueCreateInfoCount    = (uint32_t)qcis.size();
        dci.pQueueCreateInfos       = qcis.data();
        dci.enabledExtensionCount   = (uint32_t)dev_exts.size();
        dci.ppEnabledExtensionNames = dev_exts.data();
        VK_CHECK(vkCreateDevice(physical, &dci, nullptr, &device));
        vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
        vkGetDeviceQueue(device, present_family, 0, &present_queue);

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physical, &props);
        vkGetPhysicalDeviceMemoryProperties(physical, &mem_props);
        DONUT_INFO("Vulkan device: {}", props.deviceName);
        return true;
    }

    auto VulkanRenderer::Impl::create_swapchain() -> bool
    {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);

        uint32_t fmt_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmt_count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmt_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmt_count, formats.data());
        VkSurfaceFormatKHR chosen = formats[0];
        for (const auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                chosen = f;
        swapchain_format = chosen.format;

        if (caps.currentExtent.width != UINT32_MAX)
            swapchain_extent = caps.currentExtent;
        else
        {
            swapchain_extent.width  = std::clamp((uint32_t)width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
            swapchain_extent.height = std::clamp((uint32_t)height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        uint32_t image_count = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
            image_count = caps.maxImageCount;

        VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        sci.surface          = surface;
        sci.minImageCount    = image_count;
        sci.imageFormat      = chosen.format;
        sci.imageColorSpace  = chosen.colorSpace;
        sci.imageExtent      = swapchain_extent;
        sci.imageArrayLayers = 1;
        sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sci.preTransform     = caps.currentTransform;
        sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode      = VK_PRESENT_MODE_FIFO_KHR;   // always supported, vsync
        sci.clipped          = VK_TRUE;

        uint32_t fam_idx[2] = { graphics_family, present_family };
        if (graphics_family != present_family)
        {
            sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            sci.queueFamilyIndexCount = 2;
            sci.pQueueFamilyIndices   = fam_idx;
        }
        else
            sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain));
        uint32_t n = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr);
        images.resize(n);
        vkGetSwapchainImagesKHR(device, swapchain, &n, images.data());
        return true;
    }

    auto VulkanRenderer::Impl::create_image_views() -> bool
    {
        image_views.resize(images.size());
        for (size_t i = 0; i < images.size(); ++i)
        {
            VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            vci.image = images[i];
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = swapchain_format;
            vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            VK_CHECK(vkCreateImageView(device, &vci, nullptr, &image_views[i]));
        }
        return true;
    }

    auto VulkanRenderer::Impl::create_render_pass() -> bool
    {
        VkAttachmentDescription color{};
        color.format = swapchain_format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &ref;

        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1; rpci.pAttachments = &color;
        rpci.subpassCount = 1; rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 1; rpci.pDependencies = &dep;
        VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &render_pass));
        return true;
    }

    auto VulkanRenderer::Impl::create_framebuffers() -> bool
    {
        framebuffers.resize(image_views.size());
        for (size_t i = 0; i < image_views.size(); ++i)
        {
            VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fbci.renderPass = render_pass;
            fbci.attachmentCount = 1; fbci.pAttachments = &image_views[i];
            fbci.width = swapchain_extent.width; fbci.height = swapchain_extent.height; fbci.layers = 1;
            VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &framebuffers[i]));
        }
        return true;
    }

    // Depth image + a color+depth render pass + per-image framebuffers for the
    // scene view. Swapchain-sized, so recreated alongside the swapchain.
    auto VulkanRenderer::Impl::create_scene_targets() -> bool
    {
        const VkFormat depth_fmt = VK_FORMAT_D32_SFLOAT;

        VkImageCreateInfo dici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        dici.imageType = VK_IMAGE_TYPE_2D; dici.format = depth_fmt;
        dici.extent = { swapchain_extent.width, swapchain_extent.height, 1 };
        dici.mipLevels = 1; dici.arrayLayers = 1; dici.samples = VK_SAMPLE_COUNT_1_BIT;
        dici.tiling = VK_IMAGE_TILING_OPTIMAL; dici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        VK_CHECK(vkCreateImage(device, &dici, nullptr, &scene_depth_image));
        VkMemoryRequirements dreq{}; vkGetImageMemoryRequirements(device, scene_depth_image, &dreq);
        VkMemoryAllocateInfo dai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        dai.allocationSize = dreq.size; dai.memoryTypeIndex = find_memory_type(dreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &dai, nullptr, &scene_depth_mem));
        VK_CHECK(vkBindImageMemory(device, scene_depth_image, scene_depth_mem, 0));
        VkImageViewCreateInfo dvci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        dvci.image = scene_depth_image; dvci.viewType = VK_IMAGE_VIEW_TYPE_2D; dvci.format = depth_fmt;
        dvci.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(device, &dvci, nullptr, &scene_depth_view));

        VkAttachmentDescription atts[2]{};
        atts[0].format = swapchain_format; atts[0].samples = VK_SAMPLE_COUNT_1_BIT;
        atts[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; atts[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        atts[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; atts[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; atts[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        atts[1].format = depth_fmt; atts[1].samples = VK_SAMPLE_COUNT_1_BIT;
        atts[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; atts[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; atts[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        atts[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; atts[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference color_ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference depth_ref{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &color_ref; subpass.pDepthStencilAttachment = &depth_ref;
        VkSubpassDependency dep{};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = 0;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 2; rpci.pAttachments = atts; rpci.subpassCount = 1; rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 1; rpci.pDependencies = &dep;
        VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &scene_render_pass));

        scene_framebuffers.resize(image_views.size());
        for (size_t i = 0; i < image_views.size(); ++i)
        {
            VkImageView att[2] = { image_views[i], scene_depth_view };
            VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fbci.renderPass = scene_render_pass; fbci.attachmentCount = 2; fbci.pAttachments = att;
            fbci.width = swapchain_extent.width; fbci.height = swapchain_extent.height; fbci.layers = 1;
            VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &scene_framebuffers[i]));
        }
        return true;
    }

    auto VulkanRenderer::Impl::destroy_scene_targets() -> void
    {
        for (auto fb : scene_framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        scene_framebuffers.clear();
        if (scene_render_pass) { vkDestroyRenderPass(device, scene_render_pass, nullptr); scene_render_pass = VK_NULL_HANDLE; }
        if (scene_depth_view)  { vkDestroyImageView(device, scene_depth_view, nullptr); scene_depth_view = VK_NULL_HANDLE; }
        if (scene_depth_image) { vkDestroyImage(device, scene_depth_image, nullptr); scene_depth_image = VK_NULL_HANDLE; }
        if (scene_depth_mem)   { vkFreeMemory(device, scene_depth_mem, nullptr); scene_depth_mem = VK_NULL_HANDLE; }
    }

    auto VulkanRenderer::Impl::create_command_buffers() -> bool
    {
        VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = graphics_family;
        VK_CHECK(vkCreateCommandPool(device, &pci, nullptr, &command_pool));

        command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandPool = command_pool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        VK_CHECK(vkAllocateCommandBuffers(device, &cbai, command_buffers.data()));
        return true;
    }

    auto VulkanRenderer::Impl::create_sync_objects() -> bool
    {
        image_available.resize(MAX_FRAMES_IN_FLIGHT);
        in_flight.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished.resize(images.size());
        images_in_flight.assign(images.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &image_available[i]));
            VK_CHECK(vkCreateFence(device, &fci, nullptr, &in_flight[i]));
        }
        for (size_t i = 0; i < images.size(); ++i)
            VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &render_finished[i]));
        return true;
    }

    auto VulkanRenderer::Impl::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags) const -> uint32_t
    {
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
            if ((type_filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        return UINT32_MAX;
    }

    auto VulkanRenderer::Impl::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                            VkBuffer& buf, VkDeviceMemory& mem) const -> bool
    {
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = size; bci.usage = usage; bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bci, nullptr, &buf) != VK_SUCCESS) return false;
        VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(device, buf, &req);
        VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
        if (vkAllocateMemory(device, &ai, nullptr, &mem) != VK_SUCCESS) return false;
        vkBindBufferMemory(device, buf, mem, 0);
        return true;
    }

    auto VulkanRenderer::Impl::load_spirv(const std::string& path) -> std::vector<uint32_t>
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) return {};
        size_t size = (size_t)file.tellg();
        std::vector<uint32_t> data(size / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }

    auto VulkanRenderer::Impl::create_shader_module(const std::string& path, VkShaderModule& out) const -> bool
    {
        auto spv = load_spirv(path);
        if (spv.empty()) { DONUT_ERROR("Vulkan: failed to load SPIR-V {}", path); return false; }
        VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        ci.codeSize = spv.size() * 4; ci.pCode = spv.data();
        return vkCreateShaderModule(device, &ci, nullptr, &out) == VK_SUCCESS;
    }

    // Creates one geodesic render target (image + memory + view + framebuffer)
    // at w x h against the shared geo_render_pass. Used for the low- and high-res
    // progressive targets.
    auto VulkanRenderer::Impl::create_geo_target(GeoTarget& t, uint32_t w, uint32_t h) -> bool
    {
        const VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;
        t.w = w; t.h = h;

        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D; ici.format = fmt; ici.extent = { w, h, 1 };
        ici.mipLevels = 1; ici.arrayLayers = 1; ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK(vkCreateImage(device, &ici, nullptr, &t.image));
        VkMemoryRequirements im_req{}; vkGetImageMemoryRequirements(device, t.image, &im_req);
        VkMemoryAllocateInfo im_alloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        im_alloc.allocationSize = im_req.size;
        im_alloc.memoryTypeIndex = find_memory_type(im_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &im_alloc, nullptr, &t.mem));
        VK_CHECK(vkBindImageMemory(device, t.image, t.mem, 0));
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = t.image; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = fmt;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(device, &vci, nullptr, &t.view));
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = geo_render_pass; fbci.attachmentCount = 1; fbci.pAttachments = &t.view;
        fbci.width = w; fbci.height = h; fbci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &t.fb));
        return true;
    }

    auto VulkanRenderer::Impl::create_geodesic_resources() -> bool
    {
        const VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;
        const VkMemoryPropertyFlags host_vis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const float SagA_rs = 1.269e10f;

        VkAttachmentDescription color{};
        color.format = fmt; color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &ref;
        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL; deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass = 0; deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1; rpci.pAttachments = &color;
        rpci.subpassCount = 1; rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 2; rpci.pDependencies = deps;
        VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &geo_render_pass));

        if (!create_geo_target(geo_lo, GEO_LO_W, GEO_LO_H)) return false;
        if (!create_geo_target(geo_hi, GEO_HI_W, GEO_HI_H)) return false;

        create_buffer(128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, cam_buf, cam_mem);
        create_buffer(32,  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, disk_buf, disk_mem);
        create_buffer(800, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, obj_buf, obj_mem);
        create_buffer(16,  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, sim_buf, sim_mem);

        camera.set_camera_mode(CameraMode::Orbital);
        camera.set_orbital_target(glm::vec3(0.0f));
        camera.set_orbital_radius(4e11);          // ~31 r_s: outside the 12 r_s disk
        camera.set_orbital_limits(2.2e11, 1.5e12);
        camera.set_orbital_speed(0.01f);
        camera.set_zoom_speed(3e10);
        camera.set_azimuth(0.0f);
        camera.set_elevation(1.25f);

        void* p = nullptr;
        vkMapMemory(device, cam_mem, 0, 128, 0, &cam_mapped);  // camera UBO is refilled every frame

        // disk_r1=inner (ISCO 3 r_s), disk_r2=outer, disk_num=turbulence strength,
        // thickness (unused by the thin-disk model), disk_density=exposure.
        float disk_data[8] = { SagA_rs * 3.0f, SagA_rs * 12.0f, 0.4f, SagA_rs * 0.1f, 0.1f, 0, 0, 0 };
        vkMapMemory(device, disk_mem, 0, 32, 0, &p); memcpy(p, disk_data, sizeof(disk_data)); vkUnmapMemory(device, disk_mem);

        std::vector<uint8_t> obj_data(800, 0);
        int num_objects = 1; memcpy(obj_data.data(), &num_objects, 4);
        float pos_radius[4] = { 0, 0, 0, SagA_rs }; memcpy(obj_data.data() + 16, pos_radius, 16);
        float obj_color[4] = { 0, 0, 0, 1 };        memcpy(obj_data.data() + 272, obj_color, 16);
        vkMapMemory(device, obj_mem, 0, 800, 0, &p); memcpy(p, obj_data.data(), 800); vkUnmapMemory(device, obj_mem);

        vkMapMemory(device, sim_mem, 0, 16, 0, &sim_mapped);

        if (!create_hdri_cubemap("assets/hdri/HDR_blue_nebulae-1.hdr")) return false;

        VkDescriptorSetLayoutBinding binds[5]{};
        for (int i = 0; i < 4; ++i) { binds[i].binding = i; binds[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; binds[i].descriptorCount = 1; binds[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; }
        binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = 5; dslci.pBindings = binds;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &geo_set_layout));
        VkDescriptorPoolSize psizes[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.maxSets = 1; dpci.poolSizeCount = 2; dpci.pPoolSizes = psizes;
        VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &geo_pool));
        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dsai.descriptorPool = geo_pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &geo_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &geo_set));
        VkDescriptorBufferInfo bi[4] = { { cam_buf, 0, VK_WHOLE_SIZE }, { disk_buf, 0, VK_WHOLE_SIZE }, { obj_buf, 0, VK_WHOLE_SIZE }, { sim_buf, 0, VK_WHOLE_SIZE } };
        VkDescriptorImageInfo cube_info{ cube_sampler, cube_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet writes[5]{};
        for (int i = 0; i < 4; ++i) { writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[i].dstSet = geo_set; writes[i].dstBinding = i; writes[i].descriptorCount = 1; writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[i].pBufferInfo = &bi[i]; }
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[4].dstSet = geo_set; writes[4].dstBinding = 4; writes[4].descriptorCount = 1; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[4].pImageInfo = &cube_info;
        vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);

        float quad[] = {
            -1.f,  1.f, 0.f, 1.f,  -1.f, -1.f, 0.f, 0.f,   1.f, -1.f, 1.f, 0.f,
            -1.f,  1.f, 0.f, 1.f,   1.f, -1.f, 1.f, 0.f,   1.f,  1.f, 1.f, 1.f,
        };
        create_buffer(sizeof(quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host_vis, quad_vb, quad_vb_mem);
        vkMapMemory(device, quad_vb_mem, 0, sizeof(quad), 0, &p); memcpy(p, quad, sizeof(quad)); vkUnmapMemory(device, quad_vb_mem);

        VkShaderModule vmod, fmod;
        if (!create_shader_module("assets/shaders/generated/Geodesic.vertexMain.spv", vmod)) return false;
        if (!create_shader_module("assets/shaders/generated/Geodesic.fragmentMain.spv", fmod)) return false;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1; plci.pSetLayouts = &geo_set_layout;
        VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &geo_pipeline_layout));
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";
        VkVertexInputBindingDescription vib{ 0, 16, VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription via[2] = { { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R32G32_SFLOAT, 8 } };
        VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &vib;
        vin.vertexAttributeDescriptionCount = 2; vin.pVertexAttributeDescriptions = via;
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Dynamic viewport/scissor: the same pipeline renders into either the
        // low- or high-res target, sized per frame in record_command_buffer.
        VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; vps.viewportCount = 1; vps.scissorCount = 1;
        VkDynamicState dyn_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO }; dyn.dynamicStateCount = 2; dyn.pDynamicStates = dyn_states;
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; cb.attachmentCount = 1; cb.pAttachments = &cba;
        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2; gpci.pStages = stages;
        gpci.pVertexInputState = &vin; gpci.pInputAssemblyState = &ia; gpci.pViewportState = &vps;
        gpci.pRasterizationState = &rs; gpci.pMultisampleState = &ms; gpci.pColorBlendState = &cb;
        gpci.pDynamicState = &dyn;
        gpci.layout = geo_pipeline_layout; gpci.renderPass = geo_render_pass; gpci.subpass = 0;
        VkResult pr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &geo_pipeline);
        vkDestroyShaderModule(device, vmod, nullptr); vkDestroyShaderModule(device, fmod, nullptr);
        if (pr != VK_SUCCESS) { DONUT_ERROR("Vulkan: geodesic pipeline creation failed ({})", (int)pr); return false; }

        start_time = glfwGetTime();
        update_geodesic_uniforms();
        DONUT_INFO("Vulkan: geodesic resources ready ({}x{} moving / {}x{} settled)",
                   (int)GEO_LO_W, (int)GEO_LO_H, (int)GEO_HI_W, (int)GEO_HI_H);
        return true;
    }

    auto VulkanRenderer::Impl::create_hdri_cubemap(const char* path) -> bool
    {
        hdri_path = path;
        const VkMemoryPropertyFlags host_vis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const uint32_t FACE = 1024;
        const VkFormat cube_fmt = VK_FORMAT_R16G16B16A16_SFLOAT;

        VkImageCreateInfo cci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        cci.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        cci.imageType = VK_IMAGE_TYPE_2D; cci.format = cube_fmt; cci.extent = { FACE, FACE, 1 };
        cci.mipLevels = 1; cci.arrayLayers = 6; cci.samples = VK_SAMPLE_COUNT_1_BIT;
        cci.tiling = VK_IMAGE_TILING_OPTIMAL;
        cci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VK_CHECK(vkCreateImage(device, &cci, nullptr, &cube_image));
        VkMemoryRequirements creq{}; vkGetImageMemoryRequirements(device, cube_image, &creq);
        VkMemoryAllocateInfo cai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        cai.allocationSize = creq.size; cai.memoryTypeIndex = find_memory_type(creq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &cai, nullptr, &cube_mem));
        VK_CHECK(vkBindImageMemory(device, cube_image, cube_mem, 0));
        VkImageViewCreateInfo cvci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        cvci.image = cube_image; cvci.viewType = VK_IMAGE_VIEW_TYPE_CUBE; cvci.format = cube_fmt;
        cvci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
        VK_CHECK(vkCreateImageView(device, &cvci, nullptr, &cube_view));
        VkSamplerCreateInfo csm{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        csm.magFilter = VK_FILTER_LINEAR; csm.minFilter = VK_FILTER_LINEAR;
        csm.addressModeU = csm.addressModeV = csm.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(device, &csm, nullptr, &cube_sampler));

        int w = 0, h = 0, ch = 0;
        float* pixels = stbi_loadf(path, &w, &h, &ch, 4);
        if (!pixels)
        {
            DONUT_WARN("Vulkan: HDRI '{}' could not be loaded; using a dark background", path);
            VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            cbai.commandPool = command_pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
            VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(device, &cbai, &cmd));
            VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &bi);
            VkImageMemoryBarrier tb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            tb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; tb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            tb.image = cube_image; tb.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
            tb.srcAccessMask = 0; tb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &tb);
            VkClearColorValue dark{}; dark.float32[0] = 0.02f; dark.float32[1] = 0.02f; dark.float32[2] = 0.05f; dark.float32[3] = 1.0f;
            VkImageSubresourceRange rng{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
            vkCmdClearColorImage(cmd, cube_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dark, 1, &rng);
            VkImageMemoryBarrier rb = tb; rb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; rb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; rb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &rb);
            vkEndCommandBuffer(cmd);
            VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO }; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
            vkQueueSubmit(graphics_queue, 1, &si, VK_NULL_HANDLE); vkQueueWaitIdle(graphics_queue);
            vkFreeCommandBuffers(device, command_pool, 1, &cmd);
            return true;
        }

        // Apple GPUs can't linearly filter RGBA32F, so store the equirect as
        // RGBA16F (convert the loaded floats to half on the way into staging).
        const VkFormat eq_fmt = VK_FORMAT_R16G16B16A16_SFLOAT;
        size_t texel_count = (size_t)w * h * 4;
        VkDeviceSize eq_size = (VkDeviceSize)texel_count * sizeof(uint16_t);
        VkBuffer eq_staging; VkDeviceMemory eq_staging_mem;
        if (!create_buffer(eq_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host_vis, eq_staging, eq_staging_mem)) { stbi_image_free(pixels); return false; }
        void* mp = nullptr; vkMapMemory(device, eq_staging_mem, 0, eq_size, 0, &mp);
        uint16_t* dst = (uint16_t*)mp;
        for (size_t i = 0; i < texel_count; ++i) { __fp16 hf = (__fp16)pixels[i]; memcpy(&dst[i], &hf, sizeof(uint16_t)); }
        vkUnmapMemory(device, eq_staging_mem);
        stbi_image_free(pixels);

        VkImage eq_image; VkDeviceMemory eq_mem;
        VkImageCreateInfo eci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        eci.imageType = VK_IMAGE_TYPE_2D; eci.format = eq_fmt; eci.extent = { (uint32_t)w, (uint32_t)h, 1 };
        eci.mipLevels = 1; eci.arrayLayers = 1; eci.samples = VK_SAMPLE_COUNT_1_BIT;
        eci.tiling = VK_IMAGE_TILING_OPTIMAL; eci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK(vkCreateImage(device, &eci, nullptr, &eq_image));
        VkMemoryRequirements ereq{}; vkGetImageMemoryRequirements(device, eq_image, &ereq);
        VkMemoryAllocateInfo eai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        eai.allocationSize = ereq.size; eai.memoryTypeIndex = find_memory_type(ereq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &eai, nullptr, &eq_mem));
        VK_CHECK(vkBindImageMemory(device, eq_image, eq_mem, 0));
        VkImageView eq_view;
        VkImageViewCreateInfo evci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        evci.image = eq_image; evci.viewType = VK_IMAGE_VIEW_TYPE_2D; evci.format = eq_fmt;
        evci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(device, &evci, nullptr, &eq_view));
        VkSampler eq_sampler;
        VkSamplerCreateInfo esm{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        esm.magFilter = VK_FILTER_LINEAR; esm.minFilter = VK_FILTER_LINEAR;
        esm.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;         // longitude wraps
        esm.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // latitude clamps
        esm.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(device, &esm, nullptr, &eq_sampler));

        VkImageView face_views[6];
        for (uint32_t i = 0; i < 6; ++i)
        {
            VkImageViewCreateInfo fvci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            fvci.image = cube_image; fvci.viewType = VK_IMAGE_VIEW_TYPE_2D; fvci.format = cube_fmt;
            fvci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1 };
            VK_CHECK(vkCreateImageView(device, &fvci, nullptr, &face_views[i]));
        }

        VkAttachmentDescription color{};
        color.format = cube_fmt; color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &ref;
        VkSubpassDependency dep{}; dep.srcSubpass = 0; dep.dstSubpass = VK_SUBPASS_EXTERNAL;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        VkRenderPass rp;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1; rpci.pAttachments = &color; rpci.subpassCount = 1; rpci.pSubpasses = &subpass; rpci.dependencyCount = 1; rpci.pDependencies = &dep;
        VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &rp));
        VkFramebuffer face_fb[6];
        for (uint32_t i = 0; i < 6; ++i)
        {
            VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fbci.renderPass = rp; fbci.attachmentCount = 1; fbci.pAttachments = &face_views[i]; fbci.width = FACE; fbci.height = FACE; fbci.layers = 1;
            VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &face_fb[i]));
        }

        VkDescriptorSetLayoutBinding binds[2]{};
        binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; binds[1].descriptorCount = 1; binds[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayout set_layout;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; dslci.bindingCount = 2; dslci.pBindings = binds;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &set_layout));
        VkDescriptorPoolSize psizes[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6 } };
        VkDescriptorPool pool;
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO }; dpci.maxSets = 6; dpci.poolSizeCount = 2; dpci.pPoolSizes = psizes;
        VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &pool));

        VkShaderModule vmod, fmod;
        if (!create_shader_module("assets/shaders/generated/EquirectToCubemap.vertexMain.spv", vmod)) return false;
        if (!create_shader_module("assets/shaders/generated/EquirectToCubemap.fragmentMain.spv", fmod)) return false;
        VkPipelineLayout playout;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO }; plci.setLayoutCount = 1; plci.pSetLayouts = &set_layout;
        VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &playout));
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";
        VkVertexInputBindingDescription vib{ 0, 12, VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription via{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
        VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &vib; vin.vertexAttributeDescriptionCount = 1; vin.pVertexAttributeDescriptions = &via;
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp{ 0, 0, (float)FACE, (float)FACE, 0, 1 }; VkRect2D sc{ { 0, 0 }, { FACE, FACE } };
        VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; vps.viewportCount = 1; vps.pViewports = &vp; vps.scissorCount = 1; vps.pScissors = &sc;
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; cb.attachmentCount = 1; cb.pAttachments = &cba;
        VkPipeline pipeline;
        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2; gpci.pStages = stages; gpci.pVertexInputState = &vin; gpci.pInputAssemblyState = &ia; gpci.pViewportState = &vps;
        gpci.pRasterizationState = &rs; gpci.pMultisampleState = &ms; gpci.pColorBlendState = &cb; gpci.layout = playout; gpci.renderPass = rp; gpci.subpass = 0;
        VkResult pr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline);
        vkDestroyShaderModule(device, vmod, nullptr); vkDestroyShaderModule(device, fmod, nullptr);
        if (pr != VK_SUCCESS) { DONUT_ERROR("Vulkan: equirect pipeline failed ({})", (int)pr); return false; }

        float cube_verts[] = {
            -1,1,-1, -1,-1,-1, 1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
            -1,-1,1, -1,-1,-1, -1,1,-1, -1,1,-1, -1,1,1, -1,-1,1,
             1,-1,-1, 1,-1,1, 1,1,1, 1,1,1, 1,1,-1, 1,-1,-1,
            -1,-1,1, -1,1,1, 1,1,1, 1,1,1, 1,-1,1, -1,-1,1,
            -1,1,-1, 1,1,-1, 1,1,1, 1,1,1, -1,1,1, -1,1,-1,
            -1,-1,-1, -1,-1,1, 1,-1,-1, 1,-1,-1, -1,-1,1, 1,-1,1,
        };
        VkBuffer cube_vb; VkDeviceMemory cube_vb_mem;
        create_buffer(sizeof(cube_verts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host_vis, cube_vb, cube_vb_mem);
        vkMapMemory(device, cube_vb_mem, 0, sizeof(cube_verts), 0, &mp); memcpy(mp, cube_verts, sizeof(cube_verts)); vkUnmapMemory(device, cube_vb_mem);

        glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        proj[1][1] *= -1.0f;  // Vulkan clip space is Y-down vs OpenGL
        glm::mat4 views[6] = {
            glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0, -1, 0)),
            glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
            glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
            glm::lookAt(glm::vec3(0), glm::vec3( 0, -1, 0), glm::vec3(0, 0, -1)),
            glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0, -1, 0)),
            glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, -1), glm::vec3(0, -1, 0)),
        };
        VkBuffer ubo[6]; VkDeviceMemory ubo_mem[6]; VkDescriptorSet sets[6];
        for (uint32_t i = 0; i < 6; ++i)
        {
            create_buffer(128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, ubo[i], ubo_mem[i]);
            glm::mat4 mats[2] = { glm::transpose(proj), glm::transpose(views[i]) };  // SPIR-V expects row-major
            vkMapMemory(device, ubo_mem[i], 0, 128, 0, &mp); memcpy(mp, mats, 128); vkUnmapMemory(device, ubo_mem[i]);
            VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO }; dsai.descriptorPool = pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &set_layout;
            VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &sets[i]));
            VkDescriptorBufferInfo buf_info{ ubo[i], 0, VK_WHOLE_SIZE };
            VkDescriptorImageInfo img_info{ eq_sampler, eq_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet ws[2]{};
            ws[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; ws[0].dstSet = sets[i]; ws[0].dstBinding = 0; ws[0].descriptorCount = 1; ws[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; ws[0].pBufferInfo = &buf_info;
            ws[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; ws[1].dstSet = sets[i]; ws[1].dstBinding = 1; ws[1].descriptorCount = 1; ws[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; ws[1].pImageInfo = &img_info;
            vkUpdateDescriptorSets(device, 2, ws, 0, nullptr);
        }

        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandPool = command_pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(device, &cbai, &cmd));
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
        VkImageMemoryBarrier to_dst{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        to_dst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; to_dst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_dst.image = eq_image; to_dst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        to_dst.srcAccessMask = 0; to_dst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_dst);
        VkBufferImageCopy copy{}; copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; copy.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
        vkCmdCopyBufferToImage(cmd, eq_staging, eq_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        VkImageMemoryBarrier to_read = to_dst; to_read.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; to_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        to_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; to_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_read);

        VkClearValue clear{}; clear.color = { { 0, 0, 0, 1 } };
        for (uint32_t i = 0; i < 6; ++i)
        {
            VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            rpbi.renderPass = rp; rpbi.framebuffer = face_fb[i]; rpbi.renderArea = { { 0, 0 }, { FACE, FACE } }; rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;
            vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, playout, 0, 1, &sets[i], 0, nullptr);
            VkDeviceSize off = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &cube_vb, &off);
            vkCmdDraw(cmd, 36, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }
        VK_CHECK(vkEndCommandBuffer(cmd));
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO }; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(graphics_queue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(graphics_queue));

        vkFreeCommandBuffers(device, command_pool, 1, &cmd);
        for (uint32_t i = 0; i < 6; ++i) { vkDestroyBuffer(device, ubo[i], nullptr); vkFreeMemory(device, ubo_mem[i], nullptr); vkDestroyFramebuffer(device, face_fb[i], nullptr); vkDestroyImageView(device, face_views[i], nullptr); }
        vkDestroyBuffer(device, cube_vb, nullptr); vkFreeMemory(device, cube_vb_mem, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr); vkDestroyPipelineLayout(device, playout, nullptr);
        vkDestroyDescriptorPool(device, pool, nullptr); vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
        vkDestroyRenderPass(device, rp, nullptr);
        vkDestroySampler(device, eq_sampler, nullptr); vkDestroyImageView(device, eq_view, nullptr);
        vkDestroyImage(device, eq_image, nullptr); vkFreeMemory(device, eq_mem, nullptr);
        vkDestroyBuffer(device, eq_staging, nullptr); vkFreeMemory(device, eq_staging_mem, nullptr);
        DONUT_INFO("Vulkan: HDRI cubemap built from {} ({}x{} equirect -> {}^2 cube)", path, w, h, (int)FACE);
        return true;
    }

    // Runtime HDRI switch: drain the device, tear down the old cubemap, build the
    // new one, and repoint the geodesic set's samplerCube (binding 4) at it.
    auto VulkanRenderer::Impl::rebuild_hdri_cubemap(const char* path) -> void
    {
        vkDeviceWaitIdle(device);

        if (cube_sampler) vkDestroySampler(device, cube_sampler, nullptr);
        if (cube_view)    vkDestroyImageView(device, cube_view, nullptr);
        if (cube_image)   vkDestroyImage(device, cube_image, nullptr);
        if (cube_mem)     vkFreeMemory(device, cube_mem, nullptr);
        cube_sampler = VK_NULL_HANDLE; cube_view = VK_NULL_HANDLE;
        cube_image   = VK_NULL_HANDLE; cube_mem  = VK_NULL_HANDLE;

        create_hdri_cubemap(path);

        VkDescriptorImageInfo cube_info{ cube_sampler, cube_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = geo_set; write.dstBinding = 4; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &cube_info;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        // The scene sphere samples the same cube (binding 1) — repoint it too.
        if (sphere_set)
        {
            VkWriteDescriptorSet sw{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            sw.dstSet = sphere_set; sw.dstBinding = 1; sw.descriptorCount = 1;
            sw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sw.pImageInfo = &cube_info;
            vkUpdateDescriptorSets(device, 1, &sw, 0, nullptr);
        }
        if (skybox_set)
        {
            VkWriteDescriptorSet kw{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            kw.dstSet = skybox_set; kw.dstBinding = 1; kw.descriptorCount = 1;
            kw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; kw.pImageInfo = &cube_info;
            vkUpdateDescriptorSets(device, 1, &kw, 0, nullptr);
        }
    }

    auto VulkanRenderer::Impl::create_present_resources() -> bool
    {
        VkSamplerCreateInfo smci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        smci.magFilter = VK_FILTER_LINEAR; smci.minFilter = VK_FILTER_LINEAR;
        smci.addressModeU = smci.addressModeV = smci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(device, &smci, nullptr, &present_sampler));

        VkDescriptorSetLayoutBinding bind{}; bind.binding = 0; bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; bind.descriptorCount = 1; bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = 1; dslci.pBindings = &bind;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &present_set_layout));
        // One present set per geodesic target (low-/high-res), each sampling its
        // own image; record_command_buffer binds whichever was rendered this frame.
        VkDescriptorPoolSize psize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 };
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.maxSets = 2; dpci.poolSizeCount = 1; dpci.pPoolSizes = &psize;
        VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &present_pool));
        for (GeoTarget* t : { &geo_lo, &geo_hi })
        {
            VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
            dsai.descriptorPool = present_pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &present_set_layout;
            VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &t->present_set));
            VkDescriptorImageInfo ii{ present_sampler, t->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
            write.dstSet = t->present_set; write.dstBinding = 0; write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &ii;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }

        VkShaderModule vmod, fmod;
        if (!create_shader_module("assets/shaders/generated/TexturedQuad.vertexMain.spv", vmod)) return false;
        if (!create_shader_module("assets/shaders/generated/TexturedQuad.fragmentMain.spv", fmod)) return false;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1; plci.pSetLayouts = &present_set_layout;
        VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &present_pipeline_layout));
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";
        VkVertexInputBindingDescription vib{ 0, 16, VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription via[2] = { { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R32G32_SFLOAT, 8 } };
        VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &vib;
        vin.vertexAttributeDescriptionCount = 2; vin.pVertexAttributeDescriptions = via;
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; vps.viewportCount = 1; vps.scissorCount = 1;
        VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dsci{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO }; dsci.dynamicStateCount = 2; dsci.pDynamicStates = dyn;
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; cb.attachmentCount = 1; cb.pAttachments = &cba;
        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2; gpci.pStages = stages;
        gpci.pVertexInputState = &vin; gpci.pInputAssemblyState = &ia; gpci.pViewportState = &vps;
        gpci.pDynamicState = &dsci;
        gpci.pRasterizationState = &rs; gpci.pMultisampleState = &ms; gpci.pColorBlendState = &cb;
        gpci.layout = present_pipeline_layout; gpci.renderPass = render_pass; gpci.subpass = 0;
        VkResult pr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &present_pipeline);
        vkDestroyShaderModule(device, vmod, nullptr); vkDestroyShaderModule(device, fmod, nullptr);
        if (pr != VK_SUCCESS) { DONUT_ERROR("Vulkan: present pipeline creation failed ({})", (int)pr); return false; }
        DONUT_INFO("Vulkan: present pipeline ready");
        return true;
    }

    auto VulkanRenderer::Impl::create_scene_resources() -> bool
    {
        const VkMemoryPropertyFlags host_vis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        // Scene camera: normal-scale orbital viewer (matches the OpenGL world-builder).
        scene_camera.set_camera_mode(CameraMode::Orbital);
        scene_camera.set_orbital_target(glm::vec3(0.0f));
        scene_camera.set_orbital_radius(15.0);
        scene_camera.set_orbital_limits(2.0, 200.0);
        scene_camera.set_orbital_speed(0.01f);
        scene_camera.set_zoom_speed(2.0);
        scene_camera.set_azimuth(0.0f);
        scene_camera.set_elevation((float)std::numbers::pi / 3.0f);
        scene_camera.update_orbital();

        // Line grid on the XZ plane (+/-50 units, 1-unit cells). Grid.slang scales
        // the position by u_GridSize/50, so u_GridSize = 50 keeps it 1:1.
        std::vector<glm::vec3> lines;
        const int N = 50;
        for (int i = -N; i <= N; ++i)
        {
            lines.push_back({ (float)i, 0.0f, (float)-N });
            lines.push_back({ (float)i, 0.0f, (float) N });
            lines.push_back({ (float)-N, 0.0f, (float)i });
            lines.push_back({ (float) N, 0.0f, (float)i });
        }
        grid_vertex_count = (uint32_t)lines.size();
        VkDeviceSize vbsize = lines.size() * sizeof(glm::vec3);
        if (!create_buffer(vbsize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host_vis, grid_vb, grid_vb_mem)) return false;
        void* mp = nullptr; vkMapMemory(device, grid_vb_mem, 0, vbsize, 0, &mp); memcpy(mp, lines.data(), vbsize); vkUnmapMemory(device, grid_vb_mem);

        // $Globals UBO (set 0, binding 0), std140, 176 bytes; refilled each frame.
        if (!create_buffer(176, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, grid_ubo, grid_ubo_mem)) return false;
        vkMapMemory(device, grid_ubo_mem, 0, 176, 0, &grid_ubo_mapped);

        VkDescriptorSetLayoutBinding b{}; b.binding = 0; b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; b.descriptorCount = 1; b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; dslci.bindingCount = 1; dslci.pBindings = &b;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &grid_set_layout));
        VkDescriptorPoolSize psize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO }; dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &psize;
        VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &grid_pool));
        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO }; dsai.descriptorPool = grid_pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &grid_set_layout;
        VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &grid_set));
        VkDescriptorBufferInfo bi{ grid_ubo, 0, VK_WHOLE_SIZE };
        VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }; w.dstSet = grid_set; w.dstBinding = 0; w.descriptorCount = 1; w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);

        VkShaderModule vmod, fmod;
        if (!create_shader_module("assets/shaders/generated/Grid.vertexMain.spv", vmod)) return false;
        if (!create_shader_module("assets/shaders/generated/Grid.fragmentMain.spv", fmod)) return false;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO }; plci.setLayoutCount = 1; plci.pSetLayouts = &grid_set_layout;
        VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &grid_pipeline_layout));
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";
        VkVertexInputBindingDescription vib{ 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription via{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
        VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &vib;
        vin.vertexAttributeDescriptionCount = 1; vin.pVertexAttributeDescriptions = &via;
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; ia.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; vps.viewportCount = 1; vps.scissorCount = 1;
        VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dsci{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO }; dsci.dynamicStateCount = 2; dsci.pDynamicStates = dyn;
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo ds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_FALSE; ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // transparent lines: test but don't write
        VkPipelineColorBlendAttachmentState cba{};
        cba.blendEnable = VK_TRUE;
        cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; cba.colorBlendOp = VK_BLEND_OP_ADD;
        cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; cba.alphaBlendOp = VK_BLEND_OP_ADD;
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; cb.attachmentCount = 1; cb.pAttachments = &cba;
        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2; gpci.pStages = stages;
        gpci.pVertexInputState = &vin; gpci.pInputAssemblyState = &ia; gpci.pViewportState = &vps;
        gpci.pDynamicState = &dsci;
        gpci.pRasterizationState = &rs; gpci.pMultisampleState = &ms; gpci.pColorBlendState = &cb; gpci.pDepthStencilState = &ds;
        gpci.layout = grid_pipeline_layout; gpci.renderPass = scene_render_pass; gpci.subpass = 0;
        VkResult pr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &grid_pipeline);
        vkDestroyShaderModule(device, vmod, nullptr); vkDestroyShaderModule(device, fmod, nullptr);
        if (pr != VK_SUCCESS) { DONUT_ERROR("Vulkan: grid pipeline creation failed ({})", (int)pr); return false; }

        // Lit sphere: unit UV-sphere mesh (pos+normal, stride 24), placed/scaled by
        // u_Transform; samples the geodesic HDRI cubemap for ambient (binding 1).
        {
            std::vector<float> sv; std::vector<uint32_t> si;
            const int RINGS = 24, SECTORS = 48;
            for (int r = 0; r <= RINGS; ++r)
            {
                float phi = (float)std::numbers::pi * r / RINGS;
                for (int s = 0; s <= SECTORS; ++s)
                {
                    float theta = 2.0f * (float)std::numbers::pi * s / SECTORS;
                    float x = sinf(phi) * cosf(theta), y = cosf(phi), z = sinf(phi) * sinf(theta);
                    sv.push_back(x); sv.push_back(y); sv.push_back(z);   // position (unit)
                    sv.push_back(x); sv.push_back(y); sv.push_back(z);   // normal == position
                }
            }
            for (int r = 0; r < RINGS; ++r)
                for (int s = 0; s < SECTORS; ++s)
                {
                    uint32_t a = r * (SECTORS + 1) + s, b = a + SECTORS + 1;
                    si.push_back(a); si.push_back(b); si.push_back(a + 1);
                    si.push_back(b); si.push_back(b + 1); si.push_back(a + 1);
                }
            sphere_index_count = (uint32_t)si.size();
            VkDeviceSize svsz = sv.size() * sizeof(float), sisz = si.size() * sizeof(uint32_t);
            if (!create_buffer(svsz, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host_vis, sphere_vb, sphere_vb_mem)) return false;
            if (!create_buffer(sisz, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  host_vis, sphere_ib, sphere_ib_mem)) return false;
            void* sp = nullptr;
            vkMapMemory(device, sphere_vb_mem, 0, svsz, 0, &sp); memcpy(sp, sv.data(), svsz); vkUnmapMemory(device, sphere_vb_mem);
            vkMapMemory(device, sphere_ib_mem, 0, sisz, 0, &sp); memcpy(sp, si.data(), sisz); vkUnmapMemory(device, sphere_ib_mem);

            VkPhysicalDeviceProperties pdp{}; vkGetPhysicalDeviceProperties(physical, &pdp);
            VkDeviceSize min_align = pdp.limits.minUniformBufferOffsetAlignment;
            sphere_ubo_stride = ((208 + min_align - 1) / min_align) * min_align;  // per-object slot
            VkDeviceSize sphere_ubo_size = sphere_ubo_stride * MAX_SCENE_OBJECTS;
            if (!create_buffer(sphere_ubo_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, sphere_ubo, sphere_ubo_mem)) return false;
            vkMapMemory(device, sphere_ubo_mem, 0, sphere_ubo_size, 0, &sphere_ubo_mapped);

            VkDescriptorSetLayoutBinding sb[2]{};
            sb[0].binding = 0; sb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; sb[0].descriptorCount = 1; sb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            sb[1].binding = 1; sb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sb[1].descriptorCount = 1; sb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo sdslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; sdslci.bindingCount = 2; sdslci.pBindings = sb;
            VK_CHECK(vkCreateDescriptorSetLayout(device, &sdslci, nullptr, &sphere_set_layout));
            VkDescriptorPoolSize sps[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
            VkDescriptorPoolCreateInfo sdpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO }; sdpci.maxSets = 1; sdpci.poolSizeCount = 2; sdpci.pPoolSizes = sps;
            VK_CHECK(vkCreateDescriptorPool(device, &sdpci, nullptr, &sphere_pool));
            VkDescriptorSetAllocateInfo sdsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO }; sdsai.descriptorPool = sphere_pool; sdsai.descriptorSetCount = 1; sdsai.pSetLayouts = &sphere_set_layout;
            VK_CHECK(vkAllocateDescriptorSets(device, &sdsai, &sphere_set));
            VkDescriptorBufferInfo sbi{ sphere_ubo, 0, 208 };  // per-draw size; the dynamic offset selects the object slot
            VkDescriptorImageInfo sii{ cube_sampler, cube_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet sw[2]{};
            sw[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; sw[0].dstSet = sphere_set; sw[0].dstBinding = 0; sw[0].descriptorCount = 1; sw[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; sw[0].pBufferInfo = &sbi;
            sw[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; sw[1].dstSet = sphere_set; sw[1].dstBinding = 1; sw[1].descriptorCount = 1; sw[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; sw[1].pImageInfo = &sii;
            vkUpdateDescriptorSets(device, 2, sw, 0, nullptr);

            VkShaderModule svmod, sfmod;
            if (!create_shader_module("assets/shaders/generated/Sphere.vertexMain.spv", svmod)) return false;
            if (!create_shader_module("assets/shaders/generated/Sphere.fragmentMain.spv", sfmod)) return false;
            VkPipelineLayoutCreateInfo splci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO }; splci.setLayoutCount = 1; splci.pSetLayouts = &sphere_set_layout;
            VK_CHECK(vkCreatePipelineLayout(device, &splci, nullptr, &sphere_pipeline_layout));
            VkPipelineShaderStageCreateInfo sstages[2]{};
            sstages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; sstages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   sstages[0].module = svmod; sstages[0].pName = "main";
            sstages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; sstages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; sstages[1].module = sfmod; sstages[1].pName = "main";
            VkVertexInputBindingDescription svib{ 0, 6 * (uint32_t)sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };
            VkVertexInputAttributeDescription svia[2] = { { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * (uint32_t)sizeof(float) } };
            VkPipelineVertexInputStateCreateInfo svin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            svin.vertexBindingDescriptionCount = 1; svin.pVertexBindingDescriptions = &svib;
            svin.vertexAttributeDescriptionCount = 2; svin.pVertexAttributeDescriptions = svia;
            VkPipelineInputAssemblyStateCreateInfo sia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; sia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo svps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; svps.viewportCount = 1; svps.scissorCount = 1;
            VkDynamicState sdyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo sdsci{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO }; sdsci.dynamicStateCount = 2; sdsci.pDynamicStates = sdyn;
            VkPipelineRasterizationStateCreateInfo srs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; srs.polygonMode = VK_POLYGON_MODE_FILL; srs.cullMode = VK_CULL_MODE_NONE; srs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; srs.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo sms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; sms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo sds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
            sds.depthTestEnable = VK_TRUE; sds.depthWriteEnable = VK_TRUE; sds.depthCompareOp = VK_COMPARE_OP_LESS;
            VkPipelineColorBlendAttachmentState scba{}; scba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo scb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; scb.attachmentCount = 1; scb.pAttachments = &scba;
            VkGraphicsPipelineCreateInfo sgpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            sgpci.stageCount = 2; sgpci.pStages = sstages;
            sgpci.pVertexInputState = &svin; sgpci.pInputAssemblyState = &sia; sgpci.pViewportState = &svps;
            sgpci.pDynamicState = &sdsci;
            sgpci.pRasterizationState = &srs; sgpci.pMultisampleState = &sms; sgpci.pColorBlendState = &scb; sgpci.pDepthStencilState = &sds;
            sgpci.layout = sphere_pipeline_layout; sgpci.renderPass = scene_render_pass; sgpci.subpass = 0;
            VkResult spr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &sgpci, nullptr, &sphere_pipeline);
            vkDestroyShaderModule(device, svmod, nullptr); vkDestroyShaderModule(device, sfmod, nullptr);
            if (spr != VK_SUCCESS) { DONUT_ERROR("Vulkan: sphere pipeline creation failed ({})", (int)spr); return false; }
        }

        // Skybox: a unit cube (36 verts) sampling the HDRI cubemap; the vertex
        // shader forces depth 1 (pos.xyww) so it sits behind all scene geometry.
        {
            const float cube[] = {
                -1,-1,-1,  1,-1,-1,  1, 1,-1,  1, 1,-1, -1, 1,-1, -1,-1,-1,
                -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
                -1, 1, 1, -1, 1,-1, -1,-1,-1, -1,-1,-1, -1,-1, 1, -1, 1, 1,
                 1, 1, 1,  1, 1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1,  1, 1, 1,
                -1,-1,-1,  1,-1,-1,  1,-1, 1,  1,-1, 1, -1,-1, 1, -1,-1,-1,
                -1, 1,-1,  1, 1,-1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1, 1,-1
            };
            if (!create_buffer(sizeof(cube), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, host_vis, skybox_vb, skybox_vb_mem)) return false;
            void* kp = nullptr; vkMapMemory(device, skybox_vb_mem, 0, sizeof(cube), 0, &kp); memcpy(kp, cube, sizeof(cube)); vkUnmapMemory(device, skybox_vb_mem);

            // $Globals UBO (128 B: u_Projection@0, u_View@64); u_Skybox at binding 1.
            if (!create_buffer(128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host_vis, skybox_ubo, skybox_ubo_mem)) return false;
            vkMapMemory(device, skybox_ubo_mem, 0, 128, 0, &skybox_ubo_mapped);

            VkDescriptorSetLayoutBinding kb[2]{};
            kb[0].binding = 0; kb[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; kb[0].descriptorCount = 1; kb[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            kb[1].binding = 1; kb[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; kb[1].descriptorCount = 1; kb[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo kdslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; kdslci.bindingCount = 2; kdslci.pBindings = kb;
            VK_CHECK(vkCreateDescriptorSetLayout(device, &kdslci, nullptr, &skybox_set_layout));
            VkDescriptorPoolSize kps[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
            VkDescriptorPoolCreateInfo kdpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO }; kdpci.maxSets = 1; kdpci.poolSizeCount = 2; kdpci.pPoolSizes = kps;
            VK_CHECK(vkCreateDescriptorPool(device, &kdpci, nullptr, &skybox_pool));
            VkDescriptorSetAllocateInfo kdsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO }; kdsai.descriptorPool = skybox_pool; kdsai.descriptorSetCount = 1; kdsai.pSetLayouts = &skybox_set_layout;
            VK_CHECK(vkAllocateDescriptorSets(device, &kdsai, &skybox_set));
            VkDescriptorBufferInfo kbi{ skybox_ubo, 0, VK_WHOLE_SIZE };
            VkDescriptorImageInfo kii{ cube_sampler, cube_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet kw[2]{};
            kw[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; kw[0].dstSet = skybox_set; kw[0].dstBinding = 0; kw[0].descriptorCount = 1; kw[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; kw[0].pBufferInfo = &kbi;
            kw[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; kw[1].dstSet = skybox_set; kw[1].dstBinding = 1; kw[1].descriptorCount = 1; kw[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; kw[1].pImageInfo = &kii;
            vkUpdateDescriptorSets(device, 2, kw, 0, nullptr);

            VkShaderModule kvmod, kfmod;
            if (!create_shader_module("assets/shaders/generated/Skybox.vertexMain.spv", kvmod)) return false;
            if (!create_shader_module("assets/shaders/generated/Skybox.fragmentMain.spv", kfmod)) return false;
            VkPipelineLayoutCreateInfo kplci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO }; kplci.setLayoutCount = 1; kplci.pSetLayouts = &skybox_set_layout;
            VK_CHECK(vkCreatePipelineLayout(device, &kplci, nullptr, &skybox_pipeline_layout));
            VkPipelineShaderStageCreateInfo kstages[2]{};
            kstages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; kstages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   kstages[0].module = kvmod; kstages[0].pName = "main";
            kstages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; kstages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; kstages[1].module = kfmod; kstages[1].pName = "main";
            VkVertexInputBindingDescription kvib{ 0, 3 * (uint32_t)sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };
            VkVertexInputAttributeDescription kvia{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
            VkPipelineVertexInputStateCreateInfo kvin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
            kvin.vertexBindingDescriptionCount = 1; kvin.pVertexBindingDescriptions = &kvib;
            kvin.vertexAttributeDescriptionCount = 1; kvin.pVertexAttributeDescriptions = &kvia;
            VkPipelineInputAssemblyStateCreateInfo kia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; kia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo kvps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; kvps.viewportCount = 1; kvps.scissorCount = 1;
            VkDynamicState kdyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
            VkPipelineDynamicStateCreateInfo kdsci{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO }; kdsci.dynamicStateCount = 2; kdsci.pDynamicStates = kdyn;
            VkPipelineRasterizationStateCreateInfo krs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; krs.polygonMode = VK_POLYGON_MODE_FILL; krs.cullMode = VK_CULL_MODE_NONE; krs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; krs.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo kms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; kms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo kds{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
            kds.depthTestEnable = VK_FALSE; kds.depthWriteEnable = VK_FALSE; // background: neither tests nor writes depth
            VkPipelineColorBlendAttachmentState kcba{}; kcba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo kcb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; kcb.attachmentCount = 1; kcb.pAttachments = &kcba;
            VkGraphicsPipelineCreateInfo kgpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            kgpci.stageCount = 2; kgpci.pStages = kstages;
            kgpci.pVertexInputState = &kvin; kgpci.pInputAssemblyState = &kia; kgpci.pViewportState = &kvps;
            kgpci.pDynamicState = &kdsci;
            kgpci.pRasterizationState = &krs; kgpci.pMultisampleState = &kms; kgpci.pColorBlendState = &kcb; kgpci.pDepthStencilState = &kds;
            kgpci.layout = skybox_pipeline_layout; kgpci.renderPass = scene_render_pass; kgpci.subpass = 0;
            VkResult kpr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &kgpci, nullptr, &skybox_pipeline);
            vkDestroyShaderModule(device, kvmod, nullptr); vkDestroyShaderModule(device, kfmod, nullptr);
            if (kpr != VK_SUCCESS) { DONUT_ERROR("Vulkan: skybox pipeline creation failed ({})", (int)kpr); return false; }
        }

        DONUT_INFO("Vulkan: scene resources ready ({} grid verts, {} sphere indices, skybox)", grid_vertex_count, sphere_index_count);
        return true;
    }

    auto VulkanRenderer::Impl::update_scene_uniforms() -> void
    {
        struct GridUBO {
            glm::mat4 view_projection;    // 0    (SPIR-V RowMajor -> upload transposed)
            glm::mat4 transform;          // 64
            float grid_size; float p0[3]; // 128
            glm::vec3 grid_color;         // 144
            float grid_alpha;             // 156
            glm::vec3 camera_pos;         // 160
            float p1;                     // 172
        } g{};
        static_assert(sizeof(GridUBO) == 176, "GridUBO std140 layout mismatch");

        uint32_t h = swapchain_extent.height ? swapchain_extent.height : 1;
        float aspect = (float)swapchain_extent.width / (float)h;
        scene_camera.set_projection(45.0f, aspect, 0.1f, 1000.0f);
        glm::mat4 vp = scene_camera.get_projection_matrix() * scene_camera.get_view_matrix();

        // Slang's mul(M,v) + the SPIR-V RowMajor decoration means glm's column-major
        // matrices upload directly here (no transpose) to read as the intended M.
        g.view_projection = vp;
        g.transform       = glm::mat4(1.0f);
        g.grid_size       = 50.0f;
        g.grid_color      = glm::vec3(0.55f, 0.55f, 0.6f);
        g.grid_alpha      = 0.75f;
        g.camera_pos      = scene_camera.get_orbital_position();
        if (grid_ubo_mapped) memcpy(grid_ubo_mapped, &g, sizeof(g));

        struct SphereUBO {
            glm::mat4 view_projection;                    // 0
            glm::mat4 transform;                          // 64
            glm::vec3 color; float specular;              // 128, 140
            float emission; float p0[3];                  // 144
            glm::vec3 light_pos; float p1;                // 160, 172
            glm::vec3 camera_pos; int is_selected;        // 176, 188
            glm::vec3 outline_color; float outline_width; // 192, 204
        };
        static_assert(sizeof(SphereUBO) == 208, "SphereUBO std140 layout mismatch");
        glm::vec3 cam_pos = scene_camera.get_orbital_position();
        uint32_t obj_count = (uint32_t)std::min(scene_objects.size(), (size_t)MAX_SCENE_OBJECTS);
        for (uint32_t i = 0; i < obj_count; ++i)
        {
            const SceneObject& o = scene_objects[i];
            SphereUBO s{};
            s.view_projection = vp;  // same no-transpose rule as the grid
            s.transform       = glm::translate(glm::mat4(1.0f), o.position) * glm::scale(glm::mat4(1.0f), glm::vec3(o.radius));
            s.color           = o.color;
            s.specular        = 0.6f;
            s.emission        = 0.0f;
            s.light_pos       = glm::vec3(10.0f, 20.0f, 10.0f);
            s.camera_pos      = cam_pos;
            s.is_selected     = ((int)i == selected_object) ? 1 : 0;
            s.outline_color   = glm::vec3(1.0f, 1.0f, 0.0f);
            s.outline_width   = 0.15f;
            if (sphere_ubo_mapped) memcpy((char*)sphere_ubo_mapped + (VkDeviceSize)i * sphere_ubo_stride, &s, sizeof(s));
        }

        struct SkyboxUBO { glm::mat4 projection; glm::mat4 view; } sky{};
        static_assert(sizeof(SkyboxUBO) == 128, "SkyboxUBO std140 layout mismatch");
        sky.projection = scene_camera.get_projection_matrix();                 // no transpose (mul(M,v))
        sky.view       = glm::mat4(glm::mat3(scene_camera.get_view_matrix())); // strip translation so the sky stays centered
        if (skybox_ubo_mapped) memcpy(skybox_ubo_mapped, &sky, sizeof(sky));
    }

    auto VulkanRenderer::Impl::update_geodesic_uniforms() -> void
    {
        struct CamUBO {
            glm::vec3 pos; float p0; glm::vec3 right; float p1;
            glm::vec3 up; float p2; glm::vec3 fwd; float p3;
            float tan_half_fov; float aspect; uint32_t moving; int p4;
        } cam_data{};
        glm::vec3 pos, fwd;
        if (camera.get_camera_mode() == CameraMode::FPS)
        {
            pos = camera.get_position();
            fwd = camera.get_forward_direction();
        }
        else
        {
            pos = camera.get_orbital_position();
            fwd = glm::normalize(camera.get_orbital_target() - pos);
        }
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        cam_data.pos = pos; cam_data.right = right; cam_data.up = glm::cross(right, fwd); cam_data.fwd = fwd;
        cam_data.tan_half_fov = (float)tan(glm::radians(60.0f * 0.5f));
        cam_data.aspect = (float)GEO_HI_W / (float)GEO_HI_H;  // 16:9, same for both targets
        cam_data.moving = user_moving ? 1u : 0u;
        if (cam_mapped) memcpy(cam_mapped, &cam_data, sizeof(cam_data));

        // Integration budget drives how deep the geodesics are traced: too few
        // steps and grazing rays exhaust the loop mid-lensing, so the shader
        // paints them with the HDRI (Geodesic.slang) and the hole looks sliced
        // off "up to a certain depth" -- and near face-on it vanishes entirely.
        // Measured: at ~8000 steps the disk disappears at steep poses, ~15000 is
        // needed for it to resolve, INDEPENDENT of resolution. So we DON'T drop
        // steps while moving (that would make the hole flicker out mid-drag);
        // responsiveness comes from the lower-res target instead (see record).
        // Sourced from settings.toml (max_steps_static), capped GPU-safe.
        constexpr int kStepCeil = 15000;
        struct SimUBO { int steps_moving; int steps_static; float early_exit; float time; } sim;
        sim.steps_static = std::clamp(SettingsManager::get_max_steps_static(), 1000, kStepCeil);
        sim.steps_moving = sim.steps_static;  // same budget both frames; resolution is the lever
        sim.early_exit   = SettingsManager::get_early_exit_distance();
        sim.time = (float)(glfwGetTime() - start_time);
        if (sim_mapped) memcpy(sim_mapped, &sim, sizeof(sim));
    }

    static double g_ScrollAccum = 0.0;
    static GLFWscrollfun g_PrevScroll = nullptr;
    static void donut_vk_scroll_callback(GLFWwindow* w, double x, double y)
    {
        if (g_PrevScroll) g_PrevScroll(w, x, y);  // keep ImGui's scroll handling intact
        g_ScrollAccum += y;
    }

    auto VulkanRenderer::Impl::process_input() -> void
    {
        bool over_ui = imgui_init && ImGui::GetIO().WantCaptureMouse;

        double now = glfwGetTime();
        float dt = last_frame_time > 0.0 ? (float)(now - last_frame_time) : 0.0f;
        last_frame_time = now;

        double mx = 0, my = 0;
        glfwGetCursorPos(window, &mx, &my);
        bool left_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

        if (!scene_mode && camera.get_camera_mode() == CameraMode::FPS)
        {
            // Left-drag looks around (screen-up looks up); WASD/QE move.
            if (left_down && !left_was_down) { fps_last_x = mx; fps_last_y = my; }
            if (left_down && !over_ui)
                camera.on_mouse_move(float(mx - fps_last_x), float(fps_last_y - my));
            fps_last_x = mx; fps_last_y = my;
            left_was_down = left_down;

            bool over_kb = imgui_init && ImGui::GetIO().WantCaptureKeyboard;
            bool moved = false;
            if (!over_kb && dt > 0.0f)
            {
                float mdt = dt * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 4.0f : 1.0f);
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camera.move_forward(mdt);  moved = true; }
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camera.move_backward(mdt); moved = true; }
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camera.move_left(mdt);     moved = true; }
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camera.move_right(mdt);    moved = true; }
                if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) { camera.move_up(mdt);       moved = true; }
                if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) { camera.move_down(mdt);     moved = true; }
            }
            g_ScrollAccum = 0.0;  // scroll unused in free-fly
            user_moving = moved || (left_down && !over_ui);
        }
        else
        {
            Camera& cam = scene_mode ? scene_camera : camera;
            if (left_down && !left_was_down && !over_ui)
                cam.process_orbital_mouse_button(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
            else if (!left_down && left_was_down)
                cam.process_orbital_mouse_button(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
            left_was_down = left_down;

            cam.process_orbital_mouse_move(mx, my);  // orbits only while dragging

            double scroll = g_ScrollAccum; g_ScrollAccum = 0.0;
            if (scroll != 0.0 && !over_ui)
                cam.process_orbital_scroll(0.0, scroll);

            user_moving = cam.is_dragging() || cam.is_panning();
        }
    }

    auto VulkanRenderer::Impl::destroy_geodesic_resources() -> void
    {
        if (grid_pipeline) vkDestroyPipeline(device, grid_pipeline, nullptr);
        if (grid_pipeline_layout) vkDestroyPipelineLayout(device, grid_pipeline_layout, nullptr);
        if (grid_pool) vkDestroyDescriptorPool(device, grid_pool, nullptr);
        if (grid_set_layout) vkDestroyDescriptorSetLayout(device, grid_set_layout, nullptr);
        if (grid_ubo_mapped) { vkUnmapMemory(device, grid_ubo_mem); grid_ubo_mapped = nullptr; }
        if (grid_ubo) vkDestroyBuffer(device, grid_ubo, nullptr);
        if (grid_ubo_mem) vkFreeMemory(device, grid_ubo_mem, nullptr);
        if (grid_vb) vkDestroyBuffer(device, grid_vb, nullptr);
        if (grid_vb_mem) vkFreeMemory(device, grid_vb_mem, nullptr);

        if (sphere_pipeline) vkDestroyPipeline(device, sphere_pipeline, nullptr);
        if (sphere_pipeline_layout) vkDestroyPipelineLayout(device, sphere_pipeline_layout, nullptr);
        if (sphere_pool) vkDestroyDescriptorPool(device, sphere_pool, nullptr);
        if (sphere_set_layout) vkDestroyDescriptorSetLayout(device, sphere_set_layout, nullptr);
        if (sphere_ubo_mapped) { vkUnmapMemory(device, sphere_ubo_mem); sphere_ubo_mapped = nullptr; }
        if (sphere_ubo) vkDestroyBuffer(device, sphere_ubo, nullptr);
        if (sphere_ubo_mem) vkFreeMemory(device, sphere_ubo_mem, nullptr);
        if (sphere_ib) vkDestroyBuffer(device, sphere_ib, nullptr);
        if (sphere_ib_mem) vkFreeMemory(device, sphere_ib_mem, nullptr);
        if (sphere_vb) vkDestroyBuffer(device, sphere_vb, nullptr);
        if (sphere_vb_mem) vkFreeMemory(device, sphere_vb_mem, nullptr);

        if (skybox_pipeline) vkDestroyPipeline(device, skybox_pipeline, nullptr);
        if (skybox_pipeline_layout) vkDestroyPipelineLayout(device, skybox_pipeline_layout, nullptr);
        if (skybox_pool) vkDestroyDescriptorPool(device, skybox_pool, nullptr);
        if (skybox_set_layout) vkDestroyDescriptorSetLayout(device, skybox_set_layout, nullptr);
        if (skybox_ubo_mapped) { vkUnmapMemory(device, skybox_ubo_mem); skybox_ubo_mapped = nullptr; }
        if (skybox_ubo) vkDestroyBuffer(device, skybox_ubo, nullptr);
        if (skybox_ubo_mem) vkFreeMemory(device, skybox_ubo_mem, nullptr);
        if (skybox_vb) vkDestroyBuffer(device, skybox_vb, nullptr);
        if (skybox_vb_mem) vkFreeMemory(device, skybox_vb_mem, nullptr);

        if (present_pipeline) vkDestroyPipeline(device, present_pipeline, nullptr);
        if (present_pipeline_layout) vkDestroyPipelineLayout(device, present_pipeline_layout, nullptr);
        if (present_pool) vkDestroyDescriptorPool(device, present_pool, nullptr);
        if (present_set_layout) vkDestroyDescriptorSetLayout(device, present_set_layout, nullptr);
        if (present_sampler) vkDestroySampler(device, present_sampler, nullptr);

        if (geo_pipeline) vkDestroyPipeline(device, geo_pipeline, nullptr);
        if (geo_pipeline_layout) vkDestroyPipelineLayout(device, geo_pipeline_layout, nullptr);
        if (geo_pool) vkDestroyDescriptorPool(device, geo_pool, nullptr);
        if (geo_set_layout) vkDestroyDescriptorSetLayout(device, geo_set_layout, nullptr);
        if (quad_vb) vkDestroyBuffer(device, quad_vb, nullptr);
        if (quad_vb_mem) vkFreeMemory(device, quad_vb_mem, nullptr);
        if (cube_sampler) vkDestroySampler(device, cube_sampler, nullptr);
        if (cube_view) vkDestroyImageView(device, cube_view, nullptr);
        if (cube_image) vkDestroyImage(device, cube_image, nullptr);
        if (cube_mem) vkFreeMemory(device, cube_mem, nullptr);
        if (cam_mapped) { vkUnmapMemory(device, cam_mem); cam_mapped = nullptr; }
        if (sim_mapped) { vkUnmapMemory(device, sim_mem); sim_mapped = nullptr; }
        VkBuffer ubos[4] = { cam_buf, disk_buf, obj_buf, sim_buf };
        VkDeviceMemory umem[4] = { cam_mem, disk_mem, obj_mem, sim_mem };
        for (int i = 0; i < 4; ++i) { if (ubos[i]) vkDestroyBuffer(device, ubos[i], nullptr); if (umem[i]) vkFreeMemory(device, umem[i], nullptr); }
        for (GeoTarget* t : { &geo_lo, &geo_hi })
        {
            if (t->fb)    vkDestroyFramebuffer(device, t->fb, nullptr);
            if (t->view)  vkDestroyImageView(device, t->view, nullptr);
            if (t->image) vkDestroyImage(device, t->image, nullptr);
            if (t->mem)   vkFreeMemory(device, t->mem, nullptr);
        }
        if (geo_render_pass) vkDestroyRenderPass(device, geo_render_pass, nullptr);
    }

    auto VulkanRenderer::Impl::record_command_buffer(VkCommandBuffer cmd, uint32_t image_index, const glm::vec4& clear, ImDrawData* draw_data) -> bool
    {
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

        if (scene_mode)
        {
            // Scene view: opaque sphere (writes depth) then the transparent grid on
            // top, into a color+depth swapchain pass, then ImGui.
            VkClearValue cvs[2]{};
            cvs[0].color = { { clear.r, clear.g, clear.b, clear.a } };
            cvs[1].depthStencil = { 1.0f, 0 };
            VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            rpbi.renderPass = scene_render_pass; rpbi.framebuffer = scene_framebuffers[image_index];
            rpbi.renderArea = { { 0, 0 }, swapchain_extent };
            rpbi.clearValueCount = 2; rpbi.pClearValues = cvs;
            vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

            // Negative-height viewport flips Y so the scene reads like the GL path.
            VkViewport gvp{ 0, (float)swapchain_extent.height, (float)swapchain_extent.width, -(float)swapchain_extent.height, 0, 1 };
            VkRect2D gsc{ { 0, 0 }, swapchain_extent };
            vkCmdSetViewport(cmd, 0, 1, &gvp);
            vkCmdSetScissor(cmd, 0, 1, &gsc);

            // Skybox background first (forces depth 1; no depth test/write).
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline_layout, 0, 1, &skybox_set, 0, nullptr);
            VkDeviceSize skoff = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &skybox_vb, &skoff);
            vkCmdDraw(cmd, 36, 1, 0, 0);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sphere_pipeline);
            VkDeviceSize soff = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &sphere_vb, &soff);
            vkCmdBindIndexBuffer(cmd, sphere_ib, 0, VK_INDEX_TYPE_UINT32);
            uint32_t obj_count = (uint32_t)std::min(scene_objects.size(), (size_t)MAX_SCENE_OBJECTS);
            for (uint32_t i = 0; i < obj_count; ++i)
            {
                uint32_t dyn = (uint32_t)((VkDeviceSize)i * sphere_ubo_stride);  // select this object's UBO slot
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sphere_pipeline_layout, 0, 1, &sphere_set, 1, &dyn);
                vkCmdDrawIndexed(cmd, sphere_index_count, 1, 0, 0, 0);
            }

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grid_pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, grid_pipeline_layout, 0, 1, &grid_set, 0, nullptr);
            VkDeviceSize goff = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &grid_vb, &goff);
            vkCmdDraw(cmd, grid_vertex_count, 1, 0, 0);

            if (draw_data)
                ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
            vkCmdEndRenderPass(cmd);
            VK_CHECK(vkEndCommandBuffer(cmd));
            return true;
        }

        // Geodesic offscreen pass. Progressive resolution: low-res while the
        // camera moves (4x fewer pixels -> responsive), high-res once it settles
        // (resolves the photon ring). Step count is the same for both (the disk
        // vanishes below ~15000 steps at steep poses), so resolution is the lever.
        GeoTarget& gt = user_moving ? geo_lo : geo_hi;
        VkClearValue geo_clear{}; geo_clear.color = { { 0, 0, 0, 1 } };
        VkRenderPassBeginInfo grp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        grp.renderPass = geo_render_pass; grp.framebuffer = gt.fb;
        grp.renderArea = { { 0, 0 }, { gt.w, gt.h } };
        grp.clearValueCount = 1; grp.pClearValues = &geo_clear;
        vkCmdBeginRenderPass(cmd, &grp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, geo_pipeline);
        VkViewport gvp{ 0, 0, (float)gt.w, (float)gt.h, 0, 1 }; VkRect2D gsc{ { 0, 0 }, { gt.w, gt.h } };
        vkCmdSetViewport(cmd, 0, 1, &gvp); vkCmdSetScissor(cmd, 0, 1, &gsc);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, geo_pipeline_layout, 0, 1, &geo_set, 0, nullptr);
        VkDeviceSize off = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &quad_vb, &off);
        vkCmdDraw(cmd, 6, 1, 0, 0);
        vkCmdEndRenderPass(cmd);

        // Swapchain pass: upscale the geodesic image, then the ImGui UI on top
        VkClearValue cv{}; cv.color = { { clear.r, clear.g, clear.b, clear.a } };
        VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpbi.renderPass = render_pass;
        rpbi.framebuffer = framebuffers[image_index];
        rpbi.renderArea = { { 0, 0 }, swapchain_extent };
        rpbi.clearValueCount = 1; rpbi.pClearValues = &cv;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, present_pipeline);
        // Negative-height viewport flips the geodesic image vertically so the scene
        // reads the same as the OpenGL path (Vulkan's clip space is Y-down). Only
        // this draw is affected; ImGui sets its own viewport.
        VkViewport vp{ 0, (float)swapchain_extent.height, (float)swapchain_extent.width, -(float)swapchain_extent.height, 0, 1 };
        VkRect2D scissor{ { 0, 0 }, swapchain_extent };
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, present_pipeline_layout, 0, 1, &gt.present_set, 0, nullptr);
        vkCmdBindVertexBuffers(cmd, 0, 1, &quad_vb, &off);
        vkCmdDraw(cmd, 6, 1, 0, 0);
        if (draw_data)
            ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
        vkCmdEndRenderPass(cmd);

        VK_CHECK(vkEndCommandBuffer(cmd));
        return true;
    }

    auto VulkanRenderer::Impl::cleanup_swapchain() -> void
    {
        destroy_scene_targets();
        for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        framebuffers.clear();
        for (auto iv : image_views) vkDestroyImageView(device, iv, nullptr);
        image_views.clear();
        if (render_pass) { vkDestroyRenderPass(device, render_pass, nullptr); render_pass = VK_NULL_HANDLE; }
        if (swapchain)  { vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain = VK_NULL_HANDLE; }
    }

    auto VulkanRenderer::Impl::recreate_swapchain() -> bool
    {
        // Wait until the window has a non-zero size (e.g. after un-minimizing).
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        while (w == 0 || h == 0)
        {
            glfwGetFramebufferSize(window, &w, &h);
            glfwWaitEvents();
        }
        width = w; height = h;
        vkDeviceWaitIdle(device);

        cleanup_swapchain();
        // render_finished are tied to image count; recreate below via sync if it changed.
        if (!create_swapchain())   return false;
        if (!create_image_views())  return false;
        if (!create_render_pass())  return false;
        if (!create_framebuffers())return false;
        if (!create_scene_targets())return false;
        images_in_flight.assign(images.size(), VK_NULL_HANDLE);
        return true;
    }

    VulkanRenderer::VulkanRenderer()  { m_impl = new Impl(); }
    VulkanRenderer::~VulkanRenderer() { shutdown(); delete m_impl; m_impl = nullptr; }

    auto VulkanRenderer::init(void* glfwWindow, int width, int height) -> bool
    {
        Impl& v = *m_impl;
        v.window = (GLFWwindow*)glfwWindow;
        v.width = width; v.height = height;

        if (!v.create_instance())        return false;
        if (!v.pick_physical_and_device()) return false;
        if (!v.create_swapchain())       return false;
        if (!v.create_image_views())      return false;
        if (!v.create_render_pass())      return false;
        if (!v.create_framebuffers())    return false;
        if (!v.create_scene_targets())    return false;
        if (!v.create_command_buffers())  return false;
        if (!v.create_sync_objects())     return false;
        if (!v.create_geodesic_resources()) return false;
        if (!v.create_present_resources())  return false;
        if (!v.create_scene_resources())    return false;

        DONUT_INFO("Vulkan renderer ready: {} swapchain images, {}x{}",
                   (int)v.images.size(), v.swapchain_extent.width, v.swapchain_extent.height);
        return true;
    }

    auto VulkanRenderer::init_im_gui() -> bool
    {
        Impl& v = *m_impl;
        if (v.device == VK_NULL_HANDLE) return false;

        VkDescriptorPoolSize pool_size{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 };
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpci.maxSets = 1000;
        dpci.poolSizeCount = 1; dpci.pPoolSizes = &pool_size;
        VK_CHECK(vkCreateDescriptorPool(v.device, &dpci, nullptr, &v.imgui_pool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(v.window, true);
        g_PrevScroll = glfwSetScrollCallback(v.window, donut_vk_scroll_callback);  // chain ImGui + camera zoom
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion     = VK_API_VERSION_1_2;
        info.Instance       = v.instance;
        info.PhysicalDevice = v.physical;
        info.Device         = v.device;
        info.QueueFamily    = v.graphics_family;
        info.Queue          = v.graphics_queue;
        info.DescriptorPool = v.imgui_pool;
        info.RenderPass     = v.render_pass;
        info.MinImageCount  = 2;
        info.ImageCount     = (uint32_t)v.images.size();
        info.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;
        if (!ImGui_ImplVulkan_Init(&info))
        {
            DONUT_ERROR("Vulkan: ImGui_ImplVulkan_Init failed");
            return false;
        }

        v.imgui_init = true;
        DONUT_INFO("Vulkan: ImGui backend initialized");
        return true;
    }

    auto VulkanRenderer::on_resize(int width, int height) -> void
    {
        m_impl->framebuffer_resized = true;
        m_impl->width = width; m_impl->height = height;
    }

    auto VulkanRenderer::set_hdri(const std::string& path) -> void
    {
        Impl& v = *m_impl;
        if (v.device == VK_NULL_HANDLE || v.geo_set == VK_NULL_HANDLE) return;
        if (v.hdri_path == path) return;
        v.rebuild_hdri_cubemap(path.c_str());
    }

    auto VulkanRenderer::current_hdri() const -> const std::string&
    {
        return m_impl->hdri_path;
    }

    auto VulkanRenderer::set_free_fly(bool enabled) -> void
    {
        Impl& v = *m_impl;
        if (v.device == VK_NULL_HANDLE) return;
        const bool is_fps = v.camera.get_camera_mode() == CameraMode::FPS;
        if (enabled == is_fps) return;

        if (enabled)
        {
            // Seed the fly pose from the current orbital framing so the view is continuous.
            glm::vec3 pos = v.camera.get_orbital_position();
            glm::vec3 fwd = glm::normalize(v.camera.get_orbital_target() - pos);
            float pitch = glm::degrees(asin(glm::clamp(fwd.y, -1.0f, 1.0f)));
            float yaw   = glm::degrees(atan2(fwd.z, fwd.x));
            v.camera.set_camera_mode(CameraMode::FPS);
            v.camera.set_movement_speed(2.0e10f);   // scene spans ~1e11 units
            v.camera.set_mouse_sensitivity(0.15f);
            v.camera.set_position(pos);
            v.camera.set_rotation(glm::vec3(pitch, yaw, 0.0f));
        }
        else
        {
            v.camera.set_camera_mode(CameraMode::Orbital);  // orbital state was left intact
        }
    }

    auto VulkanRenderer::is_free_fly() const -> bool
    {
        return m_impl->camera.get_camera_mode() == CameraMode::FPS;
    }

    auto VulkanRenderer::set_scene_mode(bool enabled) -> void
    {
        if (m_impl) m_impl->scene_mode = enabled;
    }

    auto VulkanRenderer::is_scene_mode() const -> bool
    {
        return m_impl && m_impl->scene_mode;
    }

    auto VulkanRenderer::scene_objects() -> std::vector<SceneObject>&
    {
        return m_impl->scene_objects;
    }

    auto VulkanRenderer::set_selected_object(int index) -> void
    {
        if (m_impl) m_impl->selected_object = index;
    }

    auto VulkanRenderer::draw_frame(const glm::vec4& clear_color, const std::function<void()>& build_ui) -> void
    {
        Impl& v = *m_impl;
        if (v.device == VK_NULL_HANDLE) return;

        ImDrawData* draw_data = nullptr;
        if (v.imgui_init)
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            if (build_ui) build_ui();
            ImGui::Render();
            draw_data = ImGui::GetDrawData();
        }

        vkWaitForFences(v.device, 1, &v.in_flight[v.current_frame], VK_TRUE, UINT64_MAX);

        uint32_t image_index = 0;
        VkResult r = vkAcquireNextImageKHR(v.device, v.swapchain, UINT64_MAX,
                                           v.image_available[v.current_frame], VK_NULL_HANDLE, &image_index);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) { v.recreate_swapchain(); return; }
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) { DONUT_ERROR("Vulkan: acquire failed ({})", (int)r); return; }

        if (v.images_in_flight[image_index] != VK_NULL_HANDLE)
            vkWaitForFences(v.device, 1, &v.images_in_flight[image_index], VK_TRUE, UINT64_MAX);
        v.images_in_flight[image_index] = v.in_flight[v.current_frame];

        // The geodesic offscreen image is shared across frames in flight; wait for
        // the previous frame to finish reading it before overwriting it this frame.
        if (v.geo_in_use != VK_NULL_HANDLE)
            vkWaitForFences(v.device, 1, &v.geo_in_use, VK_TRUE, UINT64_MAX);
        v.process_input();
        if (v.scene_mode) v.update_scene_uniforms(); else v.update_geodesic_uniforms();

        vkResetCommandBuffer(v.command_buffers[v.current_frame], 0);
        if (!v.record_command_buffer(v.command_buffers[v.current_frame], image_index, clear_color, draw_data)) return;

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = &v.image_available[v.current_frame];
        submit.pWaitDstStageMask    = &wait_stage;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &v.command_buffers[v.current_frame];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = &v.render_finished[image_index];

        vkResetFences(v.device, 1, &v.in_flight[v.current_frame]);
        if (vkQueueSubmit(v.graphics_queue, 1, &submit, v.in_flight[v.current_frame]) != VK_SUCCESS)
        { DONUT_ERROR("Vulkan: queue submit failed"); return; }
        v.geo_in_use = v.in_flight[v.current_frame];

        VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = &v.render_finished[image_index];
        present.swapchainCount     = 1;
        present.pSwapchains        = &v.swapchain;
        present.pImageIndices      = &image_index;
        r = vkQueuePresentKHR(v.present_queue, &present);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || v.framebuffer_resized)
        {
            v.framebuffer_resized = false;
            v.recreate_swapchain();
        }

        v.current_frame = (v.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    auto VulkanRenderer::shutdown() -> void
    {
        Impl& v = *m_impl;
        if (v.device == VK_NULL_HANDLE) { if (v.instance && v.surface) { vkDestroySurfaceKHR(v.instance, v.surface, nullptr); v.surface = VK_NULL_HANDLE; } if (v.instance) { vkDestroyInstance(v.instance, nullptr); v.instance = VK_NULL_HANDLE; } return; }

        vkDeviceWaitIdle(v.device);
        v.destroy_geodesic_resources();
        if (v.imgui_init)
        {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            v.imgui_init = false;
        }
        if (v.imgui_pool) { vkDestroyDescriptorPool(v.device, v.imgui_pool, nullptr); v.imgui_pool = VK_NULL_HANDLE; }
        for (auto s : v.render_finished) vkDestroySemaphore(v.device, s, nullptr);
        for (auto s : v.image_available) vkDestroySemaphore(v.device, s, nullptr);
        for (auto f : v.in_flight)       vkDestroyFence(v.device, f, nullptr);
        v.render_finished.clear(); v.image_available.clear(); v.in_flight.clear();
        if (v.command_pool) { vkDestroyCommandPool(v.device, v.command_pool, nullptr); v.command_pool = VK_NULL_HANDLE; }
        v.cleanup_swapchain();
        vkDestroyDevice(v.device, nullptr); v.device = VK_NULL_HANDLE;
        if (v.surface)  { vkDestroySurfaceKHR(v.instance, v.surface, nullptr); v.surface = VK_NULL_HANDLE; }
        if (v.instance) { vkDestroyInstance(v.instance, nullptr); v.instance = VK_NULL_HANDLE; }
    }
}
