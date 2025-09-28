#pragma once

// Include libraries
#include "glad/gl_core_33.h"                // OpenGL
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>             // Window manager
#include <imgui.h>                  // GUI Library
#include <imgui_impl_glfw.h>
#include "imgui_impl_opengl3.h"

#include <filesystem>
#include <map>
#include <vector>
#include <string>

#include "image.h"

using std::string;
namespace filesystem = std::filesystem;

class ShaderProgram;
class AppState;

class AppBase
{
public:
    virtual void        run(std::filesystem::path savePNGAndTerminate = "") {};

protected:
    // These need to be public, as they're assigned to the GLFW
    static void         key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void         mousebutton_callback(GLFWwindow* window, int button, int action, int mods);
    static void         cursorpos_callback(GLFWwindow* window, double xpos, double ypos);
    static void			scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void         drop_callback(GLFWwindow* window, int count, const char** paths);
    static void         error_callback(int error, const char* description);

    // global instance of the App class (Singleton design pattern)
    // Needed for the GLFW key and mouse callbacks
    static AppBase*     static_instance;

protected:

    GLFWwindow*         m_window = nullptr;
    ImGuiIO*            m_io = nullptr;
    ImFont*             m_font = nullptr; // updated when size changes

    // Shader loading
    // All functions will start by clearing vecErrors and add any errors that occur to it before returning.
    std::unique_ptr<ShaderProgram>   compileAndLinkShaders(const string& vs,
                                                           const string& ps,
                                                           std::vector<string>& vecErrors,
                                                           std::map<string, GLuint>& vertexInputMapping);

    // Grabs pixels from current back buffer
    std::shared_ptr<Image4u8>        takeScreenShot() const;

    // Controls font and UI element scaling
    void                increaseUIScale();
    void                decreaseUIScale();
    void                setUIScale(float scale);
    void                loadFont(const char* name, float sizePixels);

    float               m_ui_scale = 1.0f;
    bool                m_font_atlas_dirty = false;

    // Takes an absolute path, return a version relative to current working directory
    static std::filesystem::path     absoluteToCwdRelativePath(const std::filesystem::path&);

    // These are overridden as necessary in the derived class for each Assignment
    virtual void        handleKeypress(GLFWwindow* window, int key, int scancode, int action, int mods) {}
    virtual void        handleMouseButton(GLFWwindow* window, int button, int action, int mods) {}
    virtual void        handleMouseMovement(GLFWwindow* window, double xpos, double ypos) {}
    virtual void        handleScroll(GLFWwindow* window, double xoffset, double yoffset) {}
    virtual void        handleDrop(GLFWwindow* window, int count, const char** paths) {}

    static GLFWkeyfun          default_key_callback;
    static GLFWmousebuttonfun  default_mouse_button_callback;
    static GLFWcursorposfun    default_cursor_pos_callback;
    static GLFWcursorposfun    default_scroll_callback;
    static GLFWdropfun         default_drop_callback;

    static void gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};

