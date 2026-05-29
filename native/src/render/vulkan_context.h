#pragma once

#include <vulkan/vulkan.h>

namespace pathtracer {

// Owns a private, ray-tracing-enabled Vulkan device, separate from Godot's.
//
// We cannot reuse Godot's VkDevice: its enabled extensions are fixed at creation
// and do not include the ray-tracing extensions. So we create our own device on
// the SAME physical GPU Godot is using (matched by VkPhysicalDevice UUID, queried
// from Godot via RenderingDevice.get_driver_resource()), enabling:
//   - VK_KHR_acceleration_structure
//   - VK_KHR_ray_tracing_pipeline
//   - VK_KHR_ray_query
//   - VK_KHR_external_memory_fd / VK_KHR_external_semaphore_fd  (presentation bridge)
//
// M0 task: implement initialize() and verify the RT extensions are present.
class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext();

    VulkanContext(const VulkanContext &) = delete;
    VulkanContext &operator=(const VulkanContext &) = delete;

    // Creates the instance, picks the RTX physical device matching Godot's, and
    // creates a logical device with the RT + external-memory extensions enabled.
    // Returns false if no suitable device is found.
    bool initialize();

    bool is_ray_tracing_available() const { return ray_tracing_available_; }

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkQueue graphics_queue() const { return graphics_queue_; }
    uint32_t graphics_queue_family() const { return graphics_queue_family_; }

private:
    void shutdown();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    uint32_t graphics_queue_family_ = 0;

    bool ray_tracing_available_ = false;
};

} // namespace pathtracer
