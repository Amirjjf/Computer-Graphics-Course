#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <Eigen/Dense>

namespace simplify {

using Vec3 = Eigen::Vector3f;

struct IndexedMesh {
    std::vector<Vec3> positions;
    std::vector<std::array<uint32_t,3>> triangles; // CCW assumed
};

// Simplify mesh to target number of triangles using Garland–Heckbert QEM.
// Returns a new mesh (positions array may retain unused vertices if present in input; triangles are valid indices).
IndexedMesh simplifyQEM(const IndexedMesh& mesh, size_t targetTriangles);

}
