#pragma once

// Caustics via light (photon) tracing. A camera-side path tracer cannot find the
// light -> glass -> diffuse-floor -> eye paths (the glass connection is specular and
// NEE can't sample it), so we trace photons forward from the lights, let them refract
// through glass, and deposit their flux into a planar caustic map over the floor. The
// camera pass then reads that map as extra incident irradiance on diffuse surfaces.
//
// GPU-free and testable: the photon pass conserves energy (deposited flux <= emitted
// power) and a glass sphere focuses flux into a bright caustic (peak >> mean).

#include <cstdint>
#include <vector>

#include "core/math.h"
#include "render/cpu_reference.h"
#include "voxel/material_palette.h"

namespace pathtracer::render {

// A flux accumulation grid over an axis-aligned rectangle on a horizontal plane.
class CausticMap {
public:
    CausticMap(float x0, float z0, float size_x, float size_z, int resolution, float floor_y);

    // Add incident flux (power) at a world position (uses x,z; y is ignored).
    void splat(const core::Vec3 &world_pos, const core::Vec3 &flux);

    // Merge another map of identical layout (used to combine per-thread maps).
    void merge(const CausticMap &other);

    // Irradiance (flux / cell area) at a world position; zero outside the grid.
    core::Vec3 irradiance(const core::Vec3 &world_pos) const;

    core::Vec3 total_flux() const;
    float peak_irradiance() const;            // max luminance irradiance over all cells
    double mean_nonzero_irradiance() const;   // average luminance over lit cells
    float floor_y() const { return floor_y_; }
    int resolution() const { return res_; }

private:
    int cell_index(float x, float z) const; // -1 if out of bounds

    float x0_, z0_, sx_, sz_, floor_y_;
    int res_;
    float cell_area_;
    std::vector<core::Vec3> cells_;
};

// Trace `num_photons` from the scene's emissive area lights, depositing the flux of
// paths that passed through at least one specular (glass) interaction onto up-facing
// diffuse surfaces in the map. Achromatic; reuses the scene's BVH. Parallelized.
void trace_caustics(const TriangleScene &scene, const voxel::MaterialPalette &palette,
                    CausticMap &map, int num_photons, uint64_t seed);

} // namespace pathtracer::render
