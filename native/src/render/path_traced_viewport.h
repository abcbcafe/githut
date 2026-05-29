#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/binder_common.hpp>

#include <memory>

namespace pathtracer {
class VulkanContext;
class PresentationBridge;
} // namespace pathtracer

namespace godot {

// The single node the game adds to a scene to enable path-traced rendering.
//
// On entering the tree it brings up a private, ray-tracing-enabled Vulkan device
// (VulkanContext) and the external-memory presentation bridge (PresentationBridge)
// that shares the rendered image back to Godot for compositing.
//
// At M0 this owns the lifecycle and exposes inspection; the renderer itself is
// filled in across M1+ (see docs/ROADMAP.md).
class PathTracedViewport : public Node3D {
    GDCLASS(PathTracedViewport, Node3D)

public:
    PathTracedViewport();
    ~PathTracedViewport() override;

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;

    // Render target resolution (defaults to the window size when zero).
    void set_render_size(const Vector2i &p_size);
    Vector2i get_render_size() const;

    // True once a Vulkan device exposing the RT extensions has been created.
    bool is_ray_tracing_available() const;

    // The shared output texture Godot composites; null until the bridge is up.
    Ref<Texture2D> get_output_texture() const;

protected:
    static void _bind_methods();

private:
    Vector2i render_size_ = Vector2i(0, 0);

    std::unique_ptr<pathtracer::VulkanContext> context_;
    std::unique_ptr<pathtracer::PresentationBridge> bridge_;
};

} // namespace godot
