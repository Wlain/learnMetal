//
// Created by cwb on 2022/9/8.
//

#ifndef LEARNMETALVULKAN_SHADERVK_H
#define LEARNMETALVULKAN_SHADERVK_H
#include "pipeline.h"

#include <vulkan/vulkan.hpp>
namespace backend
{
class DeviceVK;

class PipelineVk : public Pipeline
{
public:
    explicit PipelineVk(Device* handle);
    ~PipelineVk() override;
    void setProgram(std::string_view vertShader, std::string_view fragSource) override;
    void initVertexBuffer();
    void setAssembly();
    void setPipelineLayout();
    void setViewport();
    void setRasterization();
    void setMultisample();
    void setDepthStencil();
    void setColorBlendAttachment();
    void setRenderPass();
    void build() override;
    const vk::RenderPass& renderPass() const;
    vk::Pipeline handle() const;

private:
    DeviceVK* m_deviceVk{ nullptr };
    vk::RenderPass m_renderPass;
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_pipelineLayout;
    vk::ShaderModule m_vertexShaderModule;
    vk::ShaderModule m_fragmentShaderModule;
    std::vector<vk::PipelineShaderStageCreateInfo> m_pipelineShaderStages;
    vk::PipelineVertexInputStateCreateInfo m_vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo m_assemblyStateCreateInfo;
    vk::Viewport m_viewport;
    vk::Rect2D m_scissor;
    vk::PipelineViewportStateCreateInfo m_viewportState;
    vk::PipelineRasterizationStateCreateInfo m_rasterizer;
    vk::PipelineMultisampleStateCreateInfo m_multisampling;
    vk::PipelineColorBlendAttachmentState m_colorBlendAttachment;
    vk::PipelineColorBlendStateCreateInfo m_colorBlending;
};
} // namespace backend

#endif // LEARNMETALVULKAN_SHADERVK_H
