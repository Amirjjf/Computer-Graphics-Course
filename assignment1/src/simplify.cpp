#include "simplify.h"

#include <unordered_set>
#include <unordered_map>
#include <limits>
#include <cmath>
#include <queue>

using namespace simplify;

namespace {
    using Mat4 = Eigen::Matrix4f;

    inline float triangleArea(const Vec3& a, const Vec3& b, const Vec3& c){
        return 0.5f * ((b-a).cross(c-a)).norm();
    }

    Mat4 planeQuadric(const Vec3& a, const Vec3& b, const Vec3& c) {
        Vec3 n = (b - a).cross(c - a);
        float area = n.norm() * 0.5f;
        if (area <= 1e-12f) return Mat4::Zero();
        n.normalize();
        float d = -n.dot(a);
        Eigen::Vector4f p(n.x(), n.y(), n.z(), d);
        Mat4 K = p * p.transpose(); // outer product
        // weight by area to reduce tiny triangles' influence
        return K * area;
    }

    struct EdgeKeyHash {
        size_t operator()(const uint64_t& k) const noexcept { return std::hash<uint64_t>()(k); }
    };

    inline uint64_t edgeKey(uint32_t a, uint32_t b){
        if (a>b) std::swap(a,b);
        return (uint64_t(a) << 32) | uint64_t(b);
    }

    // Evaluate error for position x with quadric Q
    inline float vError(const Mat4& Q, const Vec3& x){
        Eigen::Vector4f h(x.x(), x.y(), x.z(), 1.0f);
        return h.dot(Q * h);
    }

    struct EdgeCandidate {
        uint32_t u, v;
        Vec3     opt;
        float    cost;
    };

    EdgeCandidate computeEdgeCandidate(uint32_t u, uint32_t v,
                                       const std::vector<Mat4>& Q,
                                       const std::vector<Vec3>& pos)
    {
        Mat4 Qsum = Q[u] + Q[v];
        Eigen::Matrix3f A = Qsum.block<3,3>(0,0);
        Eigen::Vector3f b = Qsum.block<3,1>(0,3);
        Vec3 best;
        bool solved = false;
        float det = A.determinant();
        if (std::abs(det) > 1e-8f) {
            best = A.ldlt().solve(-b);
            solved = true;
        }
        if (!solved) {
            // choose best among endpoints and midpoint
            Vec3 m = 0.5f * (pos[u] + pos[v]);
            float cu = vError(Qsum, pos[u]);
            float cv = vError(Qsum, pos[v]);
            float cm = vError(Qsum, m);
            if (cu <= cv && cu <= cm) best = pos[u];
            else if (cv <= cu && cv <= cm) best = pos[v];
            else best = m;
        }
        float cost = vError(Qsum, best);
        return {u,v,best,cost};
    }
}

IndexedMesh simplify::simplifyQEM(const IndexedMesh& inMesh, size_t targetTriangles)
{
    IndexedMesh mesh = inMesh; // work on a copy
    if (mesh.triangles.empty()) return mesh;
    if (targetTriangles == 0) targetTriangles = 1;

    const uint32_t n = static_cast<uint32_t>(mesh.positions.size());

    // Per-vertex quadrics
    std::vector<Mat4> Q(n, Mat4::Zero());
    for (auto const& t : mesh.triangles){
        const Vec3& a = mesh.positions[t[0]];
        const Vec3& b = mesh.positions[t[1]];
        const Vec3& c = mesh.positions[t[2]];
        Mat4 K = planeQuadric(a,b,c);
        Q[t[0]] += K; Q[t[1]] += K; Q[t[2]] += K;
    }

    // Union-find for vertex representatives
    std::vector<uint32_t> parent(n);
    for (uint32_t i=0;i<n;++i) parent[i]=i;
    auto find = [&](auto&& self, uint32_t a) -> uint32_t {
        if (parent[a]==a) return a;
        return parent[a] = self(self, parent[a]);
    };

    // Neighbor sets (by representative indices)
    std::vector<std::unordered_set<uint32_t>> nbr(n);
    for (auto const& t : mesh.triangles){
        uint32_t a=t[0], b=t[1], c=t[2];
        if (a!=b) { nbr[a].insert(b); nbr[b].insert(a); }
        if (b!=c) { nbr[b].insert(c); nbr[c].insert(b); }
        if (c!=a) { nbr[c].insert(a); nbr[a].insert(c); }
    }

    struct HeapEntry {
        float cost; uint32_t a,b; uint64_t gen; Vec3 opt;
        bool operator<(HeapEntry const& o) const { return cost > o.cost; } // min-heap
    };
    std::priority_queue<HeapEntry> heap;
    std::unordered_map<uint64_t, uint64_t, EdgeKeyHash> edgeGen; edgeGen.reserve(n*4);
    std::unordered_map<uint64_t, Vec3, EdgeKeyHash> edgeOpt; edgeOpt.reserve(n*4);
    uint64_t globalGen = 1;

    auto pushEdge = [&](uint32_t ua, uint32_t vb){
        if (ua==vb) return;
        uint32_t a = std::min(ua,vb), b = std::max(ua,vb);
        uint64_t key = edgeKey(a,b);
        auto cand = computeEdgeCandidate(a,b,Q,mesh.positions);
        uint64_t gen = ++globalGen;
        edgeGen[key] = gen; edgeOpt[key] = cand.opt;
        heap.push({cand.cost, a, b, gen, cand.opt});
    };

    // Initialize heap with all edges (a<b) from neighbor sets
    for (uint32_t a=0;a<n;++a){
        for (auto b : nbr[a]) if (a<b) pushEdge(a,b);
    }

    // Maintain an estimate of current triangle count and decrement using
    // the number of triangles incident to the collapsed edge (a,b), which is
    // exactly the count of common neighbors of a and b (1 for boundary, 2 interior).
    // Start from unique, non-degenerate triangle count.
    auto triKey = [](uint32_t i, uint32_t j, uint32_t k){
        uint32_t a=i,b=j,c=k; if (a>b) std::swap(a,b); if (b>c) std::swap(b,c); if (a>b) std::swap(a,b);
        return (uint64_t(a) << 42) | (uint64_t(b) << 21) | uint64_t(c);
    };
    size_t currentTris = 0;
    {
        std::unordered_set<uint64_t> triSet; triSet.reserve(mesh.triangles.size()*2);
        for (auto const& t : mesh.triangles){
            uint32_t a=t[0], b=t[1], c=t[2];
            if (a==b || b==c || c==a) continue;
            const Vec3& va = mesh.positions[a];
            const Vec3& vb = mesh.positions[b];
            const Vec3& vc = mesh.positions[c];
            if (triangleArea(va,vb,vc) <= 1e-12f) continue;
            uint64_t k = triKey(a,b,c);
            if (triSet.insert(k).second) ++currentTris;
        }
    }

    size_t collapses = 0;

    while (!heap.empty()){
        auto top = heap.top(); heap.pop();

        // Find current representatives
        uint32_t ra = find(find, top.a);
        uint32_t rb = find(find, top.b);
        if (ra==rb) continue;
        uint32_t a = std::min(ra,rb), b = std::max(ra,rb);
        uint64_t key = edgeKey(a,b);
        auto itg = edgeGen.find(key);
        if (itg==edgeGen.end() || itg->second != top.gen) continue; // stale

        // Before the collapse, estimate how many triangles are removed by this
        // edge collapse: equals number of common neighbors of (a,b).
        auto& Sa = nbr[a];
        auto& Sb = nbr[b];
        // Intersect by normalizing the larger set once for accuracy.
        size_t removedAlongEdge = 0;
        if (!Sa.empty() && !Sb.empty()){
            const auto& small = (Sa.size() <= Sb.size()) ? Sa : Sb;
            const auto& large = (Sa.size() <= Sb.size()) ? Sb : Sa;
            std::unordered_set<uint32_t> largeNorm; largeNorm.reserve(large.size()*2);
            for (auto w : large){ uint32_t rw = find(find, w); if (rw!=a && rw!=b) largeNorm.insert(rw); }
            for (auto w : small){ uint32_t rw = find(find, w); if (rw==a || rw==b) continue; if (largeNorm.count(rw)) ++removedAlongEdge; }
        }

        // Collapse b into a
        Vec3 opt = edgeOpt[key];
        mesh.positions[a] = opt;
        Q[a] = Q[a] + Q[b];
        parent[b] = a;

        // Move neighbors of b to a
        for (auto w : nbr[b]){
            uint32_t rw = find(find, w);
            if (rw==a) continue;
            nbr[rw].erase(b);
            nbr[rw].insert(a);
            nbr[a].insert(rw);
        }
        nbr[b].clear();
        nbr[a].erase(a);

        // Recompute and push edges adjacent to a
        for (auto w : nbr[a]){
            pushEdge(a, w);
        }

        ++collapses;
        if (removedAlongEdge > currentTris) removedAlongEdge = currentTris;
        currentTris -= removedAlongEdge;
        if (currentTris <= targetTriangles) break;
    }

    // Reconstruct final mesh: map each original vertex to its representative
    std::vector<uint32_t> rep(n);
    for (uint32_t i=0;i<n;++i) rep[i] = find(find, i);

    // Build used vertex map and remap to compact indices
    std::unordered_map<uint32_t, uint32_t> remap; remap.reserve(n);
    IndexedMesh out; out.positions.reserve(n); out.triangles.reserve(mesh.triangles.size());

    auto getRemap = [&](uint32_t r)->uint32_t{
        auto it = remap.find(r);
        if (it!=remap.end()) return it->second;
        uint32_t id = (uint32_t)out.positions.size();
        out.positions.push_back(mesh.positions[r]);
        remap.emplace(r, id);
        return id;
    };

    std::unordered_set<uint64_t> triSet; triSet.reserve(mesh.triangles.size()*2);

    for (auto const& t : mesh.triangles){
        uint32_t a = rep[t[0]], b = rep[t[1]], c = rep[t[2]];
        if (a==b || b==c || c==a) continue;
        const Vec3& va = mesh.positions[a];
        const Vec3& vb = mesh.positions[b];
        const Vec3& vc = mesh.positions[c];
        if (triangleArea(va,vb,vc) <= 1e-12f) continue;
        uint64_t k = triKey(a,b,c);
        if (!triSet.insert(k).second) continue; // duplicate
        uint32_t ia = getRemap(a), ib = getRemap(b), ic = getRemap(c);
        out.triangles.push_back({ia,ib,ic});
    }

    return out;
}
