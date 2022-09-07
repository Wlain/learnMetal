//
// Created by william on 2022/9/4.
//
#include <SDL.h>
#include <SDL_vulkan.h>
#include <iostream>
#include <set>
#include <shaderc/shaderc.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

struct SharingModeUtil
{
    vk::SharingMode sharingMode;
    uint32_t familyIndicesCount;
    uint32_t* familyIndicesDataPtr;
};

void windowSdlVK()
{
    uint32_t width = 640;
    uint32_t height = 480;
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_Window* window = SDL_CreateWindow("SDL Vulkan window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    /// 创建instance
    auto extensionCount = 0u;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());
    std::vector<const char*> instanceExtensions(extensions.data(), extensions.data() + extensionCount);
    instanceExtensions.emplace_back("VK_EXT_debug_utils");
    instanceExtensions.emplace_back("VK_KHR_portability_enumeration");
    instanceExtensions.emplace_back("VK_KHR_get_physical_device_properties2");
    auto layers = std::vector<const char*>{ "VK_LAYER_KHRONOS_validation" };
    auto layerNames = vk::enumerateInstanceLayerProperties();
    for (auto& layer : layerNames)
    {
        std::cout << layer.layerName << std::endl;
    }
    vk::InstanceCreateInfo infos;
    infos.setPEnabledExtensionNames(instanceExtensions)
#if defined(TARGET_OS_MAC)
        .setFlags(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR)
#endif
        .setPEnabledLayerNames(layers);

    auto instance = vk::createInstanceUnique(infos);

    /// 创建surface,这是针对所有操作系统的一种功能解决方案
    VkSurfaceKHR surfaceTmp;
    if (!SDL_Vulkan_CreateSurface(window, *instance, &surfaceTmp))
    {
        std::cout << "CreateSurface error" << std::endl;
    }
    vk::UniqueSurfaceKHR surface(surfaceTmp, *instance);

    /// 挑选一块显卡
    // 创建 gpu
    auto gpus = instance->enumeratePhysicalDevices();
    // 打印显卡名
    for (auto& gpu : gpus)
    {
        std::cout << gpu.getProperties().deviceName << "\n";
    }
    auto gpu = gpus.front();

    // 创建 Device 和 命令队列
    // 命令队列：有两个：
    // 一个是绘制命令：queueFamilyProperties
    // 一个是显示命令: presentQueueFamilyIndex
    // get the QueueFamilyProperties of the first PhysicalDevice
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = gpu.getQueueFamilyProperties();
    // get the first index into queueFamiliyProperties which supports graphics
    size_t graphicsQueueFamilyIndex = std::distance(queueFamilyProperties.begin(), std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(), [](vk::QueueFamilyProperties const& qfp) {
                                                        return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
                                                    }));
    assert(graphicsQueueFamilyIndex < queueFamilyProperties.size());
    size_t presentQueueFamilyIndex = 0u;
    for (auto i = 0ul; i < queueFamilyProperties.size(); i++)
    {
        if (gpu.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface))
        {
            presentQueueFamilyIndex = i;
        }
    }
    std::set<uint32_t> uniqueQueueFamilyIndices = { static_cast<uint32_t>(graphicsQueueFamilyIndex),
                                                    static_cast<uint32_t>(presentQueueFamilyIndex) };
    std::vector<uint32_t> familyIndices = { uniqueQueueFamilyIndices.begin(), uniqueQueueFamilyIndices.end() };
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 0.0f;
    for (auto& queueFamilyIndex : uniqueQueueFamilyIndices)
    {
        queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), static_cast<uint32_t>(queueFamilyIndex), 1, &queuePriority);
    }
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset" };
    vk::UniqueDevice device = gpu.createDeviceUnique(
        { vk::DeviceCreateFlags(),
          static_cast<uint32_t>(queueCreateInfos.size()),
          queueCreateInfos.data(),
          0u,
          nullptr,
          static_cast<uint32_t>(deviceExtensions.size()),
          deviceExtensions.data() });
    uint32_t imageCount = 2;
    SharingModeUtil sharingModeUtil{ (graphicsQueueFamilyIndex != presentQueueFamilyIndex) ?
                                         SharingModeUtil{ vk::SharingMode::eConcurrent, 2u, familyIndices.data() } :
                                         SharingModeUtil{ vk::SharingMode::eExclusive, 0u, static_cast<uint32_t*>(nullptr) } };

    // needed for validation warnings
    auto capabilities = gpu.getSurfaceCapabilitiesKHR(*surface);
    auto formats = gpu.getSurfaceFormatsKHR(*surface);
    auto format = vk::Format::eB8G8R8A8Unorm;
    auto extent = vk::Extent2D{ width, height };
    vk::SwapchainCreateInfoKHR swapChainCreateInfo(
        {},
        *surface,
        imageCount,
        format,
        vk::ColorSpaceKHR::eSrgbNonlinear,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        sharingModeUtil.sharingMode,
        sharingModeUtil.familyIndicesCount,
        sharingModeUtil.familyIndicesDataPtr,
        vk::SurfaceTransformFlagBitsKHR::eIdentity,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::PresentModeKHR::eFifo,
        true,
        nullptr);

    auto swapChain = device->createSwapchainKHRUnique(swapChainCreateInfo);
    std::vector<vk::Image> swapChainImages = device->getSwapchainImagesKHR(*swapChain);
    std::vector<vk::UniqueImageView> imageViews;
    imageViews.reserve(swapChainImages.size());
    for (auto image : swapChainImages)
    {
        vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image,
                                                    vk::ImageViewType::e2D, format,
                                                    vk::ComponentMapping{ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                                                                          vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA },
                                                    vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        imageViews.emplace_back(device->createImageViewUnique(imageViewCreateInfo));
    }
    // const char kShaderSource[]
    std::string kShaderSource = R"vertexshader(
        #version 450
        #extension GL_ARB_separate_shader_objects : enable
        out gl_PerVertex {
            vec4 gl_Position;
        };
        layout(location = 0) out vec3 fragColor;
        vec2 positions[3] = vec2[](
            vec2(0.0, -0.5),
            vec2(0.5, 0.5),
            vec2(-0.5, 0.5)
        );
        vec3 colors[3] = vec3[](
            vec3(1.0, 0.0, 0.0),
            vec3(0.0, 1.0, 0.0),
            vec3(0.0, 0.0, 1.0)
        );
        void main() {
            gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
            fragColor = colors[gl_VertexIndex];
        }
        )vertexshader";

    std::string fragmentShader = R"fragmentShader(
        #version 450
        #extension GL_ARB_separate_shader_objects : enable
        layout(location = 0) in vec3 fragColor;
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(fragColor, 1.0);
        }
        )fragmentShader";
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
    shaderc::SpvCompilationResult vertShaderModule = compiler.CompileGlslToSpv(kShaderSource, shaderc_glsl_vertex_shader, "vertex shader", options);
    if (vertShaderModule.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::cerr << vertShaderModule.GetErrorMessage();
    }
    auto vertShaderCode = std::vector<uint32_t>{ vertShaderModule.cbegin(), vertShaderModule.cend() };
    auto vertSize = std::distance(vertShaderCode.begin(), vertShaderCode.end());
    auto vertShaderCreateInfo = vk::ShaderModuleCreateInfo{ {}, vertSize * sizeof(uint32_t), vertShaderCode.data() };
    auto vertexShaderModule = device->createShaderModuleUnique(vertShaderCreateInfo);

    shaderc::SpvCompilationResult fragShaderModule = compiler.CompileGlslToSpv(fragmentShader, shaderc_glsl_fragment_shader, "fragment shader", options);
    if (fragShaderModule.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        std::cerr << fragShaderModule.GetErrorMessage();
    }
    auto fragShaderCode = std::vector<uint32_t>{ fragShaderModule.cbegin(), fragShaderModule.cend() };
    auto fragSize = std::distance(fragShaderCode.begin(), fragShaderCode.end());
    auto fragShaderCreateInfo =
        vk::ShaderModuleCreateInfo{ {}, fragSize * sizeof(uint32_t), fragShaderCode.data() };
    auto fragmentShaderModule = device->createShaderModuleUnique(fragShaderCreateInfo);

    auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo{ {},
                                                                  vk::ShaderStageFlagBits::eVertex,
                                                                  *vertexShaderModule,
                                                                  "main" };
    auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo{ {},
                                                                  vk::ShaderStageFlagBits::eFragment,
                                                                  *fragmentShaderModule,
                                                                  "main" };
    auto pipelineShaderStages = std::vector<vk::PipelineShaderStageCreateInfo>{ vertShaderStageInfo, fragShaderStageInfo };
    auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{ {}, 0u, nullptr, 0u, nullptr };
    auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{ {}, vk::PrimitiveTopology::eTriangleList, false };
    auto viewport = vk::Viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    auto scissor = vk::Rect2D{ { 0, 0 }, extent };
    auto viewportState = vk::PipelineViewportStateCreateInfo{ {}, 1, &viewport, 1, &scissor };

    auto rasterizer = vk::PipelineRasterizationStateCreateInfo{ {}, /*depthClamp*/ false,
                                                                /*rasterizeDiscard*/ false,
                                                                vk::PolygonMode::eFill,
                                                                {},
                                                                /*frontFace*/ vk::FrontFace::eCounterClockwise,
                                                                {},
                                                                {},
                                                                {},
                                                                {},
                                                                1.0f };
    auto multisampling = vk::PipelineMultisampleStateCreateInfo{ {}, vk::SampleCountFlagBits::e1, false, 1.0 };
    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{ {}, /*srcCol*/ vk::BlendFactor::eOne,
                                                                       /*dstCol*/ vk::BlendFactor::eZero,
                                                                       /*colBlend*/ vk::BlendOp::eAdd,
                                                                       /*srcAlpha*/ vk::BlendFactor::eOne,
                                                                       /*dstAlpha*/ vk::BlendFactor::eZero,
                                                                       /*alphaBlend*/ vk::BlendOp::eAdd,
                                                                       vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };

    auto colorBlending = vk::PipelineColorBlendStateCreateInfo{ {},
                                                                /*logicOpEnable=*/false,
                                                                vk::LogicOp::eCopy,
                                                                /*attachmentCount=*/1,
                                                                /*colourAttachments=*/&colorBlendAttachment };
    auto pipelineLayout = device->createPipelineLayoutUnique({}, nullptr);
    auto colorAttachment = vk::AttachmentDescription{ {}, format, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, {}, {}, {}, vk::ImageLayout::ePresentSrcKHR };
    auto colourAttachmentRef = vk::AttachmentReference{ 0, vk::ImageLayout::eColorAttachmentOptimal };
    auto subpass = vk::SubpassDescription{ {},
                                           vk::PipelineBindPoint::eGraphics,
                                           /*inAttachmentCount*/ 0,
                                           nullptr,
                                           1,
                                           &colourAttachmentRef };
    auto semaphoreCreateInfo = vk::SemaphoreCreateInfo{};
    auto imageAvailableSemaphore = device->createSemaphoreUnique(semaphoreCreateInfo);
    auto renderFinishedSemaphore = device->createSemaphoreUnique(semaphoreCreateInfo);
    auto subpassDependency = vk::SubpassDependency{ VK_SUBPASS_EXTERNAL,
                                                    0,
                                                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                    {},
                                                    vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite };
    auto renderPass = device->createRenderPassUnique(vk::RenderPassCreateInfo{ {}, 1, &colorAttachment, 1, &subpass, 1, &subpassDependency });
    auto pipelineCreateInfo = vk::GraphicsPipelineCreateInfo{
        {},
        2,
        pipelineShaderStages.data(),
        &vertexInputInfo,
        &inputAssembly,
        nullptr,
        &viewportState,
        &rasterizer,
        &multisampling,
        nullptr,
        &colorBlending,
        nullptr,
        *pipelineLayout,
        *renderPass,
        0
    };
    auto pipeline = device->createGraphicsPipelineUnique({}, pipelineCreateInfo).value;
    auto framebuffers = std::vector<vk::UniqueFramebuffer>(imageCount);
    for (size_t i = 0; i < imageViews.size(); i++)
    {
        framebuffers[i] = device->createFramebufferUnique(vk::FramebufferCreateInfo{
            {},
            *renderPass,
            1,
            &(*imageViews[i]),
            extent.width,
            extent.height,
            1 });
    }
    auto commandPoolUnique = device->createCommandPoolUnique({ {}, static_cast<uint32_t>(graphicsQueueFamilyIndex) });
    std::vector<vk::UniqueCommandBuffer> commandBuffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(commandPoolUnique.get(), vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(framebuffers.size())));
    auto deviceQueue = device->getQueue(static_cast<uint32_t>(graphicsQueueFamilyIndex), 0);
    auto presentQueue = device->getQueue(static_cast<uint32_t>(presentQueueFamilyIndex), 0);
    for (size_t i = 0; i < commandBuffers.size(); i++)
    {
        auto beginInfo = vk::CommandBufferBeginInfo{};
        commandBuffers[i]->begin(beginInfo);
        vk::ClearValue clearValues{};
        clearValues.color.setFloat32({ 1.0f, 0.0f, 0.0f, 1.0f });
        auto renderPassBeginInfo = vk::RenderPassBeginInfo{ renderPass.get(), framebuffers[i].get(), vk::Rect2D{ { 0, 0 }, extent }, 1, &clearValues };
        commandBuffers[i]->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        commandBuffers[i]->draw(3, 1, 0, 0);
        commandBuffers[i]->endRenderPass();
        commandBuffers[i]->end();
    }

    bool quit = false;
    SDL_Event e;
    while (!quit)
    {
        while (SDL_PollEvent(&e) != 0)
        {
            switch (e.type)
            {
            case SDL_QUIT:
                quit = true;
                break;
            }
        }

        auto imageIndex = device->acquireNextImageKHR(swapChain.get(), std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore.get(), {});
        vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        auto submitInfo = vk::SubmitInfo{ 1, &imageAvailableSemaphore.get(), &waitStageMask, 1, &commandBuffers[imageIndex.value].get(), 1, &renderFinishedSemaphore.get() };
        deviceQueue.submit(submitInfo, {});
        auto presentInfo = vk::PresentInfoKHR{ 1, &renderFinishedSemaphore.get(), 1, &swapChain.get(), &imageIndex.value };
        auto result = presentQueue.presentKHR(presentInfo);
        device->waitIdle();
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
}