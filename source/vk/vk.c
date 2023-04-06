#include "vk.h"
#include "vk_private.h"

#include "cglm/cglm.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "game/g_game.h"
#include <SDL2/SDL_vulkan.h>

const char *vk_instance_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};
const unsigned vk_instance_layer_count = 1;

const char *vk_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_dynamic_rendering",
    "VK_EXT_shader_object", // No supported at the moment, saddy sad
};
const unsigned vk_device_extension_count = 2;

const char *vk_device_layers[] = {
    "VK_EXT_shader_object",
};
const unsigned vk_device_layer_count = 1;

char vk_error[1024];

#define VK_PUSH_ERROR(r)                                                       \
  {                                                                            \
    memcpy(vk_error, r, strlen(r));                                            \
    return NULL;                                                               \
  }

#define VK_CHECK_R(r)                                                          \
  if (r != VK_SUCCESS) {                                                       \
    memcpy(vk_error, #r, strlen(#r));                                          \
    return NULL;                                                               \
  }

bool VK_CheckDeviceFeatures(VkExtensionProperties *extensions,
                            unsigned extension_count) {
  unsigned req = vk_device_extension_count;

  for (unsigned j = 0; j < req; j++) {
    bool found = false;
    for (unsigned i = 0; i < extension_count; ++i) {
      if (strcmp(extensions[i].extensionName, vk_device_extensions[j]) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      printf("Device extension `%s` isn't supported by this physical device.\n",
             vk_device_extensions[j]);
      return false;
    }
  }

  return true;
}

VkShaderModule VK_LoadShaderModule(vk_rend_t *rend, const char *path) {
  FILE *f = fopen(path, "rb");

  if (!f) {
    printf("`%s` file doesn't seem to exist.\n", path);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  void *shader = malloc(size);

  fread(shader, size, 1, f);
  fclose(f);

  VkShaderModuleCreateInfo shader_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = shader,
  };

  VkShaderModule module;

  if (vkCreateShaderModule(rend->device, &shader_info, NULL, &module) !=
      VK_SUCCESS) {
    printf("`%s` doesn't seem to be a valid shader.\n", path);
    free(shader);
    return NULL;
  }

  free(shader);

  return module;
}

vk_rend_t *VK_CreateRend(client_t *client, unsigned width, unsigned height) {
  vk_rend_t *rend = calloc(1, sizeof(vk_rend_t));

  rend->width = width;
  rend->height = height;
  rend->current_frame = 0;

  // INSTANCE CREATION
  {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_MAKE_VERSION(1, 3, 0),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Maidenless",
        .pApplicationName = "Maidenless",
    };

    unsigned instance_extension_count = 0;
    char *instance_extensions[10];
    SDL_Vulkan_GetInstanceExtensions(CL_GetWindow(client),
                                     &instance_extension_count, NULL);
    SDL_Vulkan_GetInstanceExtensions(CL_GetWindow(client),
                                     &instance_extension_count,
                                     (const char **)instance_extensions);

    instance_extensions[instance_extension_count] =
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    instance_extension_count++;

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = vk_instance_layer_count,
        .ppEnabledLayerNames = &vk_instance_layers[0],
        .enabledExtensionCount = instance_extension_count,
        .ppEnabledExtensionNames = (const char *const *)&instance_extensions[0],
    };

    VK_CHECK_R(vkCreateInstance(&instance_info, NULL, &rend->instance));
  }

  // Surface creation
  {
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(CL_GetWindow(client), rend->instance,
                                  &surface)) {
      VK_PUSH_ERROR("Couldn't create `VkSurfaceKHR`.");
    }

    rend->surface = surface;
  }

  // Device creation
  {
    VkPhysicalDevice physical_devices[10];
    unsigned physical_device_count = 0;
    vkEnumeratePhysicalDevices(rend->instance, &physical_device_count, NULL);
    vkEnumeratePhysicalDevices(rend->instance, &physical_device_count,
                               physical_devices);

    if (physical_device_count == 0) {
      VK_PUSH_ERROR("No GPU supporting vulkan.");
    }

    VkPhysicalDevice physical_device;

    bool found_suitable = false;

    // Dummy choice, just get the first one
    for (unsigned i = 0; i < physical_device_count; i++) {
      VkPhysicalDeviceProperties property;
      VkPhysicalDeviceFeatures features;

      vkGetPhysicalDeviceProperties(physical_devices[i], &property);
      vkGetPhysicalDeviceFeatures(physical_devices[i], &features);

      unsigned extension_count;
      vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL,
                                           &extension_count, NULL);
      VkExtensionProperties *extensions =
          malloc(sizeof(VkExtensionProperties) * extension_count);
      vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL,
                                           &extension_count, extensions);

      unsigned layer_count;
      vkEnumerateDeviceLayerProperties(physical_devices[i], &layer_count, NULL);
      VkLayerProperties *layers =
          malloc(sizeof(VkLayerProperties) * layer_count);
      vkEnumerateDeviceLayerProperties(physical_devices[i], &layer_count,
                                       layers);

      VkSurfaceCapabilitiesKHR surface_cap;
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_devices[i],
                                                rend->surface, &surface_cap);

      unsigned format_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], rend->surface,
                                           &format_count, NULL);
      VkSurfaceFormatKHR *formats =
          malloc(sizeof(VkSurfaceFormatKHR) * format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], rend->surface,
                                           &format_count, formats);

      bool all_extensions_ok =
          VK_CheckDeviceFeatures(extensions, extension_count);
      if (!all_extensions_ok) {
        printf("`%s` doesn't support all needed extensions.\n",
               property.deviceName);
        continue;
      }

      if (format_count == 0) {
        // Exit, no suitable format for this combinaison of physical device and
        // surface
        free(extensions);
        free(formats);
        free(layers);
        printf("skipping because no format\n");
        continue;
      } else {
        VkFormat desired_format = VK_FORMAT_B8G8R8A8_UNORM;
        VkColorSpaceKHR desired_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        bool found = false;
        for (unsigned f = 0; f < format_count; f++) {
          if (formats[f].colorSpace == desired_color_space &&
              formats[f].format == desired_format) {
            found = true;
          }
        }

        if (!found) {
          free(extensions);
          free(formats);
          free(layers);
          printf("skipping because no desired format\n");
          continue;
        }
      }

      // TODO: Check if ray-tracing is there
      // For now, we only take the first integrated hehe
      if (property.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        physical_device = physical_devices[i];
        found_suitable = true;
        free(extensions);
        free(formats);
        free(layers);
        break;
      }

      printf("skipping because no VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU\n");
      free(extensions);
      free(formats);
      free(layers);
    }

    if (!found_suitable) {
      VK_PUSH_ERROR("No suitable physical device found.");
    }

    rend->physical_device = physical_device;
  }

  // Logical device
  {
    unsigned queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(rend->physical_device,
                                             &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families =
        malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        rend->physical_device, &queue_family_count, queue_families);

    unsigned queue_family_graphics_index = 0;
    // unsigned queue_family_transfer_index = 0;
    bool queue_family_graphics_found = false;
    // bool queue_family_transfer_found = false;
    for (unsigned i = 0; i < queue_family_count; i++) {
      if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
          queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
        queue_family_graphics_found = true;
        queue_family_graphics_index = i;
      }
      // else if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
      //            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      //   queue_family_transfer_found = true;
      //   queue_family_transfer_index = i;
      // }
    }

    if (!queue_family_graphics_found) {
      VK_PUSH_ERROR("Didn't not find a queue that fits the requirement. "
                    "(graphics & compute).");
    }
    // if (!queue_family_transfer_found) {
    //   VK_PUSH_ERROR("Didn't not find a queue that fits the requirement. "
    //                 "(transfer).");
    // }

    rend->queue_family_graphics_index = queue_family_graphics_index;
    // rend->queue_family_transfer_index = queue_family_transfer_index;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_graphics_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family_graphics_index,
        .pQueuePriorities = &queue_priority,
        .queueCount = 1,
    };
    // VkDeviceQueueCreateInfo queue_transfer_info = {
    //     .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    //     .queueFamilyIndex = queue_family_transfer_index,
    //     .pQueuePriorities = &queue_priority,
    //     .queueCount = 1,
    // };

    // VkDeviceQueueCreateInfo queue_infos[] = {queue_graphics_info,
    //                                          queue_transfer_info};

    VkPhysicalDeviceVulkan12Features vulkan_12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vulkan_13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .dynamicRendering = VK_TRUE,
        .pNext = &vulkan_12};

    //    VkPhysicalDeviceShaderObjectFeaturesEXT vulkan_shader_object = {
    //        .sType =
    //        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
    //        .shaderObject = VK_TRUE,
    //    };
    //
    //    vulkan_12.pNext = &vulkan_shader_object;

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queue_graphics_info,
        .queueCreateInfoCount = 1,
        .pNext = &vulkan_13,
        .enabledExtensionCount = vk_device_extension_count,
        .ppEnabledExtensionNames = vk_device_extensions,
    };

    VK_CHECK_R(vkCreateDevice(rend->physical_device, &device_info, NULL,
                              &rend->device));

    vkGetDeviceQueue(rend->device, queue_family_graphics_index, 0,
                     &rend->graphics_queue);
    // vkGetDeviceQueue(rend->device, queue_family_transfer_index, 0,
    //                  &rend->transfer_queue);

    free(queue_families);
  }

  // Create swapchain and corresponding images
  {
    VkExtent2D image_extent = {
        .width = width,
        .height = height,
    };

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = rend->surface,
        .minImageCount = 3,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = image_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE};

    VK_CHECK_R(vkCreateSwapchainKHR(rend->device, &swapchain_info, NULL,
                                    &rend->swapchain));

    rend->swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;

    unsigned image_count = 0;
    vkGetSwapchainImagesKHR(rend->device, rend->swapchain, &image_count, NULL);
    VkImage *images = malloc(sizeof(VkImage) * image_count);
    VkImageView *image_views = malloc(sizeof(VkImageView) * image_count);
    vkGetSwapchainImagesKHR(rend->device, rend->swapchain, &image_count,
                            images);

    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    for (unsigned i = 0; i < image_count; i++) {
      image_view_info.image = images[i];
      VK_CHECK_R(vkCreateImageView(rend->device, &image_view_info, NULL,
                                   &image_views[i]));
    }

    rend->swapchain_images = images;
    rend->swapchain_image_views = image_views;
    rend->swapchain_image_count = image_count;
  }

  // Create as many command buffers as needed
  // 3, one for each concurrent frame
  {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = rend->queue_family_graphics_index,
    };
    VK_CHECK_R(vkCreateCommandPool(rend->device, &pool_info, NULL,
                                   &rend->graphics_command_pool));
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = rend->graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 3,
    };

    vkAllocateCommandBuffers(rend->device, &allocate_info,
                             &rend->graphics_command_buffer[0]);
  }

  // Same operations, but only one command buffer for the transfer pool
  // {
  //   VkCommandPoolCreateInfo pool_info = {
  //       .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
  //       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  //       .queueFamilyIndex = rend->queue_family_transfer_index,
  //   };
  //   VK_CHECK_R(vkCreateCommandPool(rend->device, &pool_info, NULL,
  //                                  &rend->transfer_command_pool));
  VkCommandBufferAllocateInfo allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = rend->graphics_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  vkAllocateCommandBuffers(rend->device, &allocate_info,
                           &rend->transfer_command_buffer);
  // }

  // Create default samplers
  {
    VkSamplerCreateInfo nearest_sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    };

    vkCreateSampler(rend->device, &nearest_sampler_info, NULL,
                    &rend->nearest_sampler);

    nearest_sampler_info.magFilter = VK_FILTER_LINEAR;
    nearest_sampler_info.minFilter = VK_FILTER_LINEAR;

    vkCreateSampler(rend->device, &nearest_sampler_info, NULL,
                    &rend->linear_sampler);
  }

  // Create semaphores
  {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    for (int i = 0; i < 3; i++) {
      vkCreateSemaphore(rend->device, &semaphore_info, NULL,
                        &rend->swapchain_present_semaphore[i]);
      vkCreateSemaphore(rend->device, &semaphore_info, NULL,
                        &rend->swapchain_render_semaphore[i]);

      vkCreateFence(rend->device, &fence_info, NULL, &rend->rend_fence[i]);
    }
    // The transfer queue is by default free to be used
    // So create the fence with a SIGNALED state
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(rend->device, &fence_info, NULL, &rend->transfer_fence);
  }

  // Create descriptor pool
  {
    // Dummy allocating, i dont even know if it's important
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 50},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 50},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 50},
    };

    // We allocate 50 uniforms buffers
    // We allocate

    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 200,
        .poolSizeCount = 3,
        .pPoolSizes = &pool_sizes[0],
    };

    vkCreateDescriptorPool(rend->device, &desc_pool_info, NULL,
                           &rend->descriptor_pool);
  }

  VmaAllocatorCreateInfo allocator_info = {
      .physicalDevice = rend->physical_device,
      .device = rend->device,
      .instance = rend->instance,
  };

  vmaCreateAllocator(&allocator_info, &rend->allocator);

  // Create global descriptor set layout and descriptor set
  // Create global ubo too
  {
    VkDescriptorSetLayoutBinding global_ubo_binding = {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkDescriptorSetLayoutCreateInfo desc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &global_ubo_binding,
    };

    vkCreateDescriptorSetLayout(rend->device, &desc_info, NULL,
                                &rend->global_ubo_desc_set_layout);

    VkBufferCreateInfo global_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(vk_global_ubo_t),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo global_alloc_info = {
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    VkDescriptorSetAllocateInfo global_desc_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->global_ubo_desc_set_layout,
    };

    VkDescriptorBufferInfo global_desc_buffer_info = {
        .range = sizeof(vk_global_ubo_t),
    };

    VkWriteDescriptorSet global_desc_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &global_desc_buffer_info,
    };

    for (int i = 0; i < 3; i++) {
      vmaCreateBuffer(rend->allocator, &global_buffer_info, &global_alloc_info,
                      &rend->global_buffers[i], &rend->global_allocs[i], NULL);

      vkAllocateDescriptorSets(rend->device, &global_desc_set_info,
                               &rend->global_ubo_desc_set[i]);

      global_desc_buffer_info.buffer = rend->global_buffers[i];
      global_desc_write.dstSet = rend->global_ubo_desc_set[i];

      vkUpdateDescriptorSets(rend->device, 1, &global_desc_write, 0, NULL);
    }
  }

  // Create the descriptor set holding all freaking textures
  {
    unsigned max_bindless_resources = 16536;
    // Create bindless descriptor pool
    VkDescriptorPoolSize pool_sizes_bindless[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_bindless_resources},
    };

    // Update after bind is needed here, for each binding and in the descriptor
    // set layout creation.
    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = max_bindless_resources,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_sizes_bindless[0],
    };

    vkCreateDescriptorPool(rend->device, &desc_pool_info, NULL,
                           &rend->descriptor_bindless_pool);

    // YO!
    VkDescriptorBindingFlags bindless_flags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetLayoutBinding vk_binding = {
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = max_bindless_resources,
        .binding = 0,
        .stageFlags = VK_SHADER_STAGE_ALL,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &vk_binding,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &bindless_flags,
    };

    layout_info.pNext = &extended_info;

    vkCreateDescriptorSetLayout(rend->device, &layout_info, NULL,
                                &rend->global_textures_desc_set_layout);
    unsigned max_binding = max_bindless_resources - 1;
    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT count_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &max_binding,
    };

    VkDescriptorSetAllocateInfo global_textures_desc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_bindless_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->global_textures_desc_set_layout,
        .pNext = &count_info,
    };

    vkAllocateDescriptorSets(rend->device, &global_textures_desc_info,
                             &rend->global_textures_desc_set);
  }

  // Initialize other parts of the renderer
  if (!VK_InitGBuffer(rend)) {
    VK_PUSH_ERROR("Couldn't create a specific pipeline: GBuffer.");
  }

  return rend;
}

void VK_Present(vk_rend_t *rend, unsigned image_index) {
  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pSwapchains = &rend->swapchain,
      .swapchainCount = 1,
      .pWaitSemaphores =
          &rend->swapchain_render_semaphore[rend->current_frame % 3],
      .waitSemaphoreCount = 1,
      .pImageIndices = &image_index,
  };

  vkQueuePresentKHR(rend->graphics_queue, &present_info);
  rend->current_frame++;
}

void VK_Draw(vk_rend_t *rend, game_state_t *game) {
  vkWaitForFences(rend->device, 1, &rend->rend_fence[rend->current_frame % 3],
                  true, 1000000000);
  vkResetFences(rend->device, 1, &rend->rend_fence[rend->current_frame % 3]);

  VkCommandBuffer cmd = rend->graphics_command_buffer[rend->current_frame % 3];

  vkResetCommandBuffer(cmd, 0);

  unsigned image_index = 0;
  vkAcquireNextImageKHR(
      rend->device, rend->swapchain, UINT64_MAX,
      rend->swapchain_present_semaphore[rend->current_frame % 3], NULL,
      &image_index);

  vkResetCommandBuffer(cmd, 0);

  // Copy game state first person data
  memcpy(&rend->global_ubo, &game->fps, sizeof(game->fps));

  void *data;
  vmaMapMemory(rend->allocator, rend->global_allocs[rend->current_frame % 3],
               &data);

  memcpy(data, &rend->global_ubo, sizeof(vk_global_ubo_t));

  vmaUnmapMemory(rend->allocator, rend->global_allocs[rend->current_frame % 3]);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Before rendering, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ->
  // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
  VkImageMemoryBarrier image_memory_barrier_1 = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      // .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .image = rend->swapchain_images[image_index],
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }};

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       NULL, 0, NULL, 1, &image_memory_barrier_1);

  VkClearValue clear_color;
  clear_color.color.float32[0] = 100.0f / 255.0f;
  clear_color.color.float32[1] = 237.0f / 255.0f;
  clear_color.color.float32[2] = sin((float)rend->current_frame / 120.f);
  clear_color.color.float32[3] = 1.0;

  VkClearValue clear_depth;
  clear_depth.depthStencil.depth = 1.0;

  VkRenderingAttachmentInfoKHR color_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView = rend->swapchain_image_views[image_index],
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clear_color,
  };

  VkRenderingAttachmentInfoKHR depth_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView = rend->gbuffer->depth_map_view,
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clear_depth,
  };

  const VkRenderingInfo render_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
      .renderArea =
          {
              .extent = {.width = rend->width, .height = rend->height},
              .offset = {.x = 0, .y = 0},
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_info,
      .pDepthAttachment = &depth_attachment_info,
  };

  vkCmdBeginRendering(cmd, &render_info);

  VK_DrawGBuffer(rend, game);

  vkCmdEndRendering(cmd);

  // Before presenting, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL. ->
  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  VkImageMemoryBarrier image_memory_barrier_2 = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      // .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .image = rend->swapchain_images[image_index],
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }};

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                       NULL, 1, &image_memory_barrier_2);

  vkEndCommandBuffer(cmd);

  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pWaitDstStageMask = &wait_stage,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &rend->swapchain_present_semaphore[rend->current_frame % 3],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          &rend->swapchain_render_semaphore[rend->current_frame % 3],
      .commandBufferCount = 1,
      .pCommandBuffers =
          &rend->graphics_command_buffer[rend->current_frame % 3],
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info,
                rend->rend_fence[rend->current_frame % 3]);

  VK_Present(rend, image_index);
}

void VK_DestroyCurrentMap(vk_rend_t *rend) {
  for (unsigned p = 0; p < rend->map.primitive_count; p++) {
    if (rend->map.vertex_staging_allocs[p] != VK_NULL_HANDLE) {
      vmaDestroyBuffer(rend->allocator, rend->map.vertex_staging_buffers[p],
                       rend->map.vertex_staging_allocs[p]);
    }
    if (rend->map.index_staging_allocs[p] != VK_NULL_HANDLE) {
      vmaDestroyBuffer(rend->allocator, rend->map.index_staging_buffers[p],
                       rend->map.index_staging_allocs[p]);
    }

    vmaDestroyBuffer(rend->allocator, rend->map.vertex_buffers[p],
                     rend->map.vertex_allocs[p]);
    vmaDestroyBuffer(rend->allocator, rend->map.index_buffers[p],
                     rend->map.index_allocs[p]);
  }

  for (unsigned p = 0; p < rend->map.texture_count; p++) {
    if (rend->map.textures_staging[p] != VK_NULL_HANDLE) {
      vmaDestroyBuffer(rend->allocator, rend->map.textures_staging[p],
                       rend->map.textures_staging_allocs[p]);
    }

    vkDestroyImageView(rend->device, rend->map.texture_views[p], NULL);

    vmaDestroyImage(rend->allocator, rend->map.textures[p],
                    rend->map.textures_allocs[p]);
  }

  free(rend->map.vertex_staging_buffers);
  free(rend->map.vertex_staging_allocs);
  free(rend->map.index_staging_buffers);
  free(rend->map.index_staging_allocs);

  free(rend->map.vertex_buffers);
  free(rend->map.vertex_allocs);
  free(rend->map.index_buffers);
  free(rend->map.index_allocs);

  free(rend->map.index_counts);
}

void VK_DestroyRend(vk_rend_t *rend) {
  vkDeviceWaitIdle(rend->device);

  VK_DestroyCurrentMap(rend);
  VK_DestroyGBuffer(rend);

  for (int i = 0; i < 3; i++) {
    vmaDestroyBuffer(rend->allocator, rend->global_buffers[i],
                     rend->global_allocs[i]);
  }

  vkDestroySampler(rend->device, rend->nearest_sampler, NULL);
  vkDestroySampler(rend->device, rend->linear_sampler, NULL);

  vkDestroyDescriptorSetLayout(rend->device, rend->global_ubo_desc_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(rend->device,
                               rend->global_textures_desc_set_layout, NULL);

  vkDestroyDescriptorPool(rend->device, rend->descriptor_pool, NULL);
  vkDestroyDescriptorPool(rend->device, rend->descriptor_bindless_pool, NULL);

  vmaDestroyAllocator(rend->allocator);

  vkDestroyFence(rend->device, rend->transfer_fence, NULL);

  for (int i = 0; i < 3; i++) {
    vkDestroyFence(rend->device, rend->rend_fence[i], NULL);

    vkDestroySemaphore(rend->device, rend->swapchain_present_semaphore[i],
                       NULL);
    vkDestroySemaphore(rend->device, rend->swapchain_render_semaphore[i], NULL);
  }
  vkDestroyCommandPool(rend->device, rend->graphics_command_pool, NULL);
  // vkDestroyCommandPool(rend->device, rend->transfer_command_pool, NULL);
  for (unsigned i = 0; i < rend->swapchain_image_count; i++) {
    vkDestroyImageView(rend->device, rend->swapchain_image_views[i], NULL);
  }
  vkDestroySwapchainKHR(rend->device, rend->swapchain, NULL);
  vkDestroyDevice(rend->device, NULL);
  vkDestroySurfaceKHR(rend->instance, rend->surface, NULL);

  vkDestroyInstance(rend->instance, NULL);

  free(rend->swapchain_images);
  free(rend->swapchain_image_views);

  free(rend->gbuffer);
  free(rend);
}

const char *VK_GetError() { return (const char *)vk_error; }

void VK_CreateTexturesDescriptor(vk_rend_t *rend) {
  // Should be "UpdateTexturesDescriptor", but how god vulkan is complicated
  // Add dynamically too
  VkWriteDescriptorSet *writes =
      calloc(1, sizeof(VkWriteDescriptorSet) * rend->map.texture_count);
  VkDescriptorImageInfo *image_infos =
      calloc(1, sizeof(VkDescriptorImageInfo) * rend->map.texture_count);

  for (unsigned t = 0; t < rend->map.texture_count; t++) {
    VkImageView texture = rend->map.texture_views[t];

    image_infos[t].sampler = rend->linear_sampler;
    image_infos[t].imageView = texture;
    image_infos[t].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[t].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[t].descriptorCount = 1;
    writes[t].dstArrayElement = t; // should be something else
    writes[t].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[t].dstSet = rend->global_textures_desc_set;
    writes[t].dstBinding = 0;
    writes[t].pImageInfo = &image_infos[t];
  }

  vkUpdateDescriptorSets(rend->device, rend->map.texture_count, writes, 0,
                         NULL);
}

void VK_PushMap(vk_rend_t *rend, primitive_t *primitives,
                size_t primitive_count, texture_t *textures,
                size_t texture_count) {
  vkWaitForFences(rend->device, 1, &rend->transfer_fence, true, UINT64_MAX);

  vkResetFences(rend->device, 1, &rend->transfer_fence);

  VkCommandBuffer cmd = rend->transfer_command_buffer;
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Work with all vertex and index buffers here
  {
    VkBuffer *vertex_staging_buffers =
        malloc(sizeof(VkBuffer) * primitive_count);
    VmaAllocation *vertex_staging_allocs =
        malloc(sizeof(VmaAllocation) * primitive_count);
    VkBuffer *index_staging_buffers =
        malloc(sizeof(VkBuffer) * primitive_count);
    VmaAllocation *index_staging_allocs =
        malloc(sizeof(VmaAllocation) * primitive_count);

    VkBuffer *vertex_buffers = malloc(sizeof(VkBuffer) * primitive_count);
    VmaAllocation *vertex_allocs =
        malloc(sizeof(VmaAllocation) * primitive_count);
    VkBuffer *index_buffers = malloc(sizeof(VkBuffer) * primitive_count);
    VmaAllocation *index_allocs =
        malloc(sizeof(VmaAllocation) * primitive_count);

    unsigned *index_counts = malloc(sizeof(unsigned) * primitive_count);

    for (size_t p = 0; p < primitive_count; p++) {
      VkBuffer vertex_staging_buffer;
      VmaAllocation vertex_staging_alloc;
      VkBuffer index_staging_buffer;
      VmaAllocation index_staging_alloc;

      VkBuffer vertex_buffer;
      VmaAllocation vertex_alloc;
      VkBuffer index_buffer;
      VmaAllocation index_alloc;

      primitive_t *primitive = &primitives[p];

      // Push Vertex buffer
      {
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = primitive->vertex_count * sizeof(vertex_t),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };

        VmaAllocationCreateInfo alloc_info = {
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        };

        vmaCreateBuffer(rend->allocator, &buffer_info, &alloc_info,
                        &vertex_staging_buffer, &vertex_staging_alloc, NULL);

        void *mapped_data;
        vmaMapMemory(rend->allocator, vertex_staging_alloc, &mapped_data);

        memcpy(mapped_data, primitive->vertices,
               primitive->vertex_count * sizeof(vertex_t));

        vmaUnmapMemory(rend->allocator, vertex_staging_alloc);
      }

      // Create vertex GPU buffer and initiate copy
      {
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = primitive->vertex_count * sizeof(vertex_t),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        VmaAllocationCreateInfo alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        vmaCreateBuffer(rend->allocator, &buffer_info, &alloc_info,
                        &vertex_buffer, &vertex_alloc, NULL);
        VkBufferCopy region = {
            .dstOffset = 0,
            .srcOffset = 0,
            .size = primitive->vertex_count * sizeof(vertex_t),
        };
        vkCmdCopyBuffer(cmd, vertex_staging_buffer, vertex_buffer, 1, &region);
      }

      // Push index buffer
      {
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = primitive->index_count * sizeof(unsigned),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        };
        VmaAllocationCreateInfo alloc_info = {
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
        };

        vmaCreateBuffer(rend->allocator, &buffer_info, &alloc_info,
                        &index_staging_buffer, &index_staging_alloc, NULL);

        void *mapped_data;
        vmaMapMemory(rend->allocator, index_staging_alloc, &mapped_data);

        memcpy(mapped_data, primitive->indices,
               primitive->index_count * sizeof(unsigned));

        vmaUnmapMemory(rend->allocator, index_staging_alloc);
      }

      // Create index GPU buffer and initiate copy
      {
        VkBufferCreateInfo buffer_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = primitive->index_count * sizeof(unsigned),
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        VmaAllocationCreateInfo alloc_info = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        vmaCreateBuffer(rend->allocator, &buffer_info, &alloc_info,
                        &index_buffer, &index_alloc, NULL);
        VkBufferCopy region = {
            .dstOffset = 0,
            .srcOffset = 0,
            .size = primitive->index_count * sizeof(unsigned),
        };
        vkCmdCopyBuffer(cmd, index_staging_buffer, index_buffer, 1, &region);
      }

      vertex_staging_buffers[p] = vertex_staging_buffer;
      vertex_staging_allocs[p] = vertex_staging_alloc;
      index_staging_buffers[p] = index_staging_buffer;
      index_staging_allocs[p] = index_staging_alloc;

      vertex_buffers[p] = vertex_buffer;
      vertex_allocs[p] = vertex_alloc;
      index_buffers[p] = index_buffer;
      index_allocs[p] = index_alloc;

      index_counts[p] = primitive->index_count;
    }

    rend->map.vertex_staging_buffers = vertex_staging_buffers;
    rend->map.vertex_staging_allocs = vertex_staging_allocs;
    rend->map.index_staging_buffers = index_staging_buffers;
    rend->map.index_staging_allocs = index_staging_allocs;

    rend->map.vertex_buffers = vertex_buffers;
    rend->map.vertex_allocs = vertex_allocs;
    rend->map.index_buffers = index_buffers;
    rend->map.index_allocs = index_allocs;

    rend->map.index_counts = index_counts;
    rend->map.primitive_count = primitive_count;
  }

  // Work with all textures and the related global descriptor
  {
    VkImage *vk_textures = malloc(sizeof(VkImage) * texture_count);
    VmaAllocation *texture_allocs =
        malloc(sizeof(VmaAllocation) * texture_count);

    VkImageView *texture_views = malloc(sizeof(VkImageView) * texture_count);

    VkBuffer *stagings = malloc(sizeof(VkBuffer) * texture_count);
    VmaAllocation *staging_allocs =
        malloc(sizeof(VmaAllocation) * texture_count);

    for (size_t t = 0; t < texture_count; t++) {
      texture_t *texture = &textures[t];
      VkExtent3D extent = {
          .width = texture->width,
          .height = texture->height,
          .depth = 1,
      };
      VkImageCreateInfo tex_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .imageType = VK_IMAGE_TYPE_2D,
          .format = VK_FORMAT_R8G8B8A8_SRGB,
          .extent = extent,
          .mipLevels = 1,
          .arrayLayers = 1,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .tiling = VK_IMAGE_TILING_OPTIMAL,
          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      };
      VmaAllocationCreateInfo tex_alloc_info = {.usage =
                                                    VMA_MEMORY_USAGE_GPU_ONLY};

      vmaCreateImage(rend->allocator, &tex_info, &tex_alloc_info,
                     &vk_textures[t], &texture_allocs[t], NULL);

      // Change layout of this image
      VkImageSubresourceRange range;
      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      range.baseMipLevel = 0;
      range.levelCount = 1;
      range.baseArrayLayer = 0;
      range.layerCount = 1;

      VkImageMemoryBarrier image_barrier_1 = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = vk_textures[t],
          .subresourceRange = range,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      };

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                           1, &image_barrier_1);

      // CREATE, MAP
      VkBufferCreateInfo staging_info = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          .size = texture->width * texture->height * 4,
      };

      VmaAllocationCreateInfo staging_alloc_info = {
          .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      };

      vmaCreateBuffer(rend->allocator, &staging_info, &staging_alloc_info,
                      &stagings[t], &staging_allocs[t], NULL);

      void *data;
      vmaMapMemory(rend->allocator, staging_allocs[t], &data);
      memcpy(data, texture->data, texture->width * texture->height * 4);
      vmaUnmapMemory(rend->allocator, staging_allocs[t]);

      // COPY
      VkBufferImageCopy copy_region = {
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .imageSubresource.mipLevel = 0,
          .imageSubresource.baseArrayLayer = 0,
          .imageSubresource.layerCount = 1,
          .imageExtent = extent,
      };

      vkCmdCopyBufferToImage(cmd, stagings[t], vk_textures[t],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &copy_region);

      image_barrier_1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      image_barrier_1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_barrier_1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      image_barrier_1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                           NULL, 1, &image_barrier_1);

      // Create image view and sampler
      // Almost there
      VkImageViewCreateInfo image_view_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_R8G8B8A8_SRGB,
          .components =
              {
                  .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .a = VK_COMPONENT_SWIZZLE_IDENTITY,
              },
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .image = vk_textures[t],
      };

      vkCreateImageView(rend->device, &image_view_info, NULL,
                        &texture_views[t]);
    }
    rend->map.textures = vk_textures;
    rend->map.textures_allocs = texture_allocs;
    rend->map.textures_staging = stagings;
    rend->map.textures_staging_allocs = staging_allocs;
    rend->map.texture_count = texture_count;
    rend->map.texture_views = texture_views;
  }

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &rend->transfer_command_buffer,
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info, rend->transfer_fence);

  VK_CreateTexturesDescriptor(rend);
}
