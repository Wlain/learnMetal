//
// Created by william on 2022/12/5.
//
#include "../mesh/globalMeshs.h"
#include "bufferGl.h"
#include "depthStencilStateGl.h"
#include "descriptorSetGl.h"
#include "deviceGl.h"
#include "effectBase.h"
#include "engine.h"
#include "glCommonDefine.h"
#include "glfwRendererGL.h"
#include "pipelineGl.h"
#include "textureGl.h"
#include "utils/utils.h"

using namespace backend;

class TestDepthTestGl : public EffectBase
{
public:
    using EffectBase::EffectBase;
    ~TestDepthTestGl() override = default;
    void initialize() override
    {
        m_render = dynamic_cast<GLFWRendererGL*>(m_renderer);
        buildPipeline();
        buildTexture();
        buildBuffers();
        buildDepthStencilStates();
        buildDescriptorsSets();
    }
    void buildPipeline()
    {
        std::string vertSource = getFileContents("shaders/texture.vert");
        std::string fragShader = getFileContents("shaders/texture.frag");
        m_planePipeline = MAKE_SHARED(m_planePipeline, m_render->device());
        m_planePipeline->setProgram(vertSource, fragShader);
        m_cubePipeline = MAKE_SHARED(m_cubePipeline, m_render->device());
        m_cubePipeline->setProgram(vertSource, fragShader);
    }

    void buildTexture()
    {
        m_cubeTexture = MAKE_SHARED(m_cubeTexture, m_render->device());
        m_cubeTexture->createWithFileName("textures/marble.jpg", true);
        m_planeTexture = MAKE_SHARED(m_cubeTexture, m_render->device());
        m_planeTexture->createWithFileName("textures/metal.png", true);
    }

    void buildBuffers()
    {
        m_uniformBuffer = MAKE_SHARED(m_uniformBuffer, m_render->device());
        g_mvpMatrixUbo.view = glm::translate(g_mvpMatrixUbo.view, glm::vec3(0.0f, 0.0f, -3.0f));
        m_uniformBuffer->create(sizeof(VertMVPMatrixUBO), &g_mvpMatrixUbo, Buffer::BufferUsage::DynamicDraw, Buffer::BufferType::UniformBuffer);
        m_cubeVertexBuffer = MAKE_SHARED(m_cubeVertexBuffer, m_render->device());
        m_cubeVertexBuffer->create(g_cubeVerticesPosTexCoord.size() * sizeof(g_cubeVerticesPosTexCoord[0]), (void*)g_cubeVerticesPosTexCoord.data(), Buffer::BufferUsage::StaticDraw, Buffer::BufferType::VertexBuffer);
        m_cubePipeline->setAttributeDescription(getTwoElemsAttributesDescriptions());
        m_planeVertexBuffer = MAKE_SHARED(m_planeVertexBuffer, m_render->device());
        m_planeVertexBuffer->create(g_planeVerticesPosTexCoord.size() * sizeof(g_planeVerticesPosTexCoord[0]), (void*)g_planeVerticesPosTexCoord.data(), Buffer::BufferUsage::StaticDraw, Buffer::BufferType::VertexBuffer);
        m_planePipeline->setAttributeDescription(getTwoElemsAttributesDescriptions());
    }

    void buildDepthStencilStates()
    {
        m_depthStencilState = MAKE_SHARED(m_depthStencilState, m_render->device());
        m_depthStencilState->setDepthCompareOp(CompareOp::Always);
        m_depthStencilState->setDepthTestEnable(true);
        m_depthStencilState->setDepthWriteEnable(true);
        m_cubePipeline->setDepthStencilState(m_depthStencilState);
        m_planePipeline->setDepthStencilState(m_depthStencilState);
    }

    void buildDescriptorsSets()
    {
        std::map<int32_t, DescriptorBufferInfo> bufferInfos{
            { g_mvpMatrixUboBinding, DescriptorBufferInfo{
                                         .bufferType = m_uniformBuffer->bufferType(),
                                         .buffer = m_uniformBuffer->buffer(),
                                         .offset = 0,
                                         .range = sizeof(VertMVPMatrixUBO) } }
        };
        std::map<int32_t, DescriptorImageInfo> imageInfos{
            { g_textureBinding, DescriptorImageInfo{
                                    .name = "inputTexture",
                                    .target = m_cubeTexture->target(),
                                    .texture = m_cubeTexture->handle(),
                                } }
        };
        m_cubeDescriptorSet = MAKE_SHARED(m_cubeDescriptorSet, m_render->device());
        m_cubeDescriptorSet->createDescriptorSetLayout(g_textureShaderResource);
        m_cubeDescriptorSet->createDescriptorSets(bufferInfos, imageInfos);
        m_cubePipeline->setDescriptorSet(m_cubeDescriptorSet);

        imageInfos = {
            { g_textureBinding, DescriptorImageInfo{
                                    .name = "inputTexture",
                                    .target = m_planeTexture->target(),
                                    .texture = m_planeTexture->handle(),
                                } }
        };
        m_planeDescriptorSet = MAKE_SHARED(m_planeDescriptorSet, m_render->device());
        m_planeDescriptorSet->createDescriptorSetLayout(g_textureShaderResource);
        m_planeDescriptorSet->createDescriptorSets(bufferInfos, imageInfos);
        m_planePipeline->setDescriptorSet(m_planeDescriptorSet);
    }

    void update(float deltaTime) override
    {
        EffectBase::update(deltaTime);
        g_mvpMatrixUbo.view = m_camera.viewMatrix();
        g_mvpMatrixUbo.proj = glm::perspective(glm::radians(m_camera.zoom), (float)m_width / (float)m_height, 0.1f, 100.0f);
    }

    void render() override
    {
        glClearColor(1.0f, 0.0f, 0.0f, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // cubes
        m_render->setPipeline(m_cubePipeline);
        g_mvpMatrixUbo.model = glm::mat4(1.0f);
        g_mvpMatrixUbo.model = glm::translate(g_mvpMatrixUbo.model, glm::vec3(-1.0f, 0.0f, -1.0f));
        m_uniformBuffer->update(&g_mvpMatrixUbo, sizeof(VertMVPMatrixUBO), 0);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<uint32_t>(g_cubeVerticesPosTexCoord.size()));
        g_mvpMatrixUbo.model = glm::mat4(1.0f);
        g_mvpMatrixUbo.model = glm::translate(g_mvpMatrixUbo.model, glm::vec3(2.0f, 0.0f, 0.0f));
        m_uniformBuffer->update(&g_mvpMatrixUbo, sizeof(VertMVPMatrixUBO), 0);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<uint32_t>(g_cubeVerticesPosTexCoord.size()));
        // floor
        m_render->setPipeline(m_planePipeline);
        g_mvpMatrixUbo.model = glm::mat4(1.0f);
        m_uniformBuffer->update(&g_mvpMatrixUbo, sizeof(VertMVPMatrixUBO), 0);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<uint32_t>(g_planeVerticesPosTexCoord.size()));
    }

private:
    GLFWRendererGL* m_render{ nullptr };
    std::shared_ptr<BufferGL> m_uniformBuffer;
    std::shared_ptr<DepthStencilStateGL> m_depthStencilState;

    std::shared_ptr<PipelineGL> m_planePipeline;
    std::shared_ptr<DescriptorSetGl> m_planeDescriptorSet;
    std::shared_ptr<TextureGL> m_planeTexture;
    std::shared_ptr<BufferGL> m_planeVertexBuffer;

    std::shared_ptr<PipelineGL> m_cubePipeline;
    std::shared_ptr<DescriptorSetGl> m_cubeDescriptorSet;
    std::shared_ptr<TextureGL> m_cubeTexture;
    std::shared_ptr<BufferGL> m_cubeVertexBuffer;
};

void testDepthTestGl()
{
    Device::Info info{ Device::RenderType::OpenGL, 800, 600, "OpenGL Example Depth Test" };
    DeviceGl handle(info);
    handle.init();
    GLFWRendererGL rendererGl(&handle);
    Engine engine(rendererGl);
    auto effect = std::make_shared<TestDepthTestGl>(&rendererGl);
    engine.setEffect(effect);
    engine.run();
}