#include "vk.h"
#include "vk_private.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool VK_InitShading(vk_rend_t *rend) {
  if (rend->shading) {
    printf("Shadow seems to be already initialized.\n");
    return false;
  }

  rend->shading = calloc(1, sizeof(vk_shading_t));

  // Create "Render" Target
  {
    VkExtent3D extent = {
        .depth = 1,
        .width = rend->width,
        .height = rend->height,
    };

    VkImageCreateInfo target_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT,
    };

    VmaAllocationCreateInfo target_alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateImage(rend->allocator, &target_image_info, &target_alloc_info,
                   &rend->shading->shading_image,
                   &rend->shading->shading_image_alloc, NULL);

    VkImageViewCreateInfo target_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .image = rend->shading->shading_image,
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    };

    vkCreateImageView(rend->device, &target_view_info, NULL,
                      &rend->shading->shading_view);
  }

  // Create descriptor set holding "render" target, the acceleration
  // structure, and the position/normal/albedo texture
  {
    VkDescriptorSetLayoutBinding render_target = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .binding = 0,
        .descriptorCount = 1,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding acc_structure = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .binding = 1,
        .descriptorCount = 1,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding position_feature_buffer = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .binding = 2,
        .descriptorCount = 1,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding normal_feature_buffer = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .binding = 3,
        .descriptorCount = 1,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding albedo_feature_buffer = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .binding = 4,
        .descriptorCount = 1,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding depth_feature_buffer = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .binding = 5,
        .descriptorCount = 1,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutBinding bindings[] = {
        render_target,         acc_structure,         position_feature_buffer,
        normal_feature_buffer, albedo_feature_buffer, depth_feature_buffer,
    };

    VkDescriptorSetLayoutCreateInfo hold_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 6,
        .pBindings = &bindings[0],
    };

    vkCreateDescriptorSetLayout(rend->device, &hold_layout_info, NULL,
                                &rend->shading->hold_layout);

    VkDescriptorSetAllocateInfo hold_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .descriptorPool = rend->descriptor_pool,
        .pSetLayouts = &rend->shading->hold_layout,
    };

    vkAllocateDescriptorSets(rend->device, &hold_set_info,
                             &rend->shading->hold_set);

    VkDescriptorImageInfo image_infos[4] = {
        [0] =
            {
                .sampler = rend->linear_sampler,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .imageView = rend->gbuffer->position_target.image_view,
            },
        [1] =
            {
                .sampler = rend->linear_sampler,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .imageView = rend->gbuffer->normal_target.image_view,
            },
        [2] =
            {
                .sampler = rend->linear_sampler,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .imageView = rend->gbuffer->albedo_target.image_view,
            },
        [3] =
            {
                .sampler = rend->linear_sampler,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .imageView = rend->gbuffer->depth_target.image_view,
            },
    };

    // Update descriptor set, link feature buffers
    VkWriteDescriptorSet writes[4] = {[0].pNext = NULL};

    for (int i = 0; i < 4; i++) {
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].descriptorCount = 1;
      writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writes[i].dstSet = rend->shading->hold_set;
      writes[i].dstBinding = 2 + i;
      writes[i].pImageInfo = &image_infos[i];
    }

    vkUpdateDescriptorSets(rend->device, 4, &writes[0], 0, NULL);

    // Update descriptor set, link shading image (where the shading will be
    // written to)
    VkDescriptorImageInfo shading_view_info = {
        .imageView = rend->shading->shading_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .dstSet = rend->shading->hold_set,
        .dstBinding = 0,
        .pImageInfo = &shading_view_info,
    };

    vkUpdateDescriptorSets(rend->device, 1, &write, 0, NULL);
  }

  VkShaderModule comp_shader = VK_LoadShaderModule(rend, "shading.comp.spv");

  VkPipelineShaderStageCreateInfo comp_stage = VK_PipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_COMPUTE_BIT, comp_shader);

  VkPushConstantRange push_constant_info = {
      .offset = 0,
      .size = sizeof(unsigned),
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
  };

  VkDescriptorSetLayout layouts[] = {
      rend->global_ubo_desc_set_layout,
      rend->shading->hold_layout,
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pPushConstantRanges = &push_constant_info,
      .pushConstantRangeCount = 1,
      .setLayoutCount = 2,
      .pSetLayouts = &layouts[0],
  };

  vkCreatePipelineLayout(rend->device, &pipeline_layout_info, NULL,
                         &rend->shading->pipeline_layout);

  VkComputePipelineCreateInfo ray_pipeline = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = comp_stage,
      .layout = rend->shading->pipeline_layout,
  };

  vkCreateComputePipelines(rend->device, VK_NULL_HANDLE, 1, &ray_pipeline, NULL,
                           &rend->shading->pipeline);

  vkDestroyShaderModule(rend->device, comp_shader, NULL);

  return true;
}

void VK_DrawShading(vk_rend_t *rend, game_state_t *game) {
  VkCommandBuffer cmd = rend->graphics_command_buffer[rend->current_frame % 3];
  vk_shading_t *shading = rend->shading;

  if (rend->current_frame == 0) {
    VK_TransitionColorTexture(
        cmd, rend->shading->shading_image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT);
  }
  VK_TransitionColorTexture(cmd, rend->gbuffer->position_target.image,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT);
  VK_TransitionColorTexture(cmd, rend->gbuffer->albedo_target.image,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT);
  VK_TransitionColorTexture(cmd, rend->gbuffer->normal_target.image,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT);
  VK_TransitionDepthTexture(cmd, rend->gbuffer->depth_target.image,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shading->pipeline);

  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shading->pipeline_layout, 0, 1,
      &rend->global_ubo_desc_set[rend->current_frame % 3], 0, NULL);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          shading->pipeline_layout, 1, 1, &shading->hold_set, 0,
                          NULL);

  vkCmdDispatch(cmd, (rend->width + 16) / 16, (rend->height + 16) / 16, 1);
}

void VK_DestroyShading(vk_rend_t *rend) {
  vk_shading_t *shading = rend->shading;

  vkDestroyImageView(rend->device, shading->shading_view, NULL);

  vmaDestroyImage(rend->allocator, shading->shading_image,
                  shading->shading_image_alloc);
  vkDestroyPipeline(rend->device, shading->pipeline, NULL);
  vkDestroyDescriptorSetLayout(rend->device, shading->hold_layout, NULL);
  vkDestroyPipelineLayout(rend->device, shading->pipeline_layout, NULL);
}
