#include "render/bvh.h"

#include <algorithm>
#include <limits>

namespace pathtracer::render {

using core::Aabb;
using core::Ray;
using core::Triangle;
using core::Vec3;

namespace {
constexpr int kLeafSize = 4;
constexpr float kInf = std::numeric_limits<float>::max();

Aabb empty_box() { return Aabb{{kInf, kInf, kInf}, {-kInf, -kInf, -kInf}}; }

float axis(const Vec3 &v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }
} // namespace

void Bvh::build(const std::vector<Triangle> &triangles) {
    triangles_ = triangles;
    ordered_.clear();
    nodes_.clear();
    if (triangles_.empty()) {
        return;
    }

    std::vector<Aabb> boxes(triangles_.size());
    std::vector<Vec3> centroids(triangles_.size());
    ordered_.resize(triangles_.size());
    for (std::size_t i = 0; i < triangles_.size(); ++i) {
        Aabb box = empty_box();
        box.expand(triangles_[i].a);
        box.expand(triangles_[i].b);
        box.expand(triangles_[i].c);
        boxes[i] = box;
        centroids[i] = (triangles_[i].a + triangles_[i].b + triangles_[i].c) * (1.0f / 3.0f);
        ordered_[i] = static_cast<int>(i);
    }

    nodes_.reserve(triangles_.size() * 2);
    build_recursive(0, static_cast<int>(triangles_.size()), boxes, centroids);
}

int Bvh::build_recursive(int start, int end, const std::vector<Aabb> &boxes,
                         const std::vector<Vec3> &centroids) {
    const int node_index = static_cast<int>(nodes_.size());
    nodes_.push_back(Node{});

    Aabb bounds = empty_box();
    Aabb centroid_bounds = empty_box();
    for (int i = start; i < end; ++i) {
        const int tri = ordered_[i];
        bounds.expand(boxes[tri].min);
        bounds.expand(boxes[tri].max);
        centroid_bounds.expand(centroids[tri]);
    }

    const int count = end - start;
    const Vec3 extent = centroid_bounds.max - centroid_bounds.min;
    const bool degenerate = extent.x <= 0.0f && extent.y <= 0.0f && extent.z <= 0.0f;

    if (count <= kLeafSize || degenerate) {
        nodes_[node_index].box = bounds;
        nodes_[node_index].left_or_first = start;
        nodes_[node_index].count = count;
        return node_index;
    }

    int split_axis = 0;
    if (extent.y > extent.x) split_axis = 1;
    if (axis(extent, 2) > axis(extent, split_axis)) split_axis = 2;

    const int mid = start + count / 2;
    std::nth_element(ordered_.begin() + start, ordered_.begin() + mid, ordered_.begin() + end,
                     [&](int lhs, int rhs) {
                         return axis(centroids[lhs], split_axis) < axis(centroids[rhs], split_axis);
                     });

    const int left = build_recursive(start, mid, boxes, centroids);
    const int right = build_recursive(mid, end, boxes, centroids);

    nodes_[node_index].box = bounds;
    nodes_[node_index].left_or_first = left;
    nodes_[node_index].right_child = right;
    nodes_[node_index].count = 0;
    return node_index;
}

Bvh::Hit Bvh::closest(const Ray &ray) const {
    Hit best;
    if (nodes_.empty()) {
        return best;
    }
    float closest_t = kInf;

    int stack[64];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const Node &node = nodes_[stack[--sp]];
        if (!core::intersect(ray, node.box, closest_t)) {
            continue;
        }
        if (node.count > 0) {
            for (int i = 0; i < node.count; ++i) {
                const int tri = ordered_[node.left_or_first + i];
                const auto t = core::intersect(ray, triangles_[tri]);
                if (t && *t < closest_t) {
                    closest_t = *t;
                    best.hit = true;
                    best.t = *t;
                    best.index = tri;
                }
            }
        } else {
            stack[sp++] = node.left_or_first;
            stack[sp++] = node.right_child;
        }
    }
    return best;
}

bool Bvh::any(const Ray &ray, float max_t) const {
    if (nodes_.empty()) {
        return false;
    }
    int stack[64];
    int sp = 0;
    stack[sp++] = 0;
    while (sp > 0) {
        const Node &node = nodes_[stack[--sp]];
        if (!core::intersect(ray, node.box, max_t)) {
            continue;
        }
        if (node.count > 0) {
            for (int i = 0; i < node.count; ++i) {
                const int tri = ordered_[node.left_or_first + i];
                const auto t = core::intersect(ray, triangles_[tri]);
                if (t && *t < max_t) {
                    return true;
                }
            }
        } else {
            stack[sp++] = node.left_or_first;
            stack[sp++] = node.right_child;
        }
    }
    return false;
}

} // namespace pathtracer::render
