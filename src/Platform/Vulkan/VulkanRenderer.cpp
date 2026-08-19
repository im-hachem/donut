#include "VulkanRenderer.h"
#include "Core/Log.h"
#include "Core/Camera.h"

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
    #define VK_CHECK(expr)                                                     \
        do {                                                                   \
            VkResult _r = (expr);                                              \
            if (_r != VK_SUCCESS) {                                            \
                DONUT_ERROR("Vulkan: {} failed ({})", #expr, (int)_r);         \
                return false;                                                  \
            }                                                                  \
        } while (0)

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    void VulkanPrepareGLFW()
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
        // macOS/Homebrew; hand it the loader entry point we already link against.
        glfwInitVulkanLoader(vkGetInstanceProcAddr);
    }

    struct VulkanRenderer::Impl
    {
        GLFWwindow* window = nullptr;
        int width = 0, height = 0;
        bool framebufferResized = false;

        VkInstance       instance = VK_NULL_HANDLE;
        VkSurfaceKHR     surface  = VK_NULL_HANDLE;
        VkPhysicalDevice physical = VK_NULL_HANDLE;
        VkDevice         device   = VK_NULL_HANDLE;
        uint32_t         graphicsFamily = 0, presentFamily = 0;
        VkQueue          graphicsQueue = VK_NULL_HANDLE, presentQueue = VK_NULL_HANDLE;

        VkSwapchainKHR   swapchain = VK_NULL_HANDLE;
        VkFormat         swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkExtent2D       swapchainExtent{};
        std::vector<VkImage>       images;
        std::vector<VkImageView>   imageViews;
        VkRenderPass               renderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> framebuffers;

        VkCommandPool                commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> commandBuffers;   // MAX_FRAMES_IN_FLIGHT

        std::vector<VkSemaphore> imageAvailable;        // per frame in flight
        std::vector<VkSemaphore> renderFinished;        // per swapchain image
        std::vector<VkFence>     inFlight;              // per frame in flight
        std::vector<VkFence>     imagesInFlight;        // per swapchain image
        uint32_t currentFrame = 0;

        VkDescriptorPool imguiPool = VK_NULL_HANDLE;
        bool imguiInit = false;

        VkPhysicalDeviceMemoryProperties memProps{};

        // Geodesic scene, rendered every frame into a fixed low-resolution
        // offscreen image (keeps each draw well under the Metal GPU watchdog),
        // then upscaled onto the swapchain by the present pass below.
        static constexpr uint32_t GEO_W = 480, GEO_H = 270;
        VkImage       geoImage     = VK_NULL_HANDLE;
        VkDeviceMemory geoImageMem = VK_NULL_HANDLE;
        VkImageView   geoImageView = VK_NULL_HANDLE;
        VkRenderPass  geoRenderPass = VK_NULL_HANDLE;
        VkFramebuffer geoFramebuffer = VK_NULL_HANDLE;
        VkBuffer camBuf = VK_NULL_HANDLE, diskBuf = VK_NULL_HANDLE, objBuf = VK_NULL_HANDLE, simBuf = VK_NULL_HANDLE;
        VkDeviceMemory camMem = VK_NULL_HANDLE, diskMem = VK_NULL_HANDLE, objMem = VK_NULL_HANDLE, simMem = VK_NULL_HANDLE;
        void* camMapped = nullptr;
        void* simMapped = nullptr;
        Camera camera{ 60.0f, (float)GEO_W / (float)GEO_H, 0.1f, 100.0f };
        bool leftWasDown = false;
        VkImage cubeImage = VK_NULL_HANDLE; VkDeviceMemory cubeMem = VK_NULL_HANDLE;
        VkImageView cubeView = VK_NULL_HANDLE; VkSampler cubeSampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout geoSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool geoPool = VK_NULL_HANDLE;
        VkDescriptorSet geoSet = VK_NULL_HANDLE;
        VkPipelineLayout geoPipelineLayout = VK_NULL_HANDLE;
        VkPipeline geoPipeline = VK_NULL_HANDLE;
        VkBuffer quadVB = VK_NULL_HANDLE; VkDeviceMemory quadVBMem = VK_NULL_HANDLE;
        double startTime = 0.0;
        VkFence geoInUse = VK_NULL_HANDLE;  // previous frame's fence; guards the shared geodesic image

        // Present pass: samples the geodesic image with a full-screen textured
        // quad, drawn into the swapchain render pass just before the ImGui UI.
        VkSampler presentSampler = VK_NULL_HANDLE;
        VkDescriptorSetLayout presentSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool presentPool = VK_NULL_HANDLE;
        VkDescriptorSet presentSet = VK_NULL_HANDLE;
        VkPipelineLayout presentPipelineLayout = VK_NULL_HANDLE;
        VkPipeline presentPipeline = VK_NULL_HANDLE;

        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags) const;
        bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) const;
        static std::vector<uint32_t> LoadSpirv(const std::string& path);
        bool CreateShaderModule(const std::string& path, VkShaderModule& out) const;

        bool CreateInstance();
        bool PickPhysicalAndDevice();
        bool CreateSwapchain();
        bool CreateImageViews();
        bool CreateRenderPass();
        bool CreateFramebuffers();
        bool CreateCommandBuffers();
        bool CreateSyncObjects();
        bool CreateGeodesicResources();
        bool CreateHDRICubemap(const char* path);
        bool CreatePresentResources();
        void ProcessInput();
        void UpdateGeodesicUniforms();
        void DestroyGeodesicResources();
        bool RecreateSwapchain();
        void CleanupSwapchain();
        bool RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, const glm::vec4& clear, ImDrawData* drawData);
    };

    bool VulkanRenderer::Impl::CreateInstance()
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
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> avail(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, avail.data());
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

    bool VulkanRenderer::Impl::PickPhysicalAndDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) { DONUT_ERROR("Vulkan: no physical devices"); return false; }
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());
        physical = devices[0];

        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfams(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &qCount, qfams.data());
        bool foundG = false, foundP = false;
        for (uint32_t i = 0; i < qCount; ++i)
        {
            if (!foundG && (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { graphicsFamily = i; foundG = true; }
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &present);
            if (!foundP && present) { presentFamily = i; foundP = true; }
        }
        if (!foundG || !foundP) { DONUT_ERROR("Vulkan: no graphics/present queue"); return false; }

        std::vector<const char*> devExts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        uint32_t devExtCount = 0;
        vkEnumerateDeviceExtensionProperties(physical, nullptr, &devExtCount, nullptr);
        std::vector<VkExtensionProperties> devExtProps(devExtCount);
        vkEnumerateDeviceExtensionProperties(physical, nullptr, &devExtCount, devExtProps.data());
        for (const auto& e : devExtProps)
            if (std::strcmp(e.extensionName, "VK_KHR_portability_subset") == 0)
                devExts.push_back("VK_KHR_portability_subset");

        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> qcis;
        uint32_t families[2] = { graphicsFamily, presentFamily };
        for (uint32_t i = 0; i < (graphicsFamily == presentFamily ? 1u : 2u); ++i)
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
        dci.enabledExtensionCount   = (uint32_t)devExts.size();
        dci.ppEnabledExtensionNames = devExts.data();
        VK_CHECK(vkCreateDevice(physical, &dci, nullptr, &device));
        vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentFamily, 0, &presentQueue);

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(physical, &props);
        vkGetPhysicalDeviceMemoryProperties(physical, &memProps);
        DONUT_INFO("Vulkan device: {}", props.deviceName);
        return true;
    }

    bool VulkanRenderer::Impl::CreateSwapchain()
    {
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);

        uint32_t fmtCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &fmtCount, formats.data());
        VkSurfaceFormatKHR chosen = formats[0];
        for (const auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                chosen = f;
        swapchainFormat = chosen.format;

        if (caps.currentExtent.width != UINT32_MAX)
            swapchainExtent = caps.currentExtent;
        else
        {
            swapchainExtent.width  = std::clamp((uint32_t)width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
            swapchainExtent.height = std::clamp((uint32_t)height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        sci.surface          = surface;
        sci.minImageCount    = imageCount;
        sci.imageFormat      = chosen.format;
        sci.imageColorSpace  = chosen.colorSpace;
        sci.imageExtent      = swapchainExtent;
        sci.imageArrayLayers = 1;
        sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        sci.preTransform     = caps.currentTransform;
        sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode      = VK_PRESENT_MODE_FIFO_KHR;   // always supported, vsync
        sci.clipped          = VK_TRUE;

        uint32_t famIdx[2] = { graphicsFamily, presentFamily };
        if (graphicsFamily != presentFamily)
        {
            sci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            sci.queueFamilyIndexCount = 2;
            sci.pQueueFamilyIndices   = famIdx;
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

    bool VulkanRenderer::Impl::CreateImageViews()
    {
        imageViews.resize(images.size());
        for (size_t i = 0; i < images.size(); ++i)
        {
            VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            vci.image = images[i];
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = swapchainFormat;
            vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            VK_CHECK(vkCreateImageView(device, &vci, nullptr, &imageViews[i]));
        }
        return true;
    }

    bool VulkanRenderer::Impl::CreateRenderPass()
    {
        VkAttachmentDescription color{};
        color.format = swapchainFormat;
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
        VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &renderPass));
        return true;
    }

    bool VulkanRenderer::Impl::CreateFramebuffers()
    {
        framebuffers.resize(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); ++i)
        {
            VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fbci.renderPass = renderPass;
            fbci.attachmentCount = 1; fbci.pAttachments = &imageViews[i];
            fbci.width = swapchainExtent.width; fbci.height = swapchainExtent.height; fbci.layers = 1;
            VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &framebuffers[i]));
        }
        return true;
    }

    bool VulkanRenderer::Impl::CreateCommandBuffers()
    {
        VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = graphicsFamily;
        VK_CHECK(vkCreateCommandPool(device, &pci, nullptr, &commandPool));

        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandPool = commandPool;
        cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cbai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        VK_CHECK(vkAllocateCommandBuffers(device, &cbai, commandBuffers.data()));
        return true;
    }

    bool VulkanRenderer::Impl::CreateSyncObjects()
    {
        imageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
        inFlight.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinished.resize(images.size());
        imagesInFlight.assign(images.size(), VK_NULL_HANDLE);

        VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &imageAvailable[i]));
            VK_CHECK(vkCreateFence(device, &fci, nullptr, &inFlight[i]));
        }
        for (size_t i = 0; i < images.size(); ++i)
            VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &renderFinished[i]));
        return true;
    }

    uint32_t VulkanRenderer::Impl::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags) const
    {
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & flags) == flags)
                return i;
        return UINT32_MAX;
    }

    bool VulkanRenderer::Impl::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                            VkBuffer& buf, VkDeviceMemory& mem) const
    {
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = size; bci.usage = usage; bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bci, nullptr, &buf) != VK_SUCCESS) return false;
        VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(device, buf, &req);
        VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, props);
        if (vkAllocateMemory(device, &ai, nullptr, &mem) != VK_SUCCESS) return false;
        vkBindBufferMemory(device, buf, mem, 0);
        return true;
    }

    std::vector<uint32_t> VulkanRenderer::Impl::LoadSpirv(const std::string& path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) return {};
        size_t size = (size_t)file.tellg();
        std::vector<uint32_t> data(size / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }

    bool VulkanRenderer::Impl::CreateShaderModule(const std::string& path, VkShaderModule& out) const
    {
        auto spv = LoadSpirv(path);
        if (spv.empty()) { DONUT_ERROR("Vulkan: failed to load SPIR-V {}", path); return false; }
        VkShaderModuleCreateInfo ci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        ci.codeSize = spv.size() * 4; ci.pCode = spv.data();
        return vkCreateShaderModule(device, &ci, nullptr, &out) == VK_SUCCESS;
    }

    bool VulkanRenderer::Impl::CreateGeodesicResources()
    {
        const VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;
        const VkMemoryPropertyFlags hostVis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const float SagA_rs = 1.269e10f;

        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D; ici.format = fmt; ici.extent = { GEO_W, GEO_H, 1 };
        ici.mipLevels = 1; ici.arrayLayers = 1; ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK(vkCreateImage(device, &ici, nullptr, &geoImage));
        VkMemoryRequirements imReq{}; vkGetImageMemoryRequirements(device, geoImage, &imReq);
        VkMemoryAllocateInfo imAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        imAlloc.allocationSize = imReq.size;
        imAlloc.memoryTypeIndex = FindMemoryType(imReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &imAlloc, nullptr, &geoImageMem));
        VK_CHECK(vkBindImageMemory(device, geoImage, geoImageMem, 0));
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = geoImage; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = fmt;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(device, &vci, nullptr, &geoImageView));

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
        VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &geoRenderPass));
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = geoRenderPass; fbci.attachmentCount = 1; fbci.pAttachments = &geoImageView;
        fbci.width = GEO_W; fbci.height = GEO_H; fbci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &geoFramebuffer));

        CreateBuffer(128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, camBuf, camMem);
        CreateBuffer(32,  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, diskBuf, diskMem);
        CreateBuffer(800, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, objBuf, objMem);
        CreateBuffer(16,  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, simBuf, simMem);

        camera.SetCameraMode(CameraMode::Orbital);
        camera.SetOrbitalTarget(glm::vec3(0.0f));
        camera.SetOrbitalRadius(1e11);
        camera.SetOrbitalLimits(4e10, 3e11);
        camera.SetOrbitalSpeed(0.01f);
        camera.SetZoomSpeed(1e10);
        camera.SetAzimuth(0.0f);
        camera.SetElevation(1.25f);

        void* p = nullptr;
        vkMapMemory(device, camMem, 0, 128, 0, &camMapped);  // camera UBO is refilled every frame

        float diskData[8] = { SagA_rs * 2.2f, SagA_rs * 5.2f, 2.0f, SagA_rs * 0.1f, 0.1f, 0, 0, 0 };
        vkMapMemory(device, diskMem, 0, 32, 0, &p); memcpy(p, diskData, sizeof(diskData)); vkUnmapMemory(device, diskMem);

        std::vector<uint8_t> objData(800, 0);
        int numObjects = 1; memcpy(objData.data(), &numObjects, 4);
        float posRadius[4] = { 0, 0, 0, SagA_rs }; memcpy(objData.data() + 16, posRadius, 16);
        float objColor[4] = { 0, 0, 0, 1 };        memcpy(objData.data() + 272, objColor, 16);
        vkMapMemory(device, objMem, 0, 800, 0, &p); memcpy(p, objData.data(), 800); vkUnmapMemory(device, objMem);

        vkMapMemory(device, simMem, 0, 16, 0, &simMapped);

        if (!CreateHDRICubemap("Assets/HDRI/HDR_blue_nebulae-1.hdr")) return false;

        VkDescriptorSetLayoutBinding binds[5]{};
        for (int i = 0; i < 4; ++i) { binds[i].binding = i; binds[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; binds[i].descriptorCount = 1; binds[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; }
        binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = 5; dslci.pBindings = binds;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &geoSetLayout));
        VkDescriptorPoolSize psizes[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.maxSets = 1; dpci.poolSizeCount = 2; dpci.pPoolSizes = psizes;
        VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &geoPool));
        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dsai.descriptorPool = geoPool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &geoSetLayout;
        VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &geoSet));
        VkDescriptorBufferInfo bi[4] = { { camBuf, 0, VK_WHOLE_SIZE }, { diskBuf, 0, VK_WHOLE_SIZE }, { objBuf, 0, VK_WHOLE_SIZE }, { simBuf, 0, VK_WHOLE_SIZE } };
        VkDescriptorImageInfo cubeInfo{ cubeSampler, cubeView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet writes[5]{};
        for (int i = 0; i < 4; ++i) { writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[i].dstSet = geoSet; writes[i].dstBinding = i; writes[i].descriptorCount = 1; writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[i].pBufferInfo = &bi[i]; }
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[4].dstSet = geoSet; writes[4].dstBinding = 4; writes[4].descriptorCount = 1; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[4].pImageInfo = &cubeInfo;
        vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);

        float quad[] = {
            -1.f,  1.f, 0.f, 1.f,  -1.f, -1.f, 0.f, 0.f,   1.f, -1.f, 1.f, 0.f,
            -1.f,  1.f, 0.f, 1.f,   1.f, -1.f, 1.f, 0.f,   1.f,  1.f, 1.f, 1.f,
        };
        CreateBuffer(sizeof(quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostVis, quadVB, quadVBMem);
        vkMapMemory(device, quadVBMem, 0, sizeof(quad), 0, &p); memcpy(p, quad, sizeof(quad)); vkUnmapMemory(device, quadVBMem);

        VkShaderModule vmod, fmod;
        if (!CreateShaderModule("Assets/Shaders/generated/Geodesic.vertexMain.spv", vmod)) return false;
        if (!CreateShaderModule("Assets/Shaders/generated/Geodesic.fragmentMain.spv", fmod)) return false;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1; plci.pSetLayouts = &geoSetLayout;
        VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &geoPipelineLayout));
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";
        VkVertexInputBindingDescription vib{ 0, 16, VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription via[2] = { { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R32G32_SFLOAT, 8 } };
        VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &vib;
        vin.vertexAttributeDescriptionCount = 2; vin.pVertexAttributeDescriptions = via;
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp{ 0, 0, (float)GEO_W, (float)GEO_H, 0, 1 }; VkRect2D sc{ { 0, 0 }, { GEO_W, GEO_H } };
        VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; vps.viewportCount = 1; vps.pViewports = &vp; vps.scissorCount = 1; vps.pScissors = &sc;
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; cb.attachmentCount = 1; cb.pAttachments = &cba;
        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2; gpci.pStages = stages;
        gpci.pVertexInputState = &vin; gpci.pInputAssemblyState = &ia; gpci.pViewportState = &vps;
        gpci.pRasterizationState = &rs; gpci.pMultisampleState = &ms; gpci.pColorBlendState = &cb;
        gpci.layout = geoPipelineLayout; gpci.renderPass = geoRenderPass; gpci.subpass = 0;
        VkResult pr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &geoPipeline);
        vkDestroyShaderModule(device, vmod, nullptr); vkDestroyShaderModule(device, fmod, nullptr);
        if (pr != VK_SUCCESS) { DONUT_ERROR("Vulkan: geodesic pipeline creation failed ({})", (int)pr); return false; }

        startTime = glfwGetTime();
        UpdateGeodesicUniforms();
        DONUT_INFO("Vulkan: geodesic resources ready ({}x{} offscreen)", (int)GEO_W, (int)GEO_H);
        return true;
    }

    bool VulkanRenderer::Impl::CreateHDRICubemap(const char* path)
    {
        const VkMemoryPropertyFlags hostVis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const uint32_t FACE = 1024;
        const VkFormat cubeFmt = VK_FORMAT_R16G16B16A16_SFLOAT;

        VkImageCreateInfo cci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        cci.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        cci.imageType = VK_IMAGE_TYPE_2D; cci.format = cubeFmt; cci.extent = { FACE, FACE, 1 };
        cci.mipLevels = 1; cci.arrayLayers = 6; cci.samples = VK_SAMPLE_COUNT_1_BIT;
        cci.tiling = VK_IMAGE_TILING_OPTIMAL;
        cci.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VK_CHECK(vkCreateImage(device, &cci, nullptr, &cubeImage));
        VkMemoryRequirements creq{}; vkGetImageMemoryRequirements(device, cubeImage, &creq);
        VkMemoryAllocateInfo cai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        cai.allocationSize = creq.size; cai.memoryTypeIndex = FindMemoryType(creq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &cai, nullptr, &cubeMem));
        VK_CHECK(vkBindImageMemory(device, cubeImage, cubeMem, 0));
        VkImageViewCreateInfo cvci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        cvci.image = cubeImage; cvci.viewType = VK_IMAGE_VIEW_TYPE_CUBE; cvci.format = cubeFmt;
        cvci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
        VK_CHECK(vkCreateImageView(device, &cvci, nullptr, &cubeView));
        VkSamplerCreateInfo csm{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        csm.magFilter = VK_FILTER_LINEAR; csm.minFilter = VK_FILTER_LINEAR;
        csm.addressModeU = csm.addressModeV = csm.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(device, &csm, nullptr, &cubeSampler));

        int w = 0, h = 0, ch = 0;
        float* pixels = stbi_loadf(path, &w, &h, &ch, 4);
        if (!pixels)
        {
            DONUT_WARN("Vulkan: HDRI '{}' could not be loaded; using a dark background", path);
            VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            cbai.commandPool = commandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
            VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(device, &cbai, &cmd));
            VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(cmd, &bi);
            VkImageMemoryBarrier tb{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            tb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; tb.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            tb.image = cubeImage; tb.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
            tb.srcAccessMask = 0; tb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &tb);
            VkClearColorValue dark{}; dark.float32[0] = 0.02f; dark.float32[1] = 0.02f; dark.float32[2] = 0.05f; dark.float32[3] = 1.0f;
            VkImageSubresourceRange rng{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
            vkCmdClearColorImage(cmd, cubeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dark, 1, &rng);
            VkImageMemoryBarrier rb = tb; rb.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; rb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; rb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &rb);
            vkEndCommandBuffer(cmd);
            VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO }; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
            vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE); vkQueueWaitIdle(graphicsQueue);
            vkFreeCommandBuffers(device, commandPool, 1, &cmd);
            return true;
        }

        // Apple GPUs can't linearly filter RGBA32F, so store the equirect as
        // RGBA16F (convert the loaded floats to half on the way into staging).
        const VkFormat eqFmt = VK_FORMAT_R16G16B16A16_SFLOAT;
        size_t texelCount = (size_t)w * h * 4;
        VkDeviceSize eqSize = (VkDeviceSize)texelCount * sizeof(uint16_t);
        VkBuffer eqStaging; VkDeviceMemory eqStagingMem;
        if (!CreateBuffer(eqSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, hostVis, eqStaging, eqStagingMem)) { stbi_image_free(pixels); return false; }
        void* mp = nullptr; vkMapMemory(device, eqStagingMem, 0, eqSize, 0, &mp);
        uint16_t* dst = (uint16_t*)mp;
        for (size_t i = 0; i < texelCount; ++i) { __fp16 hf = (__fp16)pixels[i]; memcpy(&dst[i], &hf, sizeof(uint16_t)); }
        vkUnmapMemory(device, eqStagingMem);
        stbi_image_free(pixels);

        VkImage eqImage; VkDeviceMemory eqMem;
        VkImageCreateInfo eci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        eci.imageType = VK_IMAGE_TYPE_2D; eci.format = eqFmt; eci.extent = { (uint32_t)w, (uint32_t)h, 1 };
        eci.mipLevels = 1; eci.arrayLayers = 1; eci.samples = VK_SAMPLE_COUNT_1_BIT;
        eci.tiling = VK_IMAGE_TILING_OPTIMAL; eci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK(vkCreateImage(device, &eci, nullptr, &eqImage));
        VkMemoryRequirements ereq{}; vkGetImageMemoryRequirements(device, eqImage, &ereq);
        VkMemoryAllocateInfo eai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        eai.allocationSize = ereq.size; eai.memoryTypeIndex = FindMemoryType(ereq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device, &eai, nullptr, &eqMem));
        VK_CHECK(vkBindImageMemory(device, eqImage, eqMem, 0));
        VkImageView eqView;
        VkImageViewCreateInfo evci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        evci.image = eqImage; evci.viewType = VK_IMAGE_VIEW_TYPE_2D; evci.format = eqFmt;
        evci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(device, &evci, nullptr, &eqView));
        VkSampler eqSampler;
        VkSamplerCreateInfo esm{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        esm.magFilter = VK_FILTER_LINEAR; esm.minFilter = VK_FILTER_LINEAR;
        esm.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;         // longitude wraps
        esm.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // latitude clamps
        esm.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(device, &esm, nullptr, &eqSampler));

        VkImageView faceViews[6];
        for (uint32_t i = 0; i < 6; ++i)
        {
            VkImageViewCreateInfo fvci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            fvci.image = cubeImage; fvci.viewType = VK_IMAGE_VIEW_TYPE_2D; fvci.format = cubeFmt;
            fvci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1 };
            VK_CHECK(vkCreateImageView(device, &fvci, nullptr, &faceViews[i]));
        }

        VkAttachmentDescription color{};
        color.format = cubeFmt; color.samples = VK_SAMPLE_COUNT_1_BIT;
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
        VkFramebuffer faceFB[6];
        for (uint32_t i = 0; i < 6; ++i)
        {
            VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
            fbci.renderPass = rp; fbci.attachmentCount = 1; fbci.pAttachments = &faceViews[i]; fbci.width = FACE; fbci.height = FACE; fbci.layers = 1;
            VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &faceFB[i]));
        }

        VkDescriptorSetLayoutBinding binds[2]{};
        binds[0].binding = 0; binds[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; binds[0].descriptorCount = 1; binds[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        binds[1].binding = 1; binds[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; binds[1].descriptorCount = 1; binds[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayout setLayout;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO }; dslci.bindingCount = 2; dslci.pBindings = binds;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &setLayout));
        VkDescriptorPoolSize psizes[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6 } };
        VkDescriptorPool pool;
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO }; dpci.maxSets = 6; dpci.poolSizeCount = 2; dpci.pPoolSizes = psizes;
        VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &pool));

        VkShaderModule vmod, fmod;
        if (!CreateShaderModule("Assets/Shaders/generated/EquirectToCubemap.vertexMain.spv", vmod)) return false;
        if (!CreateShaderModule("Assets/Shaders/generated/EquirectToCubemap.fragmentMain.spv", fmod)) return false;
        VkPipelineLayout playout;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO }; plci.setLayoutCount = 1; plci.pSetLayouts = &setLayout;
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

        float cubeVerts[] = {
            -1,1,-1, -1,-1,-1, 1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
            -1,-1,1, -1,-1,-1, -1,1,-1, -1,1,-1, -1,1,1, -1,-1,1,
             1,-1,-1, 1,-1,1, 1,1,1, 1,1,1, 1,1,-1, 1,-1,-1,
            -1,-1,1, -1,1,1, 1,1,1, 1,1,1, 1,-1,1, -1,-1,1,
            -1,1,-1, 1,1,-1, 1,1,1, 1,1,1, -1,1,1, -1,1,-1,
            -1,-1,-1, -1,-1,1, 1,-1,-1, 1,-1,-1, -1,-1,1, 1,-1,1,
        };
        VkBuffer cubeVB; VkDeviceMemory cubeVBMem;
        CreateBuffer(sizeof(cubeVerts), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostVis, cubeVB, cubeVBMem);
        vkMapMemory(device, cubeVBMem, 0, sizeof(cubeVerts), 0, &mp); memcpy(mp, cubeVerts, sizeof(cubeVerts)); vkUnmapMemory(device, cubeVBMem);

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
        VkBuffer ubo[6]; VkDeviceMemory uboMem[6]; VkDescriptorSet sets[6];
        for (uint32_t i = 0; i < 6; ++i)
        {
            CreateBuffer(128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, ubo[i], uboMem[i]);
            glm::mat4 mats[2] = { glm::transpose(proj), glm::transpose(views[i]) };  // SPIR-V expects row-major
            vkMapMemory(device, uboMem[i], 0, 128, 0, &mp); memcpy(mp, mats, 128); vkUnmapMemory(device, uboMem[i]);
            VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO }; dsai.descriptorPool = pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &setLayout;
            VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &sets[i]));
            VkDescriptorBufferInfo bufInfo{ ubo[i], 0, VK_WHOLE_SIZE };
            VkDescriptorImageInfo imgInfo{ eqSampler, eqView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
            VkWriteDescriptorSet ws[2]{};
            ws[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; ws[0].dstSet = sets[i]; ws[0].dstBinding = 0; ws[0].descriptorCount = 1; ws[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; ws[0].pBufferInfo = &bufInfo;
            ws[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; ws[1].dstSet = sets[i]; ws[1].dstBinding = 1; ws[1].descriptorCount = 1; ws[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; ws[1].pImageInfo = &imgInfo;
            vkUpdateDescriptorSets(device, 2, ws, 0, nullptr);
        }

        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandPool = commandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(device, &cbai, &cmd));
        VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
        VkImageMemoryBarrier toDst{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toDst.image = eqImage; toDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        toDst.srcAccessMask = 0; toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
        VkBufferImageCopy copy{}; copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; copy.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
        vkCmdCopyBufferToImage(cmd, eqStaging, eqImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        VkImageMemoryBarrier toRead = toDst; toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead);

        VkClearValue clear{}; clear.color = { { 0, 0, 0, 1 } };
        for (uint32_t i = 0; i < 6; ++i)
        {
            VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
            rpbi.renderPass = rp; rpbi.framebuffer = faceFB[i]; rpbi.renderArea = { { 0, 0 }, { FACE, FACE } }; rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;
            vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, playout, 0, 1, &sets[i], 0, nullptr);
            VkDeviceSize off = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &cubeVB, &off);
            vkCmdDraw(cmd, 36, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }
        VK_CHECK(vkEndCommandBuffer(cmd));
        VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO }; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(graphicsQueue));

        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
        for (uint32_t i = 0; i < 6; ++i) { vkDestroyBuffer(device, ubo[i], nullptr); vkFreeMemory(device, uboMem[i], nullptr); vkDestroyFramebuffer(device, faceFB[i], nullptr); vkDestroyImageView(device, faceViews[i], nullptr); }
        vkDestroyBuffer(device, cubeVB, nullptr); vkFreeMemory(device, cubeVBMem, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr); vkDestroyPipelineLayout(device, playout, nullptr);
        vkDestroyDescriptorPool(device, pool, nullptr); vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
        vkDestroyRenderPass(device, rp, nullptr);
        vkDestroySampler(device, eqSampler, nullptr); vkDestroyImageView(device, eqView, nullptr);
        vkDestroyImage(device, eqImage, nullptr); vkFreeMemory(device, eqMem, nullptr);
        vkDestroyBuffer(device, eqStaging, nullptr); vkFreeMemory(device, eqStagingMem, nullptr);
        DONUT_INFO("Vulkan: HDRI cubemap built from {} ({}x{} equirect -> {}^2 cube)", path, w, h, (int)FACE);
        return true;
    }

    bool VulkanRenderer::Impl::CreatePresentResources()
    {
        VkSamplerCreateInfo smci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        smci.magFilter = VK_FILTER_LINEAR; smci.minFilter = VK_FILTER_LINEAR;
        smci.addressModeU = smci.addressModeV = smci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(device, &smci, nullptr, &presentSampler));

        VkDescriptorSetLayoutBinding bind{}; bind.binding = 0; bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; bind.descriptorCount = 1; bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = 1; dslci.pBindings = &bind;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &presentSetLayout));
        VkDescriptorPoolSize psize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &psize;
        VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &presentPool));
        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dsai.descriptorPool = presentPool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &presentSetLayout;
        VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &presentSet));
        VkDescriptorImageInfo ii{ presentSampler, geoImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = presentSet; write.dstBinding = 0; write.descriptorCount = 1; write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; write.pImageInfo = &ii;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        VkShaderModule vmod, fmod;
        if (!CreateShaderModule("Assets/Shaders/generated/TexturedQuad.vertexMain.spv", vmod)) return false;
        if (!CreateShaderModule("Assets/Shaders/generated/TexturedQuad.fragmentMain.spv", fmod)) return false;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1; plci.pSetLayouts = &presentSetLayout;
        VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &presentPipelineLayout));
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
        gpci.layout = presentPipelineLayout; gpci.renderPass = renderPass; gpci.subpass = 0;
        VkResult pr = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, nullptr, &presentPipeline);
        vkDestroyShaderModule(device, vmod, nullptr); vkDestroyShaderModule(device, fmod, nullptr);
        if (pr != VK_SUCCESS) { DONUT_ERROR("Vulkan: present pipeline creation failed ({})", (int)pr); return false; }
        DONUT_INFO("Vulkan: present pipeline ready");
        return true;
    }

    void VulkanRenderer::Impl::UpdateGeodesicUniforms()
    {
        struct CamUBO {
            glm::vec3 pos; float p0; glm::vec3 right; float p1;
            glm::vec3 up; float p2; glm::vec3 fwd; float p3;
            float tanHalfFov; float aspect; uint32_t moving; int p4;
        } camData{};
        glm::vec3 pos   = camera.GetOrbitalPosition();
        glm::vec3 fwd   = glm::normalize(camera.GetOrbitalTarget() - pos);
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        camData.pos = pos; camData.right = right; camData.up = glm::cross(right, fwd); camData.fwd = fwd;
        camData.tanHalfFov = (float)tan(glm::radians(60.0f * 0.5f));
        camData.aspect = (float)GEO_W / (float)GEO_H;
        camData.moving = camera.IsDragging() || camera.IsPanning() ? 1u : 0u;
        if (camMapped) memcpy(camMapped, &camData, sizeof(camData));

        // Fewer integration steps while the camera moves keeps dragging responsive;
        // more steps once it settles renders the disk in full.
        struct SimUBO { int stepsMoving; int stepsStatic; float earlyExit; float time; } sim;
        sim.stepsMoving = 3500; sim.stepsStatic = 5000; sim.earlyExit = 5e12f;
        sim.time = (float)(glfwGetTime() - startTime);
        if (simMapped) memcpy(simMapped, &sim, sizeof(sim));
    }

    static double g_ScrollAccum = 0.0;
    static GLFWscrollfun g_PrevScroll = nullptr;
    static void DonutVkScrollCallback(GLFWwindow* w, double x, double y)
    {
        if (g_PrevScroll) g_PrevScroll(w, x, y);  // keep ImGui's scroll handling intact
        g_ScrollAccum += y;
    }

    void VulkanRenderer::Impl::ProcessInput()
    {
        bool overUI = imguiInit && ImGui::GetIO().WantCaptureMouse;

        bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (leftDown && !leftWasDown && !overUI)
            camera.ProcessOrbitalMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
        else if (!leftDown && leftWasDown)
            camera.ProcessOrbitalMouseButton(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
        leftWasDown = leftDown;

        double mx = 0, my = 0;
        glfwGetCursorPos(window, &mx, &my);
        camera.ProcessOrbitalMouseMove(mx, my);  // tracks last position internally; orbits only while dragging

        double scroll = g_ScrollAccum; g_ScrollAccum = 0.0;
        if (scroll != 0.0 && !overUI)
            camera.ProcessOrbitalScroll(0.0, scroll);
    }

    void VulkanRenderer::Impl::DestroyGeodesicResources()
    {
        if (presentPipeline) vkDestroyPipeline(device, presentPipeline, nullptr);
        if (presentPipelineLayout) vkDestroyPipelineLayout(device, presentPipelineLayout, nullptr);
        if (presentPool) vkDestroyDescriptorPool(device, presentPool, nullptr);
        if (presentSetLayout) vkDestroyDescriptorSetLayout(device, presentSetLayout, nullptr);
        if (presentSampler) vkDestroySampler(device, presentSampler, nullptr);

        if (geoPipeline) vkDestroyPipeline(device, geoPipeline, nullptr);
        if (geoPipelineLayout) vkDestroyPipelineLayout(device, geoPipelineLayout, nullptr);
        if (geoPool) vkDestroyDescriptorPool(device, geoPool, nullptr);
        if (geoSetLayout) vkDestroyDescriptorSetLayout(device, geoSetLayout, nullptr);
        if (quadVB) vkDestroyBuffer(device, quadVB, nullptr);
        if (quadVBMem) vkFreeMemory(device, quadVBMem, nullptr);
        if (cubeSampler) vkDestroySampler(device, cubeSampler, nullptr);
        if (cubeView) vkDestroyImageView(device, cubeView, nullptr);
        if (cubeImage) vkDestroyImage(device, cubeImage, nullptr);
        if (cubeMem) vkFreeMemory(device, cubeMem, nullptr);
        if (camMapped) { vkUnmapMemory(device, camMem); camMapped = nullptr; }
        if (simMapped) { vkUnmapMemory(device, simMem); simMapped = nullptr; }
        VkBuffer ubos[4] = { camBuf, diskBuf, objBuf, simBuf };
        VkDeviceMemory umem[4] = { camMem, diskMem, objMem, simMem };
        for (int i = 0; i < 4; ++i) { if (ubos[i]) vkDestroyBuffer(device, ubos[i], nullptr); if (umem[i]) vkFreeMemory(device, umem[i], nullptr); }
        if (geoFramebuffer) vkDestroyFramebuffer(device, geoFramebuffer, nullptr);
        if (geoRenderPass) vkDestroyRenderPass(device, geoRenderPass, nullptr);
        if (geoImageView) vkDestroyImageView(device, geoImageView, nullptr);
        if (geoImage) vkDestroyImage(device, geoImage, nullptr);
        if (geoImageMem) vkFreeMemory(device, geoImageMem, nullptr);
    }

    bool VulkanRenderer::Impl::RecordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, const glm::vec4& clear, ImDrawData* drawData)
    {
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

        // Geodesic offscreen pass
        VkClearValue geoClear{}; geoClear.color = { { 0, 0, 0, 1 } };
        VkRenderPassBeginInfo grp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        grp.renderPass = geoRenderPass; grp.framebuffer = geoFramebuffer;
        grp.renderArea = { { 0, 0 }, { GEO_W, GEO_H } };
        grp.clearValueCount = 1; grp.pClearValues = &geoClear;
        vkCmdBeginRenderPass(cmd, &grp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, geoPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, geoPipelineLayout, 0, 1, &geoSet, 0, nullptr);
        VkDeviceSize off = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &quadVB, &off);
        vkCmdDraw(cmd, 6, 1, 0, 0);
        vkCmdEndRenderPass(cmd);

        // Swapchain pass: upscale the geodesic image, then the ImGui UI on top
        VkClearValue cv{}; cv.color = { { clear.r, clear.g, clear.b, clear.a } };
        VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpbi.renderPass = renderPass;
        rpbi.framebuffer = framebuffers[imageIndex];
        rpbi.renderArea = { { 0, 0 }, swapchainExtent };
        rpbi.clearValueCount = 1; rpbi.pClearValues = &cv;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPipeline);
        // Negative-height viewport flips the geodesic image vertically so the scene
        // reads the same as the OpenGL path (Vulkan's clip space is Y-down). Only
        // this draw is affected; ImGui sets its own viewport.
        VkViewport vp{ 0, (float)swapchainExtent.height, (float)swapchainExtent.width, -(float)swapchainExtent.height, 0, 1 };
        VkRect2D scissor{ { 0, 0 }, swapchainExtent };
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPipelineLayout, 0, 1, &presentSet, 0, nullptr);
        vkCmdBindVertexBuffers(cmd, 0, 1, &quadVB, &off);
        vkCmdDraw(cmd, 6, 1, 0, 0);
        if (drawData)
            ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        vkCmdEndRenderPass(cmd);

        VK_CHECK(vkEndCommandBuffer(cmd));
        return true;
    }

    void VulkanRenderer::Impl::CleanupSwapchain()
    {
        for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        framebuffers.clear();
        for (auto iv : imageViews) vkDestroyImageView(device, iv, nullptr);
        imageViews.clear();
        if (renderPass) { vkDestroyRenderPass(device, renderPass, nullptr); renderPass = VK_NULL_HANDLE; }
        if (swapchain)  { vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain = VK_NULL_HANDLE; }
    }

    bool VulkanRenderer::Impl::RecreateSwapchain()
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

        CleanupSwapchain();
        // renderFinished are tied to image count; recreate below via sync if it changed.
        if (!CreateSwapchain())   return false;
        if (!CreateImageViews())  return false;
        if (!CreateRenderPass())  return false;
        if (!CreateFramebuffers())return false;
        imagesInFlight.assign(images.size(), VK_NULL_HANDLE);
        return true;
    }

    VulkanRenderer::VulkanRenderer()  { m_Impl = new Impl(); }
    VulkanRenderer::~VulkanRenderer() { Shutdown(); delete m_Impl; m_Impl = nullptr; }

    bool VulkanRenderer::Init(void* glfwWindow, int width, int height)
    {
        Impl& v = *m_Impl;
        v.window = (GLFWwindow*)glfwWindow;
        v.width = width; v.height = height;

        if (!v.CreateInstance())        return false;
        if (!v.PickPhysicalAndDevice()) return false;
        if (!v.CreateSwapchain())       return false;
        if (!v.CreateImageViews())      return false;
        if (!v.CreateRenderPass())      return false;
        if (!v.CreateFramebuffers())    return false;
        if (!v.CreateCommandBuffers())  return false;
        if (!v.CreateSyncObjects())     return false;
        if (!v.CreateGeodesicResources()) return false;
        if (!v.CreatePresentResources())  return false;

        DONUT_INFO("Vulkan renderer ready: {} swapchain images, {}x{}",
                   (int)v.images.size(), v.swapchainExtent.width, v.swapchainExtent.height);
        return true;
    }

    bool VulkanRenderer::InitImGui()
    {
        Impl& v = *m_Impl;
        if (v.device == VK_NULL_HANDLE) return false;

        VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 };
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpci.maxSets = 1000;
        dpci.poolSizeCount = 1; dpci.pPoolSizes = &poolSize;
        VK_CHECK(vkCreateDescriptorPool(v.device, &dpci, nullptr, &v.imguiPool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(v.window, true);
        g_PrevScroll = glfwSetScrollCallback(v.window, DonutVkScrollCallback);  // chain ImGui + camera zoom
        ImGui_ImplVulkan_InitInfo info{};
        info.ApiVersion     = VK_API_VERSION_1_2;
        info.Instance       = v.instance;
        info.PhysicalDevice = v.physical;
        info.Device         = v.device;
        info.QueueFamily    = v.graphicsFamily;
        info.Queue          = v.graphicsQueue;
        info.DescriptorPool = v.imguiPool;
        info.RenderPass     = v.renderPass;
        info.MinImageCount  = 2;
        info.ImageCount     = (uint32_t)v.images.size();
        info.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;
        if (!ImGui_ImplVulkan_Init(&info))
        {
            DONUT_ERROR("Vulkan: ImGui_ImplVulkan_Init failed");
            return false;
        }

        v.imguiInit = true;
        DONUT_INFO("Vulkan: ImGui backend initialized");
        return true;
    }

    void VulkanRenderer::OnResize(int width, int height)
    {
        m_Impl->framebufferResized = true;
        m_Impl->width = width; m_Impl->height = height;
    }

    void VulkanRenderer::DrawFrame(const glm::vec4& clearColor, const std::function<void()>& buildUI)
    {
        Impl& v = *m_Impl;
        if (v.device == VK_NULL_HANDLE) return;

        ImDrawData* drawData = nullptr;
        if (v.imguiInit)
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            if (buildUI) buildUI();
            ImGui::Render();
            drawData = ImGui::GetDrawData();
        }

        vkWaitForFences(v.device, 1, &v.inFlight[v.currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = 0;
        VkResult r = vkAcquireNextImageKHR(v.device, v.swapchain, UINT64_MAX,
                                           v.imageAvailable[v.currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) { v.RecreateSwapchain(); return; }
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) { DONUT_ERROR("Vulkan: acquire failed ({})", (int)r); return; }

        if (v.imagesInFlight[imageIndex] != VK_NULL_HANDLE)
            vkWaitForFences(v.device, 1, &v.imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        v.imagesInFlight[imageIndex] = v.inFlight[v.currentFrame];

        // The geodesic offscreen image is shared across frames in flight; wait for
        // the previous frame to finish reading it before overwriting it this frame.
        if (v.geoInUse != VK_NULL_HANDLE)
            vkWaitForFences(v.device, 1, &v.geoInUse, VK_TRUE, UINT64_MAX);
        v.ProcessInput();
        v.UpdateGeodesicUniforms();

        vkResetCommandBuffer(v.commandBuffers[v.currentFrame], 0);
        if (!v.RecordCommandBuffer(v.commandBuffers[v.currentFrame], imageIndex, clearColor, drawData)) return;

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = &v.imageAvailable[v.currentFrame];
        submit.pWaitDstStageMask    = &waitStage;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &v.commandBuffers[v.currentFrame];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = &v.renderFinished[imageIndex];

        vkResetFences(v.device, 1, &v.inFlight[v.currentFrame]);
        if (vkQueueSubmit(v.graphicsQueue, 1, &submit, v.inFlight[v.currentFrame]) != VK_SUCCESS)
        { DONUT_ERROR("Vulkan: queue submit failed"); return; }
        v.geoInUse = v.inFlight[v.currentFrame];

        VkPresentInfoKHR present{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = &v.renderFinished[imageIndex];
        present.swapchainCount     = 1;
        present.pSwapchains        = &v.swapchain;
        present.pImageIndices      = &imageIndex;
        r = vkQueuePresentKHR(v.presentQueue, &present);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || v.framebufferResized)
        {
            v.framebufferResized = false;
            v.RecreateSwapchain();
        }

        v.currentFrame = (v.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VulkanRenderer::Shutdown()
    {
        Impl& v = *m_Impl;
        if (v.device == VK_NULL_HANDLE) { if (v.instance && v.surface) { vkDestroySurfaceKHR(v.instance, v.surface, nullptr); v.surface = VK_NULL_HANDLE; } if (v.instance) { vkDestroyInstance(v.instance, nullptr); v.instance = VK_NULL_HANDLE; } return; }

        vkDeviceWaitIdle(v.device);
        v.DestroyGeodesicResources();
        if (v.imguiInit)
        {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            v.imguiInit = false;
        }
        if (v.imguiPool) { vkDestroyDescriptorPool(v.device, v.imguiPool, nullptr); v.imguiPool = VK_NULL_HANDLE; }
        for (auto s : v.renderFinished) vkDestroySemaphore(v.device, s, nullptr);
        for (auto s : v.imageAvailable) vkDestroySemaphore(v.device, s, nullptr);
        for (auto f : v.inFlight)       vkDestroyFence(v.device, f, nullptr);
        v.renderFinished.clear(); v.imageAvailable.clear(); v.inFlight.clear();
        if (v.commandPool) { vkDestroyCommandPool(v.device, v.commandPool, nullptr); v.commandPool = VK_NULL_HANDLE; }
        v.CleanupSwapchain();
        vkDestroyDevice(v.device, nullptr); v.device = VK_NULL_HANDLE;
        if (v.surface)  { vkDestroySurfaceKHR(v.instance, v.surface, nullptr); v.surface = VK_NULL_HANDLE; }
        if (v.instance) { vkDestroyInstance(v.instance, nullptr); v.instance = VK_NULL_HANDLE; }
    }
}
