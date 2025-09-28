#pragma once

#include <Eigen/Dense>
using Eigen::MatrixBase;
using Eigen::Vector3f, Eigen::Vector4f, Eigen::Matrix4f;

template<typename DerivedInput, typename DerivedLimit>
inline typename DerivedInput::PlainObject clip(const MatrixBase<DerivedInput>& a, const MatrixBase<DerivedLimit>& low, const MatrixBase<DerivedLimit>& high)
{
    return typename DerivedInput::PlainObject(a.cwiseMin(high).cwiseMax(low));
}

template<typename Derived>
inline typename Derived::PlainObject clip(const MatrixBase<Derived>& a, typename Derived::Scalar low, typename Derived::Scalar high)
{
    return typename Derived::PlainObject(a.cwiseMin(high).cwiseMax(low));
}

template<typename T>
inline T clip(const T& a, const T& low, const T& high)
{
    return a < low ? low : (a > high ? high : a);
}

class VecUtils
{
public:
	// transforms a 3D point using a matrix, returning a 3D point
	static Vector3f transformPoint(const Matrix4f& mat, const Vector3f& point)
	{
		return (mat * Vector4f{ point(0), point(1), point(2), 1.0f }).head(3);
	}

	// transform a 3D direction using a matrix, returning a direction
	static Vector3f transformDirection(const Matrix4f& mat, const Vector3f& dir)
	{
		return mat.block(0, 0, 3, 3) * dir;
	}
};
