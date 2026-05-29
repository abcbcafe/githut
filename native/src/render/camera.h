#pragma once

#include "core/math.h"

namespace pathtracer::render {

// Pinhole camera that generates primary rays, mirroring how the raygen shader will
// set up rays on the GPU. Kept CPU-testable and Vulkan-free; the GPU path uses the
// same math. Screen coordinates are in [0,1] with (0.5, 0.5) at the image center.
class PinholeCamera {
public:
    PinholeCamera(const core::Vec3 &position, const core::Vec3 &forward, const core::Vec3 &up,
                  float fov_y_radians, float aspect)
        : position_(position), aspect_(aspect) {
        forward_ = core::normalize(forward);
        right_ = core::normalize(core::cross(forward_, up));
        up_ = core::cross(right_, forward_); // re-orthogonalized
        half_height_ = std::tan(fov_y_radians * 0.5f);
        half_width_ = aspect * half_height_;
    }

    core::Ray generate_ray(float screen_x, float screen_y) const {
        const float ndc_x = (screen_x * 2.0f - 1.0f) * half_width_;
        const float ndc_y = (screen_y * 2.0f - 1.0f) * half_height_;
        const core::Vec3 dir = core::normalize(forward_ + right_ * ndc_x + up_ * ndc_y);
        return core::Ray{position_, dir};
    }

    const core::Vec3 &position() const { return position_; }
    const core::Vec3 &forward() const { return forward_; }
    const core::Vec3 &right() const { return right_; }
    const core::Vec3 &up() const { return up_; }

private:
    core::Vec3 position_;
    core::Vec3 forward_;
    core::Vec3 right_;
    core::Vec3 up_;
    float aspect_;
    float half_width_;
    float half_height_;
};

} // namespace pathtracer::render
