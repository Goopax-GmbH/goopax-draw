#include "../vulkan/semaphore.hpp"
#include "pipeline/particle.hpp"
#include "pipeline/text.hpp"
#include "pipeline/wireframe.hpp"
#include <goopax_draw/window_vulkan.h>
#include <span>

namespace goopax_draw::vulkan
{

struct swapData;

class Renderer
{
public:
    sdl_window_vulkan& window;

    VkFormat depthFormat;
    VkRenderPass renderPass;

    std::optional<PipelineParticles> pipelineParticles;
    std::optional<PipelineWireframe> pipelineWireframe;
    std::optional<PipelineText> pipelineText;

    struct
    {
    } overlay;

    Semaphore imageAvailableSemaphore;
    VkFence inFlightFence;

    std::vector<std::unique_ptr<swapData>> swaps;

    goopax::buffer<float> potentialDummy;

    void render(const goopax::buffer<Eigen::Vector<float, 3>>& x,
                float distance = 2,
                Eigen::Vector<float, 2> theta = { 0, 0 },
                Eigen::Vector<float, 2> xypos = { 0, 0 });

    void render(const goopax::buffer<Eigen::Vector<float, 3>>& x,
                const goopax::buffer<float>& potential,
                float distance = 2,
                Eigen::Vector<float, 2> theta = { 0, 0 },
                Eigen::Vector<float, 2> xypos = { 0, 0 });

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
