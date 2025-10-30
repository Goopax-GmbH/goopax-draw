#pragma once

#if WITH_VULKAN

#include "window_sdl.h"
#ifdef _WIN32 //defined(VK_USE_PLATFORM_WIN32_KHR)
  #include <vulkan/vk_platform.h>
  #include <vulkan/vulkan_core.h>
  #include <windows.h>
  #include <vulkan/vulkan.h>
  #include <vulkan/vulkan_win32.h>
#else
  #include <vulkan/vulkan.h>
#endif

void call_vulkan(VkResult result);

class sdl_window_vulkan : public sdl_window
{
public:
#define setfunc(FUNC) PFN_##FUNC FUNC

    setfunc(vkGetPhysicalDeviceSurfaceSupportKHR);
    setfunc(vkCreateSwapchainKHR);
    setfunc(vkGetSwapchainImagesKHR);
    setfunc(vkGetPhysicalDeviceSurfaceFormatsKHR);
    setfunc(vkCreateFence);
    setfunc(vkDestroyFence);
    setfunc(vkAllocateCommandBuffers);
    setfunc(vkFreeCommandBuffers);
    setfunc(vkCreateCommandPool);
    setfunc(vkDestroyCommandPool);
    setfunc(vkDestroySwapchainKHR);
    setfunc(vkAcquireNextImageKHR);
    setfunc(vkQueuePresentKHR);
    setfunc(vkCmdPipelineBarrier);
    setfunc(vkBeginCommandBuffer);
    setfunc(vkEndCommandBuffer);
    setfunc(vkQueueSubmit);
    setfunc(vkWaitForFences);
    setfunc(vkResetFences);
    setfunc(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    setfunc(vkGetPhysicalDeviceImageFormatProperties);
    setfunc(vkCreateShaderModule);
    setfunc(vkDestroySemaphore);
    setfunc(vkCreateImageView);
    setfunc(vkDestroyPipeline);
    setfunc(vkDestroyPipelineLayout);
    setfunc(vkDestroyRenderPass);
    setfunc(vkDestroyImageView);
    setfunc(vkFreeMemory);
    setfunc(vkDestroyImage);
    setfunc(vkDestroyFramebuffer);
    setfunc(vkCreateImage);
    setfunc(vkGetPhysicalDeviceMemoryProperties);
    setfunc(vkGetPhysicalDeviceFormatProperties);
    setfunc(vkCreateRenderPass);
    setfunc(vkCreatePipelineLayout);
    setfunc(vkCreateFramebuffer);
    setfunc(vkCreateSemaphore);
    setfunc(vkGetImageMemoryRequirements);
    setfunc(vkCreateGraphicsPipelines);
    setfunc(vkAllocateMemory);
    setfunc(vkDestroyShaderModule);
    setfunc(vkBindImageMemory);
    setfunc(vkResetCommandBuffer);
    setfunc(vkCmdBeginRenderPass);
    setfunc(vkCmdBindPipeline);
    setfunc(vkCmdSetViewport);
    setfunc(vkCmdSetScissor);
    setfunc(vkCmdBindVertexBuffers);
    setfunc(vkCmdPushConstants);
    setfunc(vkCmdDraw);
    setfunc(vkCmdEndRenderPass);
    setfunc(vkGetDeviceQueue);
    setfunc(vkDeviceWaitIdle);
    setfunc(vkCmdBindIndexBuffer);
    setfunc(vkCmdDrawIndexed);
    setfunc(vkCreateSampler);
    setfunc(vkCmdBindDescriptorSets);
    setfunc(vkCreateDescriptorPool);
    setfunc(vkCreateDescriptorSetLayout);
    setfunc(vkAllocateDescriptorSets);
    setfunc(vkUpdateDescriptorSets);
    setfunc(vkGetBufferDeviceAddress);
#ifdef _WIN32
    setfunc(vkGetMemoryWin32HandleKHR);
#else
    setfunc(vkGetMemoryFdKHR);
#endif

#undef setfunc

    VkInstance instance = nullptr;
    VkSurfaceKHR surface = nullptr;
    VkDevice vkDevice = nullptr;
    VkQueue vkQueue = nullptr;
    VkSwapchainKHR swapchain = nullptr;
    VkFence fence = nullptr;
    VkCommandPool commandPool = nullptr;
    std::vector<std::pair<VkCommandBuffer, VkFence>> commandBuffers;

    static constexpr uint64_t timeout = 60000000000ul;

    VkSurfaceFormatKHR format;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;

    std::vector<goopax::image_buffer<2, Eigen::Vector<uint8_t, 4>, true>> images;

    void draw_goopax(
        std::function<void(goopax::image_buffer<2, Eigen::Vector<Tuint8_t, 4>, true>& image)> func) final override;

    void create_swapchain();
    void destroy_swapchain();

public:
    sdl_window_vulkan(const char* name, Eigen::Vector<Tuint, 2> size, uint32_t flags = 0);
    ~sdl_window_vulkan();
};
#endif
