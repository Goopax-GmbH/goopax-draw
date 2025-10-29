#include <goopax_extra/output.hpp>

#include "shaders/overlay.frag.spv.cpp"
#include "shaders/overlay.vert.spv.cpp"
#include "shaders/particles.frag.spv.cpp"
#include "shaders/particles.vert.spv.cpp"
#include "shaders/particles_pot.vert.spv.cpp"
#include <boost/iostreams/device/mapped_file.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <goopax_draw/particle/renderer_vulkan.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../ext/stb_truetype.h" // For text rendering

namespace goopax_draw::vulkan
{

const std::string font_filename = "/usr/share/fonts/Myriad Pro/Myriad Pro Regular/Myriad Pro Regular.ttf";
// const std::string font_filename = "/usr/share/fonts/liberation-fonts/LiberationSans-Regular.ttf";

using namespace goopax;
using namespace std;
using Eigen::Vector;

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

VkShaderModule Renderer::createShaderModule(span<unsigned char> prog)
{
    VkShaderModuleCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = 0,
                                            .codeSize = prog.size(),
                                            .pCode = reinterpret_cast<const uint32_t*>(prog.data()) };

    VkShaderModule module;
    call_vulkan(window.vkCreateShaderModule(window.vkDevice, &createInfo, nullptr, &module));
    return module;
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

constexpr backend_create_params vulkan_vertex_flags = {
    .vulkan = { .usage_bits = VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT
                              | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR }
};

constexpr backend_create_params vulkan_index_flags = {
    .vulkan = { .usage_bits = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT
                              | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT }
};

void Renderer::updateText(const string& text, Vector<float, 2> tl, Vector<float, 2> size, float lineheight)
{
    overlay.image.fill({ 100, 100, 0, 128 });

    auto* data = new vector<Chardata<int>>(text.size());

    Vector<float, 2> pos_orig = { 2, 60 };
    Vector<float, 2> pos = pos_orig;
    for (unsigned int k = 0; k < text.size(); ++k)
    {
        if (text[k] == '\n')
        {
            pos[0] = pos_orig[0];
            pos[1] += lineheight;
            continue;
        }

        auto c = overlay.cdata[text[k] - 32];
        (*data)[k] = { .x0 = c.x0,
                       .y0 = c.y0,
                       .dx = uint16_t(c.x1 - c.x0),
                       .dy = uint16_t(c.y1 - c.y0),
                       .dest_offset = pos + Vector<float, 2>{ c.xoff, c.yoff } };
        pos[0] += c.xadvance;
    }
    overlay.textdata.copy_from_host_async(data->data()).set_callback([data]() { delete data; });
    overlay.write_text(text.size());

    overlay.vertexBuffer =
        vector<Vector<float, 2>>{ tl, { tl[0] + size[0], tl[1] }, tl + size, { tl[0], tl[1] + size[1] } };
}

void Renderer::render(const buffer<Vector<float, 3>>& x, float distance, float theta, Vector<float, 2> xypos)
{
    if (potentialDummy.size() != x.size())
    {
        potentialDummy.assign(window.device, x.size(), vulkan_vertex_flags);
        potentialDummy.fill(0.9f);
    }

    render(x, potentialDummy, distance, theta, xypos);
}

void Renderer::render(const buffer<Vector<float, 3>>& x,
                      const buffer<float>& potential,
                      float distance,
                      float theta,
                      Vector<float, 2> xypos)
{
    // static constexpr uint64_t timeout = 60000000000ul;
    window.vkWaitForFences(window.vkDevice, 1, &inFlightFence, VK_TRUE, window.timeout);
    window.vkResetFences(window.vkDevice, 1, &inFlightFence);

tryagain:
    auto extent = window.surfaceCapabilities.currentExtent;

    float aspect_ratio = float(extent.width) / extent.height; // From your Vulkan swapchain or window
    float fov = glm::radians(60.0f); // Field of view (60 degrees is a common starting point; adjust as needed)
    float near_clip = 0.01f;         // Near clipping plane (small positive value)
    float far_clip = 100.0f;         // Far clipping plane (larger than D + scene depth)

    // Compute camera position after rotation around y-axis
    glm::vec3 camera_pos = glm::vec3(distance * sin(theta) + xypos[0], xypos[1], distance * cos(theta));

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

        window.vkCmdBindPipeline(s.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particles.graphicsPipeline);
    }

    {
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        window.vkCmdSetViewport(s.commandBuffer, 0, 1, &viewport);

        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = { extent.width, extent.height };
        window.vkCmdSetScissor(s.commandBuffer, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = { reinterpret_cast<VkBuffer>(x.get_handle()),
                                     reinterpret_cast<VkBuffer>(potential.get_handle()) };
        VkDeviceSize offsets[] = { 0, 0 };
        window.vkCmdBindVertexBuffers(s.commandBuffer, 0, 2, vertexBuffers, offsets);

        window.vkCmdPushConstants(
            s.commandBuffer, particles.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &matrix[0][0]);

        window.vkCmdDraw(s.commandBuffer, x.size(), 1, 0, 0);
    }

    // Draw cube
    if (cube.cubeSize > 0)
    {
        window.vkCmdBindPipeline(s.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cube.graphicsPipeline);
        VkBuffer vb[] = { get_vulkan_buffer(cube.vertexBuffer) };
        VkDeviceSize offs[] = { 0 };
        window.vkCmdBindVertexBuffers(s.commandBuffer, 0, 1, vb, offs);
        window.vkCmdBindIndexBuffer(s.commandBuffer, get_vulkan_buffer(cube.indexBuffer), 0, VK_INDEX_TYPE_UINT32);
        window.vkCmdPushConstants(
            s.commandBuffer, cube.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &matrix[0][0]);
        window.vkCmdDrawIndexed(s.commandBuffer, 24, 1, 0, 0, 0);
    }

    // Draw overlay (after 3D to overlay)
    {
        window.vkCmdBindPipeline(s.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlay.graphicsPipeline);

        VkDescriptorImageInfo update_imageInfos[] = {
            { .sampler = {}, .imageView = overlay.textureView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL }
        };

        VkWriteDescriptorSet update_writes[] = { { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                   .pNext = nullptr,
                                                   .dstSet = overlay.descriptorSet,
                                                   .dstBinding = 0,
                                                   .dstArrayElement = 0,
                                                   .descriptorCount = 1,
                                                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                   .pImageInfo = update_imageInfos,
                                                   .pBufferInfo = nullptr,
                                                   .pTexelBufferView = nullptr } };

        window.vkUpdateDescriptorSets(window.vkDevice, 1, update_writes, 0, nullptr);

        window.vkCmdBindDescriptorSets(s.commandBuffer,
                                       VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       overlay.pipelineLayout,
                                       0,
                                       1,
                                       &overlay.descriptorSet,
                                       0,
                                       nullptr);
        VkBuffer vertexBuffers[] = { get_vulkan_buffer(overlay.vertexBuffer),
                                     get_vulkan_buffer(overlay.texCoordBuffer) };
        VkDeviceSize offsets[] = { 0, 0 };

        window.vkCmdBindVertexBuffers(s.commandBuffer, 0, 2, vertexBuffers, offsets);
        window.vkCmdBindIndexBuffer(s.commandBuffer, get_vulkan_buffer(overlay.indexBuffer), 0, VK_INDEX_TYPE_UINT32);

        // Bind vertices, tex coords, descriptor set for texture/sampler
        glm::mat4 ortho = glm::ortho(0.f, (float)extent.width, 0.f, (float)extent.height, -1.f, 1.f);

        window.vkCmdPushConstants(
            s.commandBuffer, overlay.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &ortho);

        window.vkCmdDrawIndexed(s.commandBuffer, 6, 1, 0, 0, 0);
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
    window.vkDestroyPipeline(window.vkDevice, particles.graphicsPipeline, nullptr);
    window.vkDestroyPipelineLayout(window.vkDevice, particles.pipelineLayout, nullptr);
    window.vkDestroyRenderPass(window.vkDevice, renderPass, nullptr);
}

Renderer::Renderer(sdl_window_vulkan& window0, float cubeSize, array<unsigned int, 2> overlaySize)
    : window(window0)
    , imageAvailableSemaphore(window)
{
    depthFormat = findDepthFormat();
    cube.cubeSize = cubeSize;

    // createRenderPass();
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

    //    createGraphicsPipeline();
    {
        VkShaderModule vertShaderModule = createShaderModule(particles_pot_vert_spv);
        VkShaderModule fragShaderModule = createShaderModule(particles_frag_spv);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        VkVertexInputBindingDescription bindingDescriptions[2] = {};
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(float) * 3;
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = sizeof(float);
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attributeDescriptions[2] = {};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;
        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[1].offset = 0;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 2;
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions;
        vertexInputInfo.vertexAttributeDescriptionCount = 2;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil = {};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE; // Matching your commented-out blend

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPushConstantRange pushConstant = {};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstant.offset = 0;
        pushConstant.size = sizeof(glm::mat4);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

        call_vulkan(
            window.vkCreatePipelineLayout(window.vkDevice, &pipelineLayoutInfo, nullptr, &particles.pipelineLayout));

        VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = 2;
        dynamicState.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = particles.pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        call_vulkan(window.vkCreateGraphicsPipelines(
            window.vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &particles.graphicsPipeline));

        window.vkDestroyShaderModule(window.vkDevice, fragShaderModule, nullptr);
        window.vkDestroyShaderModule(window.vkDevice, vertShaderModule, nullptr);
    }

    {
        // In constructor, after createGraphicsPipeline()
        createGraphicsPipelineLines();
        createGraphicsPipeline2D();
        createCubeBuffers(); // Default size; can resize later
        createOverlayResources(overlaySize);
        // Load font for text (assume arial.ttf is in your assets)

        {
            boost::iostreams::mapped_file_source ttf_buffer(font_filename);
            // unsigned char ttf_buffer[1<<20];
            // fread(ttf_buffer, 1, 1<<20, fopen("/usr/share/fonts/Myriad Pro/Myriad Pro Regular/Myriad Pro
            // Regular.ttf", "rb")); unsigned char temp_bitmap[512*512];

            overlay.textdata.assign(window.device, 256);

            image_buffer_map map(overlay.characters);
            float fontsize = 60;
            stbtt_BakeFontBitmap(reinterpret_cast<const uint8_t*>(ttf_buffer.data()),
                                 0,
                                 fontsize,
                                 (unsigned char*)&map[{ 0, 0 }],
                                 512,
                                 512,
                                 32,
                                 96,
                                 overlay.cdata); // Bake font atlas
        }

        // cout << "character map: " << map << endl;

        /*
      for (uint k=0; k<96; ++k)
      {
        auto& c = overlay.cdata[k];
        for (uint y=c.y0; y<c.y1; ++y)
          {
            for (uint x=c.x0; x<c.x1; ++x)
          {
            cout << char('0' + (int)map[{x, y}][0]/26);
          }
            cout << endl;
          }
        cout << endl;
      }
        */

        overlay.write_text.assign(window.device, [this](gpu_uint N) {
            gpu_for_group(0, N, [&](gpu_uint k) {
                auto cd = overlay.textdata[k];
                gpu_for_local(0, cd.dy + 1, [&](gpu_int y) {
                    gpu_for(0, cd.dx + 1, [&](gpu_int x) {
                        Vector<gpu_float, 2> rsrc = { x + cd.x0, y + cd.y0 };
                        Vector<gpu_float, 2> rdest = { x + cd.dest_offset[0], y + cd.dest_offset[1] };
                        Vector<gpu_uint, 2> rdest_u = rdest.cast<gpu_uint>();
                        rsrc += rdest - rdest_u.cast<gpu_float>();
                        gpu_float c = image_resource(overlay.characters).read(rsrc, filter_linear | address_none)[0];
                        image_resource(overlay.image).write(rdest_u, { 1, 1, 1, c });
                    });
                });
            });
        });

        // Upload temp_bitmap to a texture (similar to createImage, but with VK_FORMAT_R8_UNORM and sampling)
    }

    createSyncObjects();
}

Renderer::~Renderer()
{
    cleanup();
}

void Renderer::createGraphicsPipelineLines()
{
    // Similar to existing createGraphicsPipeline, but change:
    // inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    // Remove potential input (only positions, binding 0)
    // vertexInputInfo.vertexBindingDescriptionCount = 1;
    // vertexInputInfo.vertexAttributeDescriptionCount = 1;
    // rasterizer.lineWidth = 2.0f;  // Thicker lines for visibility
    // Use same shaders or custom ones (e.g., simple color in frag)
    // call_vulkan(vkCreateGraphicsPipelines(..., &graphicsPipelineLines));

    VkShaderModule vertShaderModule = createShaderModule(particles_vert_spv);
    VkShaderModule fragShaderModule = createShaderModule(particles_frag_spv);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkVertexInputBindingDescription bindingDescriptions[1] = {};
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(float) * 3;
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[1] = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE; // Matching your commented-out blend

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    call_vulkan(window.vkCreatePipelineLayout(window.vkDevice, &pipelineLayoutInfo, nullptr, &cube.pipelineLayout));

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = cube.pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    call_vulkan(window.vkCreateGraphicsPipelines(
        window.vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &cube.graphicsPipeline));

    window.vkDestroyShaderModule(window.vkDevice, fragShaderModule, nullptr);
    window.vkDestroyShaderModule(window.vkDevice, vertShaderModule, nullptr);
}

void Renderer::createGraphicsPipeline2D()
{
    // Similar, but:
    // inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Bindings: 0 for pos (Vec2), 1 for UV (Vec2)
    // Disable depth: depthStencil.depthTestEnable = VK_FALSE; depthStencil.depthWriteEnable = VK_FALSE;
    // Enable blending if needed for transparency: colorBlendAttachment.blendEnable = VK_TRUE; etc.
    // Shaders: Custom vert/frag for textured quad (sample texture, add text from atlas)
    // Push constants for ortho matrix
    // call_vulkan(vkCreateGraphicsPipelines(..., &graphicsPipeline2D));

    VkShaderModule vertShaderModule = createShaderModule(overlay_vert_spv);
    VkShaderModule fragShaderModule = createShaderModule(overlay_frag_spv);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkVertexInputBindingDescription bindingDescriptions[2] = {};
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(float) * 2;
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescriptions[1].binding = 1;
    bindingDescriptions[1].stride = sizeof(float) * 2;
    bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[2] = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = 0;
    attributeDescriptions[1].binding = 1;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 2;
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(glm::mat4);

    {
        VkSamplerCreateInfo info = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .pNext = nullptr,
                                     .flags = 0,
                                     .magFilter = VK_FILTER_NEAREST,
                                     .minFilter = VK_FILTER_NEAREST,
                                     .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .mipLodBias = 0,
                                     .anisotropyEnable = false,
                                     .maxAnisotropy = 0,
                                     .compareEnable = false,
                                     .compareOp = VK_COMPARE_OP_NEVER,
                                     .minLod = 0,
                                     .maxLod = 0,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = false };

        call_vulkan(window.vkCreateSampler(window.vkDevice, &info, nullptr, &overlay.sampler));
    }
    {

        std::vector<VkDescriptorSetLayoutBinding> bindings = { { .binding = 0,
                                                                 .descriptorType =
                                                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                 .descriptorCount = 1,
                                                                 .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                                 .pImmutableSamplers = &overlay.sampler } };

        VkDescriptorSetLayoutCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .pNext = nullptr,
                                                 .flags = 0,
                                                 .bindingCount = (unsigned int)bindings.size(),
                                                 .pBindings = bindings.data() };

        call_vulkan(window.vkCreateDescriptorSetLayout(window.vkDevice, &info, nullptr, &overlay.descriptorSetLayout));
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &overlay.descriptorSetLayout;

    call_vulkan(window.vkCreatePipelineLayout(window.vkDevice, &pipelineLayoutInfo, nullptr, &overlay.pipelineLayout));

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = overlay.pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    call_vulkan(window.vkCreateGraphicsPipelines(
        window.vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &overlay.graphicsPipeline));

    window.vkDestroyShaderModule(window.vkDevice, fragShaderModule, nullptr);
    window.vkDestroyShaderModule(window.vkDevice, vertShaderModule, nullptr);

    {
        constexpr size_t max_size = 16;
        array<VkDescriptorPoolSize, 4> poolSizes = {
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = max_size },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = max_size },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = max_size },
            VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = max_size }
        };

        VkDescriptorPoolCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .pNext = nullptr,
                                            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                            .maxSets = max_size / 4,
                                            .poolSizeCount = poolSizes.size(),
                                            .pPoolSizes = poolSizes.data() };

        call_vulkan(window.vkCreateDescriptorPool(window.vkDevice, &info, nullptr, &overlay.descriptorPool));
    }

    {
        VkDescriptorSetAllocateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             .pNext = nullptr,
                                             .descriptorPool = overlay.descriptorPool,
                                             .descriptorSetCount = 1,
                                             .pSetLayouts = &overlay.descriptorSetLayout };

        call_vulkan(window.vkAllocateDescriptorSets(window.vkDevice, &info, &overlay.descriptorSet));
    }
}

void Renderer::createCubeBuffers()
{
    // 8 vertices
    std::vector<Eigen::Vector<float, 3>> vertices = {
        { -cube.cubeSize, -cube.cubeSize, -cube.cubeSize }, { cube.cubeSize, -cube.cubeSize, -cube.cubeSize },
        { cube.cubeSize, cube.cubeSize, -cube.cubeSize },   { -cube.cubeSize, cube.cubeSize, -cube.cubeSize },
        { -cube.cubeSize, -cube.cubeSize, cube.cubeSize },  { cube.cubeSize, -cube.cubeSize, cube.cubeSize },
        { cube.cubeSize, cube.cubeSize, cube.cubeSize },    { -cube.cubeSize, cube.cubeSize, cube.cubeSize }
    };
    cube.vertexBuffer.assign(window.device, vertices.size(), vulkan_vertex_flags);
    cube.vertexBuffer = std::move(vertices);

    // 24 indices for 12 lines
    std::vector<uint32_t> indices = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7 };
    cube.indexBuffer.assign(window.device, indices.size(), vulkan_index_flags);
    cube.indexBuffer = std::move(indices);
}

void Renderer::createOverlayResources(array<unsigned int, 2> overlaySize)
{
    overlay.image.assign(window.device,
                         overlaySize,
                         BUFFER_READ_WRITE,
                         backend_create_params{ .vulkan = { .image_format = window.format.format } });
    // Create quad: positions and UVs for a screen-space quad (e.g., bottom-left, size 200x50)
    overlay.vertexBuffer.assign(window.device,
                                vector<Vector<float, 2>>{ { 0, 0 },
                                                          { overlay.image.width(), 0 },
                                                          { overlay.image.width(), overlay.image.height() },
                                                          { 0, overlay.image.height() } },
                                vulkan_vertex_flags); // Scale/position in shader
    overlay.texCoordBuffer.assign(
        window.device, vector<Vector<float, 2>>{ { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } }, vulkan_vertex_flags);
    overlay.indexBuffer.assign(window.device, vector<unsigned int>{ 0, 1, 2, 0, 2, 3 }, vulkan_index_flags);

    // Indices: {0,1,2, 0,2,3}

    // Create texture (e.g., 512x512, VK_FORMAT_R8G8B8A8_UNORM)
    /*
      createImage(512, 512, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, overlayTexture, overlayTextureMemory);
    */

    overlay.characters.assign(window.device, { 512, 512 }, BUFFER_READ_WRITE);
    overlay.image.fill({ 0, 0, 0, 0 });
    // overlay.image.fill({128,0,255,255}, {300,100}, {200,200});
    // overlay.image.fill({255,0,128,128}, {100,100}, {200,200});
    //  Create image view and sampler (linear filtering)

    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = get_vulkan_image(overlay.image), createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
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
        call_vulkan(window.vkCreateImageView(window.vkDevice, &createInfo, nullptr, &overlay.textureView));
    }
}

void Renderer::destroyAdditionalResources()
{
    // vkDestroyPipeline, vkDestroyImage, etc. for new resources
}
}
