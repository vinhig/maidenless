#include "vk.h"
#include "vk_private.h"

#include "cglm/cglm.h"

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

  VkShaderModule vertex_shader = VK_LoadShaderModule(rend, "gbuffer.vert.spv");
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
      .pNext = NULL,
  };

  vkCreatePipelineLayout(rend->device, &pipeline_layout_info, NULL,
                         &rend->gbuffer->pipeline_layout);

  const VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &rend->swapchain_format,
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
  };

  vkCreateGraphicsPipelines(rend->device, VK_NULL_HANDLE, 1, &pipeline_info,
                            NULL, &rend->gbuffer->pipeline);

  vkDestroyShaderModule(rend->device, vertex_shader, NULL);
  vkDestroyShaderModule(rend->device, fragment_shader, NULL);

  return true;
}

void VK_DrawGBuffer(vk_rend_t *rend) {
  vk_gbuffer_t *gbuffer = rend->gbuffer;
  VkCommandBuffer cmd = rend->command_buffer[rend->current_frame % 3];

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gbuffer->pipeline);

  mat4 proj, view, view_proj;

  vec3 eye = {20.0 * sinf(glm_rad(rend->current_frame)), 12.0, 20.0 * cosf(glm_rad(rend->current_frame))};
  vec3 center = {0.0, 0.0, 0.0};
  vec3 up = {0.0, 1.0, 0.0};

  glm_lookat(eye, center, up, view);
  glm_perspective(glm_rad(90.0f), (float)rend->width / (float)rend->height, 0.01f, 50.0f,
                  proj);
  proj[1][1] *= -1;
  glm_mat4_mul(proj, view, view_proj);

  vkCmdPushConstants(cmd, rend->gbuffer->pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4),
                     &view_proj[0]);

  for (unsigned i = 0; i < rend->map.primitive_count; i++) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &rend->map.vertex_buffers[i], &offset);
    vkCmdBindIndexBuffer(cmd, rend->map.index_buffers[i], offset,
                         VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, rend->map.index_counts[i], 1, 0, 0, 0);
  }
}

void VK_DestroyGBuffer(vk_rend_t *rend) {
  vkDestroyPipeline(rend->device, rend->gbuffer->pipeline, NULL);
  vkDestroyPipelineLayout(rend->device, rend->gbuffer->pipeline_layout, NULL);
}
