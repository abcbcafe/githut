#include "render/vulkan_context.h"

#include "core/logger.h"

namespace pathtracer {

VulkanContext::~VulkanContext() {
    shutdown();
}

bool VulkanContext::initialize() {
    // M0 implementation outline (to be filled in on the SDK workstation):
    //   1. vkCreateInstance with the required instance extensions + (debug) the
    //      VK_LAYER_KHRONOS_validation layer.
    //   2. Enumerate physical devices; pick the one whose UUID matches the device
    //      Godot reports via RenderingServer/RenderingDevice driver resources, and
    //      that advertises VK_KHR_ray_tracing_pipeline + VK_KHR_acceleration_structure.
    //   3. vkCreateDevice enabling the RT, ray-query, and external-memory/-semaphore
    //      FD extensions, plus the required feature chain
    //      (VkPhysicalDeviceAccelerationStructureFeaturesKHR,
    //       VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
    //       VkPhysicalDeviceBufferDeviceAddressFeatures).
    //   4. Retrieve the graphics/compute queue and set ray_tracing_available_.
    PT_WARN("VulkanContext::initialize is not implemented yet (M0).");
    ray_tracing_available_ = false;
    return false;
}

void VulkanContext::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    physical_device_ = VK_NULL_HANDLE;
    graphics_queue_ = VK_NULL_HANDLE;
    ray_tracing_available_ = false;
}

} // namespace pathtracer
