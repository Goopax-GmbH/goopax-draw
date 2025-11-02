#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <goopax_draw/particle/pipeline/wireframe.hpp>

#include "../shaders/particles.frag.spv.cpp"
#include "../shaders/particles.vert.spv.cpp"

using namespace goopax;
using namespace std;
using Eigen::Vector;

namespace goopax_draw::vulkan
{
void PipelineWireframe::draw(VkExtent2D extent, VkCommandBuffer cb, glm::mat4 matrix)
{
    window.vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkBuffer vb[] = { get_vulkan_buffer(vertexBuffer) };
    VkDeviceSize offs[] = { 0 };
    window.vkCmdBindVertexBuffers(cb, 0, 1, vb, offs);
    window.vkCmdBindIndexBuffer(cb, get_vulkan_buffer(indexBuffer), 0, VK_INDEX_TYPE_UINT32);
    window.vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &matrix[0][0]);
    window.vkCmdDrawIndexed(cb, 24, 1, 0, 0, 0);
}

PipelineWireframe::PipelineWireframe(sdl_window_vulkan& window, VkRenderPass renderPass, float cubeSize0)
    : Pipeline(window)
    , cubeSize(cubeSize0)
{
    VkShaderModule vertShaderModule = window.createShaderModule(particles_vert_spv);
    VkShaderModule fragShaderModule = window.createShaderModule(particles_frag_spv);

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
        // 8 vertices
        std::vector<Eigen::Vector<float, 3>> vertices = {
            { -cubeSize, -cubeSize, -cubeSize }, { cubeSize, -cubeSize, -cubeSize }, { cubeSize, cubeSize, -cubeSize },
            { -cubeSize, cubeSize, -cubeSize },  { -cubeSize, -cubeSize, cubeSize }, { cubeSize, -cubeSize, cubeSize },
            { cubeSize, cubeSize, cubeSize },    { -cubeSize, cubeSize, cubeSize }
        };
        vertexBuffer.assign(window.device, vertices.size(), vulkan_vertex_flags);
        vertexBuffer = std::move(vertices);

        // 24 indices for 12 lines
        std::vector<uint32_t> indices = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7 };
        indexBuffer.assign(window.device, indices.size(), Pipeline::vulkan_index_flags);
        indexBuffer = std::move(indices);
    }
}

}
