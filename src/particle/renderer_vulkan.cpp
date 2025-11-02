#include <goopax_extra/output.hpp>
#include <goopax_extra/param.hpp>

#include "shaders/particles.frag.spv.cpp"
#include "shaders/particles.vert.spv.cpp"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <goopax_draw/particle/renderer_vulkan.hpp>

namespace goopax_draw::vulkan
{

using namespace goopax;
using namespace std;
using Eigen::Vector;

PARAMOPT<string> FONT_FILENAME("font", "/usr/share/fonts/Myriad Pro/Myriad Pro Regular/Myriad Pro Regular.ttf");
PARAMOPT<float> FONT_SIZE("font_size", 60);

struct swapData
{
    Renderer& renderer;

    VkImageView imageView;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkFramebuffer framebuffer;

    VkCommandBuffer commandBuffer;
    Semaphore renderFinishedSemaphore;

    swapData(Renderer& renderer0, VkImage image);
    ~swapData();
};

swapData::swapData(Renderer& renderer0, VkImage image)
    : renderer(renderer0)
    , renderFinishedSemaphore(renderer.window)
{
    auto& window = renderer.window;

    auto& extent = window.surfaceCapabilities.currentExtent;
    {
        renderer.createImage(extent.width,
                             extent.height,
                             renderer.depthFormat,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             depthImage,
                             depthImageMemory);

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = renderer.depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        call_vulkan(window.vkCreateImageView(window.vkDevice, &viewInfo, nullptr, &depthImageView));
    }

    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = window.format.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        call_vulkan(window.vkCreateImageView(window.vkDevice, &createInfo, nullptr, &imageView));
    }

    {
        VkImageView attachments[] = { imageView, depthImageView };
        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderer.renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;
        call_vulkan(window.vkCreateFramebuffer(window.vkDevice, &framebufferInfo, nullptr, &framebuffer));
    }

    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = window.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        call_vulkan(window.vkAllocateCommandBuffers(window.vkDevice, &allocInfo, &commandBuffer));
    }
}

swapData::~swapData()
{
    auto& window = renderer.window;

    window.vkDestroyFramebuffer(window.vkDevice, framebuffer, nullptr);
    window.vkDestroyImageView(window.vkDevice, imageView, nullptr);

    window.vkDestroyImageView(window.vkDevice, depthImageView, nullptr);
    window.vkFreeMemory(window.vkDevice, depthImageMemory, nullptr);
    window.vkDestroyImage(window.vkDevice, depthImage, nullptr);
}

void Renderer::createImage(uint32_t width,
                           uint32_t height,
                           VkFormat format,
                           VkImageTiling tiling,
                           VkImageUsageFlags usage,
                           VkMemoryPropertyFlags properties,
                           VkImage& image,
                           VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    call_vulkan(window.vkCreateImage(window.vkDevice, &imageInfo, nullptr, &image));

    VkMemoryRequirements memRequirements;
    window.vkGetImageMemoryRequirements(window.vkDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    call_vulkan(window.vkAllocateMemory(window.vkDevice, &allocInfo, nullptr, &imageMemory));

    window.vkBindImageMemory(window.vkDevice, image, imageMemory, 0);
}

void Renderer::render(const buffer<Vector<float, 3>>& x, float distance, Vector<float, 2> theta, Vector<float, 2> xypos)
{
    if (potentialDummy.size() != x.size())
    {
        potentialDummy.assign(window.device, x.size(), Pipeline::vulkan_vertex_flags);
        potentialDummy.fill(0.9f);
    }

    render(x, potentialDummy, distance, theta, xypos);
}

void Renderer::render(const buffer<Vector<float, 3>>& x,
                      const buffer<float>& potential,
                      float distance,
                      Vector<float, 2> theta,
                      Vector<float, 2> xypos)
{
    window.vkWaitForFences(window.vkDevice, 1, &inFlightFence, VK_TRUE, window.timeout);
    window.vkResetFences(window.vkDevice, 1, &inFlightFence);

tryagain:
    auto extent = window.surfaceCapabilities.currentExtent;

    float aspect_ratio = float(extent.width) / extent.height; // From your Vulkan swapchain or window
    float fov = glm::radians(60.0f); // Field of view (60 degrees is a common starting point; adjust as needed)
    float near_clip = 0.01f;         // Near clipping plane (small positive value)
    float far_clip = 100.0f;         // Far clipping plane (larger than D + scene depth)

    // Compute camera position after rotation around y-axis
    glm::vec3 camera_pos = glm::vec3(-distance * sin(theta[0]) * cos(theta[1]) + xypos[0],
                                     distance * sin(theta[1]) + xypos[1],
                                     distance * cos(theta[0]) * cos(theta[1]));

    // Create view matrix: camera looking at origin with up as (0,1,0)
    glm::mat4 view = glm::lookAt(camera_pos, glm::vec3(xypos[0], xypos[1], 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Create perspective projection matrix
    glm::mat4 projection = glm::perspective(fov, aspect_ratio, near_clip, far_clip);
    projection[1][1] *= -1; // Flip Y (Vulkan top-left origin)

    // Combined matrix for your shader uniform (projection * view)
    glm::mat4 matrix = projection * view;

    uint32_t imageIndex;
    auto err = window.vkAcquireNextImageKHR(window.vkDevice,
                                            window.swapchain,
                                            window.timeout,
                                            imageAvailableSemaphore.vkSemaphore,
                                            VK_NULL_HANDLE,
                                            &imageIndex);

    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        cout << "vkAcquireNextImageKHR returned VK_OUT_OF_DATE_KHR" << endl
             << "Probably the window has been resized." << endl;
        destroySwapData();
        window.destroy_swapchain();
        window.create_swapchain();
        createSwapData();
        cout << "Trying again." << endl;
        goto tryagain;
    }
    else if (err == VK_SUBOPTIMAL_KHR)
    {
        cout << "vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR" << endl;
    }
    else
    {
        call_vulkan(err);
    }

    auto& s = *swaps[imageIndex];

    window.vkResetCommandBuffer(s.commandBuffer, 0);

    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        call_vulkan(window.vkBeginCommandBuffer(s.commandBuffer, &beginInfo));

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = s.framebuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = { extent.width, extent.height };

        VkClearValue clearValues[2] = {};
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = { 0.0f, 0 };
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues = clearValues;

        window.vkCmdBeginRenderPass(s.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    if (pipelineParticles)
    {
        pipelineParticles->draw(extent, s.commandBuffer, matrix, x, potential);
    }
    if (pipelineWireframe)
    {
        pipelineWireframe->draw(extent, s.commandBuffer, matrix);
    }
    if (pipelineText)
    {
        pipelineText->draw(extent, s.commandBuffer);
    }

    window.vkCmdEndRenderPass(s.commandBuffer);
    call_vulkan(window.vkEndCommandBuffer(s.commandBuffer));

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphore.vkSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &s.commandBuffer;

    VkSemaphore signalSemaphores[] = { swaps[imageIndex]->renderFinishedSemaphore.vkSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    auto queue = reinterpret_cast<VkQueue>(window.device.get_device_queue());

    call_vulkan(window.vkQueueSubmit(queue, 1, &submitInfo, inFlightFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { window.swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    err = window.vkQueuePresentKHR(queue, &presentInfo);
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        cout << "vkQueuePresentKHR returned VK_OUT_OF_DATE_KHR" << endl;
    }
    else if (err == VK_SUBOPTIMAL_KHR)
    {
        cout << "vkQueuePresentKHR returned VK_SUBOPTIMAL_KHR" << endl
             << "Probably the window has been resized." << endl;
        destroySwapData();
        window.destroy_swapchain();
        window.create_swapchain();
        createSwapData();
    }
    else
    {
        call_vulkan(err);
    }
}

void Renderer::createSwapData()
{
    for (unsigned int k = 0; k < window.images.size(); ++k)
    {
        swaps.push_back(make_unique<swapData>(*this, get_vulkan_image(window.images[k])));
    }
}

void Renderer::destroySwapData()
{
    window.vkDeviceWaitIdle(window.vkDevice);
    swaps.clear();
}

void Renderer::cleanup()
{
    destroySwapData();
    window.vkDestroyFence(window.vkDevice, inFlightFence, nullptr);
    window.vkDestroyRenderPass(window.vkDevice, renderPass, nullptr);
}

Renderer::Renderer(sdl_window_vulkan& window0, float cubeSize, array<unsigned int, 2> overlaySize)
    : window(window0)
    , imageAvailableSemaphore(window)
{
    depthFormat = findDepthFormat();

    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = window.format.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 2;
        renderPassInfo.pAttachments = attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        call_vulkan(window.vkCreateRenderPass(window.vkDevice, &renderPassInfo, nullptr, &renderPass));
    }

    pipelineParticles.emplace(window, renderPass);
    if (cubeSize != 0)
    {
        pipelineWireframe.emplace(window, renderPass, cubeSize);
    }
    if (filesystem::exists(FONT_FILENAME()))
    {
        pipelineText.emplace(window, renderPass, FONT_FILENAME(), FONT_SIZE(), overlaySize);
    }
    else
    {
        cout << "Font '" << FONT_FILENAME() << "' does not exist. Disabling text overlay" << endl;
    }

    createSyncObjects();
}

Renderer::~Renderer()
{
    cleanup();
}

}
