#include "render/path_traced_viewport.h"

#include <godot_cpp/classes/engine.hpp>

#include "bridge/presentation_bridge.h"
#include "core/logger.h"
#include "render/vulkan_context.h"

using namespace godot;

PathTracedViewport::PathTracedViewport() = default;
PathTracedViewport::~PathTracedViewport() = default;

void PathTracedViewport::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_render_size", "size"), &PathTracedViewport::set_render_size);
    ClassDB::bind_method(D_METHOD("get_render_size"), &PathTracedViewport::get_render_size);
    ClassDB::add_property("PathTracedViewport",
            PropertyInfo(Variant::VECTOR2I, "render_size"),
            "set_render_size", "get_render_size");

    ClassDB::bind_method(D_METHOD("is_ray_tracing_available"),
            &PathTracedViewport::is_ray_tracing_available);
    ClassDB::bind_method(D_METHOD("get_output_texture"),
            &PathTracedViewport::get_output_texture);
}

void PathTracedViewport::_ready() {
    // Don't spin up Vulkan while the node merely lives in the editor's edited scene.
    if (Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    PT_LOG("PathTracedViewport: initializing ray-tracing device");

    context_ = std::make_unique<pathtracer::VulkanContext>();
    if (!context_->initialize()) {
        PT_ERR("PathTracedViewport: failed to create a ray-tracing-capable Vulkan device");
        context_.reset();
        return;
    }

    bridge_ = std::make_unique<pathtracer::PresentationBridge>(context_.get());
    if (!bridge_->initialize(get_render_size())) {
        PT_ERR("PathTracedViewport: failed to set up the presentation bridge");
        bridge_.reset();
    }
}

void PathTracedViewport::_process(double /*delta*/) {
    if (!context_ || !bridge_) {
        return;
    }
    // M1+: extract the scene, trace a frame, signal the timeline semaphore so
    // Godot can composite the shared image. Stubbed during M0.
}

void PathTracedViewport::_exit_tree() {
    bridge_.reset();
    context_.reset();
}

void PathTracedViewport::set_render_size(const Vector2i &p_size) {
    render_size_ = p_size;
}

Vector2i PathTracedViewport::get_render_size() const {
    return render_size_;
}

bool PathTracedViewport::is_ray_tracing_available() const {
    return context_ && context_->is_ray_tracing_available();
}

Ref<Texture2D> PathTracedViewport::get_output_texture() const {
    return bridge_ ? bridge_->get_output_texture() : Ref<Texture2D>();
}
