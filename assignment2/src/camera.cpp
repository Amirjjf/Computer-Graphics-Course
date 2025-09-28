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

using namespace std;        // enables writing "string" instead of std::string, etc.
using namespace Eigen;      // enables writing "Vector3f" instead of "Eigen::Vector3f", etc.

#include "camera.h"

Camera::Camera()
{
    mStartRot = Matrix4f::Identity();
    mCurrentRot = Matrix4f::Identity();
}

void Camera::SetDimensions(int w, int h)
{
    mDimensions[0] = w;
    mDimensions[1] = h;
}

void Camera::SetPerspective(float fovy)
{
    mPerspective[0] = fovy;
}

void Camera::SetViewport(int x, int y, int w, int h)
{
    mViewport[0] = x;
    mViewport[1] = y;
    mViewport[2] = w;
    mViewport[3] = h;
    mPerspective[1] = float( w ) / h;
}

void Camera::SetCenter(const Vector3f& center)
{
    mStartCenter = mCurrentCenter = center;
}

void Camera::SetRotation(const Matrix4f& rotation)
{
    mStartRot = mCurrentRot = rotation;
}

void Camera::SetDistance(const float distance)
{
    mStartDistance = mCurrentDistance = distance;
}

void Camera::MouseClick(int button, int x, int y)
{
    mStartClick[0] = x;
    mStartClick[1] = y;

    mButtonState = button;
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        mCurrentRot = mStartRot;
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        mCurrentCenter = mStartCenter;
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        mCurrentDistance = mStartDistance;
        break;        
    default:
        break;
    }
}

void Camera::MouseDrag(int x, int y)
{
    switch (mButtonState)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        ArcBallRotation(x,y);
        break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        PlaneTranslation(x,y);
        break;
    case GLFW_MOUSE_BUTTON_RIGHT:
        DistanceZoom(x,y);
        break;
    default:
        break;
    }
}


void Camera::MouseRelease(int x, int y)
{
	(void)x; (void)y;
    mStartRot = mCurrentRot;
    mStartCenter = mCurrentCenter;
    mStartDistance = mCurrentDistance;
    
    mButtonState = -1;
}


void Camera::ArcBallRotation(int x, int y)
{
    float sx, sy, sz, ex, ey, ez;
    float scale;
    float sl, el;
    float dotprod;
    
    // find vectors from center of window
    sx = mStartClick[0] - ( mDimensions[0] / 2.f );
    sy = mStartClick[1] - ( mDimensions[1] / 2.f );
    ex = x - ( mDimensions[0] / 2.f );
    ey = y - ( mDimensions[1] / 2.f );
    
    // invert y coordinates (raster versus device coordinates)
    sy = -sy;
    ey = -ey;
    
    // scale by inverse of size of window and magical sqrt2 factor
    if (mDimensions[0] > mDimensions[1]) {
        scale = (float) mDimensions[1];
    } else {
        scale = (float) mDimensions[0];
    }

    scale = 1.f / scale;
    
    sx *= scale;
    sy *= scale;
    ex *= scale;
    ey *= scale;

    // project points to unit circle
    sl = hypot(sx, sy);
    el = hypot(ex, ey);
    
    if (sl > 1.f) {
        sx /= sl;
        sy /= sl;
        sl = 1.0;
    }
    if (el > 1.f) {
        ex /= el;
        ey /= el;
        el = 1.f;
    }
    
    // project up to unit sphere - find Z coordinate
    sz = sqrtf(1.0f - sl * sl);
    ez = sqrtf(1.0f - el * el);
    
    // rotate (sx,sy,sz) into (ex,ey,ez)
    
    // compute angle from dot-product of unit vectors (and double it).
    // compute axis from cross product.
    dotprod = sx * ex + sy * ey + sz * ez;

    if( dotprod != 1 )
    {
        Vector3f axis( sy * ez - ey * sz, sz * ex - ez * sx, sx * ey - ex * sy );
        axis.normalize();
        
        float angle = 2.0f * acosf( dotprod );

        //mCurrentRot = rotation4f( axis, angle );
        Matrix3f R = Matrix3f(AngleAxis<float>(angle, axis));
        mCurrentRot = Matrix4f::Identity();
        mCurrentRot.block(0, 0, 3, 3) = R;
        mCurrentRot = mCurrentRot * mStartRot;
    }
    else
    {
        mCurrentRot = mStartRot;
    }


}

void Camera::PlaneTranslation(int x, int y)
{
    // map window x,y into viewport x,y

    // start
    int sx = mStartClick[0] - mViewport[0];
    int sy = mStartClick[1] - mViewport[1];

    // current
    int cx = x - mViewport[0];
    int cy = y - mViewport[1];


    // compute "distance" of image plane (wrt projection matrix)
    float d = float(mViewport[3])/2.0f / tanf(mPerspective[0]*EIGEN_PI / 180.0f / 2.0f);

    // compute up plane intersect of clickpoint (wrt fovy)
    float su = -sy + mViewport[3]/2.0f;
    float cu = -cy + mViewport[3]/2.0f;

    // compute right plane intersect of clickpoint (ASSUMED FOVY is 1)
    float sr = (sx - mViewport[2]/2.0f);
    float cr = (cx - mViewport[2]/2.0f);

    Vector2f move(cr-sr, cu-su);

    // this maps move
    move *= -mCurrentDistance/d;

    mCurrentCenter = mStartCenter +
        + move[0] * Vector3f(mCurrentRot(0,0),mCurrentRot(0,1),mCurrentRot(0,2))
        + move[1] * Vector3f(mCurrentRot(1,0),mCurrentRot(1,1),mCurrentRot(1,2));

}

void Camera::ApplyViewport() const
{
    glViewport(mViewport[0],mViewport[1],mViewport[2],mViewport[3]);
}

Matrix4f Camera::GetPerspective() const
{
    float f = float(1.0 / tan((mPerspective[0]*EIGEN_PI)/180.0f / 2.0f)); // 1/tangent = cotangent, use double on purpose inside and convert end result to float
    float aspect = mPerspective[1];
    float zfar = 50.0f;
    float znear = 0.01f;

    Matrix4f P{
        { f / aspect, 0.0f,                      0.0f,                             0.0f },
        {       0.0f,    f,                      0.0f,                             0.0f },
        {       0.0f, 0.0f, (zfar+znear)/(znear-zfar), 2.0f*zfar*znear / (znear - zfar) },
        {       0.0f, 0.0f,                     -1.0f,                             0.0f }
    };

    return P;
    //glMultMatrixf(P.data());
    //gluPerspective(mPerspective[0], mPerspective[1], .01, 1000.0);
}


Matrix4f Camera::GetModelview() const
{
    //// back up distance
    //gluLookAt(0,0,mCurrentDistance,
    //          0,0,0,
    //          0.0, 1.0, 0.0);
    Matrix4f T(Matrix4f::Identity());
    T(2, 3) = -mCurrentDistance;

    Matrix4f C(Matrix4f::Identity());
    C.block(0, 3, 3, 1) = -mCurrentCenter;

    //return C * mCurrentRot * T;
    return T * mCurrentRot * C;
    // rotate object
    //glMultMatrixf(mCurrentRot.data());

    //translate object to center
    //glTranslatef(-mCurrentCenter[0],-mCurrentCenter[1],-mCurrentCenter[2]);    
}

void Camera::DistanceZoom(int x, int y)
{
	(void)x;
    int sy = mStartClick[1] - mViewport[1];
    int cy = y - mViewport[1];

    float delta = float(cy-sy)/mViewport[3];

    // exponential zoom factor
    mCurrentDistance = mStartDistance * expf(delta);  
}

#include "eigen_json_serializers.h"

void Camera::get_json(nlohmann::json& j) const {
    j["rotation"] = mCurrentRot;
    j["center"] = mCurrentCenter;
    j["zoom_distance"] = mCurrentDistance;
}
void Camera::set_json(const nlohmann::json& j) {
    mStartRot = mCurrentRot = j["rotation"];
    mStartCenter = mCurrentCenter = j["center"];
    mStartDistance = mCurrentDistance = j["zoom_distance"];
}
