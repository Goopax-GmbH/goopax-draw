#pragma once

#include "pipeline.hpp"
#include <glm/glm.hpp>

namespace goopax_draw::vulkan
{
class PipelineWireframe : public Pipeline
{
    goopax::buffer<Eigen::Vector<float, 3>> vertexBuffer;
    goopax::buffer<uint32_t> indexBuffer;
    float cubeSize;

public:
    void draw(VkCommandBuffer cb, glm::mat4 matrix);
    PipelineWireframe(sdl_window_vulkan& window, VkRenderPass renderPass, float cubeSize0);
};

}
