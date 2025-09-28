/*	OpenGL 3.3 example
    This is a standard 'modern' example which implements line expansion via a geometry shader.
    See examples/OpenGL31 for a reference which doesn't require geometry shaders.
*/
//#include "im3d_example.h"
// Include libraries
#include "glad/gl_core_33.h"                // OpenGL
#include "im3d.h"
#include <fmt/core.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>             // Window manager

#include <Eigen/Dense>              // Linear algebra
#include <Eigen/Geometry>

#ifndef CS3100_IM3D_GLSL
#define CS3100_IM3D_GLSL "assets/shaders/im3d.glsl"
#endif /* CS3100_IM3D_GLSL */

using namespace Im3d;
using namespace Eigen;

#include "Utils.h"

static GLuint g_Im3dVertexArray;
static GLuint g_Im3dVertexBuffer;
static GLuint g_Im3dShaderPoints;
static GLuint g_Im3dShaderLines;
static GLuint g_Im3dShaderTriangles;
static AppData g_appData;
static Matrix4f g_modelToClip = Matrix4f::Zero();


namespace Im3d
{
    bool LinkShaderProgram(GLuint _handle);
    GLuint LoadCompileShader(GLenum _stage, const char* _path, const char* _defines);
    const char* GetGlEnumString(GLenum _enum);
    const char* GlGetString(GLenum _name);

#define glAssert(call) \
        do { \
            (call); \
            GLenum err = glGetError(); \
            if (err != GL_NO_ERROR) { \
                fail(fmt::format("glAssert failed: {}, {}, {}, {}", #call, __FILE__, __LINE__, Im3d::GetGlEnumString(err))); \
            } \
        } while (0)
} // namespace Im3d


// Resource init/shutdown will be app specific. In general you'll need one shader for each of the 3
// draw primitive types (points, lines, triangles), plus some number of vertex buffers.
bool Im3d_Init()
{
    {	GLuint vs = LoadCompileShader(GL_VERTEX_SHADER,   CS3100_IM3D_GLSL, "VERTEX_SHADER\0POINTS\0");
        GLuint fs = LoadCompileShader(GL_FRAGMENT_SHADER, CS3100_IM3D_GLSL, "FRAGMENT_SHADER\0POINTS\0");
        if (vs && fs) {
            glAssert(g_Im3dShaderPoints = glCreateProgram());
            glAssert(glAttachShader(g_Im3dShaderPoints, vs));
            glAssert(glAttachShader(g_Im3dShaderPoints, fs));
            bool ret = LinkShaderProgram(g_Im3dShaderPoints);
            glAssert(glDeleteShader(vs));
            glAssert(glDeleteShader(fs));
            if (!ret)
            {
                return false;
            }			
        }
        else
        {
            return false;
        }
    }

    {	GLuint vs = LoadCompileShader(GL_VERTEX_SHADER,   CS3100_IM3D_GLSL, "VERTEX_SHADER\0LINES\0");
        GLuint gs = LoadCompileShader(GL_GEOMETRY_SHADER, CS3100_IM3D_GLSL, "GEOMETRY_SHADER\0LINES\0");
        GLuint fs = LoadCompileShader(GL_FRAGMENT_SHADER, CS3100_IM3D_GLSL, "FRAGMENT_SHADER\0LINES\0");
        if (vs && gs && fs)
        {
            glAssert(g_Im3dShaderLines = glCreateProgram());
            glAssert(glAttachShader(g_Im3dShaderLines, vs));
            glAssert(glAttachShader(g_Im3dShaderLines, gs));
            glAssert(glAttachShader(g_Im3dShaderLines, fs));
            bool ret = LinkShaderProgram(g_Im3dShaderLines);
            glAssert(glDeleteShader(vs));
            glAssert(glDeleteShader(gs));
            glAssert(glDeleteShader(fs));
            if (!ret)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    {	GLuint vs = LoadCompileShader(GL_VERTEX_SHADER,   CS3100_IM3D_GLSL, "VERTEX_SHADER\0TRIANGLES\0");
        GLuint fs = LoadCompileShader(GL_FRAGMENT_SHADER, CS3100_IM3D_GLSL, "FRAGMENT_SHADER\0TRIANGLES\0");
        if (vs && fs)
        {
            glAssert(g_Im3dShaderTriangles = glCreateProgram());
            glAssert(glAttachShader(g_Im3dShaderTriangles, vs));
            glAssert(glAttachShader(g_Im3dShaderTriangles, fs));
            bool ret = LinkShaderProgram(g_Im3dShaderTriangles);
            glAssert(glDeleteShader(vs));
            glAssert(glDeleteShader(fs));
            if (!ret)
            {
                return false;
            }		
        }
        else
        {
            return false;
        }
    }

    glAssert(glGenBuffers(1, &g_Im3dVertexBuffer));;
    glAssert(glGenVertexArrays(1, &g_Im3dVertexArray));	
    glAssert(glBindVertexArray(g_Im3dVertexArray));
    glAssert(glBindBuffer(GL_ARRAY_BUFFER, g_Im3dVertexBuffer));
    glAssert(glEnableVertexAttribArray(0));
    glAssert(glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Im3d::VertexData), (GLvoid*)offsetof(Im3d::VertexData, m_positionSize)));
    glAssert(glEnableVertexAttribArray(1));
    glAssert(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Im3d::VertexData), (GLvoid*)offsetof(Im3d::VertexData, m_color)));
    glAssert(glBindVertexArray(0));

    return true;
}

void Im3d_Shutdown()
{
    glAssert(glDeleteVertexArrays(1, &g_Im3dVertexArray));
    glAssert(glDeleteBuffers(1, &g_Im3dVertexBuffer));
    glAssert(glDeleteProgram(g_Im3dShaderPoints));
    glAssert(glDeleteProgram(g_Im3dShaderLines));
    glAssert(glDeleteProgram(g_Im3dShaderTriangles));
}

// At the top of each frame, the application must fill the Im3d::AppData struct and then call Im3d::NewFrame().
// The example below shows how to do this, in particular how to generate the 'cursor ray' from a mouse position
// which is necessary for interacting with gizmos.
void Im3d_NewFrame(GLFWwindow* window,
                   int width,
                   int height,
                   const Matrix4f& world_to_view,
                   const Matrix4f& view_to_clip,
                   float dt,
                   float mousex,
                   float mousey)
{
    AppData& ad = GetAppData();
    ad.m_viewportSize.x = width;
    ad.m_viewportSize.y = height;

    g_modelToClip = view_to_clip * world_to_view;

    Matrix4f view_to_world = world_to_view.inverse();
    Vector3f camPos = view_to_world.block(0, 3, 3, 1);
    Vector3f camDir = view_to_world.block(0, 2, 3, 1);

    ad.m_deltaTime     = dt;
    ad.m_viewportSize  = Vec2((float)width, (float)height);
    ad.m_viewOrigin    = Vec3(camPos(0), camPos(1), camPos(2)); // for VR use the head position
    ad.m_viewDirection = Vec3(camDir(0), camDir(1), camDir(2));
    ad.m_worldUp       = Vec3(0.0f, 1.0f, 0.0f); // used internally for generating orthonormal bases

    Matrix4f clip_to_view = view_to_clip.inverse();
    Vector3f o = view_to_world.block(0, 3, 3, 1); // ray origin

    Vector4f clipxy{ 2.0f * float(mousex) / width - 1.0f,
                     -2.0f * float(mousey) / height + 1.0f,
                      1.0f,
                      1.0f };

    Vector4f e4 = view_to_world * clip_to_view * clipxy;    // end point
    Vector3f d = e4({ 0, 1, 2 }) / e4(3);   // project
    d -= o;

    // m_projScaleY controls how gizmos are scaled in world space to maintain a constant screen height
    ad.m_projScaleY = 1.0f; // tanf(camFovRad * 0.5f) * 2.0f;

    // World space cursor ray from mouse position; for VR this might be the position/orientation of the HMD or a tracked controller.
    ad.m_cursorRayOrigin = Vec3(o(0), o(1), o(2));
    ad.m_cursorRayDirection = Vec3(d(0), d(1), d(2));

    // Set cull frustum planes. This is only required if IM3D_CULL_GIZMOS or IM3D_CULL_PRIMTIIVES is enable via
    // im3d_config.h, or if any of the IsVisible() functions are called.
    //ad.setCullFrustum(g_Example->m_camViewProj, true);

    // Fill the key state array; using GetAsyncKeyState here but this could equally well be done via the window proc.
    // All key states have an equivalent (and more descriptive) 'Action_' enum.
    ad.m_keyDown[Im3d::Mouse_Left] = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // The following key states control which gizmo to use for the generic Gizmo() function. Here using the left ctrl
    // key as an additional predicate.
    //bool ctrlDown = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
    bool ctrlDown = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    ad.m_keyDown[Im3d::Key_L/*Action_GizmoLocal*/] = ctrlDown && glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    ad.m_keyDown[Im3d::Key_T/*Action_GizmoTranslation*/] = ctrlDown && glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    ad.m_keyDown[Im3d::Key_R/*Action_GizmoRotation*/]    = ctrlDown && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    ad.m_keyDown[Im3d::Key_S/*Action_GizmoScale*/]       = ctrlDown && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;

 // Enable gizmo snapping by setting the translation/rotation/scale increments to be > 0
    ad.m_snapTranslation = ctrlDown ? 0.5f : 0.0f;
    ad.m_snapRotation    = ctrlDown ? 30.0f*EIGEN_PI/180.0f : 0.0f;
    ad.m_snapScale       = ctrlDown ? 0.5f : 0.0f;

    Im3d::NewFrame();
}

// After all Im3d calls have been made for a frame, the user must call Im3d::EndFrame() to finalize draw data, then
// access the draw lists for rendering. Draw lists are only valid between calls to EndFrame() and NewFrame().
// The example below shows the simplest approach to rendering draw lists; variations on this are possible. See the 
// shader source file for more details.
void Im3d_EndFrame()
{
    Im3d::EndFrame();

    // Primitive rendering.

    glAssert(glEnable(GL_BLEND));
    glAssert(glBlendEquation(GL_FUNC_ADD));
    glAssert(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glAssert(glEnable(GL_PROGRAM_POINT_SIZE));
    //glAssert(glDisable(GL_DEPTH_TEST));
    glAssert(glDisable(GL_CULL_FACE));

    //glAssert(glViewport(0, 0, (GLsizei)g_Example->m_width, (GLsizei)g_Example->m_height));
        
    for (U32 i = 0, n = Im3d::GetDrawListCount(); i < n; ++i)
    {
        const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];
 
        if (drawList.m_layerId == Im3d::MakeId("NamedLayer"))
        {
         // The application may group primitives into layers, which can be used to change the draw state (e.g. enable depth testing, use a different shader)
        }
    
        GLenum prim;
        GLuint sh;
        switch (drawList.m_primType)
        {
            case Im3d::DrawPrimitive_Points:
                prim = GL_POINTS;
                sh = g_Im3dShaderPoints;
                glAssert(glDisable(GL_CULL_FACE)); // points are view-aligned
                break;
            case Im3d::DrawPrimitive_Lines:
                prim = GL_LINES;
                sh = g_Im3dShaderLines;
                glAssert(glDisable(GL_CULL_FACE)); // lines are view-aligned
                break;
            case Im3d::DrawPrimitive_Triangles:
                prim = GL_TRIANGLES;
                sh = g_Im3dShaderTriangles;
                //glAssert(glEnable(GL_CULL_FACE)); // culling valid for triangles, but optional
                break;
            default:
                IM3D_ASSERT(false);
                return;
        };
    
        glAssert(glBindVertexArray(g_Im3dVertexArray));
        glAssert(glBindBuffer(GL_ARRAY_BUFFER, g_Im3dVertexBuffer));
        glAssert(glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)drawList.m_vertexCount * sizeof(Im3d::VertexData), (GLvoid*)drawList.m_vertexData, GL_STREAM_DRAW));
    
        AppData& ad = GetAppData();
        glAssert(glUseProgram(sh));
        glAssert(glUniform2f(glGetUniformLocation(sh, "uViewport"), ad.m_viewportSize.x, ad.m_viewportSize.y));
        glAssert(glUniformMatrix4fv(glGetUniformLocation(sh, "uViewProjMatrix"), 1, false, (const GLfloat*)g_modelToClip.data()));
        glAssert(glDrawArrays(prim, 0, (GLsizei)drawList.m_vertexCount));
    }

    glAssert(glBindVertexArray(0));
    glAssert(glBindBuffer(GL_ARRAY_BUFFER, 0));

    glAssert(glDisable(GL_BLEND));
    glAssert(glDisable(GL_PROGRAM_POINT_SIZE));
    //glAssert(glEnable(GL_DEPTH_TEST));
    glAssert(glEnable(GL_CULL_FACE));

 // Text rendering.
 // This is common to all examples since we're using ImGui to draw the text lists, see im3d_example.cpp.
    //g_Example->drawTextDrawListsImGui(Im3d::GetTextDrawLists(), Im3d::GetTextDrawListCount());
}

namespace Im3d
{
#define IM3D_STRINGIFY_(_t) #_t
#define IM3D_STRINGIFY(_t) IM3D_STRINGIFY_(_t)
#define IM3D_ASSERT_MSG(e, msg, ...) \
    do { \
        if (!(e)) { \
            fail(fmt::format("IM3D_ASSERT failed: {}, {}, {}", #e, __FILE__, __LINE__));\
        } \
    } while (0)

#undef IM3D_ASSERT
#define IM3D_ASSERT(e)                 IM3D_ASSERT_MSG(e, 0, 0)
#define IM3D_VERIFY_MSG(e, msg, ...)   IM3D_ASSERT_MSG(e, msg, ## __VA_ARGS__)
#define IM3D_VERIFY(e)                 IM3D_VERIFY_MSG(e, 0, 0)

    static const char* StripPath(const char* _path)
    {
        int i = 0, last = 0;
        while (_path[i] != '\0') {
            if (_path[i] == '\\' || _path[i] == '/') {
                last = i + 1;
            }
            ++i;
        }
        return &_path[last];
    }

    static bool LoadShader(const char* _path, const char* _defines, string& _out_)
    {
        fprintf(stdout, "Loading shader: '%s'", StripPath(_path));
        if (_defines)
        {
            fprintf(stdout, " ");
            while (*_defines != '\0')
            {
                fprintf(stdout, " %s,", _defines);
                _out_ += "#define ";
                _out_ += string(_defines) + '\n';
                _defines = strchr(_defines, 0);
                IM3D_ASSERT(_defines);
                ++_defines;
            }
        }
        fprintf(stdout, "\n");

        FILE* fin = fopen(_path, "rb");
        if (!fin)
        {
            fprintf(stderr, "Error opening '%s'\n", _path);
            return false;
        }
        IM3D_VERIFY(fseek(fin, 0, SEEK_END) == 0); // not portable but should work almost everywhere
        long fsize = ftell(fin);
        IM3D_VERIFY(fseek(fin, 0, SEEK_SET) == 0);

        int srcbeg = _out_.size();
        _out_.resize(srcbeg + fsize, '\0');
        if (fread(_out_.data() + srcbeg, 1, fsize, fin) != fsize)
        {
            fclose(fin);
            fprintf(stderr, "Error reading '%s'\n", _path);
            return false;
        }
        fclose(fin);

        return true;
    }

GLuint LoadCompileShader(GLenum _stage, const char* _path, const char* _defines)
{
    string src = "#version 330\n";
    if (!LoadShader(_path, _defines, src))
    {
        return 0;
    }

    GLuint ret = 0;
    glAssert(ret = glCreateShader(_stage));
    const GLchar* pd = src.c_str();
    GLint ps = src.size();
    glAssert(glShaderSource(ret, 1, &pd, &ps));

    glAssert(glCompileShader(ret));
    GLint compileStatus = GL_FALSE;
    glAssert(glGetShaderiv(ret, GL_COMPILE_STATUS, &compileStatus));
    if (compileStatus == GL_FALSE)
    {
        fprintf(stderr, "Error compiling '%s':\n\n", _path);
        GLint len;
        glAssert(glGetShaderiv(ret, GL_INFO_LOG_LENGTH, &len));
        char* log = new GLchar[len];
        glAssert(glGetShaderInfoLog(ret, len, 0, log));
        fprintf(stderr, "%s", log);
        delete[] log;

        //fprintf(stderr, "\n\n%s", src.data());
        fprintf(stderr, "\n");
        glAssert(glDeleteShader(ret));

        return 0;
    }

    return ret;
}

bool LinkShaderProgram(GLuint _handle)
{
    IM3D_ASSERT(_handle != 0);

    glAssert(glLinkProgram(_handle));
    GLint linkStatus = GL_FALSE;
    glAssert(glGetProgramiv(_handle, GL_LINK_STATUS, &linkStatus));
    if (linkStatus == GL_FALSE)
    {
        fprintf(stderr, "Error linking program:\n\n");
        GLint len;
        glAssert(glGetProgramiv(_handle, GL_INFO_LOG_LENGTH, &len));
        GLchar* log = new GLchar[len];
        glAssert(glGetProgramInfoLog(_handle, len, 0, log));
        fprintf(stderr, "%s", log);
        fprintf(stderr, "\n");
        delete[] log;

        return false;
    }

    return true;
}

const char* GetGlEnumString(GLenum _enum)
{
#define CASE_ENUM(e) case e: return #e
    switch (_enum)
    {
        // errors
        CASE_ENUM(GL_NONE);
        CASE_ENUM(GL_INVALID_ENUM);
        CASE_ENUM(GL_INVALID_VALUE);
        CASE_ENUM(GL_INVALID_OPERATION);
        CASE_ENUM(GL_INVALID_FRAMEBUFFER_OPERATION);
        CASE_ENUM(GL_OUT_OF_MEMORY);

    default: return "Unknown GLenum";
    };
#undef CASE_ENUM
}

const char* GlGetString(GLenum _name)
{
    const char* ret;
    glAssert(ret = (const char*)glGetString(_name));
    return ret ? ret : "";
}


} // namespace Im3d
