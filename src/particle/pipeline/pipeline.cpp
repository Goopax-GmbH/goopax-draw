#include <goopax_draw/particle/pipeline/pipeline.hpp>

namespace goopax_draw::vulkan
{

Pipeline::Pipeline(sdl_window_vulkan& window0)
    : window(window0)
{
}

Pipeline::~Pipeline()
{
    window.vkDestroyPipeline(window.vkDevice, pipeline, nullptr);
    window.vkDestroyPipelineLayout(window.vkDevice, pipelineLayout, nullptr);
}

}
