// Include libraries
#include "glad/gl_core_33.h"                // OpenGL
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>             // Window manager
#include <imgui.h>                  // GUI Library
#include <imgui_impl_glfw.h>
#include "imgui_impl_opengl3.h"
#include "im3d.h"

#include <Eigen/Dense>              // Linear algebra
#include <Eigen/Geometry>
#include <fmt/core.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include <cmath>

using std::vector, std::cerr, std::endl;
using Eigen::Vector3f, Eigen::Matrix3f, Eigen::Vector4f, Eigen::Matrix4f;

#include "Utils.h"
#include "curve.h"

// Cubic Bezier basis matrix
static Matrix4f B_Bezier {
	{ 1.0f, -3.0f,  3.0f, -1.0f },
	{ 0.0f,  3.0f, -6.0f,  3.0f },
	{ 0.0f,  0.0f,  3.0f, -3.0f },
	{ 0.0f,  0.0f,  0.0f,  1.0f }
};

// Cubic B-spline basis matrix
static Matrix4f B_BSpline = 1.0f/6.0f *
	Matrix4f{
		{ 1.0f, -3.0f,  3.0f, -1.0f },
		{ 4.0f,  0.0f, -6.0f,  3.0f },
		{ 1.0f,  3.0f,  3.0f, -3.0f },
		{ 0.0f,  0.0f,  0.0f,  1.0f }
	};


namespace
{
    constexpr float kEps = 1e-6f;

    inline Vector3f pickPerpendicular(const Vector3f& t)
    {
        Vector3f axis = (std::abs(t.y()) < 0.9f) ? Vector3f::UnitY() : Vector3f::UnitX();
        Vector3f n = axis - axis.dot(t) * t;
        if (n.squaredNorm() < kEps)
        {
            axis = Vector3f::UnitZ();
            n = axis - axis.dot(t) * t;
        }
        if (n.squaredNorm() < kEps)
            n = Vector3f::UnitX();
        return n.normalized();
    }

    void computeCurveFrames(std::vector<CurvePoint>& curve)
    {
        if (curve.empty())
            return;

        auto normalizeOr = [](Vector3f v, const Vector3f& fallback) {
            if (v.squaredNorm() < kEps) return fallback;
            v.normalize();
            return v;
        };

        // ensure tangents are normalized
        for (auto& cp : curve)
        {
            if (cp.tangent.squaredNorm() > kEps)
                cp.tangent.normalize();
        }

        Vector3f T0 = curve[0].tangent.squaredNorm() > kEps ? curve[0].tangent : Vector3f::UnitZ();
        Vector3f N0 = pickPerpendicular(T0);
        Vector3f B0 = T0.cross(N0);
        if (B0.squaredNorm() < kEps)
        {
            B0 = pickPerpendicular(T0).cross(T0);
        }
        B0.normalize();
        N0 = B0.cross(T0).normalized();
        curve[0].normal = N0;
        curve[0].binormal = B0;

        for (size_t i = 1; i < curve.size(); ++i)
        {
            Vector3f Tprev = curve[i-1].tangent;
            Vector3f Nprev = curve[i-1].normal;
            Vector3f Bprev = curve[i-1].binormal;
            Vector3f T = curve[i].tangent;
            if (T.squaredNorm() < kEps)
                T = Tprev;
            else
                T.normalize();

            float dot = std::clamp(Tprev.dot(T), -1.0f, 1.0f);
            Vector3f axis = Tprev.cross(T);
            float axisLen = axis.norm();
            Vector3f N = Nprev;
            Vector3f B = Bprev;
            if (axisLen > kEps && dot < 0.99999f)
            {
                axis /= axisLen;
                float angle = std::acos(dot);
                Eigen::AngleAxisf rot(angle, axis);
                N = rot * Nprev;
                B = rot * Bprev;
            }
            else if (dot < -0.9999f)
            {
                N = -Nprev;
                B = -Bprev;
            }

            // Re-orthogonalize
            N -= N.dot(T) * T;
            if (N.squaredNorm() < kEps)
            {
                N = pickPerpendicular(T);
            }
            else
            {
                N.normalize();
            }

            B = T.cross(N);
            if (B.squaredNorm() < kEps)
            {
                Vector3f alt = pickPerpendicular(T).cross(T);
                B = normalizeOr(alt, Vector3f::UnitY());
                N = normalizeOr(B.cross(T), N);
            }
            else
            {
                B.normalize();
            }

            curve[i].tangent = T;
            curve[i].normal = N;
            curve[i].binormal = B;
        }
    }
}

// This is the core routine of the curve evaluation code. Unlike
// tessellateBezier/tessellateBspline, this is only designed to
// work on 4 control points.
// - The spline basis used is determined by the matrix B (see slides).
// - The tessellated points are to be APPENDED to the vector "dest".
// - num_intervals describes the parameter-space distance between
//   successive points: it should be 1/num_intervals.
// - when include_last_point is true, the last point appended to dest
//   should be evaluated at t=1. Otherwise the last point should be
//   from t=1-1/num_intervals. This is to skip generating double points
//   at the places where one cubic segment ends and another begins.
// - For example, when num_intervals==2 and include_last_point==true,
//   dest should be appended with points evaluated at t=0, t=0.5, t=1.
//   OTOH if include_last_point==false, only t=0 and t=0.5 would be appended.
void tessellateCubicSplineSegment(	const Vector3f& p0,
							const Vector3f& p1,
							const Vector3f& p2,
							const Vector3f& p3,
							unsigned num_intervals,
							bool include_last_point,
							const Matrix4f& B,
							vector<CurvePoint>& dest)
{
	// Geometry matrix with control points as columns (3x4)
	Eigen::Matrix<float, 3, 4> G;
	G.col(0) = p0;
	G.col(1) = p1;
	G.col(2) = p2;
	G.col(3) = p3;
	Eigen::Matrix<float, 3, 4> GB = G * B;

	// Analytic second-derivative coefficients for curvature bounds
	const Vector3f secondA = 2.0f * GB.col(2);
	const Vector3f secondB = 6.0f * GB.col(3);

	auto secondNorm = [&](float t) {
		return (secondA + t * secondB).norm();
	};
	auto secondBound = [&](float t0, float t1) {
		float maxVal = std::max(secondNorm(t0), secondNorm(t1));
		float b2 = secondB.squaredNorm();
		if (b2 > 1e-12f) {
			float tCrit = -secondA.dot(secondB) / b2;
			if (tCrit > t0 && tCrit < t1) {
				maxVal = std::max(maxVal, secondNorm(tCrit));
			}
		}
		return maxVal;
	};
	auto evalPoint = [&](float t) -> CurvePoint {
		Vector4f T; T << 1.0f, t, t * t, t * t * t;
		Vector4f dT; dT << 0.0f, 1.0f, 2.0f * t, 3.0f * t * t;
		Vector3f pos = GB * T;
		Vector3f tan = GB * dT;
		if (tan.squaredNorm() > 1e-12f) tan.normalize();
		return CurvePoint{ pos, tan, Vector3f::Zero(), Vector3f::Zero() };
	};

	unsigned effectiveIntervals = std::max(1u, num_intervals);
	float baseDt = 1.0f / static_cast<float>(effectiveIntervals);
	float baseSecond = secondBound(0.0f, 1.0f);
	float tolerance = (baseSecond > 0.0f) ? (0.125f * baseSecond * baseDt * baseDt) : 0.0f;

	unsigned maxDepth = 10u + static_cast<unsigned>(std::ceil(std::log2(static_cast<float>(effectiveIntervals) + 1.0f)));
	maxDepth = std::min<unsigned>(maxDepth, 18u);

	CurvePoint start = evalPoint(0.0f);
	CurvePoint end   = evalPoint(1.0f);
	dest.push_back(start);

	std::function<void(float,float,const CurvePoint&,const CurvePoint&,unsigned)> subdivide;
	subdivide = [&](float t0, float t1, const CurvePoint& c0, const CurvePoint& c1, unsigned depth) {
		float dt = t1 - t0;
		float bound = 0.125f * secondBound(t0, t1) * dt * dt;
		bool reachedDepth = depth >= maxDepth;
		if (bound <= tolerance || reachedDepth) {
			if (include_last_point || t1 < 1.0f - 1e-6f) {
				dest.push_back(c1);
			}
			return;
		}
		float tm = 0.5f * (t0 + t1);
		CurvePoint cm = evalPoint(tm);
		subdivide(t0, tm, c0, cm, depth + 1);
		subdivide(tm, t1, cm, c1, depth + 1);
	};

	subdivide(0.0f, 1.0f, start, end, 0);
}

// The P argument holds the control points. Steps determines the step
// size per individual curve segment and should be fed directly to
// tessellateCubicSplineSegment(...).
void tessellateBezier(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals) {
    // Check
    if (P.size() < 4 || P.size() % 3 != 1) {
		fail("evalBezier must be called with 3n+1 control points.");
	}

	// clear the output array.
	dest.clear();

	// YOUR CODE HERE (R1):
	// Stitch together cubic Bézier segments: {p0..p3}, {p3..p6}, ...
	// Avoid duplicating the shared endpoint by excluding the last
	// sample except for the final segment.

	const size_t n = (P.size() - 1) / 3; // number of cubic segments
	dest.reserve(dest.size() + n * (num_intervals + 1));

	for (size_t seg = 0; seg < n; ++seg)
	{
		size_t i = seg * 3;
		bool last = (seg == n - 1);
		tessellateCubicSplineSegment(
			P[i + 0], P[i + 1], P[i + 2], P[i + 3],
			num_intervals,
			/*include_last_point*/ last,
			B_Bezier,
			dest);
	}

	computeCurveFrames(dest);
}

// Like above, the P argument holds the control points and num_intervals determines
// the step size for the individual curve segments.
void tessellateBspline(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals)
{
    // Check
    if (P.size() < 4) {
		fail("tessellateBspline must be called with 4 or more control points.");
    }

	dest.clear();

	//YOUR CODE HERE (R2)
	// Sliding window over control points: {p0..p3}, {p1..p4}, ..., {p_{n-4}..p_{n-1}}
	const size_t nSeg = (P.size() >= 4) ? (P.size() - 3) : 0;
	dest.reserve(dest.size() + nSeg * (num_intervals + 1));

	for (size_t seg = 0; seg < nSeg; ++seg)
	{
		bool last = (seg == nSeg - 1);
		tessellateCubicSplineSegment(
			P[seg + 0], P[seg + 1], P[seg + 2], P[seg + 3],
			num_intervals,
			/*include_last_point*/ last,
			B_BSpline,
			dest);
	}

	computeCurveFrames(dest);
}

// Piecewise: concatenate multiple Bezier segments arrays. If connect=false, leave gaps between segments.
void tessellateBezierPiecewise(const std::vector<std::vector<Vector3f>>& segments, std::vector<CurvePoint>& dest, unsigned num_intervals, bool connect)
{
	dest.clear();
	if (segments.empty()) return;
	for (size_t si = 0; si < segments.size(); ++si) {
		const auto& seg = segments[si];
		if (seg.size() < 4 || (seg.size() % 3) != 1) continue;
		size_t before = dest.size();
		tessellateBezier(seg, dest, num_intervals);
		// If not connecting, ensure a hard break between segments by duplicating the last point slightly offset in parameterization is unnecessary for polyline.
		// Here we simply ensure we don't accidentally merge identical endpoints: already handled by tessellateBezier.
		// If connect==false and there is another segment, we could insert a separator if the consumer needs it.
		if (!connect && si + 1 < segments.size()) {
			// Insert a tiny break by duplicating the last point; sweep code will treat gaps by distance check.
			// No-op here; gap handling will occur in sweep based on positional distance.
		}
	}
}

void tessellateBsplinePiecewise(const std::vector<std::vector<Vector3f>>& segments, std::vector<CurvePoint>& dest, unsigned num_intervals, bool connect)
{
	dest.clear();
	if (segments.empty()) return;
	for (size_t si = 0; si < segments.size(); ++si) {
		const auto& seg = segments[si];
		if (seg.size() < 4) continue;
		size_t before = dest.size();
		tessellateBspline(seg, dest, num_intervals);
		if (!connect && si + 1 < segments.size()) {
			// See note above; sweeping will observe gaps via positional jumps.
		}
	}
}

// Uniform Catmull-Rom (interpolating) using 4-point segments with sliding window.
// We handle endpoints by duplicating end points (natural end conditions).
void tessellateCatmullRom(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals)
{
	dest.clear();
	if (P.size() < 2) return;

	// Build an extended control point list with endpoint duplication for 4-point windows
	vector<Vector3f> Q;
	Q.reserve(P.size() + 2);
	if (P.size() == 2) {
		// Straight segment between two points: simple linear interpolation via a single cubic with duplicated neighbors
		Q.push_back(P[0]); Q.push_back(P[0]); Q.push_back(P[1]); Q.push_back(P[1]);
	} else {
		Q.push_back(P[0]);
		Q.insert(Q.end(), P.begin(), P.end());
		Q.push_back(P.back());
	}

	// Catmull-Rom basis matrix in power basis with geometry [P_{i-1}, P_i, P_{i+1}, P_{i+2}]
	// Positions: 0.5 * [ -1  3 -3  1 ; 2 -5  4 -1 ; -1  0  1  0 ; 0  2  0  0 ] * G * [t^3 t^2 t 1]^T (transposed arrangement)
	// We will use column vector [1 t t^2 t^3], so transpose the typical presentation accordingly.
	Matrix4f B_CR;
	// Coefficients for columns correspond to powers [1, t, t^2, t^3]
	// Derived so that C(t) = G * (B_CR * [1 t t^2 t^3]^T)
	B_CR <<
		0.0f,  1.0f,  0.0f,  0.0f,   // constant term
		-0.5f,  0.0f,  0.5f,  0.0f,   // t term
		1.0f, -2.5f,  2.0f, -0.5f,   // t^2 term
		-0.5f,  1.5f, -1.5f,  0.5f;  // t^3 term

	// Slide over 4-point windows
	const size_t nSeg = (Q.size() >= 4) ? (Q.size() - 3) : 0;
	for (size_t seg = 0; seg < nSeg; ++seg) {
		bool last = (seg == nSeg - 1);
		tessellateCubicSplineSegment(
			Q[seg + 0], Q[seg + 1], Q[seg + 2], Q[seg + 3],
			num_intervals,
			/*include_last_point*/ last,
			B_CR,
			dest);
	}

	computeCurveFrames(dest);
}

// Helper: safe normalize with fallback
static inline Vector3f safe_normalize(const Vector3f& v, const Vector3f& fallback = Vector3f::UnitX())
{
	float n2 = v.squaredNorm();
	if (n2 > 1e-12f) return v / std::sqrt(n2);
	return fallback;
}

// Build closed κ-curve as a sequence of cubic Bezier segments that interpolate the given points,
// using angle-bisector tangents and handle lengths derived from local geometry to reduce loops/cusps.
// This follows the spirit of Yan et al. 2017 but uses a practical, robust construction suitable for interactive use.
void tessellateKappaClosed(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals)
{
	dest.clear();
	const size_t n = P.size();
	if (n < 3) return;

	// Work in XY plane (z preserved); compute 2D geometry where appropriate
	auto to2 = [](const Vector3f& v){ return Eigen::Vector2f(v.x(), v.y()); };

	// Precompute unit direction vectors between successive points (closed)
	vector<Vector3f> dirs(n);
	vector<float> lens(n);
	for (size_t i = 0; i < n; ++i)
	{
		const Vector3f& a = P[i];
		const Vector3f& b = P[(i + 1) % n];
		Vector3f d = b - a;
		lens[i] = d.norm();
		if (lens[i] > 1e-8f)
			dirs[i] = d / lens[i];
		else
			dirs[i] = Vector3f(1.0f, 0.0f, 0.0f);
	}

	// Compute angle-bisector tangents at each point
	vector<Vector3f> tangents(n);
	for (size_t i = 0; i < n; ++i)
	{
		const Vector3f& dprev = dirs[(i + n - 1) % n]; // from i-1 to i
		const Vector3f& dnext = dirs[i];               // from i to i+1
		Vector3f bis = safe_normalize(dprev + dnext, dnext);
		// If nearly opposite (reflex/cusp), use perpendicular to dprev to avoid degeneracy
		if ((dprev + dnext).squaredNorm() < 1e-8f)
		{
			Vector3f z(0,0,1);
			bis = safe_normalize(z.cross(dprev));
			if (bis.squaredNorm() < 1e-8f) bis = safe_normalize(z.cross(dnext));
		}
		tangents[i] = bis;
	}

	// Handle lengths based on local turn angle and chord lengths.
	// We choose symmetric handles l_i_out and l_{i}_in for adjacent segments using a bounded factor.
	vector<float> handleLenOut(n), handleLenIn(n);
	for (size_t i = 0; i < n; ++i)
	{
		const Vector3f& dprev = dirs[(i + n - 1) % n];
		const Vector3f& dnext = dirs[i];
		float dot = std::clamp(dprev.dot(dnext), -1.0f, 1.0f);
		float theta = std::acos(dot); // [0, pi]
		// local scale: min of adjacent edge lengths
		float s = std::min(lens[(i + n - 1) % n], lens[i]);
		// Gain decreases as corner gets sharper; parameters tuned for stability
		float k = 0.5f * (1.0f - theta / static_cast<float>(EIGEN_PI)); // in [0,0.5]
		// Clamp and floor to avoid vanishing handles
		float L = std::max(0.1f * s, k * s);
		handleLenOut[i] = L;
		handleLenIn[(i + 1) % n] = L; // symmetric to next segment's incoming
	}

	// Build cubic Bezier control points for each segment [Pi -> P_{i+1}]
	// For segment i: B0=Pi, B1=Pi + t_i * Lout_i, B2=P_{i+1} - t_{i+1} * Lin_{i+1}, B3=P_{i+1}
	vector<Vector3f> bez;
	bez.reserve(4 * n);
	for (size_t i = 0; i < n; ++i)
	{
		const Vector3f& Pi = P[i];
		const Vector3f& Pj = P[(i + 1) % n];
		Vector3f ti = safe_normalize(tangents[i]);
		Vector3f tj = safe_normalize(tangents[(i + 1) % n]);

		float Lout = std::min(handleLenOut[i], lens[i] * 0.5f); // don't overshoot half edge length
		float Lin  = std::min(handleLenIn[(i + 1) % n], lens[i] * 0.5f);

		Vector3f B0 = Pi;
		Vector3f B1 = Pi + ti * Lout;
		Vector3f B3 = Pj;
		Vector3f B2 = Pj - tj * Lin;

		// Prevent self-overlap by projecting handles onto segment direction if they bend too much
		Vector3f segDir = safe_normalize(Pj - Pi);
		if ( (B1 - B0).dot(segDir) < 0.0f ) B1 = B0 + segDir * (B1 - B0).norm();
		if ( (B3 - B2).dot(segDir) < 0.0f ) B2 = B3 - segDir * (B3 - B2).norm();

		bez.push_back(B0);
		bez.push_back(B1);
		bez.push_back(B2);
		bez.push_back(B3);
	}

	// Tessellate chained Bezier segments (closed): n segments, avoid duplicate joints
	dest.reserve(n * (num_intervals + 1));
	for (size_t seg = 0; seg < n; ++seg)
	{
		size_t i4 = 4 * seg;
		bool last = (seg == n - 1);
		tessellateCubicSplineSegment(
			bez[i4 + 0], bez[i4 + 1], bez[i4 + 2], bez[i4 + 3],
			num_intervals,
			/*include_last_point*/ last,
			B_Bezier,
			dest);
	}

	computeCurveFrames(dest);
}

void tessellateCircle(const vector<Vector3f>& P, vector<CurvePoint>& dest, unsigned num_intervals) {
	if (P.size() != 2) {
		fail("tessellateCircle must be called with exactly two control points.");
	}
	dest.clear();
	num_intervals *= 4; // a bit smoother
	float rad = P[0](0);
	for (int i = 0; i < num_intervals; ++i) {
		float ang = (2.f * EIGEN_PI * i) / float(num_intervals-1);
		float c = cos(ang), s = sin(ang);
		dest.push_back(
			CurvePoint{
				Vector3f(c * rad, s * rad, .0f),
				Vector3f(-s, c, .0f),
				Vector3f(-c, -s, .0f),
				Vector3f(.0f, .0f, 1.f)
			});
	}
}

void drawCurve(const vector<CurvePoint>& curve, bool draw_frames)
{
    // Just the curve
	Im3d::BeginLineStrip();
	Im3d::SetColor(1.0f, 1.0f, 1.0f);
	for (unsigned i = 0; i < curve.size(); ++i)
		Im3d::Vertex(curve[i].position(0), curve[i].position(1), curve[i].position(2));
	Im3d::End();
    // Tangent, normal, binormal if desired
    if (draw_frames) {
        Im3d::BeginLines();
        for (unsigned i = 0; i < curve.size(); ++i) {
            CurvePoint c = curve[i];
            Vector3f p = c.position;
            Vector3f offset[3] = { c.tangent, c.normal, c.binormal };
            for (int i = 0; i < 3; ++i) {
                Im3d::SetColor(i == 1, i == 2, i == 0);
                Im3d::Vertex(p(0), p(1), p(2));
                Vector3f o = offset[i] * .2f;
                Im3d::Vertex(p(0) + o(0), p(1) + o(1), p(2) + o(2));
            }
        }
        Im3d::End();
    }
}

