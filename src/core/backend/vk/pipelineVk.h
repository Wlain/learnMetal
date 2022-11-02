//
// Created by cwb on 2022/9/8.
//

#ifndef LEARNMETALVULKAN_SHADERVK_H
#define LEARNMETALVULKAN_SHADERVK_H
#include "pipeline.h"
#ifndef VULKAN_HPP_NO_CONSTRUCTORS
    #define VULKAN_HPP_NO_CONSTRUCTORS // 从 vulkan.hpp 中删除所有结构和联合构造函数
#endif
#include <vulkan/vulkan.hpp>
namespace backend
{
class DeviceVK;

class PipelineVk : public Pipeline
{
public:
    explicit PipelineVk(Device* device);
    ~PipelineVk() override;
    void setProgram(std::string_view vertShader, std::string_view fragSource) override;
    void initVertexBuffer(const VkPipelineVertexInputStateCreateInfo& info);
    void setPipelineLayout(vk::PipelineLayout layout);
    void setViewport();
    void setRasterization();
    void setMultisample();
    void setDepthStencil(vk::PipelineDepthStencilStateCreateInfo info);
    void setColorBlendAttachment();
    void setRenderPass();
    void build() override;
    [[nodiscard]] vk::Pipeline handle() const;
    [[nodiscard]] vk::Pipeline& handle();
    void setTopology(Topology topology) override;

private:
    vk::ShaderModule createShaderModule(const std::vector<uint32_t>& code);
    vk::PipelineShaderStageCreateInfo getShaderCreateInfo(vk::ShaderModule shaderModule, vk::ShaderStageFlagBits stage);

private:
    DeviceVK* m_deviceVk{ nullptr };
    vk::RenderPass m_renderPass;
    vk::ShaderModule m_vertexShaderModule;
    vk::ShaderModule m_fragmentShaderModule;
    vk::Pipeline m_pipeline;
    vk::PipelineLayout m_pipelineLayout;
    std::vector<vk::PipelineShaderStageCreateInfo> m_pipelineShaderStages;
    vk::PipelineVertexInputStateCreateInfo m_vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo m_inputAssembly;
    vk::Viewport m_viewport;
    vk::Rect2D m_scissor;
    vk::PipelineViewportStateCreateInfo m_viewportState;
    vk::PipelineRasterizationStateCreateInfo m_rasterizer;
    vk::PipelineMultisampleStateCreateInfo m_multisampling;
    vk::PipelineDepthStencilStateCreateInfo m_depthStencilState;
    vk::PipelineColorBlendAttachmentState m_colorBlendAttachment;
    vk::PipelineColorBlendStateCreateInfo m_colorBlending;
    vk::PrimitiveTopology m_topology{};
};
} // namespace backend

#endif // LEARNMETALVULKAN_SHADERVK_H
