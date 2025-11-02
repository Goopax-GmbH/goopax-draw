#pragma once

#include <goopax_draw/window_vulkan.h>

namespace goopax_draw::vulkan
{

class Pipeline
{
public:
    static constexpr goopax::backend_create_params vulkan_vertex_flags = {
        .vulkan = { .usage_bits = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                  | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR }
    };

    static constexpr goopax::backend_create_params vulkan_index_flags = {
        .vulkan = { .usage_bits = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                                  | VK_BUFFER_USAGE_TRANSFER_DST_BIT }
    };

    sdl_window_vulkan& window;
    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline pipeline = nullptr;

    Pipeline(sdl_window_vulkan& window0);
    ~Pipeline();
};

}
