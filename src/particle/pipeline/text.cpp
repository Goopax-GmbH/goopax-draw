#include "../shaders/overlay.frag.spv.cpp"
#include "../shaders/overlay.vert.spv.cpp"
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <goopax_draw/particle/pipeline/text.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <goopax_draw/../../ext/stb_truetype.h>

using namespace goopax;
using namespace std;
using Eigen::Vector;

namespace goopax_draw::vulkan
{
void PipelineText::draw(VkExtent2D extent, VkCommandBuffer cb)
{
    window.vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDescriptorImageInfo update_imageInfos[] = {
        { .sampler = {}, .imageView = textureView, .imageLayout = VK_IMAGE_LAYOUT_GENERAL }
    };

    VkWriteDescriptorSet update_writes[] = { { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                               .pNext = nullptr,
                                               .dstSet = descriptorSet,
                                               .dstBinding = 0,
                                               .dstArrayElement = 0,
                                               .descriptorCount = 1,
                                               .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               .pImageInfo = update_imageInfos,
                                               .pBufferInfo = nullptr,
                                               .pTexelBufferView = nullptr } };

    window.vkUpdateDescriptorSets(window.vkDevice, 1, update_writes, 0, nullptr);

    window.vkCmdBindDescriptorSets(
        cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    VkBuffer vertexBuffers[] = { get_vulkan_buffer(vertexBuffer), get_vulkan_buffer(texCoordBuffer) };
    VkDeviceSize offsets[] = { 0, 0 };

    window.vkCmdBindVertexBuffers(cb, 0, 2, vertexBuffers, offsets);
    window.vkCmdBindIndexBuffer(cb, get_vulkan_buffer(indexBuffer), 0, VK_INDEX_TYPE_UINT32);

    // Bind vertices, tex coords, descriptor set for texture/sampler
    glm::mat4 ortho = glm::ortho(0.f, (float)extent.width, 0.f, (float)extent.height, -1.f, 1.f);

    window.vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &ortho);

    window.vkCmdDrawIndexed(cb, 6, 1, 0, 0, 0);
}

void PipelineText::updateText(const string& text, Vector<float, 2> tl)
{
    image.fill((bgColor * 255).cast<uint8_t>());

    auto* data = new vector<Chardata<int>>(text.size());

    Vector<float, 2> pos_orig = { 2, fontSize };
    Vector<float, 2> pos = pos_orig;
    for (unsigned int k = 0; k < text.size(); ++k)
    {
        if (text[k] == '\n')
        {
            pos[0] = pos_orig[0];
            pos[1] += fontSize;
            continue;
        }

        auto c = cdata[text[k] - 32];
        (*data)[k] = { .x0 = c.x0,
                       .y0 = c.y0,
                       .dx = uint16_t(c.x1 - c.x0),
                       .dy = uint16_t(c.y1 - c.y0),
                       .dest_offset = pos + Vector<float, 2>{ c.xoff, c.yoff } };
        pos[0] += c.xadvance;
    }
    textdata.copy_from_host_async(data->data(), 0, text.size()).set_callback([data]() { delete data; });
    write_text(text.size());

    Vector<float, 2> size = { image.width(), image.height() };
    vertexBuffer = vector<Vector<float, 2>>{ tl, { tl[0] + size[0], tl[1] }, tl + size, { tl[0], tl[1] + size[1] } };
}

PipelineText::PipelineText(sdl_window_vulkan& window,
                           VkRenderPass renderPass,
                           const std::filesystem::path& fontFilename,
                           float fontSize0,
                           std::array<unsigned int, 2> overlaySize)
    : Pipeline(window)
    , fontSize(fontSize0)
{
    VkShaderModule vertShaderModule = window.createShaderModule(overlay_vert_spv);
    VkShaderModule fragShaderModule = window.createShaderModule(overlay_frag_spv);

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

        call_vulkan(window.vkCreateSampler(window.vkDevice, &info, nullptr, &sampler));
    }
    {

        std::vector<VkDescriptorSetLayoutBinding> bindings = { { .binding = 0,
                                                                 .descriptorType =
                                                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                 .descriptorCount = 1,
                                                                 .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                                 .pImmutableSamplers = &sampler } };

        VkDescriptorSetLayoutCreateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .pNext = nullptr,
                                                 .flags = 0,
                                                 .bindingCount = (unsigned int)bindings.size(),
                                                 .pBindings = bindings.data() };

        call_vulkan(window.vkCreateDescriptorSetLayout(window.vkDevice, &info, nullptr, &descriptorSetLayout));
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    call_vulkan(window.vkCreatePipelineLayout(window.vkDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout));

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
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    call_vulkan(
        window.vkCreateGraphicsPipelines(window.vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

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

        call_vulkan(window.vkCreateDescriptorPool(window.vkDevice, &info, nullptr, &descriptorPool));
    }

    {
        VkDescriptorSetAllocateInfo info = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             .pNext = nullptr,
                                             .descriptorPool = descriptorPool,
                                             .descriptorSetCount = 1,
                                             .pSetLayouts = &descriptorSetLayout };

        call_vulkan(window.vkAllocateDescriptorSets(window.vkDevice, &info, &descriptorSet));
    }

    {
        image.assign(window.device,
                     overlaySize,
                     BUFFER_READ_WRITE,
                     backend_create_params{ .vulkan = { .image_format = (uint32_t)window.format.format } });

        // Create quad: positions and UVs for a screen-space quad (e.g., bottom-left, size 200x50)
        vertexBuffer.assign(
            window.device,
            vector<Vector<float, 2>>{
                { 0, 0 }, { image.width(), 0 }, { image.width(), image.height() }, { 0, image.height() } },
            Pipeline::vulkan_vertex_flags); // Scale/position in shader
        texCoordBuffer.assign(window.device,
                              vector<Vector<float, 2>>{ { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } },
                              Pipeline::vulkan_vertex_flags);
        indexBuffer.assign(window.device, vector<unsigned int>{ 0, 1, 2, 0, 2, 3 }, Pipeline::vulkan_index_flags);

        characters.assign(window.device, { 512, 512 }, BUFFER_READ_WRITE);
        image.fill({ 0, 0, 0, 0 });

        {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = get_vulkan_image(image), createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
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
            call_vulkan(window.vkCreateImageView(window.vkDevice, &createInfo, nullptr, &textureView));
        }
        {
            vector<char> ttf_buffer(std::filesystem::file_size(fontFilename));
            ifstream file(fontFilename, std::ios::binary);
            file.read(ttf_buffer.data(), ttf_buffer.size());

            textdata.assign(window.device, 256);

            image_buffer_map map(characters);
            stbtt_BakeFontBitmap(reinterpret_cast<const uint8_t*>(ttf_buffer.data()),
                                 0,
                                 fontSize,
                                 (unsigned char*)&map[{ 0, 0 }],
                                 512,
                                 512,
                                 32,
                                 96,
                                 cdata); // Bake font atlas
        }
    }
    write_text.assign(window.device, [this](gpu_uint N) {
        gpu_for_group(0, N, [&](gpu_uint k) {
            auto cd = textdata[k];
            gpu_for_local(0, cd.dy + 1, [&](gpu_int y) {
                gpu_for(0, cd.dx + 1, [&](gpu_int x) {
                    Vector<gpu_float, 2> rsrc = { x + cd.x0, y + cd.y0 };
                    Vector<gpu_float, 2> rdest = { x + cd.dest_offset[0], y + cd.dest_offset[1] };
                    Vector<gpu_uint, 2> rdest_u = rdest.cast<gpu_uint>();
                    rsrc += rdest - rdest_u.cast<gpu_float>();
                    gpu_float c = image_resource(characters).read(rsrc, filter_linear | address_none)[0];
                    image_resource(image).write(
                        rdest_u, bgColor.cast<gpu_float>() + c * (textColor - bgColor).cast<gpu_float>());
                });
            });
        });
    });
}

PipelineText::~PipelineText()
{
    window.vkFreeDescriptorSets(window.vkDevice, descriptorPool, 1, &descriptorSet);
    window.vkDestroyDescriptorSetLayout(window.vkDevice, descriptorSetLayout, nullptr);
    window.vkDestroyDescriptorPool(window.vkDevice, descriptorPool, nullptr);
    window.vkDestroySampler(window.vkDevice, sampler, nullptr);
    window.vkDestroyImageView(window.vkDevice, textureView, nullptr);
}

}
