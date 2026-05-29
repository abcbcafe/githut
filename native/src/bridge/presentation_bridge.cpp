#include "bridge/presentation_bridge.h"

#include "core/logger.h"
#include "render/vulkan_context.h"

namespace pathtracer {

PresentationBridge::PresentationBridge(VulkanContext *p_context) : context_(p_context) {}

PresentationBridge::~PresentationBridge() {
    shutdown();
}

bool PresentationBridge::initialize(const godot::Vector2i &p_size) {
    size_ = p_size;

    // M0 implementation outline (to be filled in on the SDK workstation):
    //   1. Create the render-target VkImage with VkExternalMemoryImageCreateInfo
    //      (VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT) and back it with memory
    //      allocated using VkExportMemoryAllocateInfo.
    //   2. vkGetMemoryFdKHR to export the opaque FD.
    //   3. On the Godot side, RenderingDevice.texture_create_from_extension with the
    //      imported handle, then wrap as Texture2DRD -> output_texture_.
    //   4. Create an exportable timeline semaphore (frame_ready_) and share its FD
    //      so Godot can wait on frame completion.
    PT_WARN("PresentationBridge::initialize is not implemented yet (M0).");
    return false;
}

void PresentationBridge::shutdown() {
    if (context_ == nullptr) {
        return;
    }
    VkDevice device = context_->device();
    if (device != VK_NULL_HANDLE) {
        if (frame_ready_ != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame_ready_, nullptr);
            frame_ready_ = VK_NULL_HANDLE;
        }
        if (render_target_ != VK_NULL_HANDLE) {
            vkDestroyImage(device, render_target_, nullptr);
            render_target_ = VK_NULL_HANDLE;
        }
        if (render_target_memory_ != VK_NULL_HANDLE) {
            vkFreeMemory(device, render_target_memory_, nullptr);
            render_target_memory_ = VK_NULL_HANDLE;
        }
    }
    output_texture_.unref();
}

} // namespace pathtracer
