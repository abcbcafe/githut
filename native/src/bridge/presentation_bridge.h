#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <vulkan/vulkan.h>

namespace pathtracer {

class VulkanContext;

// Shares the path tracer's rendered image back to Godot for compositing.
//
// Strategy (the highest-risk M0 piece, see docs/ARCHITECTURE.md):
//   - Allocate the render-target VkImage on our RT device backed by external
//     memory (VK_KHR_external_memory_fd, opaque FD on Linux).
//   - Export the FD and import it on Godot's RenderingDevice via
//     texture_create_from_extension, wrapping it as a Texture2DRD drawn fullscreen.
//   - Synchronize with a shared timeline semaphore (VK_KHR_external_semaphore_fd):
//     our device signals frame-ready; Godot waits before sampling.
//
// Fallback if interop is troublesome: CPU readback + per-frame upload into a Godot
// ImageTexture (correct but slow; acceptable since perf is not an early priority).
class PresentationBridge {
public:
    explicit PresentationBridge(VulkanContext *p_context);
    ~PresentationBridge();

    PresentationBridge(const PresentationBridge &) = delete;
    PresentationBridge &operator=(const PresentationBridge &) = delete;

    // Allocates the shared render target and registers it with Godot. A zero size
    // falls back to the current window size.
    bool initialize(const godot::Vector2i &p_size);

    // The Godot-side texture to composite; null until initialize() succeeds.
    godot::Ref<godot::Texture2D> get_output_texture() const { return output_texture_; }

private:
    void shutdown();

    VulkanContext *context_ = nullptr;
    godot::Vector2i size_;

    VkImage render_target_ = VK_NULL_HANDLE;
    VkDeviceMemory render_target_memory_ = VK_NULL_HANDLE;
    VkSemaphore frame_ready_ = VK_NULL_HANDLE; // timeline semaphore

    godot::Ref<godot::Texture2D> output_texture_;
};

} // namespace pathtracer
