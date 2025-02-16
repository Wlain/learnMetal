//
// Created by cwb on 2022/9/8.
//

#include "deviceVk.h"

#include "commonMacro.h"
#include "vkCommonDefine.h"
#define GLFW_INCLUDE_NONE
#include "textureVk.h"

#include <GLFW/glfw3.h>
#include <set>
#include <unordered_set>

#ifndef NDEBUG
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger)
{
    auto address = vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(address);
    if (func)
    {
        return func(instance, pCreateInfo, pAllocator, pMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator)
{
    auto address = vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(address);
    if (func)
    {
        return func(instance, messenger, pAllocator);
    }
}

bool checkValidationLayerSupport()
{
    auto layerProperties = vk::enumerateInstanceLayerProperties();
    return std::any_of(layerProperties.begin(), layerProperties.end(), [](const auto& property) {
        return std::strcmp("VK_LAYER_KHRONOS_validation", property.layerName) == 0;
    });
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    LOG_ERROR("validation layer:\n {}", pCallbackData->pMessage);
    return VK_FALSE;
}
#endif

std::vector<const char*> getLayers()
{
    std::vector<const char*> layers;
#ifndef NDEBUG
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
    return layers;
}

std::vector<const char*> getInstanceExtensions()
{
    auto glfwExtensionCount = 0u;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#ifndef NDEBUG
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    return extensions;
}

std::vector<const char*> getDeviceExtensions()
{
    return {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset"
    };
}

bool checkDeviceExtensionSupport(vk::PhysicalDevice gpu)
{
    auto availableExtensions = gpu.enumerateDeviceExtensionProperties();
    auto deviceExtensions = getDeviceExtensions();
    auto requiredExtensions = std::unordered_set<std::string>(deviceExtensions.cbegin(), deviceExtensions.cend());
    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
    auto it = std::find_if(std::begin(availableFormats), std::end(availableFormats), [](vk::SurfaceFormatKHR format) {
        return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return it == availableFormats.cend() ? availableFormats.front() : *it;
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
    auto it = std::find(std::begin(availablePresentModes), std::end(availablePresentModes), vk::PresentModeKHR::eMailbox);
    return it == availablePresentModes.cend() ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eMailbox;
}

namespace backend
{
vk::Extent2D DeviceVk::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    return vk::Extent2D{ static_cast<uint32_t>(m_info.width), static_cast<uint32_t>(m_info.height) };
    //    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    //    {
    //        return capabilities.currentExtent;
    //    }
    //    else
    //    {
    //        auto actualExtent = vk::Extent2D{ static_cast<uint32_t>(m_info.width), static_cast<uint32_t>(m_info.height) };
    //        actualExtent.width = std::clamp(
    //            actualExtent.width,
    //            capabilities.minImageExtent.width,
    //            capabilities.maxImageExtent.width);
    //        actualExtent.height = std::clamp(
    //            actualExtent.height,
    //            capabilities.minImageExtent.height,
    //            capabilities.maxImageExtent.height);
    //        return actualExtent;
    //    }
}

DeviceVk::~DeviceVk()
{
    m_device.destroy(m_renderPass);
    for (auto& view : m_swapchainImagesViews)
    {
        m_device.destroy(view);
    }
    for (auto& framebuffer : m_swapchainFramebuffers)
    {
        m_device.destroy(framebuffer);
    }
    m_swapchainFramebuffers.clear();
    m_device.freeCommandBuffers(m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
    for (auto fence : m_inflightFences)
    {
        m_device.destroyFence(fence);
    }
    for (auto semaphore : m_renderFinishedSemaphores)
    {
        m_device.destroySemaphore(semaphore);
    }
    for (auto semaphore : m_imageAvailableSemaphores)
    {
        m_device.destroySemaphore(semaphore);
    }
    m_device.destroyCommandPool(m_commandPool);
    m_depthTexture = nullptr;
    m_device.destroy(m_swapChain);
    m_device.destroy();
#ifndef NDEBUG
    m_instance.destroyDebugUtilsMessengerEXT(m_messenger);
#endif
    m_instance.destroy(m_surface);
    m_instance.destroy();
}

DeviceVk::DeviceVk(const Info& info) :
    Device(info)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void DeviceVk::init()
{
    Device::init();
    initInstance();
#ifndef NDEBUG
    initDebugger();
#endif
    initSurface();
    pickPhysicalDevice();
    initDevice();
    creatSwapChain();
    createImageViews();
    creatRenderPass();
    createFrameBuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

const vk::Instance& DeviceVk::instance() const
{
    return m_instance;
}

const vk::PhysicalDevice& DeviceVk::gpu() const
{
    return m_gpu;
}

const vk::Device& DeviceVk::handle() const
{
    return m_device;
}

void DeviceVk::initDevice()
{
    // 创建 Device 和 命令队列
    // 两个命令队列
    // 一个是绘制命令：queueFamilyProperties
    // 一个是显示命令: presentQueueFamilyIndex
    // get the QueueFamilyProperties of the first PhysicalDevice
    auto indices = findQueueFamilyIndices(m_gpu);
    auto uniqueQueueFamilyIndices = std::set<uint32_t>{
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 0.0f;
    for (auto& index : uniqueQueueFamilyIndices)
    {
        auto queueCreateInfo = vk::DeviceQueueCreateInfo{
            .queueFamilyIndex = index,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.emplace_back(queueCreateInfo);
    }
    auto gpuDeviceFeatures = vk::PhysicalDeviceFeatures{};
    auto layer = getLayers();
    auto deviceExtensions = getDeviceExtensions();
    auto createInfo = vk::DeviceCreateInfo{
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(layer.size()),
        .ppEnabledLayerNames = layer.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &gpuDeviceFeatures
    };
    m_device = m_gpu.createDevice(createInfo);
    m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.value(), 0);
    m_presentQueue = m_device.getQueue(indices.presentFamily.value(), 0);
}

void DeviceVk::initInstance()
{
#ifndef NDEBUG
    if (!checkValidationLayerSupport())
    {
        LOG_ERROR("validation layer is requested, but not available!");
    }
#endif
    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo appInfo{
        .pApplicationName = m_info.title.c_str(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = m_info.title.c_str(),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_1
    };
    // initialize the vk::InstanceCreateInfo
    auto layers = getLayers();
    auto instanceExtensions = getInstanceExtensions();
    auto instanceCreateInfo = vk::InstanceCreateInfo{
        .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
        .ppEnabledExtensionNames = instanceExtensions.data()
    };
    m_instance = vk::createInstance(instanceCreateInfo);
}
#ifndef NDEBUG
void DeviceVk::initDebugger()
{
    auto messageSeverity = vk::DebugUtilsMessageSeverityFlagsEXT{
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    };
    auto messageType = vk::DebugUtilsMessageTypeFlagsEXT{
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
    };
    auto createInfo = vk::DebugUtilsMessengerCreateInfoEXT{
        .messageSeverity = messageSeverity,
        .messageType = messageType,
        .pfnUserCallback = debugCallback
    };
    m_messenger = m_instance.createDebugUtilsMessengerEXT(createInfo);
    (void)m_messenger;
}
#endif

void DeviceVk::initSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface))
    {
        LOG_ERROR("failed to create window surface!");
    }
    m_surface = surface;
}

void DeviceVk::pickPhysicalDevice()
{
    /// 挑选一块显卡
    // 创建 gpu
    auto gpus = m_instance.enumeratePhysicalDevices();
    if (gpus.empty())
    {
        LOG_ERROR("failed to find gpus with Vulkan support!");
    }
    // 打印显卡名
    for (const auto& gpu : gpus)
    {
        LOG_INFO("gpu:{}", gpu.getProperties().deviceName);
        if (isDeviceSuitable(gpu))
        {
            m_gpu = gpu;
            break;
        }
    }
    if (!m_gpu)
    {
        LOG_ERROR("failed to find a suitable GPU!");
    }
}

void DeviceVk::creatSwapChain()
{
    auto details = querySwapchainSupport(m_gpu);
    auto surfaceFormat = chooseSwapSurfaceFormat(details.formats);
    auto presentMode = chooseSwapPresentMode(details.presentModes);
    auto extent = chooseSwapExtent(details.capabilities);
    uint32_t imageCount = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
    {
        imageCount = details.capabilities.maxImageCount;
    }
    auto createInfo = vk::SwapchainCreateInfoKHR{
        .surface = m_surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform = details.capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = VK_TRUE
    };
    QueueFamilyIndices indices = findQueueFamilyIndices(m_gpu);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    m_swapChain = m_device.createSwapchainKHR(createInfo);
    m_swapchainImages = m_device.getSwapchainImagesKHR(m_swapChain);
    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;
    m_swapchainImagesViews.reserve(m_swapchainImages.size());
}

const vk::SwapchainKHR& DeviceVk::swapChain() const
{
    return m_swapChain;
}

const vk::SurfaceKHR& DeviceVk::surface() const
{
    return m_surface;
}

uint32_t DeviceVk::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
    auto memProperties = m_gpu.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    LOG_ERROR("failed to find suitable memory type!");
    ASSERT(0);
    return 0;
}

bool DeviceVk::isDeviceSuitable(vk::PhysicalDevice gpu)
{
    auto indices = findQueueFamilyIndices(gpu);
    bool extensionsSupported = checkDeviceExtensionSupport(gpu);
    bool swapchainAdequate{ false };
    if (extensionsSupported)
    {
        auto details = querySwapchainSupport(gpu);
        swapchainAdequate = !details.formats.empty() && !details.presentModes.empty();
    }
    return indices.isComplete() && extensionsSupported && swapchainAdequate;
}

QueueFamilyIndices DeviceVk::findQueueFamilyIndices(vk::PhysicalDevice gpu)
{
    auto queueFamilyProperties = gpu.getQueueFamilyProperties();
    QueueFamilyIndices indices;
    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilyProperties)
    {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphicsFamily = i;
        }
        if (gpu.getSurfaceSupportKHR(i, m_surface))
        {
            indices.presentFamily = i;
        }
        if (indices.isComplete()) { break; }
        ++i;
    }
    return indices;
}

DeviceVk::SwapchainSupportDetails DeviceVk::querySwapchainSupport(vk::PhysicalDevice gpu) const
{
    SwapchainSupportDetails details;
    details.capabilities = gpu.getSurfaceCapabilitiesKHR(m_surface);
    details.formats = gpu.getSurfaceFormatsKHR(m_surface);
    details.presentModes = gpu.getSurfacePresentModesKHR(m_surface);
    return details;
}

const vk::Queue& DeviceVk::graphicsQueue() const
{
    return m_graphicsQueue;
}

const vk::Queue& DeviceVk::presentQueue() const
{
    return m_presentQueue;
}

const std::vector<vk::Image>& DeviceVk::swapchainImages() const
{
    return m_swapchainImages;
}

const std::vector<vk::ImageView>& DeviceVk::swapchainImageViews() const
{
    return m_swapchainImagesViews;
}

vk::Format DeviceVk::swapchainImageFormat() const
{
    return m_swapchainImageFormat;
}

const vk::Extent2D& DeviceVk::swapchainExtent() const
{
    return m_swapchainExtent;
}

void DeviceVk::createImageViews()
{
    for (auto image : m_swapchainImages)
    {
        vk::ImageViewCreateInfo imageViewCreateInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = m_swapchainImageFormat,
            .components = {
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity },
            .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
        };
        m_swapchainImagesViews.emplace_back(m_device.createImageView(imageViewCreateInfo));
    }
}

void DeviceVk::createFrameBuffers()
{
    std::array<vk::ImageView, 2> attachments;
    m_depthTexture = MAKE_SHARED(m_depthTexture, this);
    m_depthTexture->createDepthTexture(width(), height(), Texture::DepthPrecision::F32);
    attachments[1] = m_depthTexture->imageView();
    auto framebufferInfo = vk::FramebufferCreateInfo{
        .renderPass = m_renderPass,
        .attachmentCount = 2,
        .pAttachments = attachments.data(),
        .width = swapchainExtent().width,
        .height = swapchainExtent().height,
        .layers = 1
    };
    for (const auto& view : swapchainImageViews())
    {
        attachments[0] = view;
        m_swapchainFramebuffers.push_back(m_device.createFramebuffer(framebufferInfo));
    }
}

void DeviceVk::createCommandBuffers()
{
    auto allocInfo = vk::CommandBufferAllocateInfo{
        .commandPool = m_commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = static_cast<uint32_t>(m_swapchainFramebuffers.size())
    };
    m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);
}

void DeviceVk::createSyncObjects()
{
    auto semaphoreInfo = vk::SemaphoreCreateInfo{};
    auto fenceInfo = vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        m_imageAvailableSemaphores.push_back(m_device.createSemaphore(semaphoreInfo));
        m_renderFinishedSemaphores.push_back(m_device.createSemaphore(semaphoreInfo));
        m_inflightFences.push_back(m_device.createFence(fenceInfo));
    }
    m_imagesInflight.resize(swapchainImageViews().size(), vk::Fence{ nullptr });
}

void DeviceVk::createCommandPool()
{
    auto queueFamilyIndices = findQueueFamilyIndices(m_gpu);
    auto poolInfo = vk::CommandPoolCreateInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()
    };
    m_commandPool = m_device.createCommandPool(poolInfo);
}

void DeviceVk::cleanupSwapchain()
{
}

void DeviceVk::creatRenderPass()
{
    std::array<vk::AttachmentDescription, 2> attachments;
    attachments[0] = vk::AttachmentDescription{
        .format = swapchainImageFormat(),
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR
    };
    attachments[1] = vk::AttachmentDescription{
        .format = vk::Format::eD32Sfloat,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR
    };
    auto colorAttachmentRef = vk::AttachmentReference{
        .attachment = 0,
        .layout = vk::ImageLayout::eColorAttachmentOptimal
    };
    auto depthAttachmentRef = vk::AttachmentReference{
        .attachment = 1,
        .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
    };
    auto subpass = vk::SubpassDescription{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef,
    };
    auto dependency = vk::SubpassDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
    };
    auto renderPassInfo = vk::RenderPassCreateInfo{
        .attachmentCount = 2,
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };
    m_renderPass = m_device.createRenderPass(renderPassInfo);
}

const vk::RenderPass& DeviceVk::renderPass() const
{
    return m_renderPass;
}

const vk::CommandPool& DeviceVk::commandPool() const
{
    return m_commandPool;
}

const std::vector<vk::CommandBuffer>& DeviceVk::commandBuffers() const
{
    return m_commandBuffers;
}

const std::vector<vk::Framebuffer>& DeviceVk::swapchainFramebuffers() const
{
    return m_swapchainFramebuffers;
}

const std::vector<vk::Fence>& DeviceVk::inflightFences() const
{
    return m_inflightFences;
}

const std::vector<vk::Fence>& DeviceVk::imagesInflight() const
{
    return m_imagesInflight;
}

const std::vector<vk::Semaphore>& DeviceVk::imageAvailableSemaphores() const
{
    return m_imageAvailableSemaphores;
}

const std::vector<vk::Semaphore>& DeviceVk::renderFinishedSemaphores() const
{
    return m_renderFinishedSemaphores;
}

vk::CommandBuffer DeviceVk::beginSingleTimeCommands()
{
    auto allocInfo = vk::CommandBufferAllocateInfo{
        .commandPool = m_commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };
    vk::CommandBuffer commandBuffers;
    VK_CHECK(m_device.allocateCommandBuffers(&allocInfo, &commandBuffers));
    auto beginInfo = vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    commandBuffers.begin(beginInfo);
    return commandBuffers;
}

void DeviceVk::endSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
    commandBuffer.end();
    auto submitInfo = vk::SubmitInfo{
        .sType = vk::StructureType::eSubmitInfo,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    m_graphicsQueue.submit(submitInfo, nullptr);
    m_device.waitIdle();
    m_device.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
}

vk::RenderPassBeginInfo DeviceVk::getSingleRenderPassBeginInfo()
{
    static std::array clearValues = {
        vk::ClearValue{ .color = { .float32 = std::array<float, 4>{ 1.0f, 0.0f, 0.0f, 1.0f } } },
        vk::ClearValue{ .depthStencil = { 1.0f, 0 } }
    };
    static auto renderPassInfo = vk::RenderPassBeginInfo{
        .renderPass = renderPass(),
        .renderArea = {
            .offset = { 0, 0 },
            .extent = swapchainExtent() },
        .clearValueCount = 2,
        .pClearValues = clearValues.data(),
    };
    return renderPassInfo;
}

std::function<void(const vk::CommandBuffer& cb, vk::Pipeline pipeline, vk::PipelineLayout& layout, const vk::DescriptorSet& descriptorSet)> DeviceVk::bindPipeline()
{
    static auto bindPipeline = [](const vk::CommandBuffer& cb, vk::Pipeline pipeline, vk::PipelineLayout& layout, const vk::DescriptorSet& descriptorSet) {
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, descriptorSet, nullptr);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    };
    return bindPipeline;
}
} // namespace backend