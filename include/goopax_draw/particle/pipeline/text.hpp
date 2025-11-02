#pragma once

#include "pipeline.hpp"
#include <filesystem>
#include <glm/glm.hpp>
#include <goopax_draw/../../ext/stb_truetype.h>

namespace goopax_draw::vulkan
{
template<typename T>
struct Chardata
{
    using uint16_type = typename goopax::change_gpu_mode<uint16_t, T>::type;
    using float_type = typename goopax::change_gpu_mode<float, T>::type;
    uint16_type x0;
    uint16_type y0;
    uint16_type dx;
    uint16_type dy;
    Eigen::Vector<float_type, 2> dest_offset;
};
}

GOOPAX_PREPARE_STRUCT(goopax_draw::vulkan::Chardata);

namespace goopax_draw::vulkan
{

class PipelineText : public Pipeline
{
    const Eigen::Vector<float, 4> bgColor = { 0, 0, 0, 0.5f };
    const Eigen::Vector<float, 4> textColor = { 1, 1, 1, 1 };

    goopax::buffer<Eigen::Vector<float, 2>> vertexBuffer;   // Positions (quad)
    goopax::buffer<Eigen::Vector<float, 2>> texCoordBuffer; // UVs
    goopax::buffer<uint32_t> indexBuffer;
    stbtt_bakedchar cdata[96]; // For font baking (ASCII 32-127)
    goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true> image;
    goopax::image_buffer<2, Eigen::Vector<uint8_t, 1>, true> characters;
    goopax::buffer<Chardata<int>> textdata;

    VkImageView textureView;
    VkSampler sampler;
    goopax::kernel<void(unsigned int N)> write_text;

    VkDescriptorPool descriptorPool = nullptr;
    VkDescriptorSetLayout descriptorSetLayout = nullptr;
    VkDescriptorSet descriptorSet = nullptr;

    const float fontSize;

public:
    void updateText(const std::string& text, Eigen::Vector<float, 2> tl);

    void draw(VkExtent2D extent, VkCommandBuffer cb);
    PipelineText(sdl_window_vulkan& window,
                 VkRenderPass renderPass,
                 const std::filesystem::path& fontFilename,
                 float fontSize0,
                 std::array<unsigned int, 2> overlaySize);
    ~PipelineText();
};

}
