#pragma once

#include "cglm/types.h"
#include "vk_mem_alloc.h"

#include <stdbool.h>
#include <vulkan/vulkan_core.h>

typedef struct client_t client_t;

void *CL_GetWindow(client_t *client);
// GBuffer stuff
bool VK_InitGBuffer(vk_rend_t *rend);
void VK_DrawGBuffer(vk_rend_t *rend, game_state_t *game);
void VK_DestroyGBuffer(vk_rend_t *rend);

bool VK_InitShading(vk_rend_t *rend);
void VK_DrawShading(vk_rend_t *rend, game_state_t *game);
void VK_DestroyShading(vk_rend_t *rend);

// VK utils
VkShaderModule VK_LoadShaderModule(vk_rend_t *rend, const char *path);

void VK_TransitionColorTexture(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout from_layout,
                               VkImageLayout to_layout,
                               VkPipelineStageFlags from_stage,
                               VkPipelineStageFlags to_stage,
                               VkAccessFlags access_mask);
void VK_TransitionDepthTexture(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout from_layout,
                               VkImageLayout to_layout,
                               VkPipelineStageFlags from_stage,
                               VkPipelineStageFlags to_stage,
                               VkAccessFlags access_mask);

extern VkResult (*vkSetDebugUtilsObjectName)(
    VkDevice device, const VkDebugUtilsObjectNameInfoEXT *pNameInfo);

typedef struct render_target_t {
  VkImage image;
  VkImageView image_view;
  VmaAllocation alloc;
  VkRenderingAttachmentInfo attachment_info;
} render_target_t;

typedef struct vk_shading_t {
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkDescriptorSetLayout hold_layout;
  VkDescriptorSet hold_set;

  VkImage shading_image;
  VkImageView shading_view;
  VmaAllocation shading_image_alloc;
} vk_shading_t;

typedef struct vk_gbuffer_t {
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  render_target_t depth_target;
  render_target_t position_target;
  render_target_t normal_target;
  render_target_t albedo_target;
} vk_gbuffer_t;

typedef struct vk_model_t {
  // Only GPU Visible
  VkBuffer *vertex_buffers;
  VmaAllocation *vertex_allocs;
  VkBuffer *index_buffers;
  VmaAllocation *index_allocs;
  // Staging buffers
  VkBuffer *vertex_staging_buffers;
  VmaAllocation *vertex_staging_allocs;
  VkBuffer *index_staging_buffers;
  VmaAllocation *index_staging_allocs;
  // Only GPU Visible
  VkImage *textures;
  VkImageView *texture_views;
  VmaAllocation *textures_allocs;
  // Staging textures
  VkBuffer *textures_staging;
  VmaAllocation *textures_staging_allocs;

  // How much should be drawn
  unsigned *index_counts;

  unsigned primitive_count;
  unsigned texture_count;
} vk_model_t;

typedef struct vk_global_ubo_t {
  mat4 view;
  mat4 proj;
  mat4 view_proj;
  vec4 view_dir;
  vec2 view_dim;
} vk_global_ubo_t;

struct vk_rend_t {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  // VkQueue transfer_queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;

  unsigned queue_family_graphics_index;
  unsigned queue_family_transfer_index;

  VkFormat swapchain_format;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  unsigned swapchain_image_count;
  VkSemaphore swapchain_present_semaphore[3];
  VkSemaphore swapchain_render_semaphore[3];

  VkFence rend_fence[3];
  VkFence transfer_fence;

  // Yes only one, because i come from OpenGL...
  VkCommandPool graphics_command_pool;
  VkCommandBuffer graphics_command_buffer[3];
  // VkCommandPool transfer_command_pool;
  VkCommandBuffer transfer_command_buffer;
  VkDescriptorPool descriptor_pool;
  VkDescriptorPool descriptor_bindless_pool;

  VkDescriptorSetLayout global_ubo_desc_set_layout;
  VkDescriptorSet global_ubo_desc_set[3];

  VkDescriptorSetLayout global_textures_desc_set_layout;
  VkDescriptorSet global_textures_desc_set;

  VkBuffer global_buffers[3];
  VmaAllocation global_allocs[3];

  VkSampler nearest_sampler;
  VkSampler linear_sampler;

  vk_gbuffer_t *gbuffer;
  vk_shading_t *shading;

  VmaAllocator allocator;

  // TODO: shouldn't be there, but this is a speedrun
  vk_model_t map;
  vk_model_t models[256];
  unsigned model_count;

  vk_global_ubo_t global_ubo;

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
