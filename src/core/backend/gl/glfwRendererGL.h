//
// Created by cwb on 2022/9/5.
//

#ifndef LEARNMETALVULKAN_GLFWRENDERERGL_H
#define LEARNMETALVULKAN_GLFWRENDERERGL_H

#include "glfwRenderer.h"
class GLFWRendererGL : public GLFWRenderer
{
public:
    using GLFWRenderer::GLFWRenderer;
    ~GLFWRendererGL() override;
    void initGlfw() override;
    void swapWindow() override;
    void initSwapChain() override;
};


#endif // LEARNMETALVULKAN_GLFWRENDERERGL_H
