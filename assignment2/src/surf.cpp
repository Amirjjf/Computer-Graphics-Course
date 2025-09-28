// Include libraries
#include "glad/gl_core_33.h"                // OpenGL
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>             // Window manager
#include <imgui.h>                  // GUI Library
#include <imgui_impl_glfw.h>
#include "imgui_impl_opengl3.h"

#include <Eigen/Dense>              // Linear algebra
#include <Eigen/Geometry>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cstdint>

using namespace std;        // enables writing "string" instead of std::string, etc.
using namespace Eigen;      // enables writing "Vector3f" instead of "Eigen::Vector3f", etc.

#include "surf.h"
//#include "extra.h"
#include "Utils.h"

using namespace std;

namespace
{
    struct Tet { int v[4]; };
    // Decompose a unit cube into 6 tetrahedra (vertex indices local to cube corner grid)
    // Cube vertex ordering: (x,y,z) in {0,1}^3 mapped to id = x + 2*y + 4*z
    static const Tet cubeTets[6] = {
        {{0, 1, 3, 7}},
        {{0, 3, 2, 7}},
        {{0, 2, 6, 7}},
        {{0, 6, 4, 7}},
        {{0, 4, 5, 7}},
        {{0, 5, 1, 7}},
    };

    inline Vector3f lerp(const Vector3f& a, const Vector3f& b, float t) { return a + t * (b - a); }
    inline float    lerp(float a, float b, float t) { return a + t * (b - a); }

    inline Vector3f safe_normalize(const Vector3f& v)
    {
        float n2 = v.squaredNorm();
        if (n2 > 1e-12f) return v / std::sqrt(n2);
        return Vector3f(0.0f, 0.0f, 1.0f);
    }

    struct VolumeView {
        const uint8_t*  u8  = nullptr;
        const uint16_t* u16 = nullptr;
        const float*    f32 = nullptr;
        Vector3i dims;
        std::string dtype;
        inline float at(int x,int y,int z) const {
            size_t idx = size_t(x) + size_t(dims.x())*(size_t(y) + size_t(dims.y())*size_t(z));
            if (dtype == "uint8") return float(u8[idx]) / 255.0f;
            else if (dtype == "uint16") return float(u16[idx]) / 65535.0f;
            else return f32[idx];
        }
    };

    inline Vector3f gridToWorld(const Vector3i& ijk, const Vector3f& spacing, const Vector3f& origin) {
        return origin + spacing.cwiseProduct(Vector3f(float(ijk.x()), float(ijk.y()), float(ijk.z())));
    }

    // Estimate gradient by central differences for normals
    Vector3f gradientAt(const VolumeView& V, int x, int y, int z) {
        auto clampi = [&](int v, int lo, int hi){ return std::max(lo, std::min(hi, v)); };
        int xm = clampi(x-1, 0, V.dims.x()-1), xp = clampi(x+1, 0, V.dims.x()-1);
        int ym = clampi(y-1, 0, V.dims.y()-1), yp = clampi(y+1, 0, V.dims.y()-1);
        int zm = clampi(z-1, 0, V.dims.z()-1), zp = clampi(z+1, 0, V.dims.z()-1);
        float gx = 0.5f * (V.at(xp,y,z) - V.at(xm,y,z));
        float gy = 0.5f * (V.at(x,yp,z) - V.at(x,ym,z));
        float gz = 0.5f * (V.at(x,y,zp) - V.at(x,y,zm));
        Vector3f g(gx,gy,gz);
        if (g.squaredNorm() > 1e-12f) g.normalize();
        return g;
    }
    // This is a generic function that generates a set of triangle
    // faces for a sweeping a profile curve along "something".  For
    // instance, say you want to sweep the profile curve [01234]:
    //
    //   4     9     10
    //    3     8     11
    //    2 --> 7 --> 12 ----------[in this direction]--------->
    //    1     6     13 
    //   0     5     14
    //
    // Then the "diameter" is 5, and the "length" is how many times
    // the profile is repeated in the sweep direction.  This function
    // generates faces in terms of vertex indices.  It is assumed that
    // the indices go as shown in the picture (the first dia vertices
    // correspond to the first repetition of the profile curve, and so
    // on).  It will generate faces [0 5 1], [1 5 6], [1 6 2], ...
    // The boolean variable "closed" will determine whether the
    // function closes the curve (that is, connects the last profile
    // to the first profile).
    static vector< Vector3i > triSweep( unsigned dia, unsigned len, bool closed )
    {
        vector< Vector3i > ret;

        if (dia < 2 || len < 2)
        {
            return ret;
        }

        const unsigned sweepCount = closed ? len : (len - 1);

        for (unsigned sweep = 0; sweep < sweepCount; ++sweep)
        {
            const unsigned curr = sweep;
            const unsigned next = (sweep + 1) % len;

            for (unsigned i = 0; i < dia - 1; ++i)
            {
                const unsigned a = curr * dia + i;
                const unsigned b = next * dia + i;
                const unsigned c = curr * dia + i + 1;
                const unsigned d = next * dia + i + 1;

                ret.emplace_back(static_cast<int>(a), static_cast<int>(b), static_cast<int>(c));
                ret.emplace_back(static_cast<int>(c), static_cast<int>(b), static_cast<int>(d));
            }
        }

        return ret;
    }
    
    // We're only implenting swept surfaces where the profile curve is
    // flat on the xy-plane.  This is a check function.
    static bool checkFlat(const vector<CurvePoint>&profile)
    {
        for (unsigned i=0; i<profile.size(); i++)
            if (profile[i].position[2] != 0.0 ||
                profile[i].tangent[2] != 0.0 ||
                profile[i].normal[2] != 0.0)
                return false;
    
        return true;
    }
}

GeneratedSurface makeSurfRev(const vector<CurvePoint>&profile, unsigned steps)
{
    GeneratedSurface surface;
    
    if (!checkFlat(profile))
    {
        cerr << "surfRev profile curve must be flat on xy plane." << endl;
        exit(0);
    }

    if (profile.empty() || steps == 0)
    {
        return surface;
    }

    const unsigned dia = static_cast<unsigned>(profile.size());
    surface.positions.reserve(static_cast<size_t>(dia) * static_cast<size_t>(steps));
    surface.normals.reserve(static_cast<size_t>(dia) * static_cast<size_t>(steps));

    const Vector3f axis(0.0f, 1.0f, 0.0f);

    for (unsigned step = 0; step < steps; ++step)
    {
        const float angle = (2.0f * static_cast<float>(EIGEN_PI) * static_cast<float>(step)) / static_cast<float>(steps);
        const float c = std::cos(angle);
        const float s = std::sin(angle);

        Matrix3f R;
        R <<  c, 0.0f,  s,
              0.0f, 1.0f, 0.0f,
             -s, 0.0f,  c;

        for (const auto& cp : profile)
        {
            Vector3f pos = R * cp.position;
            surface.positions.push_back(pos);

            Vector3f tangent = R * cp.tangent;
            Vector3f sweepDir = axis.cross(pos);
            Vector3f candidate = sweepDir.cross(tangent);

            if (candidate.squaredNorm() < 1e-8f)
            {
                Vector3f rotatedNormal = R * cp.normal;
                if (rotatedNormal.squaredNorm() > 1e-8f)
                {
                    candidate = -rotatedNormal;
                }
            }

            if (candidate.squaredNorm() < 1e-8f)
            {
                Vector3f radial(pos.x(), 0.0f, pos.z());
                if (radial.squaredNorm() > 1e-8f)
                {
                    candidate = radial;
                }
                else
                {
                    candidate = axis.cross(tangent);
                }
            }

            if (candidate.squaredNorm() > 1e-8f)
            {
                candidate.normalize();
            }
            else
            {
                candidate = Vector3f::UnitY();
            }

            Vector3f radial(pos.x(), 0.0f, pos.z());
            if (radial.squaredNorm() > 1e-8f && candidate.dot(radial) < 0.0f)
            {
                candidate = -candidate;
            }

            surface.normals.push_back(candidate);
        }
    }

    if (dia >= 2 && steps >= 2)
    {
        surface.indices = triSweep(dia, steps, true);
    }

    return surface;
}


// Helper to compute arc-length normalized parameters for a polyline curve
static inline std::vector<float> cumulativeNormalized(const std::vector<CurvePoint>& C)
{
    std::vector<float> u(C.size(), 0.0f);
    if (C.size() <= 1) return u;
    float acc = 0.0f;
    for (size_t i = 1; i < C.size(); ++i)
    {
        acc += (C[i].position - C[i-1].position).norm();
        u[i] = acc;
    }
    if (acc > 1e-20f)
    {
        for (auto &v : u) v /= acc;
    }
    else
    {
        // fallback to uniform if zero length
        for (size_t i = 0; i < C.size(); ++i) u[i] = float(i) / float(std::max<size_t>(1, C.size()-1));
    }
    return u;
}

GeneratedSurface makeGenCyl(const vector<CurvePoint>&profile, const vector<CurvePoint>&sweep )
{
    GeneratedSurface surface;

    if (!checkFlat(profile))
    {
        cerr << "genCyl profile curve must be flat on xy plane." << endl;
        exit(0);
    }

    if (profile.empty() || sweep.empty())
    {
        return surface;
    }

    vector<CurvePoint> sweepSamples = sweep;
    bool closeSweep = false;
    const float eps = 1e-6f;

    if (sweepSamples.size() > 1)
    {
        if ((sweepSamples.front().position - sweepSamples.back().position).squaredNorm() < eps)
        {
            closeSweep = true;
            sweepSamples.pop_back();
        }
    }

    const unsigned dia = static_cast<unsigned>(profile.size());
    const unsigned len = static_cast<unsigned>(sweepSamples.size());

    if (dia < 2 || len < 2)
    {
        return surface;
    }

    surface.positions.reserve(static_cast<size_t>(dia) * static_cast<size_t>(len));
    surface.normals.reserve(static_cast<size_t>(dia) * static_cast<size_t>(len));

    const float frameEps = 1e-6f;

    auto orthogonalVector = [&](const Vector3f& tDir) -> Vector3f
    {
        Vector3f axis = (std::abs(tDir.x()) < 0.9f) ? Vector3f::UnitX() : Vector3f::UnitY();
        Vector3f ortho = axis - axis.dot(tDir) * tDir;
        if (ortho.squaredNorm() < frameEps)
        {
            axis = Vector3f::UnitZ();
            ortho = axis - axis.dot(tDir) * tDir;
        }
        if (ortho.squaredNorm() < frameEps)
        {
            return Vector3f::UnitX();
        }
        return ortho.normalized();
    };

    auto makeFrame = [&](const Vector3f& tDir, Vector3f nHint, const Vector3f& fallbackN, Vector3f& outN, Vector3f& outB)
    {
        if (nHint.squaredNorm() > frameEps)
        {
            nHint -= nHint.dot(tDir) * tDir;
        }

        if (nHint.squaredNorm() < frameEps)
        {
            Vector3f candidate = fallbackN;
            if (candidate.squaredNorm() > frameEps)
            {
                candidate -= candidate.dot(tDir) * tDir;
                if (candidate.squaredNorm() >= frameEps)
                {
                    nHint = candidate;
                }
            }
        }

        if (nHint.squaredNorm() < frameEps)
        {
            nHint = orthogonalVector(tDir);
        }

        nHint.normalize();

        Vector3f bVec = tDir.cross(nHint);
        if (bVec.squaredNorm() < frameEps)
        {
            nHint = orthogonalVector(tDir);
            bVec = tDir.cross(nHint);
        }

        bVec.normalize();
        nHint = bVec.cross(tDir);
        if (nHint.squaredNorm() > frameEps)
        {
            nHint.normalize();
        }

        outN = nHint;
        outB = bVec;
    };

    Vector3f prevTangent = Vector3f::Zero();
    Vector3f prevNormal = Vector3f::Zero();
    Vector3f prevBinormal = Vector3f::Zero();
    bool hasPrevFrame = false;

    for (unsigned j = 0; j < len; ++j)
    {
        const auto& sp = sweepSamples[j];

        Vector3f t = sp.tangent;
        if (t.squaredNorm() < frameEps)
        {
            if (j + 1 < len)
            {
                t = sweepSamples[j + 1].position - sp.position;
            }
            else if (closeSweep && len > 1)
            {
                t = sweepSamples[0].position - sp.position;
            }
            else if (j > 0)
            {
                t = sp.position - sweepSamples[j - 1].position;
            }
        }
        if (t.squaredNorm() < frameEps)
        {
            t = Vector3f::UnitY();
        }
        t.normalize();

        Vector3f normalHint = sp.normal;
        if (normalHint.squaredNorm() < frameEps && sp.binormal.squaredNorm() > frameEps)
        {
            normalHint = sp.binormal.cross(t);
        }

        Vector3f n;
        Vector3f b;

        if (!hasPrevFrame)
        {
            makeFrame(t, normalHint, Vector3f::Zero(), n, b);
        }
        else
        {
            Vector3f transportedN = prevNormal;
            Vector3f transportedB = prevBinormal;

            Vector3f axis = prevTangent.cross(t);
            float axisNorm = axis.norm();
            float dot = std::max(-1.0f, std::min(1.0f, prevTangent.dot(t)));
            if (axisNorm > frameEps)
            {
                axis.normalize();
                float angle = std::atan2(axisNorm, dot);
                AngleAxisf rotation(angle, axis);
                transportedN = rotation * transportedN;
                transportedB = rotation * transportedB;
            }
            else if (dot < 0.0f)
            {
                transportedN = -transportedN;
                transportedB = -transportedB;
            }

            if (normalHint.squaredNorm() > frameEps)
            {
                normalHint -= normalHint.dot(t) * t;
                if (normalHint.squaredNorm() > frameEps)
                {
                    normalHint.normalize();
                    if (normalHint.dot(transportedN) < 0.0f)
                    {
                        normalHint = -normalHint;
                    }
                }
                else
                {
                    normalHint.setZero();
                }
            }

            makeFrame(t, normalHint, transportedN, n, b);

            if (n.dot(transportedN) < 0.0f)
            {
                n = -n;
                b = -b;
            }
        }

        Matrix3f frame;
        frame.col(0) = n;
        frame.col(1) = b;
        frame.col(2) = t;

        for (const auto& cp : profile)
        {
            Vector3f localPos = cp.position;
            Vector3f worldPos = sp.position + frame * localPos;
            surface.positions.push_back(worldPos);

            Vector3f tangentU = frame * cp.tangent;
            Vector3f tangentV = t;
            Vector3f normalCandidate = tangentV.cross(tangentU);

            Vector3f localNormal = frame * cp.normal;

            if (normalCandidate.squaredNorm() < frameEps)
            {
                if (localNormal.squaredNorm() > frameEps)
                {
                    normalCandidate = -localNormal;
                }
                else
                {
                    normalCandidate = n;
                }
            }

            if (normalCandidate.squaredNorm() > frameEps)
            {
                normalCandidate.normalize();
            }
            else
            {
                normalCandidate = n;
            }

            if (localNormal.squaredNorm() > frameEps && normalCandidate.dot(localNormal) > 0.0f)
            {
                normalCandidate = -normalCandidate;
            }

            surface.normals.push_back(normalCandidate);
        }

        prevTangent = t;
        prevNormal = n;
        prevBinormal = b;
        hasPrevFrame = true;
    }

    surface.indices = triSweep(dia, len, closeSweep);

    return surface;
}

GeneratedSurface makeGenCyl(const std::vector<CurvePoint>& profile,
                            const std::vector<CurvePoint>& sweep,
                            const std::vector<CurvePoint>& scale)
{
    // Prepare sweep samples and closure exactly as in the 2-curve version
    if (!checkFlat(profile))
    {
        cerr << "genCyl profile curve must be flat on xy plane." << endl;
        return GeneratedSurface{};
    }
    if (profile.empty() || sweep.empty()) return GeneratedSurface{};

    vector<CurvePoint> sweepSamples = sweep;
    bool closeSweep = false;
    const float eps = 1e-6f;
    if (sweepSamples.size() > 1)
    {
        if ((sweepSamples.front().position - sweepSamples.back().position).squaredNorm() < eps)
        {
            closeSweep = true;
            sweepSamples.pop_back();
        }
    }

    const unsigned dia = static_cast<unsigned>(profile.size());
    const unsigned len = static_cast<unsigned>(sweepSamples.size());
    GeneratedSurface surface;
    if (dia < 2 || len < 2) return surface;

    surface.positions.reserve(static_cast<size_t>(dia) * static_cast<size_t>(len));
    surface.normals.reserve(static_cast<size_t>(dia) * static_cast<size_t>(len));

    // Build per-sweep scale factors by matching arc-length parameters
    auto uSweep = cumulativeNormalized(sweepSamples);
    auto uScale = cumulativeNormalized(scale);
    std::vector<float> svals(scale.size(), 1.0f);
    for (size_t i = 0; i < scale.size(); ++i)
    {
        float v = scale[i].position.x();
        if (!std::isfinite(v) || std::abs(v) < 1e-12f)
        {
            // fallback: planar magnitude (xy)
            float sx = scale[i].position.x();
            float sy = scale[i].position.y();
            v = std::sqrt(std::max(0.0f, sx*sx + sy*sy));
            if (!std::isfinite(v) || v < 1e-12f) v = 1.0f;
        }
        svals[i] = std::max(1e-4f, v);
    }

    auto sampleScale = [&](float u) -> float
    {
        if (uScale.empty()) return 1.0f;
        if (u <= uScale.front()) return svals.front();
        if (u >= uScale.back()) return svals.back();
        // find segment
        size_t hi = 1;
        while (hi < uScale.size() && uScale[hi] < u) ++hi;
        size_t lo = hi - 1;
        float t = (u - uScale[lo]) / std::max(1e-12f, (uScale[hi] - uScale[lo]));
        return (1.0f - t) * svals[lo] + t * svals[hi];
    };

    const float frameEps = 1e-6f;
    auto orthogonalVector = [&](const Vector3f& tDir) -> Vector3f
    {
        Vector3f axis = (std::abs(tDir.x()) < 0.9f) ? Vector3f::UnitX() : Vector3f::UnitY();
        Vector3f ortho = axis - axis.dot(tDir) * tDir;
        if (ortho.squaredNorm() < frameEps)
        {
            axis = Vector3f::UnitZ();
            ortho = axis - axis.dot(tDir) * tDir;
        }
        if (ortho.squaredNorm() < frameEps)
        {
            return Vector3f::UnitX();
        }
        return ortho.normalized();
    };

    auto makeFrame = [&](const Vector3f& tDir, Vector3f nHint, const Vector3f& fallbackN, Vector3f& outN, Vector3f& outB)
    {
        if (nHint.squaredNorm() > frameEps)
        {
            nHint -= nHint.dot(tDir) * tDir;
        }
        if (nHint.squaredNorm() < frameEps)
        {
            Vector3f candidate = fallbackN;
            if (candidate.squaredNorm() > frameEps)
            {
                candidate -= candidate.dot(tDir) * tDir;
                if (candidate.squaredNorm() >= frameEps)
                {
                    nHint = candidate;
                }
            }
        }
        if (nHint.squaredNorm() < frameEps)
        {
            nHint = orthogonalVector(tDir);
        }
        nHint.normalize();
        Vector3f bVec = tDir.cross(nHint);
        if (bVec.squaredNorm() < frameEps)
        {
            nHint = orthogonalVector(tDir);
            bVec = tDir.cross(nHint);
        }
        bVec.normalize();
        nHint = bVec.cross(tDir);
        if (nHint.squaredNorm() > frameEps) nHint.normalize();
        outN = nHint; outB = bVec;
    };

    Vector3f prevTangent = Vector3f::Zero();
    Vector3f prevNormal = Vector3f::Zero();
    Vector3f prevBinormal = Vector3f::Zero();
    bool hasPrevFrame = false;

    for (unsigned j = 0; j < len; ++j)
    {
        const auto& sp = sweepSamples[j];
        float s = sampleScale(uSweep[j]);
        Vector3f t = sp.tangent;
        if (t.squaredNorm() < frameEps)
        {
            if (j + 1 < len) t = sweepSamples[j + 1].position - sp.position;
            else if (closeSweep && len > 1) t = sweepSamples[0].position - sp.position;
            else if (j > 0) t = sp.position - sweepSamples[j - 1].position;
        }
        if (t.squaredNorm() < frameEps) t = Vector3f::UnitY();
        t.normalize();

        Vector3f normalHint = sp.normal;
        if (normalHint.squaredNorm() < frameEps && sp.binormal.squaredNorm() > frameEps)
            normalHint = sp.binormal.cross(t);

        Vector3f n, b;
        if (!hasPrevFrame) makeFrame(t, normalHint, Vector3f::Zero(), n, b);
        else {
            Vector3f transportedN = prevNormal;
            Vector3f transportedB = prevBinormal;
            Vector3f axis = prevTangent.cross(t);
            float axisNorm = axis.norm();
            float dot = std::max(-1.0f, std::min(1.0f, prevTangent.dot(t)));
            if (axisNorm > frameEps)
            {
                axis.normalize();
                float angle = std::atan2(axisNorm, dot);
                AngleAxisf rotation(angle, axis);
                transportedN = rotation * transportedN;
                transportedB = rotation * transportedB;
            }
            else if (dot < 0.0f)
            {
                transportedN = -transportedN;
                transportedB = -transportedB;
            }
            if (normalHint.squaredNorm() > frameEps)
            {
                normalHint -= normalHint.dot(t) * t;
                if (normalHint.squaredNorm() > frameEps)
                {
                    normalHint.normalize();
                    if (normalHint.dot(transportedN) < 0.0f) normalHint = -normalHint;
                }
                else normalHint.setZero();
            }
            makeFrame(t, normalHint, transportedN, n, b);
            if (n.dot(transportedN) < 0.0f) { n = -n; b = -b; }
        }

        Matrix3f frame; frame.col(0) = n; frame.col(1) = b; frame.col(2) = t;
        for (const auto& cp : profile)
        {
            // Uniform scaling of the profile in its XY-plane
            Vector3f localPos = cp.position; localPos.x() *= s; localPos.y() *= s;
            Vector3f worldPos = sp.position + frame * localPos;
            surface.positions.push_back(worldPos);

            Vector3f tangentU = frame * (cp.tangent * s);
            Vector3f tangentV = t;
            Vector3f normalCandidate = tangentV.cross(tangentU);

            Vector3f localNormal = frame * cp.normal;
            if (normalCandidate.squaredNorm() < frameEps)
            {
                if (localNormal.squaredNorm() > frameEps) normalCandidate = -localNormal;
                else normalCandidate = n;
            }
            if (normalCandidate.squaredNorm() > frameEps) normalCandidate.normalize();
            else normalCandidate = n;
            if (localNormal.squaredNorm() > frameEps && normalCandidate.dot(localNormal) > 0.0f)
                normalCandidate = -normalCandidate;
            surface.normals.push_back(normalCandidate);
        }

        prevTangent = t; prevNormal = n; prevBinormal = b; hasPrevFrame = true;
    }

    surface.indices = triSweep(dia, len, closeSweep);
    return surface;
}

GeneratedSurface makeGenCylPiecewise(const std::vector<CurvePoint>& profile, const std::vector<std::vector<CurvePoint>>& sweepSegments)
{
    GeneratedSurface merged;
    if (!checkFlat(profile))
    {
        cerr << "genCyl profile curve must be flat on xy plane." << endl;
        exit(0);
    }
    if (profile.empty() || sweepSegments.empty()) return merged;

    for (const auto& seg : sweepSegments)
    {
        if (seg.size() < 2) continue;
        GeneratedSurface part = makeGenCyl(profile, seg);
        size_t offset = merged.positions.size();
        merged.positions.insert(merged.positions.end(), part.positions.begin(), part.positions.end());
        merged.normals.insert(merged.normals.end(), part.normals.begin(), part.normals.end());
        merged.indices.reserve(merged.indices.size() + part.indices.size());
        for (auto f : part.indices) merged.indices.emplace_back(int(offset) + f.x(), int(offset) + f.y(), int(offset) + f.z());
    }

    return merged;
}

GeneratedSurface makeIsoSurfaceRAW(const std::string& rawPath,
                                   const Vector3i& dims,
                                   float iso,
                                   const Vector3f& spacing,
                                   const Vector3f& origin,
                                   const std::string& dtype)
{
    GeneratedSurface surface;
    if (dims.x() < 2 || dims.y() < 2 || dims.z() < 2) return surface;

    // Load volume
    std::vector<uint8_t>  buf8;
    std::vector<uint16_t> buf16;
    std::vector<float>    buf32;
    size_t voxelCount = size_t(dims.x()) * size_t(dims.y()) * size_t(dims.z());
    if (dtype == "uint8") buf8.resize(voxelCount);
    else if (dtype == "uint16") buf16.resize(voxelCount);
    else if (dtype == "float32") buf32.resize(voxelCount);
    else {
        std::cerr << "Unsupported dtype: " << dtype << std::endl;
        return surface;
    }
    {
        std::ifstream f(rawPath, std::ios::binary);
        if (!f) {
            std::cerr << "Failed to open RAW volume: " << rawPath << std::endl;
            return surface;
        }
        if (dtype == "uint8") f.read(reinterpret_cast<char*>(buf8.data()), std::streamsize(buf8.size()));
        else if (dtype == "uint16") f.read(reinterpret_cast<char*>(buf16.data()), std::streamsize(buf16.size()*sizeof(uint16_t)));
        else f.read(reinterpret_cast<char*>(buf32.data()), std::streamsize(buf32.size()*sizeof(float)));
        if (!f) {
            std::cerr << "Failed to read expected number of bytes from RAW volume." << std::endl;
            return surface;
        }
    }

    VolumeView V;
    V.dims = dims;
    V.dtype = dtype;
    V.u8  = buf8.empty() ? nullptr : buf8.data();
    V.u16 = buf16.empty() ? nullptr : buf16.data();
    V.f32 = buf32.empty() ? nullptr : buf32.data();

    auto cubeCorner = [](int corner)->Vector3i{
        return Vector3i( (corner & 1) ? 1:0, (corner & 2) ? 1:0, (corner & 4) ? 1:0 );
    };

    // Marching tetrahedra
    std::vector<Vector3f>& outV = surface.positions;
    std::vector<Vector3f>& outN = surface.normals;
    std::vector<Vector3i>& outI = surface.indices;

    auto emitTri = [&](const Vector3f& a, const Vector3f& b, const Vector3f& c,
                       const Vector3f& na, const Vector3f& nb, const Vector3f& nc){
        int base = (int)outV.size();
        outV.push_back(a); outV.push_back(b); outV.push_back(c);
        outN.push_back(na); outN.push_back(nb); outN.push_back(nc);
        outI.emplace_back(base+0, base+1, base+2);
    };

    auto interp = [&](const Vector3f& p0, const Vector3f& p1, float s0, float s1) -> Vector3f {
        float t = (iso - s0) / (s1 - s0 + 1e-20f);
        t = std::clamp(t, 0.0f, 1.0f);
        return lerp(p0, p1, t);
    };

    for (int z = 0; z < dims.z()-1; ++z)
    for (int y = 0; y < dims.y()-1; ++y)
    for (int x = 0; x < dims.x()-1; ++x)
    {
        // Cube 8 corners
        Vector3f Pc[8]; float Sc[8]; Vector3f Nc[8];
        for (int c = 0; c < 8; ++c) {
            Vector3i off = cubeCorner(c);
            int xi = x + off.x();
            int yi = y + off.y();
            int zi = z + off.z();
            Pc[c] = gridToWorld(Vector3i(xi,yi,zi), spacing, origin);
            Sc[c] = V.at(xi,yi,zi);
            Nc[c] = gradientAt(V, xi, yi, zi);
        }

        // Process 6 tetrahedra
        for (int t = 0; t < 6; ++t) {
            int i0 = cubeTets[t].v[0];
            int i1 = cubeTets[t].v[1];
            int i2 = cubeTets[t].v[2];
            int i3 = cubeTets[t].v[3];
            int ids[4] = { i0, i1, i2, i3 };
            float s[4] = { Sc[i0], Sc[i1], Sc[i2], Sc[i3] };
            Vector3f p[4] = { Pc[i0], Pc[i1], Pc[i2], Pc[i3] };
            Vector3f n[4] = { Nc[i0], Nc[i1], Nc[i2], Nc[i3] };

            int mask = 0; for (int k=0;k<4;++k) if (s[k] >= iso) mask |= (1<<k);
            if (mask == 0 || mask == 15) continue; // no intersection

            auto edgeP = [&](int a,int b)->Vector3f{ return interp(p[a], p[b], s[a], s[b]); };
            auto edgeN = [&](int a,int b)->Vector3f{ return safe_normalize(lerp(n[a], n[b], 0.5f)); };

            switch (mask) {
                case 1: case 14: {
                    bool inv = (mask==14);
                    int a=0,b=1,c=2,d=3;
                    Vector3f v0 = edgeP(a,b), v1 = edgeP(a,c), v2 = edgeP(a,d);
                    Vector3f na = edgeN(a,b), nb = edgeN(a,c), nc = edgeN(a,d);
                    if (!inv) emitTri(v0,v1,v2, na,nb,nc); else emitTri(v0,v2,v1, na,nc,nb);
                } break;
                case 2: case 13: {
                    bool inv = (mask==13);
                    int a=1,b=0,c=2,d=3;
                    Vector3f v0 = edgeP(a,b), v1 = edgeP(a,c), v2 = edgeP(a,d);
                    Vector3f na = edgeN(a,b), nb = edgeN(a,c), nc = edgeN(a,d);
                    if (!inv) emitTri(v0,v1,v2, na,nb,nc); else emitTri(v0,v2,v1, na,nc,nb);
                } break;
                case 3: case 12: {
                    bool inv = (mask==12);
                    int a=0,b=2,c=1,d=3;
                    Vector3f v0 = edgeP(a,c), v1 = edgeP(b,c), v2 = edgeP(a,d);
                    Vector3f v3 = edgeP(b,d);
                    Vector3f n0 = edgeN(a,c), n1 = edgeN(b,c), n2 = edgeN(a,d), n3 = edgeN(b,d);
                    if (!inv) { emitTri(v0,v1,v2, n0,n1,n2); emitTri(v1,v3,v2, n1,n3,n2);} else { emitTri(v0,v2,v1, n0,n2,n1); emitTri(v1,v2,v3, n1,n2,n3);} 
                } break;
                case 4: case 11: {
                    bool inv = (mask==11);
                    int a=2,b=0,c=1,d=3;
                    Vector3f v0 = edgeP(a,b), v1 = edgeP(a,c), v2 = edgeP(a,d);
                    Vector3f na = edgeN(a,b), nb = edgeN(a,c), nc = edgeN(a,d);
                    if (!inv) emitTri(v0,v1,v2, na,nb,nc); else emitTri(v0,v2,v1, na,nc,nb);
                } break;
                case 5: case 10: {
                    bool inv = (mask==10);
                    int a=0,b=1,c=2,d=3;
                    Vector3f v0 = edgeP(a,b), v1 = edgeP(b,c), v2 = edgeP(c,a);
                    Vector3f na = edgeN(a,b), nb = edgeN(b,c), nc = edgeN(c,a);
                    if (!inv) emitTri(v0,v1,v2, na,nb,nc); else emitTri(v0,v2,v1, na,nc,nb);
                } break;
                case 6: case 9: {
                    bool inv = (mask==9);
                    int a=1,b=0,c=2,d=3;
                    Vector3f v0 = edgeP(a,b), v1 = edgeP(b,c), v2 = edgeP(c,a);
                    Vector3f na = edgeN(a,b), nb = edgeN(b,c), nc = edgeN(c,a);
                    if (!inv) emitTri(v0,v1,v2, na,nb,nc); else emitTri(v0,v2,v1, na,nc,nb);
                } break;
                case 7: case 8: {
                    bool inv = (mask==8);
                    int a=3,b=0,c=1,d=2;
                    Vector3f v0 = edgeP(a,b), v1 = edgeP(a,c), v2 = edgeP(a,d);
                    Vector3f na = edgeN(a,b), nb = edgeN(a,c), nc = edgeN(a,d);
                    if (!inv) emitTri(v0,v1,v2, na,nb,nc); else emitTri(v0,v2,v1, na,nc,nb);
                } break;
                default: break;
            }
        }
    }

    return surface;
}
