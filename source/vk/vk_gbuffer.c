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

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
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
  vkCmdDraw(cmd, 3, 1, 0, 0);
}

void VK_DestroyGBuffer(vk_rend_t *rend) {
  vkDestroyPipeline(rend->device, rend->gbuffer->pipeline, NULL);
  vkDestroyPipelineLayout(rend->device, rend->gbuffer->pipeline_layout, NULL);
}
