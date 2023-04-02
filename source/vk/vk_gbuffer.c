#include "vk.h"
#include "vk_private.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL_vulkan.h>

bool VK_InitGBuffer(vk_rend_t *rend) {
  if (rend->gbuffer) {
    printf("GBuffer seems to be already initialized.\n");
    return false;
  }

  rend->gbuffer = calloc(1, sizeof(vk_gbuffer_t));

  {
    VkVertexInputBindingDescription main_binding = {
        .binding = 0,
        .stride = sizeof(vertex_t),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription pos_attribute = {
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0,
    };

    VkVertexInputAttributeDescription norm_attribute = {
        .binding = 0,
        .location = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = sizeof(float) * 3,
    };

    VkVertexInputAttributeDescription uv_attribute = {
        .binding = 0,
        .location = 2,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = sizeof(float) * 3 * 2,
    };

    VkVertexInputAttributeDescription attributes[3] = {
        [0] = pos_attribute,
        [1] = norm_attribute,
        [2] = uv_attribute,
    };

    VkShaderModule vertex_shader =
        VK_LoadShaderModule(rend, "gbuffer.vert.spv");
    if (!vertex_shader) {
      printf("Couldn't create vertex shader module from "
             "`gbuffer.vert.spv`.\n");
      return false;
    }
    VkShaderModule fragment_shader =
        VK_LoadShaderModule(rend, "gbuffer.frag.spv");
    if (!fragment_shader) {
      printf("Couldn't create vertex shader module from "
             "`gbuffer.frag.spv`.\n");
      return false;
    }

    VkPipelineShaderStageCreateInfo vertex_stage =
        VK_PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                         vertex_shader);
    VkPipelineShaderStageCreateInfo fragment_stage =
        VK_PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                         fragment_shader);

    VkPipelineShaderStageCreateInfo stages[2] = {
        [0] = vertex_stage,
        [1] = fragment_stage,
    };

    VkViewport viewport = {
        .height = rend->height,
        .width = rend->width,
        .x = 0,
        .y = 0,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .extent =
            {
                .height = rend->height,
                .width = rend->width,
            },
        .offset =
            {
                .x = 0,
                .y = 0,
            },
    };

    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineColorBlendAttachmentState blend_attachment =
        VK_PipelineColorBlendAttachmentState();

    VkPipelineVertexInputStateCreateInfo input_state_info =
        VK_PipelineVertexInputStateCreateInfo();

    input_state_info.pVertexAttributeDescriptions = &attributes[0];
    input_state_info.vertexAttributeDescriptionCount = 3;
    input_state_info.pVertexBindingDescriptions = &main_binding;
    input_state_info.vertexBindingDescriptionCount = 1;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info =
        VK_PipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    VkPipelineRasterizationStateCreateInfo rasterization_info =
        VK_PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

    VkPipelineMultisampleStateCreateInfo multisample_info =
        VK_PipelineMultisampleStateCreateInfo();

    VkPipelineColorBlendStateCreateInfo blending_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };

    VkPushConstantRange push_constant_info = {
        .offset = 0,
        .size = sizeof(mat4),
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pPushConstantRanges = &push_constant_info,
        .pushConstantRangeCount = 1,
        .pSetLayouts = &rend->global_desc_set_layout,
        .setLayoutCount = 1,
        .pNext = NULL,
    };

    vkCreatePipelineLayout(rend->device, &pipeline_layout_info, NULL,
                           &rend->gbuffer->pipeline_layout);

    VkPipelineDepthStencilStateCreateInfo depth_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
        .stencilTestEnable = VK_FALSE,
    };

    const VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &rend->swapchain_format,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = &stages[0],
        .pVertexInputState = &input_state_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisample_info,
        .pColorBlendState = &blending_info,
        .layout = rend->gbuffer->pipeline_layout,
        .pNext = &pipeline_rendering_create_info,
        .pDepthStencilState = &depth_state_info,
    };

    vkCreateGraphicsPipelines(rend->device, VK_NULL_HANDLE, 1, &pipeline_info,
                              NULL, &rend->gbuffer->pipeline);

    vkDestroyShaderModule(rend->device, vertex_shader, NULL);
    vkDestroyShaderModule(rend->device, fragment_shader, NULL);
  }

  // Create depth texture
  {
    // hey
    VkExtent3D extend = {
        .depth = 1,
        .width = rend->width,
        .height = rend->height,
    };

    VkImageCreateInfo depth_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = extend,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    };

    VmaAllocationCreateInfo depth_alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateImage(rend->allocator, &depth_image_info, &depth_alloc_info,
                   &rend->gbuffer->depth_map_image,
                   &rend->gbuffer->depth_map_alloc, NULL);

    VkImageViewCreateInfo depth_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .image = rend->gbuffer->depth_map_image,
        .format = VK_FORMAT_D32_SFLOAT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
    };

    vkCreateImageView(rend->device, &depth_view_info, NULL,
                      &rend->gbuffer->depth_map_view);
  }

  return true;
}

void VK_DrawGBuffer(vk_rend_t *rend) {
  vk_gbuffer_t *gbuffer = rend->gbuffer;
  VkCommandBuffer cmd = rend->graphics_command_buffer[rend->current_frame % 3];

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gbuffer->pipeline);

  vec3 eye = {20.0 * sinf(glm_rad(rend->current_frame)), 12.0,
              20.0 * cosf(glm_rad(rend->current_frame))};
  vec3 center = {0.0, 0.0, 0.0};
  vec3 up = {0.0, 1.0, 0.0};

  glm_lookat(eye, center, up, rend->global_ubo.view);
  glm_perspective(glm_rad(90.0f), (float)rend->width / (float)rend->height,
                  0.01f, 50.0f, rend->global_ubo.proj);
  rend->global_ubo.proj[1][1] *= -1;
  glm_mat4_mul(rend->global_ubo.proj, rend->global_ubo.view,
               rend->global_ubo.view_proj);

  vkCmdPushConstants(cmd, rend->gbuffer->pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4),
                     &rend->global_ubo.view_proj[0]);

  void *data;
  vmaMapMemory(rend->allocator, rend->global_allocs[rend->current_frame % 3],
               &data);

  memcpy(data, &rend->global_ubo, sizeof(vk_global_ubo_t));

  vmaUnmapMemory(rend->allocator, rend->global_allocs[rend->current_frame % 3]);

  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rend->gbuffer->pipeline_layout, 0,
      1, &rend->global_desc_set[rend->current_frame % 3], 0, NULL);

  for (unsigned i = 0; i < rend->map.primitive_count; i++) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &rend->map.vertex_buffers[i], &offset);
    vkCmdBindIndexBuffer(cmd, rend->map.index_buffers[i], offset,
                         VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, rend->map.index_counts[i], 1, 0, 0, 0);
  }
}

void VK_DestroyGBuffer(vk_rend_t *rend) {
  vkDestroyImageView(rend->device, rend->gbuffer->depth_map_view, NULL);
  vmaDestroyImage(rend->allocator, rend->gbuffer->depth_map_image,
                  rend->gbuffer->depth_map_alloc);
  vkDestroyPipeline(rend->device, rend->gbuffer->pipeline, NULL);
  vkDestroyPipelineLayout(rend->device, rend->gbuffer->pipeline_layout, NULL);
}
