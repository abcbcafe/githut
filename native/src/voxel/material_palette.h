#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "voxel/voxel_chunk.h"

namespace pathtracer::voxel {

// PBR surface parameters, mirroring Godot's BaseMaterial3D metallic-roughness model
// so material import is close to 1:1. Texture references are deferred (added with
// the bindless texture array); coarse voxels start with flat parameters.
struct PbrMaterial {
    std::array<float, 3> albedo{0.8f, 0.8f, 0.8f};
    float metallic = 0.0f;
    float roughness = 0.8f;
    std::array<float, 3> emission{0.0f, 0.0f, 0.0f}; // emissive voxels act as area lights

    // Dielectric (glass) parameters. transmission > 0 marks a smooth dielectric:
    // the path tracer reflects/refracts by Fresnel using `ior`, and `attenuation`
    // is the per-channel Beer-Lambert absorption coefficient inside the glass
    // (per unit distance), giving tinted glass.
    float transmission = 0.0f;
    float ior = 1.5f;
    float dispersion = 0.0f; // Cauchy B coefficient (um^2); >0 splits colors (prism)
    std::array<float, 3> attenuation{0.0f, 0.0f, 0.0f};

    // Per-band acoustic coefficients (reused by the geometric acoustic tracer).
    // Three coarse bands: low / mid / high. Absorption in [0,1].
    std::array<float, 3> absorption{0.1f, 0.1f, 0.1f};
    std::array<float, 3> scattering{0.2f, 0.2f, 0.2f};

    bool is_emissive() const {
        return emission[0] > 0.0f || emission[1] > 0.0f || emission[2] > 0.0f;
    }
    bool is_glass() const { return transmission > 0.0f; }
};

// Maps voxel MaterialId -> PbrMaterial. Index 0 (air) is always present and unused
// for shading; ids are assigned sequentially as materials are registered.
class MaterialPalette {
public:
    MaterialPalette() {
        materials_.emplace_back(); // slot 0 = air placeholder
    }

    MaterialId add(const PbrMaterial &material) {
        materials_.push_back(material);
        return static_cast<MaterialId>(materials_.size() - 1);
    }

    bool contains(MaterialId id) const { return id < materials_.size(); }
    const PbrMaterial &get(MaterialId id) const { return materials_[id]; }

    std::size_t size() const { return materials_.size(); }

private:
    std::vector<PbrMaterial> materials_;
};

} // namespace pathtracer::voxel
