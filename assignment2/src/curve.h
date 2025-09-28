#pragma once

#include <vector>
#include <map>
#include <nlohmann/json.hpp>
#include "eigen_json_serializers.h"


template<class P>
struct SplineCurveT
{
    string      type; // { "bezier", "bspline", "circle", "catmull-rom" }
    vector<P>   control_points;
    // Optional: piecewise segments for curves with gaps or sharp corners.
    // If present and non-empty, these define separate curve segments.
    // For bezier-piecewise: each segment must follow 4+3*n rule.
    // For bspline-piecewise: each segment must have at least 4 points.
    std::vector<std::vector<P>> segments;
};

typedef SplineCurveT<Vector3f> SplineCurve;

// The CurvePoint object stores information about a point on a curve
// after it has been tesselated: the vertex (V), the tangent (T), the
// normal (N), and the binormal (B).  It is the responsiblility of
// functions that create these objects to fill in all the data.
struct CurvePoint
{
    Vector3f    position = Vector3f::Zero();   // Position
    Vector3f    tangent = Vector3f::Zero(); // Tangent  (unit)
    Vector3f    normal = Vector3f::Zero(); // Normal   (unit)
    Vector3f    binormal = Vector3f::Zero(); // Binormal (unit)
    //float       t = 0.0f;
};

// JSON serialization for SplineCurve and CurvePoint types
using json = nlohmann::json;

static inline void to_json(json& j, const CurvePoint& p) {
    j = json{ {"position", p.position}, {"tangent", p.tangent}, {"normal", p.normal}, {"binormal", p.binormal} };
}

static inline void from_json(const json& j, CurvePoint& p) {
    p.position = j.at("position").get<Vector3f>();
    p.tangent = j.at("tangent").get<Vector3f>();
    p.normal = j.at("normal").get<Vector3f>();
    p.binormal = j.at("binormal").get<Vector3f>();
}

template<class P>
static inline void to_json(json& j, const SplineCurveT<P>& c) {
    j = json{ {"type", c.type}, {"control_points", c.control_points} };
    if (!c.segments.empty()) j["segments"] = c.segments;
}

template<class P>
static inline void from_json(const json& j, SplineCurveT<P>& c) {
    j.at("type").get_to(c.type);
    if (j.contains("control_points")) j.at("control_points").get_to(c.control_points);
    if (j.contains("segments")) c.segments = j.at("segments").template get<std::vector<std::vector<P>>>();
    assert(c.type == "bezier" || c.type == "bspline" || c.type == "circle" || c.type == "catmull-rom" || c.type == "kappa" || c.type == "bezier-piecewise" || c.type == "bspline-piecewise");
}


////////////////////////////////////////////////////////////////////////////
// The following two functions take an array of control points (stored
// in P) and generate an STL Vector of CurvePoints.  They should
// return an empty array if the number of control points in C is
// inconsistent with the type of curve.  In both these functions,
// "step" indicates the number of samples PER PIECE.  E.g., a
// 7-control-point Bezier curve will have two pieces (and the 4th
// control point is shared).
////////////////////////////////////////////////////////////////////////////

// Assume number of control points properly specifies a piecewise
// Bezier curve.  I.e., C.size() == 4 + 3*n, n=0,1,...
void tessellateBezier(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals);

// Piecewise variants: concatenate segments with optional gaps (inserted by caller)
void tessellateBezierPiecewise(const std::vector<std::vector<Vector3f>>& segments, std::vector<CurvePoint>& dest, unsigned num_intervals, bool connect = false);

// Bsplines only require that there are at least 4 control points.
void tessellateBspline(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals);

void tessellateBsplinePiecewise(const std::vector<std::vector<Vector3f>>& segments, std::vector<CurvePoint>& dest, unsigned num_intervals, bool connect = false);

// Catmull-Rom splines (uniform, C1, interpolating). At least 2 control points.
void tessellateCatmullRom(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals);

// κ-curves (Yan et al. 2017), closed-curve variant. Requires at least 3 points.
// Assumes the input control points define a closed loop (first point implicitly connects to last).
// For now we implement a robust, loop-free heuristic with angle-bisector tangents and bounded handle lengths.
void tessellateKappaClosed(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals);

// Two control points: the x-coordinate of the first gives the radius.
void tessellateCircle(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals);

// Draw the curve. For the extras that require computing coordinate
// frames, we suggest you append a mechanism for optionally drawing
// the frames as well.
void drawCurve(const vector<CurvePoint>& curve, bool draw_frames);
