#include "vk.h"

#include <vulkan/vulkan_core.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL_vulkan.h>

void *CL_GetWindow(client_t *client);

const char *vk_instance_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};

const char *vk_device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                      "VK_KHR_dynamic_rendering"};

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

struct vk_rend_t {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;

  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  unsigned swapchain_image_count;
  VkSemaphore swapchain_present_semaphore[3];
  VkSemaphore swapchain_render_semaphore[3];
  VkFence rend_fence[3];

  unsigned queue_family_index;

  // Yes only one, but i come from OpenGL...
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer[3];

  unsigned current_frame;

  unsigned width;
  unsigned height;
};

vk_rend_t *VK_CreateRend(client_t *client, unsigned width, unsigned height) {
  vk_rend_t *rend = malloc(sizeof(vk_rend_t));

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
        .enabledLayerCount =
            0, // sizeof(vk_instance_layers) / sizeof(vk_instance_layers[0]),
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

      if (format_count == 0) {
        // Exit, no suitable format for this combinaison of physical device and
        // surface
        free(extensions);
        free(formats);
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
        break;
      }

      printf("skipping because no VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU\n");
      free(extensions);
      free(formats);
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

    unsigned queue_family_index = 0;
    bool queue_family_found = false;
    for (int i = 0; i < queue_family_count; i++) {
      if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
          queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
        queue_family_found = true;
        queue_family_index = i;
        break;
      }
    }

    if (!queue_family_found) {
      VK_PUSH_ERROR("Didn't not find a queue that fits the requirement. "
                    "(graphics & compute).");
    }

    rend->queue_family_index = queue_family_index;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family_index,
        .pQueuePriorities = &queue_priority,
        .queueCount = 1,
    };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
    };

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queue_info,
        .queueCreateInfoCount = 1,
        .pNext = &dynamic_rendering_feature,
        .enabledExtensionCount =
            sizeof(vk_device_extensions) / sizeof(vk_device_extensions[0]),
        .ppEnabledExtensionNames = vk_device_extensions,
    };

    VK_CHECK_R(vkCreateDevice(rend->physical_device, &device_info, NULL,
                              &rend->device));

    vkGetDeviceQueue(rend->device, queue_family_index, 0, &rend->queue);
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
        .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE};

    VK_CHECK_R(vkCreateSwapchainKHR(rend->device, &swapchain_info, NULL,
                                    &rend->swapchain));

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
  // Create as many command buffers as need
  // 3, one for each concurrent frame
  {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = rend->queue_family_index,
    };
    VK_CHECK_R(vkCreateCommandPool(rend->device, &pool_info, NULL,
                                   &rend->command_pool));
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = rend->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 3,
    };

    vkAllocateCommandBuffers(rend->device, &allocate_info,
                             &rend->command_buffer[0]);
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

  vkQueuePresentKHR(rend->queue, &present_info);
  rend->current_frame++;
}

void VK_Draw(vk_rend_t *rend) {
  vkWaitForFences(rend->device, 1, &rend->rend_fence[rend->current_frame % 3],
                  true, 1000000000);
  vkResetFences(rend->device, 1, &rend->rend_fence[rend->current_frame % 3]);

  VkCommandBuffer cmd = rend->command_buffer[rend->current_frame % 3];

  vkResetCommandBuffer(cmd, 0);

  unsigned image_index = 0;
  vkAcquireNextImageKHR(
      rend->device, rend->swapchain, UINT64_MAX,
      rend->swapchain_present_semaphore[rend->current_frame % 3], NULL,
      &image_index);

  vkResetCommandBuffer(cmd, 0);

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

  VkClearValue clear_color = {};
  clear_color.color.float32[0] = 100.0f / 255.0f;
  clear_color.color.float32[1] = 237.0f / 255.0f;
  clear_color.color.float32[2] = sin((float)rend->current_frame / 120.f);
  clear_color.color.float32[3] = 1.0;

  VkRenderingAttachmentInfoKHR color_attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
      .imageView = rend->swapchain_image_views[image_index],
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clear_color,
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
  };

  vkCmdBeginRendering(cmd, &render_info);

  // Draw calls here

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
      .pCommandBuffers = &rend->command_buffer[rend->current_frame % 3],
  };

  vkQueueSubmit(rend->queue, 1, &submit_info,
                rend->rend_fence[rend->current_frame % 3]);

  VK_Present(rend, image_index);
}

void VK_DestroyRend(vk_rend_t *rend) {
  vkDeviceWaitIdle(rend->device);

  for (int i = 0; i < 3; i++) {
    vkDestroyFence(rend->device, rend->rend_fence[i], NULL);

    vkDestroySemaphore(rend->device, rend->swapchain_present_semaphore[i],
                       NULL);
    vkDestroySemaphore(rend->device, rend->swapchain_render_semaphore[i], NULL);
  }
  vkDestroyCommandPool(rend->device, rend->command_pool, NULL);
  for (unsigned i = 0; i < rend->swapchain_image_count; i++) {
    vkDestroyImageView(rend->device, rend->swapchain_image_views[i], NULL);
  }
  vkDestroySwapchainKHR(rend->device, rend->swapchain, NULL);
  vkDestroyDevice(rend->device, NULL);
  vkDestroySurfaceKHR(rend->instance, rend->surface, NULL);

  vkDestroyInstance(rend->instance, NULL);
}

const char *VK_GetError() { return (const char *)vk_error; }
