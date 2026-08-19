#include "VulkanContext.h"
#include "Core/Log.h"

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "stb_image_write.h"

#include <vector>
#include <cstring>
#include <cstdlib>
#include <fstream>

namespace Donut
{
    // Logs and returns false from the enclosing function on any non-success result.
    #define VK_CHECK(expr)                                                     \
        do {                                                                   \
            VkResult _r = (expr);                                              \
            if (_r != VK_SUCCESS) {                                            \
                DONUT_ERROR("Vulkan: {} failed ({})", #expr, (int)_r);         \
                return false;                                                  \
            }                                                                  \
        } while (0)

    struct VulkanContext::Impl
    {
        VkInstance       instance       = VK_NULL_HANDLE;
        VkPhysicalDevice physical       = VK_NULL_HANDLE;
        VkDevice         device         = VK_NULL_HANDLE;
        VkQueue          graphicsQueue  = VK_NULL_HANDLE;
        uint32_t         graphicsFamily = 0;
        VkPhysicalDeviceMemoryProperties memProps{};

        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags) const
        {
            for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
                if ((typeFilter & (1u << i)) &&
                    (memProps.memoryTypes[i].propertyFlags & flags) == flags)
                    return i;
            return UINT32_MAX;
        }

        bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
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
    };

    VulkanContext::VulkanContext()  { m_Impl = new Impl(); }
    VulkanContext::~VulkanContext() { Shutdown(); delete m_Impl; m_Impl = nullptr; }

    bool VulkanContext::Init()
    {
        Impl& v = *m_Impl;

#ifdef __APPLE__
        // The Homebrew Vulkan loader does not auto-discover MoltenVK or the
        // validation layers; point it at both unless already configured.
        if (!getenv("VK_ICD_FILENAMES"))
            setenv("VK_ICD_FILENAMES", "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json", 0);
        if (!getenv("VK_LAYER_PATH"))
            setenv("VK_LAYER_PATH", "/opt/homebrew/share/vulkan/explicit_layer.d", 0);
#endif

        // Instance
        VkApplicationInfo app{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
        app.pApplicationName = "Donut";
        app.apiVersion       = VK_API_VERSION_1_2;

        // MoltenVK is a portability driver: without the portability-enumeration
        // extension + flag, vkEnumeratePhysicalDevices returns zero devices.
        std::vector<const char*> exts = {
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
        };

        // Enable validation layers when they are installed (optional).
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
        VK_CHECK(vkCreateInstance(&ici, nullptr, &v.instance));

        // Physical device
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(v.instance, &deviceCount, nullptr);
        if (deviceCount == 0) { DONUT_ERROR("Vulkan: no physical devices"); return false; }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(v.instance, &deviceCount, devices.data());
        v.physical = devices[0];

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(v.physical, &props);
        vkGetPhysicalDeviceMemoryProperties(v.physical, &v.memProps);

        // Graphics queue family
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(v.physical, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfams(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(v.physical, &qCount, qfams.data());
        bool found = false;
        for (uint32_t i = 0; i < qCount; ++i)
            if (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { v.graphicsFamily = i; found = true; break; }
        if (!found) { DONUT_ERROR("Vulkan: no graphics queue family"); return false; }

        // Logical device
        // MoltenVK requires VK_KHR_portability_subset to be enabled if present.
        std::vector<const char*> devExts;
        uint32_t devExtCount = 0;
        vkEnumerateDeviceExtensionProperties(v.physical, nullptr, &devExtCount, nullptr);
        std::vector<VkExtensionProperties> devExtProps(devExtCount);
        vkEnumerateDeviceExtensionProperties(v.physical, nullptr, &devExtCount, devExtProps.data());
        for (const auto& e : devExtProps)
            if (std::strcmp(e.extensionName, "VK_KHR_portability_subset") == 0)
                devExts.push_back("VK_KHR_portability_subset");

        float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = v.graphicsFamily;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;

        VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
        dci.queueCreateInfoCount    = 1;
        dci.pQueueCreateInfos       = &qci;
        dci.enabledExtensionCount   = (uint32_t)devExts.size();
        dci.ppEnabledExtensionNames = devExts.data();
        VK_CHECK(vkCreateDevice(v.physical, &dci, nullptr, &v.device));
        vkGetDeviceQueue(v.device, v.graphicsFamily, 0, &v.graphicsQueue);

        DONUT_INFO("Vulkan device: {} (API {}.{}.{}, validation {})",
                   props.deviceName,
                   VK_API_VERSION_MAJOR(props.apiVersion),
                   VK_API_VERSION_MINOR(props.apiVersion),
                   VK_API_VERSION_PATCH(props.apiVersion),
                   layers.empty() ? "off" : "on");
        return true;
    }

    bool VulkanContext::SelfTestClear()
    {
        Impl& v = *m_Impl;
        if (v.device == VK_NULL_HANDLE) return false;

        const uint32_t W = 64, H = 64;
        const VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;

        // Offscreen colour image
        VkImage image = VK_NULL_HANDLE; VkDeviceMemory imageMem = VK_NULL_HANDLE;
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType   = VK_IMAGE_TYPE_2D;
        ici.format      = fmt;
        ici.extent      = { W, H, 1 };
        ici.mipLevels   = 1;
        ici.arrayLayers = 1;
        ici.samples     = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ici.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(v.device, &ici, nullptr, &image));

        VkMemoryRequirements imReq{};
        vkGetImageMemoryRequirements(v.device, image, &imReq);
        VkMemoryAllocateInfo imAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        imAlloc.allocationSize  = imReq.size;
        imAlloc.memoryTypeIndex = v.FindMemoryType(imReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(v.device, &imAlloc, nullptr, &imageMem));
        VK_CHECK(vkBindImageMemory(v.device, image, imageMem, 0));

        VkImageView view = VK_NULL_HANDLE;
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image    = image;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format   = fmt;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(v.device, &vci, nullptr, &view));

        // Render pass (clear -> store, leave in TRANSFER_SRC for readback)
        VkAttachmentDescription color{};
        color.format         = fmt;
        color.samples        = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;

        // Ensure colour writes finish before the read-back copy.
        VkSubpassDependency dep{};
        dep.srcSubpass    = 0;
        dep.dstSubpass    = VK_SUBPASS_EXTERNAL;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dep.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1; rpci.pAttachments = &color;
        rpci.subpassCount    = 1; rpci.pSubpasses   = &subpass;
        rpci.dependencyCount = 1; rpci.pDependencies = &dep;
        VK_CHECK(vkCreateRenderPass(v.device, &rpci, nullptr, &renderPass));

        VkFramebuffer fb = VK_NULL_HANDLE;
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = renderPass;
        fbci.attachmentCount = 1; fbci.pAttachments = &view;
        fbci.width = W; fbci.height = H; fbci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(v.device, &fbci, nullptr, &fb));

        // Host-visible staging buffer for read-back
        VkBuffer staging = VK_NULL_HANDLE; VkDeviceMemory stagingMem = VK_NULL_HANDLE;
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size  = (VkDeviceSize)W * H * 4;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(v.device, &bci, nullptr, &staging));
        VkMemoryRequirements bReq{};
        vkGetBufferMemoryRequirements(v.device, staging, &bReq);
        VkMemoryAllocateInfo bAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        bAlloc.allocationSize  = bReq.size;
        bAlloc.memoryTypeIndex = v.FindMemoryType(bReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(v.device, &bAlloc, nullptr, &stagingMem));
        VK_CHECK(vkBindBufferMemory(v.device, staging, stagingMem, 0));

        // Command buffer: clear via render pass, then copy image -> buffer
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pci.queueFamilyIndex = v.graphicsFamily;
        VK_CHECK(vkCreateCommandPool(v.device, &pci, nullptr, &pool));

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandPool = pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(v.device, &cbai, &cmd));

        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

        VkClearValue clear{};
        clear.color = { { 0.2f, 0.4f, 0.8f, 1.0f } };   // -> RGBA8 (51, 102, 204, 255)
        VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpbi.renderPass = renderPass; rpbi.framebuffer = fb;
        rpbi.renderArea = { { 0, 0 }, { W, H } };
        rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(cmd);

        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent      = { W, H, 1 };
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK_CHECK(vkCreateFence(v.device, &fci, nullptr, &fence));
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(v.graphicsQueue, 1, &submit, fence));
        VK_CHECK(vkWaitForFences(v.device, 1, &fence, VK_TRUE, UINT64_MAX));

        // Read back + verify
        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(v.device, stagingMem, 0, bci.size, 0, &mapped));
        const uint8_t* px = (const uint8_t*)mapped;
        DONUT_INFO("Vulkan clear self-test: pixel RGBA = ({}, {}, {}, {})",
                   (int)px[0], (int)px[1], (int)px[2], (int)px[3]);
        bool ok = px[0] > 45 && px[0] < 60 && px[1] > 95 && px[1] < 110 &&
                  px[2] > 195 && px[2] < 210 && px[3] == 255;
        vkUnmapMemory(v.device, stagingMem);
        DONUT_INFO("Vulkan clear self-test: {}", ok ? "PASS" : "FAIL");

        // Cleanup
        vkDestroyFence(v.device, fence, nullptr);
        vkDestroyCommandPool(v.device, pool, nullptr);
        vkDestroyBuffer(v.device, staging, nullptr);
        vkFreeMemory(v.device, stagingMem, nullptr);
        vkDestroyFramebuffer(v.device, fb, nullptr);
        vkDestroyRenderPass(v.device, renderPass, nullptr);
        vkDestroyImageView(v.device, view, nullptr);
        vkDestroyImage(v.device, image, nullptr);
        vkFreeMemory(v.device, imageMem, nullptr);
        return ok;
    }

    static std::vector<uint32_t> LoadSpirv(const std::string& path)
    {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return {};
        size_t size = (size_t)f.tellg();
        std::vector<uint32_t> data(size / 4);
        f.seekg(0);
        f.read((char*)data.data(), (std::streamsize)size);
        return data;
    }

    bool VulkanContext::SelfTestTriangle()
    {
        Impl& v = *m_Impl;
        if (v.device == VK_NULL_HANDLE) return false;

        const uint32_t W = 64, H = 64;
        const VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;

        // Offscreen image + view (as in the clear test)
        VkImage image = VK_NULL_HANDLE; VkDeviceMemory imageMem = VK_NULL_HANDLE;
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D; ici.format = fmt; ici.extent = { W, H, 1 };
        ici.mipLevels = 1; ici.arrayLayers = 1; ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VK_CHECK(vkCreateImage(v.device, &ici, nullptr, &image));
        VkMemoryRequirements imReq{}; vkGetImageMemoryRequirements(v.device, image, &imReq);
        VkMemoryAllocateInfo imAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        imAlloc.allocationSize = imReq.size;
        imAlloc.memoryTypeIndex = v.FindMemoryType(imReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(v.device, &imAlloc, nullptr, &imageMem));
        VK_CHECK(vkBindImageMemory(v.device, image, imageMem, 0));
        VkImageView view = VK_NULL_HANDLE;
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = image; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = fmt;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(v.device, &vci, nullptr, &view));

        // Render pass + framebuffer
        VkAttachmentDescription color{};
        color.format = fmt; color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorRef;
        VkSubpassDependency dep{};
        dep.srcSubpass = 0; dep.dstSubpass = VK_SUBPASS_EXTERNAL;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT; dep.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1; rpci.pAttachments = &color;
        rpci.subpassCount = 1; rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 1; rpci.pDependencies = &dep;
        VK_CHECK(vkCreateRenderPass(v.device, &rpci, nullptr, &renderPass));
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = renderPass; fbci.attachmentCount = 1; fbci.pAttachments = &view;
        fbci.width = W; fbci.height = H; fbci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(v.device, &fbci, nullptr, &fb));

        // Shader modules from Slang SPIR-V
        auto vspv = LoadSpirv("Assets/Shaders/generated/VkPipelineTest.vertexMain.spv");
        auto fspv = LoadSpirv("Assets/Shaders/generated/VkPipelineTest.fragmentMain.spv");
        if (vspv.empty() || fspv.empty()) { DONUT_ERROR("Vulkan: VkPipelineTest SPIR-V not found"); return false; }
        VkShaderModule vmod = VK_NULL_HANDLE, fmod = VK_NULL_HANDLE;
        VkShaderModuleCreateInfo smci{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        smci.codeSize = vspv.size() * 4; smci.pCode = vspv.data();
        VK_CHECK(vkCreateShaderModule(v.device, &smci, nullptr, &vmod));
        smci.codeSize = fspv.size() * 4; smci.pCode = fspv.data();
        VK_CHECK(vkCreateShaderModule(v.device, &smci, nullptr, &fmod));

        // Graphics pipeline
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp{ 0, 0, (float)W, (float)H, 0, 1 };
        VkRect2D scissor{ { 0, 0 }, { W, H } };
        VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        vps.viewportCount = 1; vps.pViewports = &vp; vps.scissorCount = 1; vps.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        cb.attachmentCount = 1; cb.pAttachments = &cba;

        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        VK_CHECK(vkCreatePipelineLayout(v.device, &plci, nullptr, &layout));

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2; gpci.pStages = stages;
        gpci.pVertexInputState = &vin; gpci.pInputAssemblyState = &ia;
        gpci.pViewportState = &vps; gpci.pRasterizationState = &rs;
        gpci.pMultisampleState = &ms; gpci.pColorBlendState = &cb;
        gpci.layout = layout; gpci.renderPass = renderPass; gpci.subpass = 0;
        VK_CHECK(vkCreateGraphicsPipelines(v.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline));

        // Readback staging buffer
        VkBuffer staging = VK_NULL_HANDLE; VkDeviceMemory stagingMem = VK_NULL_HANDLE;
        VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bci.size = (VkDeviceSize)W * H * 4; bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VK_CHECK(vkCreateBuffer(v.device, &bci, nullptr, &staging));
        VkMemoryRequirements bReq{}; vkGetBufferMemoryRequirements(v.device, staging, &bReq);
        VkMemoryAllocateInfo bAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        bAlloc.allocationSize = bReq.size;
        bAlloc.memoryTypeIndex = v.FindMemoryType(bReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(v.device, &bAlloc, nullptr, &stagingMem));
        VK_CHECK(vkBindBufferMemory(v.device, staging, stagingMem, 0));

        // Record + submit
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pci.queueFamilyIndex = v.graphicsFamily;
        VK_CHECK(vkCreateCommandPool(v.device, &pci, nullptr, &pool));
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cbai.commandPool = pool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(v.device, &cbai, &cmd));
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
        VkClearValue clear{}; clear.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpbi.renderPass = renderPass; rpbi.framebuffer = fb;
        rpbi.renderArea = { { 0, 0 }, { W, H } };
        rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent = { W, H, 1 };
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging, 1, &region);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK_CHECK(vkCreateFence(v.device, &fci, nullptr, &fence));
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(v.graphicsQueue, 1, &submit, fence));
        VK_CHECK(vkWaitForFences(v.device, 1, &fence, VK_TRUE, UINT64_MAX));

        // Verify: centre pixel should be the mid-gradient (not black)
        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(v.device, stagingMem, 0, bci.size, 0, &mapped));
        const uint8_t* px = (const uint8_t*)mapped;
        size_t c = ((size_t)(H / 2) * W + (W / 2)) * 4;
        DONUT_INFO("Vulkan triangle self-test: centre pixel RGBA = ({}, {}, {}, {})",
                   (int)px[c + 0], (int)px[c + 1], (int)px[c + 2], (int)px[c + 3]);
        bool ok = (px[c + 0] > 40 || px[c + 1] > 40) && px[c + 3] == 255;
        vkUnmapMemory(v.device, stagingMem);
        DONUT_INFO("Vulkan triangle self-test: {}", ok ? "PASS" : "FAIL");

        // Cleanup
        vkDestroyFence(v.device, fence, nullptr);
        vkDestroyCommandPool(v.device, pool, nullptr);
        vkDestroyBuffer(v.device, staging, nullptr);
        vkFreeMemory(v.device, stagingMem, nullptr);
        vkDestroyPipeline(v.device, pipeline, nullptr);
        vkDestroyPipelineLayout(v.device, layout, nullptr);
        vkDestroyShaderModule(v.device, vmod, nullptr);
        vkDestroyShaderModule(v.device, fmod, nullptr);
        vkDestroyFramebuffer(v.device, fb, nullptr);
        vkDestroyRenderPass(v.device, renderPass, nullptr);
        vkDestroyImageView(v.device, view, nullptr);
        vkDestroyImage(v.device, image, nullptr);
        vkFreeMemory(v.device, imageMem, nullptr);
        return ok;
    }

    bool VulkanContext::RenderGeodesic(const char* pngPath)
    {
        Impl& v = *m_Impl;
        if (v.device == VK_NULL_HANDLE) return false;

        const uint32_t W = 384, H = 216;
        const VkFormat fmt = VK_FORMAT_R8G8B8A8_UNORM;
        const float SagA_rs = 1.269e10f;

        // Offscreen colour target
        VkImage image = VK_NULL_HANDLE; VkDeviceMemory imageMem = VK_NULL_HANDLE;
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D; ici.format = fmt; ici.extent = { W, H, 1 };
        ici.mipLevels = 1; ici.arrayLayers = 1; ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        VK_CHECK(vkCreateImage(v.device, &ici, nullptr, &image));
        VkMemoryRequirements imReq{}; vkGetImageMemoryRequirements(v.device, image, &imReq);
        VkMemoryAllocateInfo imAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        imAlloc.allocationSize = imReq.size;
        imAlloc.memoryTypeIndex = v.FindMemoryType(imReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(v.device, &imAlloc, nullptr, &imageMem));
        VK_CHECK(vkBindImageMemory(v.device, image, imageMem, 0));
        VkImageView view = VK_NULL_HANDLE;
        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = image; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = fmt;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VK_CHECK(vkCreateImageView(v.device, &vci, nullptr, &view));

        VkAttachmentDescription color{};
        color.format = fmt; color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorRef;
        VkSubpassDependency dep{};
        dep.srcSubpass = 0; dep.dstSubpass = VK_SUBPASS_EXTERNAL;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT; dep.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpci.attachmentCount = 1; rpci.pAttachments = &color;
        rpci.subpassCount = 1; rpci.pSubpasses = &subpass;
        rpci.dependencyCount = 1; rpci.pDependencies = &dep;
        VK_CHECK(vkCreateRenderPass(v.device, &rpci, nullptr, &renderPass));
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = renderPass; fbci.attachmentCount = 1; fbci.pAttachments = &view;
        fbci.width = W; fbci.height = H; fbci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(v.device, &fbci, nullptr, &fb));

        // Uniform buffers (host-visible), filled to match the shader's std140 layout
        const VkMemoryPropertyFlags hostVis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBuffer camBuf, diskBuf, objBuf, simBuf;
        VkDeviceMemory camMem, diskMem, objMem, simMem;
        v.CreateBuffer(128, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, camBuf, camMem);
        v.CreateBuffer(32,  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, diskBuf, diskMem);
        v.CreateBuffer(800, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, objBuf, objMem);
        v.CreateBuffer(16,  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, hostVis, simBuf, simMem);

        struct CamUBO {
            glm::vec3 pos; float p0; glm::vec3 right; float p1;
            glm::vec3 up; float p2; glm::vec3 fwd; float p3;
            float tanHalfFov; float aspect; uint32_t moving; int p4;
        } cam{};
        glm::vec3 camPos(1e11f, 0.32e11f, 0.0f);
        glm::vec3 fwd = glm::normalize(glm::vec3(0.0f) - camPos);
        glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        glm::vec3 up = glm::cross(right, fwd);
        cam.pos = camPos; cam.right = right; cam.up = up; cam.fwd = fwd;
        cam.tanHalfFov = 0.57735f; cam.aspect = (float)W / (float)H; cam.moving = 0;
        void* p = nullptr;
        vkMapMemory(v.device, camMem, 0, 128, 0, &p); memcpy(p, &cam, sizeof(cam)); vkUnmapMemory(v.device, camMem);

        float disk[8] = { SagA_rs * 2.2f, SagA_rs * 5.2f, 2.0f, SagA_rs * 0.1f, 0.1f, 0, 0, 0 };
        vkMapMemory(v.device, diskMem, 0, 32, 0, &p); memcpy(p, disk, sizeof(disk)); vkUnmapMemory(v.device, diskMem);

        std::vector<uint8_t> objData(800, 0);
        int numObjects = 1; memcpy(objData.data(), &numObjects, 4);
        float posRadius[4] = { 0, 0, 0, SagA_rs }; memcpy(objData.data() + 16, posRadius, 16);
        float objColor[4] = { 0, 0, 0, 1 };        memcpy(objData.data() + 272, objColor, 16);
        vkMapMemory(v.device, objMem, 0, 800, 0, &p); memcpy(p, objData.data(), 800); vkUnmapMemory(v.device, objMem);

        struct SimUBO { int stepsMoving; int stepsStatic; float earlyExit; float time; } sim{ 6000, 6000, 5e12f, 0.0f };
        vkMapMemory(v.device, simMem, 0, 16, 0, &p); memcpy(p, &sim, sizeof(sim)); vkUnmapMemory(v.device, simMem);

        // Dark cubemap (stands in for the HDRI for now)
        VkImage cube = VK_NULL_HANDLE; VkDeviceMemory cubeMem = VK_NULL_HANDLE;
        VkImageCreateInfo cci{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        cci.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        cci.imageType = VK_IMAGE_TYPE_2D; cci.format = fmt; cci.extent = { 1, 1, 1 };
        cci.mipLevels = 1; cci.arrayLayers = 6; cci.samples = VK_SAMPLE_COUNT_1_BIT;
        cci.tiling = VK_IMAGE_TILING_OPTIMAL;
        cci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK(vkCreateImage(v.device, &cci, nullptr, &cube));
        VkMemoryRequirements cubeReq{}; vkGetImageMemoryRequirements(v.device, cube, &cubeReq);
        VkMemoryAllocateInfo cubeAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        cubeAlloc.allocationSize = cubeReq.size;
        cubeAlloc.memoryTypeIndex = v.FindMemoryType(cubeReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(v.device, &cubeAlloc, nullptr, &cubeMem));
        VK_CHECK(vkBindImageMemory(v.device, cube, cubeMem, 0));
        VkImageView cubeView = VK_NULL_HANDLE;
        VkImageViewCreateInfo cvci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        cvci.image = cube; cvci.viewType = VK_IMAGE_VIEW_TYPE_CUBE; cvci.format = fmt;
        cvci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
        VK_CHECK(vkCreateImageView(v.device, &cvci, nullptr, &cubeView));
        VkSampler sampler = VK_NULL_HANDLE;
        VkSamplerCreateInfo smci{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
        smci.magFilter = VK_FILTER_LINEAR; smci.minFilter = VK_FILTER_LINEAR;
        smci.addressModeU = smci.addressModeV = smci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_CHECK(vkCreateSampler(v.device, &smci, nullptr, &sampler));

        VkBuffer cubeStaging; VkDeviceMemory cubeStagingMem;
        v.CreateBuffer(6 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, hostVis, cubeStaging, cubeStagingMem);
        uint8_t cubePixels[6 * 4];
        for (int i = 0; i < 6; ++i) { cubePixels[i * 4 + 0] = 6; cubePixels[i * 4 + 1] = 6; cubePixels[i * 4 + 2] = 14; cubePixels[i * 4 + 3] = 255; }
        vkMapMemory(v.device, cubeStagingMem, 0, 24, 0, &p); memcpy(p, cubePixels, 24); vkUnmapMemory(v.device, cubeStagingMem);

        // Descriptor set: 4 UBOs (bindings 0-3) + cubemap sampler (binding 4)
        VkDescriptorSetLayoutBinding binds[5]{};
        for (int i = 0; i < 4; ++i) { binds[i].binding = i; binds[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; binds[i].descriptorCount = 1; binds[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; }
        binds[4].binding = 4; binds[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; binds[4].descriptorCount = 1; binds[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo dslci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        dslci.bindingCount = 5; dslci.pBindings = binds;
        VK_CHECK(vkCreateDescriptorSetLayout(v.device, &dslci, nullptr, &setLayout));
        VkDescriptorPoolSize psizes[2] = { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorPoolCreateInfo dpci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        dpci.maxSets = 1; dpci.poolSizeCount = 2; dpci.pPoolSizes = psizes;
        VK_CHECK(vkCreateDescriptorPool(v.device, &dpci, nullptr, &pool));
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dsai.descriptorPool = pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &setLayout;
        VK_CHECK(vkAllocateDescriptorSets(v.device, &dsai, &set));

        // Load geodesic SPIR-V + build the pipeline
        auto vspv = LoadSpirv("Assets/Shaders/generated/Geodesic.vertexMain.spv");
        auto fspv = LoadSpirv("Assets/Shaders/generated/Geodesic.fragmentMain.spv");
        if (vspv.empty() || fspv.empty()) { DONUT_ERROR("Vulkan: geodesic SPIR-V not found"); return false; }
        VkShaderModule vmod, fmod;
        VkShaderModuleCreateInfo smci2{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        smci2.codeSize = vspv.size() * 4; smci2.pCode = vspv.data(); VK_CHECK(vkCreateShaderModule(v.device, &smci2, nullptr, &vmod));
        smci2.codeSize = fspv.size() * 4; smci2.pCode = fspv.data(); VK_CHECK(vkCreateShaderModule(v.device, &smci2, nullptr, &fmod));

        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipelineLayoutCreateInfo plci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plci.setLayoutCount = 1; plci.pSetLayouts = &setLayout;
        VK_CHECK(vkCreatePipelineLayout(v.device, &plci, nullptr, &layout));

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";
        VkVertexInputBindingDescription vib{ 0, 16, VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription via[2] = { { 0, 0, VK_FORMAT_R32G32_SFLOAT, 0 }, { 1, 0, VK_FORMAT_R32G32_SFLOAT, 8 } };
        VkPipelineVertexInputStateCreateInfo vin{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &vib;
        vin.vertexAttributeDescriptionCount = 2; vin.pVertexAttributeDescriptions = via;
        VkPipelineInputAssemblyStateCreateInfo ia{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO }; ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp{ 0, 0, (float)W, (float)H, 0, 1 }; VkRect2D sc{ { 0, 0 }, { W, H } };
        VkPipelineViewportStateCreateInfo vps{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO }; vps.viewportCount = 1; vps.pViewports = &vp; vps.scissorCount = 1; vps.pScissors = &sc;
        VkPipelineRasterizationStateCreateInfo rs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO }; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo ms{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO }; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba{}; cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO }; cb.attachmentCount = 1; cb.pAttachments = &cba;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkGraphicsPipelineCreateInfo gpci{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        gpci.stageCount = 2; gpci.pStages = stages;
        gpci.pVertexInputState = &vin; gpci.pInputAssemblyState = &ia; gpci.pViewportState = &vps;
        gpci.pRasterizationState = &rs; gpci.pMultisampleState = &ms; gpci.pColorBlendState = &cb;
        gpci.layout = layout; gpci.renderPass = renderPass; gpci.subpass = 0;
        VK_CHECK(vkCreateGraphicsPipelines(v.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline));

        // Fullscreen quad (position.xy, texcoord.uv)
        float quad[] = {
            -1.f,  1.f, 0.f, 1.f,  -1.f, -1.f, 0.f, 0.f,   1.f, -1.f, 1.f, 0.f,
            -1.f,  1.f, 0.f, 1.f,   1.f, -1.f, 1.f, 0.f,   1.f,  1.f, 1.f, 1.f,
        };
        VkBuffer vbuf; VkDeviceMemory vbufMem;
        v.CreateBuffer(sizeof(quad), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostVis, vbuf, vbufMem);
        vkMapMemory(v.device, vbufMem, 0, sizeof(quad), 0, &p); memcpy(p, quad, sizeof(quad)); vkUnmapMemory(v.device, vbufMem);

        // Write the descriptor set
        VkDescriptorBufferInfo bi[4] = {
            { camBuf, 0, VK_WHOLE_SIZE }, { diskBuf, 0, VK_WHOLE_SIZE }, { objBuf, 0, VK_WHOLE_SIZE }, { simBuf, 0, VK_WHOLE_SIZE } };
        VkDescriptorImageInfo ii{ sampler, cubeView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        VkWriteDescriptorSet writes[5]{};
        for (int i = 0; i < 4; ++i) { writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[i].dstSet = set; writes[i].dstBinding = i; writes[i].descriptorCount = 1; writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; writes[i].pBufferInfo = &bi[i]; }
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; writes[4].dstSet = set; writes[4].dstBinding = 4; writes[4].descriptorCount = 1; writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[4].pImageInfo = &ii;
        vkUpdateDescriptorSets(v.device, 5, writes, 0, nullptr);

        VkBuffer readback; VkDeviceMemory readbackMem;
        v.CreateBuffer((VkDeviceSize)W * H * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, hostVis, readback, readbackMem);

        // Record + submit
        VkCommandPool cpool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo pci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO }; pci.queueFamilyIndex = v.graphicsFamily;
        VK_CHECK(vkCreateCommandPool(v.device, &pci, nullptr, &cpool));
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO }; cbai.commandPool = cpool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(v.device, &cbai, &cmd));
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO }; begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

        VkImageMemoryBarrier toDst{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toDst.image = cube; toDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
        toDst.srcAccessMask = 0; toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
        VkBufferImageCopy cubeCopy{}; cubeCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 6 }; cubeCopy.imageExtent = { 1, 1, 1 };
        vkCmdCopyBufferToImage(cmd, cubeStaging, cube, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &cubeCopy);
        VkImageMemoryBarrier toRead = toDst;
        toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead);

        VkClearValue clear{}; clear.color = { { 0, 0, 0, 1 } };
        VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpbi.renderPass = renderPass; rpbi.framebuffer = fb; rpbi.renderArea = { { 0, 0 }, { W, H } };
        rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &set, 0, nullptr);
        VkDeviceSize voff = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &voff);
        vkCmdDraw(cmd, 6, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
        VkBufferImageCopy region{}; region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }; region.imageExtent = { W, H, 1 };
        vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1, &region);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkFence fence = VK_NULL_HANDLE; VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK_CHECK(vkCreateFence(v.device, &fci, nullptr, &fence));
        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO }; submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
        VK_CHECK(vkQueueSubmit(v.graphicsQueue, 1, &submit, fence));
        VK_CHECK(vkWaitForFences(v.device, 1, &fence, VK_TRUE, UINT64_MAX));

        vkMapMemory(v.device, readbackMem, 0, (VkDeviceSize)W * H * 4, 0, &p);
        stbi_write_png(pngPath, W, H, 4, p, W * 4);
        vkUnmapMemory(v.device, readbackMem);
        DONUT_INFO("Vulkan geodesic render written to {}", pngPath);

        vkDestroyFence(v.device, fence, nullptr);
        vkDestroyCommandPool(v.device, cpool, nullptr);
        vkDestroyBuffer(v.device, readback, nullptr); vkFreeMemory(v.device, readbackMem, nullptr);
        vkDestroyBuffer(v.device, vbuf, nullptr); vkFreeMemory(v.device, vbufMem, nullptr);
        vkDestroyPipeline(v.device, pipeline, nullptr); vkDestroyPipelineLayout(v.device, layout, nullptr);
        vkDestroyShaderModule(v.device, vmod, nullptr); vkDestroyShaderModule(v.device, fmod, nullptr);
        vkDestroyDescriptorPool(v.device, pool, nullptr); vkDestroyDescriptorSetLayout(v.device, setLayout, nullptr);
        vkDestroySampler(v.device, sampler, nullptr); vkDestroyImageView(v.device, cubeView, nullptr);
        vkDestroyImage(v.device, cube, nullptr); vkFreeMemory(v.device, cubeMem, nullptr);
        vkDestroyBuffer(v.device, cubeStaging, nullptr); vkFreeMemory(v.device, cubeStagingMem, nullptr);
        vkDestroyBuffer(v.device, camBuf, nullptr); vkFreeMemory(v.device, camMem, nullptr);
        vkDestroyBuffer(v.device, diskBuf, nullptr); vkFreeMemory(v.device, diskMem, nullptr);
        vkDestroyBuffer(v.device, objBuf, nullptr); vkFreeMemory(v.device, objMem, nullptr);
        vkDestroyBuffer(v.device, simBuf, nullptr); vkFreeMemory(v.device, simMem, nullptr);
        vkDestroyFramebuffer(v.device, fb, nullptr); vkDestroyRenderPass(v.device, renderPass, nullptr);
        vkDestroyImageView(v.device, view, nullptr); vkDestroyImage(v.device, image, nullptr); vkFreeMemory(v.device, imageMem, nullptr);
        return true;
    }

    void VulkanContext::Shutdown()
    {
        Impl& v = *m_Impl;
        if (v.device)   { vkDestroyDevice(v.device, nullptr);   v.device = VK_NULL_HANDLE; }
        if (v.instance) { vkDestroyInstance(v.instance, nullptr); v.instance = VK_NULL_HANDLE; }
    }

    bool VulkanSelfTest()
    {
        VulkanContext ctx;
        if (!ctx.Init())
        {
            DONUT_ERROR("Vulkan: initialization failed");
            return false;
        }
        bool ok = ctx.SelfTestClear();
        ok = ctx.SelfTestTriangle() && ok;
        ctx.Shutdown();
        return ok;
    }
}
