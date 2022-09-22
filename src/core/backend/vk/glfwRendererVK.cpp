//
// Created by cwb on 2022/9/5.
//

#include "glfwRendererVk.h"

#include "pipelineVk.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace backend
{
GLFWRendererVK::GLFWRendererVK(Device* handle) :
    GLFWRenderer(handle)
{
    m_deviceVk = dynamic_cast<DeviceVK*>(handle);
    m_device = m_deviceVk->handle();
    m_instance = m_deviceVk->instance();
    m_swapChain = m_deviceVk->swapChain();
    m_surface = m_deviceVk->surface();
}

GLFWRendererVK::~GLFWRendererVK()
{
    for (auto& framebuffer : m_frameBuffers)
    {
        m_device.destroy(framebuffer);
    }
    for (auto& cmd : m_commandBuffers)
    {
        m_device.freeCommandBuffers(m_commandPool, cmd);
    }
    m_device.destroyCommandPool(m_commandPool);
    m_pipeline = nullptr;
    m_device.destroy(m_imageAvailableSemaphore);
    m_device.destroy(m_renderFinishedSemaphore);
    m_device.destroy(m_swapChain);
    m_device.destroy();
    m_instance.destroy(m_surface);
    m_instance.destroy();
    // 销毁window
    glfwDestroyWindow(m_window);
}

void GLFWRendererVK::swapBuffers()
{
    auto imageIndex = m_device.acquireNextImageKHR(m_swapChain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphore, {});
    vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    auto submitInfo = vk::SubmitInfo{ 1, &m_imageAvailableSemaphore, &waitStageMask, 1, &m_commandBuffers[imageIndex.value], 1, &m_renderFinishedSemaphore };
    m_deviceQueue.submit(submitInfo, {});
    auto presentInfo = vk::PresentInfoKHR{ 1, &m_renderFinishedSemaphore, 1, &m_swapChain, &imageIndex.value };
    auto result = m_presentQueue.presentKHR(presentInfo);
    (void)result;
    m_device.waitIdle();
}

void GLFWRendererVK::initCommandBuffer()
{
    uint32_t imageCount = m_deviceVk->imageViews().size();
    m_frameBuffers = std::vector<vk::Framebuffer>(imageCount);
    for (size_t i = 0; i < imageCount; i++)
    {
        m_frameBuffers[i] = m_device.createFramebuffer(vk::FramebufferCreateInfo{
            {},
            m_pipeline->renderPass(),
            1,
            &(m_deviceVk->imageViews()[i]),
            m_deviceVk->width(),
            m_deviceVk->height(),
            1 });
    }
    const auto& queueFamilyIndices = m_deviceVk->uniqueQueueFamilyIndices();
    m_commandPool = m_device.createCommandPool({ { vk::CommandPoolCreateFlagBits::eResetCommandBuffer }, queueFamilyIndices.front() });
    m_commandBuffers = m_device.allocateCommandBuffers(vk::CommandBufferAllocateInfo(m_commandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(m_frameBuffers.size())));
    m_deviceQueue = m_device.getQueue(static_cast<uint32_t>(queueFamilyIndices.front()), 0);
    m_presentQueue = m_device.getQueue(static_cast<uint32_t>(queueFamilyIndices.back()), 0);
}

void GLFWRendererVK::commit(const vk::Buffer& buffer)
{
    for (size_t i = 0; i < m_commandBuffers.size(); i++)
    {
        auto beginInfo = vk::CommandBufferBeginInfo{};
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        m_commandBuffers[i].begin(beginInfo);
        vk::ClearValue clearValues{};
        static float red = 0.0f;
        red = red > 1.0f ? 0.0f : red + 0.005f;
        clearValues.color.setFloat32({ red, 0.0f, 0.0f, 1.0f });
        auto renderPassBeginInfo = vk::RenderPassBeginInfo{
            m_pipeline->renderPass(),
            m_frameBuffers[i],
            { { 0, 0 }, { m_deviceVk->width(), m_deviceVk->height() } },
            1,
            &clearValues
        };
        m_commandBuffers[i].beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline); // 等价于 opengl的 bind program 和一设定些状态
        m_commandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline->handle());
        m_commandBuffers[i].bindVertexBuffers( 0, buffer, { 0 } );
        m_commandBuffers[i].draw(3, 1, 0, 0);
        m_commandBuffers[i].endRenderPass();
        m_commandBuffers[i].end();
    }
}

void GLFWRendererVK::setPipeline(const std::shared_ptr<Pipeline>& pipeline)
{
    m_pipeline = std::dynamic_pointer_cast<PipelineVk>(pipeline);
}

void GLFWRendererVK::initSemaphore()
{
    auto semaphoreCreateInfo = vk::SemaphoreCreateInfo{};
    m_imageAvailableSemaphore = m_deviceVk->handle().createSemaphore(semaphoreCreateInfo);
    m_renderFinishedSemaphore = m_deviceVk->handle().createSemaphore(semaphoreCreateInfo);
}
} // namespace backend