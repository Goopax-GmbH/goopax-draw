#include "../../../ext/stb_truetype.h" // For text rendering

#include "../vulkan/semaphore.hpp"
#include <goopax_draw/window_vulkan.h>
#include <span>

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

struct swapData;

class Renderer
{
public:
    sdl_window_vulkan& window;

    VkFormat depthFormat;
    VkRenderPass renderPass;
    struct
    {
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;
    } particles;

    struct
    {
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline; // Separate pipeline for lines
        goopax::buffer<Eigen::Vector<float, 3>> vertexBuffer;
        goopax::buffer<uint32_t> indexBuffer;
        float cubeSize;
    } cube;

    struct
    {
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;                            // For 2D quads
        goopax::buffer<Eigen::Vector<float, 2>> vertexBuffer;   // Positions (quad)
        goopax::buffer<Eigen::Vector<float, 2>> texCoordBuffer; // UVs
        goopax::buffer<uint32_t> indexBuffer;
        stbtt_bakedchar cdata[96]; // For font baking (ASCII 32-127)
        goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true> image;
        goopax::image_buffer<2, Eigen::Vector<uint8_t, 1>, true> characters;
        // buffer<stbtt_bakedchar<int>> cdata;
        goopax::buffer<Chardata<int>> textdata;

        VkImageView textureView;
        VkSampler sampler;
        goopax::kernel<void(unsigned int N)> write_text;

        VkDescriptorPool descriptorPool = nullptr;
        VkDescriptorSetLayout descriptorSetLayout = nullptr;
        VkDescriptorSet descriptorSet = nullptr;
    } overlay;

    Semaphore imageAvailableSemaphore;
    VkFence inFlightFence;

    std::vector<std::unique_ptr<swapData>> swaps;

    goopax::buffer<float> potentialDummy;

    void
    updateText(const std::string& text, Eigen::Vector<float, 2> tl, Eigen::Vector<float, 2> size, float lineheight);

    // New for wireframe cube
    // New for overlay
    // VkImage overlayTexture;
    // VkDeviceMemory overlayTextureMemory;

    void render(const goopax::buffer<Eigen::Vector<float, 3>>& x,
                float distance = 2,
                float theta = 0,
                Eigen::Vector<float, 2> xypos = { 0, 0 });

    void render(const goopax::buffer<Eigen::Vector<float, 3>>& x,
                const goopax::buffer<float>& potential,
                float distance = 2,
                float theta = 0,
                Eigen::Vector<float, 2> xypos = { 0, 0 });

    // New helper functions
    void createCubeBuffers();
    void createOverlayResources(std::array<unsigned int, 2> overlaySize);
    // void updateOverlayTexture(const std::string& text, const std::string& imagePath);
    void createGraphicsPipelineLines();
    void createGraphicsPipeline2D();
    void destroyAdditionalResources();

    VkShaderModule createShaderModule(std::span<unsigned char> prog);

    Renderer(sdl_window_vulkan& window0, float cubeSize, std::array<unsigned int, 2> overlaySize);
    ~Renderer();

    void cleanup();

    void createImage(uint32_t width,
                     uint32_t height,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkImage& image,
                     VkDeviceMemory& imageMemory);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        window.vkGetPhysicalDeviceMemoryProperties(get_vulkan_physical_device(window.device), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }

    VkFormat
    findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            window.vkGetPhysicalDeviceFormatProperties(get_vulkan_physical_device(window.device), format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }
        throw std::runtime_error("Failed to find supported format!");
    }

    VkFormat findDepthFormat()
    {
        return findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
                                   VK_IMAGE_TILING_OPTIMAL,
                                   VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void createSyncObjects()
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        assert(swaps.empty());

        createSwapData();
        call_vulkan(window.vkCreateFence(window.vkDevice, &fenceInfo, nullptr, &inFlightFence));
    }

    void createSwapData();
    void destroySwapData();
};
}
