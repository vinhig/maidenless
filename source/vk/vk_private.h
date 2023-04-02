#pragma once

#include "vk_mem_alloc.h"
#include <stdbool.h>
#include <vulkan/vulkan_core.h>

typedef struct client_t client_t;

void *CL_GetWindow(client_t *client);
// GBuffer stuff
bool VK_InitGBuffer(vk_rend_t *rend);
void VK_DrawGBuffer(vk_rend_t *rend);
void VK_DestroyGBuffer(vk_rend_t *rend);

// VK utils
VkShaderModule VK_LoadShaderModule(vk_rend_t *rend, const char *path);

typedef struct vk_gbuffer_t {
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
} vk_gbuffer_t;

typedef struct vk_map_t {
  VkBuffer *vertex_buffers;
  VmaAllocation *vertex_allocs;
  VkBuffer *index_buffers;
  VmaAllocation *index_allocs;
  unsigned *index_counts;

  unsigned primitive_count;
} vk_map_t;

struct vk_rend_t {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;

  VkFormat swapchain_format;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  unsigned swapchain_image_count;
  VkSemaphore swapchain_present_semaphore[3];
  VkSemaphore swapchain_render_semaphore[3];
  VkFence rend_fence[3];

  unsigned queue_family_index;

  // Yes only one, because i come from OpenGL...
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer[3];

  vk_gbuffer_t *gbuffer;

  VmaAllocator allocator;

  // TODO: shouldn't be there, but this is a speedrun
  vk_map_t map;

  unsigned current_frame;

  unsigned width;
  unsigned height;
};

static inline VkPipelineShaderStageCreateInfo
VK_PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                 VkShaderModule module) {
  return (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pName = "main",
      .stage = stage,
      .module = module,
  };
}

static inline VkPipelineVertexInputStateCreateInfo
VK_PipelineVertexInputStateCreateInfo() {
  return (VkPipelineVertexInputStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };
}

static inline VkPipelineInputAssemblyStateCreateInfo
VK_PipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology) {
  return (VkPipelineInputAssemblyStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = topology,
      .primitiveRestartEnable = false,
  };
}

static inline VkPipelineRasterizationStateCreateInfo
VK_PipelineRasterizationStateCreateInfo(VkPolygonMode polygon_mode) {
  return (VkPipelineRasterizationStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = false,
      .rasterizerDiscardEnable = false,
      .polygonMode = polygon_mode,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
  };
}

static inline VkPipelineMultisampleStateCreateInfo
VK_PipelineMultisampleStateCreateInfo() {
  return (VkPipelineMultisampleStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .minSampleShading = 1.0f,
      .pSampleMask = NULL,
  };
}

static inline VkPipelineColorBlendAttachmentState
VK_PipelineColorBlendAttachmentState() {
  return (VkPipelineColorBlendAttachmentState){
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };
}
