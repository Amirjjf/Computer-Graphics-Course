#pragma once

#include "curve.h"

#include <iostream>


struct ParsedSurface {
    std::string type;
    std::vector<unsigned> curve_indices;
    // For isosurface extraction from a volume file (RAW)
    std::string volume_file;     // path to RAW file
    Vector3i    dims = Vector3i(0,0,0); // Nx, Ny, Nz
    float       iso = 0.0f;      // isovalue (normalized if dtype is uint8)
    Vector3f    spacing = Vector3f(1.0f,1.0f,1.0f); // voxel spacing
    Vector3f    origin  = Vector3f(0.0f,0.0f,0.0f); // grid origin
    std::string dtype;           // "uint8" (default), "uint16", or "float32"
};

// GeneratedSurface is just a struct that contains vertices, normals, and
// faces.  VV[i] is the position of vertex i, and VN[i] is the normal
// of vertex i.  A face is a triple i,j,k corresponding to a triangle
// with (vertex i, normal i), (vertex j, normal j), ...
struct GeneratedSurface
{
    std::vector<Vector3f> positions, normals;
    std::vector<Vector3i> indices;
};

// Sweep a profile curve that lies flat on the xy-plane around the
// y-axis.  The number of divisions is given by steps.
GeneratedSurface makeSurfRev(const std::vector<CurvePoint>& profile, unsigned steps);

GeneratedSurface makeGenCyl(const std::vector<CurvePoint>& profile, const vector<CurvePoint>& sweep);
// Variant with per-sweep uniform scale controlled by a third curve.
// The scale curve is sampled along its arc-length parameter and matched to the sweep's
// arc-length parameter; the x-component of each sampled point defines the scale factor.
// If x is near zero, the planar magnitude of the sample is used instead. Values are clamped
// to a small positive epsilon to avoid degeneracy.
GeneratedSurface makeGenCyl(const std::vector<CurvePoint>& profile,
                            const std::vector<CurvePoint>& sweep,
                            const std::vector<CurvePoint>& scale);

// Piecewise sweep: generate disjoint surface strips per sweep segment (no stitching across gaps)
GeneratedSurface makeGenCylPiecewise(const std::vector<CurvePoint>& profile, const std::vector<std::vector<CurvePoint>>& sweepSegments);


// JSON serialization for ParsedSurface
using json = nlohmann::json;

static inline void to_json(json& j, const ParsedSurface& s) {
    j = json{ {"type", s.type} };
    if (s.type == "revolution" || s.type == "gen_cyl") {
        j["curve_indices"] = s.curve_indices;
    } else if (s.type == "isosurface") {
        j["volume_file"] = s.volume_file;
        j["dims"] = std::vector<int>{ s.dims.x(), s.dims.y(), s.dims.z() };
        j["iso"] = s.iso;
        j["spacing"] = std::vector<float>{ s.spacing.x(), s.spacing.y(), s.spacing.z() };
        j["origin"]  = std::vector<float>{ s.origin.x(),  s.origin.y(),  s.origin.z() };
        j["dtype"] = s.dtype;
    }
}

static inline void from_json(const json& j, ParsedSurface& s) {
    j.at("type").get_to(s.type);
    if (s.type == "revolution" || s.type == "gen_cyl") {
        j.at("curve_indices").get_to(s.curve_indices);
    } else if (s.type == "isosurface") {
        s.volume_file = j.at("volume_file").get<std::string>();
        auto dimsVec = j.at("dims").get<std::vector<int>>();
        if (dimsVec.size() != 3) throw std::runtime_error("isosurface dims must be [nx,ny,nz]");
        s.dims = Vector3i(dimsVec[0], dimsVec[1], dimsVec[2]);
        s.iso = j.value("iso", 0.5f);
        if (j.contains("spacing")) {
            auto sp = j.at("spacing").get<std::vector<float>>();
            if (sp.size() == 3) s.spacing = Vector3f(sp[0], sp[1], sp[2]);
        }
        if (j.contains("origin")) {
            auto org = j.at("origin").get<std::vector<float>>();
            if (org.size() == 3) s.origin = Vector3f(org[0], org[1], org[2]);
        }
    s.dtype = j.value("dtype", std::string("uint16"));
    }
}

// Build an isosurface mesh from a RAW volume file using marching tetrahedra
GeneratedSurface makeIsoSurfaceRAW(const std::string& rawPath,
                                   const Vector3i& dims,
                                   float iso,
                                   const Vector3f& spacing = Vector3f(1,1,1),
                                   const Vector3f& origin = Vector3f(0,0,0),
                                   const std::string& dtype = std::string("uint8"));
