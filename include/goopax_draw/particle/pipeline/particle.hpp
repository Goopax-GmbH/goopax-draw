#pragma once

#include "pipeline.hpp"
#include <glm/glm.hpp>

namespace goopax_draw::vulkan
{
class PipelineParticles : public Pipeline
{
public:
    void draw(VkExtent2D extent,
              VkCommandBuffer cb,
              glm::mat4 matrix,
              const goopax::buffer<Eigen::Vector<float, 3>>& x,
              const goopax::buffer<float>& potential);
    PipelineParticles(sdl_window_vulkan& window, VkRenderPass renderPass);
};

}
