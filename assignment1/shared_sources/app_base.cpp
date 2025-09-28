#include <filesystem>
#include <string>
#include <fstream>
#include <iostream>
#include <regex>

#include <fmt/core.h>

#include <Eigen/Dense>              // Linear algebra

using namespace Eigen;
using namespace std;

#include "app_base.h"
#include "Utils.h"
#include "ShaderProgram.h"

#ifndef CS3100_TTF_PATH
#define CS3100_TTF_PATH "roboto_mono.ttf"
#endif

//------------------------------------------------------------------------
// Static data members

AppBase* AppBase::static_instance = 0; // The single instance we allow of App

GLFWkeyfun          AppBase::default_key_callback = nullptr;
GLFWmousebuttonfun  AppBase::default_mouse_button_callback = nullptr;
GLFWcursorposfun    AppBase::default_cursor_pos_callback = nullptr;
GLFWdropfun         AppBase::default_drop_callback = nullptr;
GLFWscrollfun       AppBase::default_scroll_callback = nullptr;
void AppBase::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Always forward events to ImGui
    default_scroll_callback(window, xoffset, yoffset);

    // Already handled?
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    static_instance->handleScroll(window, xoffset, yoffset);
}


unique_ptr<ShaderProgram> AppBase::compileAndLinkShaders(
    const string& vs,
    const string& ps,
    vector<string>& vecErrors,
    map<string, GLuint>& vertexInputMapping)
{
    vecErrors.clear();

    auto shader = unique_ptr<ShaderProgram>(nullptr);
    try
    {
        shader = make_unique<ShaderProgram>(vs, ps);
    }
    catch (ShaderProgram::ShaderCompilationException& e)
    {
        char* msg = const_cast<char*>(e.msg_.c_str());
        char* line = strtok(msg, "\n");;
        while (line)
        {
            vecErrors.push_back("Shader compilation or linking failed!");
            vecErrors.push_back(string(line));
            line = strtok(nullptr, "\n");
        }
        return shader;
    }

    auto shader_id = shader->getHandle();

    // enumerate and print uniforms used by the compiled shader (for debugging)
    GLint numUniforms;
    glGetProgramiv(shader_id, GL_ACTIVE_UNIFORMS, &numUniforms);
    for (GLint i = 0; i < numUniforms; ++i) {
        char name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        glGetActiveUniform(shader_id, i, sizeof(name), &length, &size, &type, name);
        string typestr = GetGLTypeString(type);
        GLint location = glGetUniformLocation(shader_id, name);

        // Print or process uniform information
        cout << fmt::format("Uniform #{}: name = {}, type = {} ({}), size = {}, location = {}\n", i, name, typestr, type, size, location);
    }

    // do the same for input attributes
    GLint numAttributes;
    glGetProgramiv(shader_id, GL_ACTIVE_ATTRIBUTES, &numAttributes);
    for (GLint i = 0; i < numAttributes; ++i) {
        char name[256];
        GLsizei length;
        GLint size;
        GLenum type;
        glGetActiveAttrib(shader_id, i, sizeof(name), &length, &size, &type, name);
        string typestr = GetGLTypeString(type);
        // Get the location of the attribute
        GLint location = glGetAttribLocation(shader_id, name);

        vertexInputMapping[name] = location;

        // Print or process attribute information
        cout << fmt::format("Attribute #{}: name = {}, type = {} ({}), size = {}, location = {}\n", i, name, typestr, type, size, location);
    }

    return shader;
}

shared_ptr<Image4u8> AppBase::takeScreenShot() const
{
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    ImageBase<Vector3<uint8_t>> pixels(Vector2i(width, height), { 0, 0, 0 });
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    // OpenGL returns scanlines bottom-to-top => reverse
    shared_ptr<Image4u8> tmp = make_shared<Image4u8>(Vector2i(width, height), Vector4u8{ 0, 0, 0, 255 });
    for (int r = 0; r < height; ++r)
        for (int c = 0; c < width; ++c)
            tmp->pixel(c, height - r - 1).block(0, 0, 3, 1) = pixels.pixel(c, r);
    return tmp;
}


//------------------------------------------------------------------------
// Font management stuff

void AppBase::loadFont(const char* name, float sizePixels)
{
    string fontPathRel = fmt::format("assets/fonts/{}", name); // relative path: only ascii

    string fontPathAbs = (filesystem::current_path() / fontPathRel).string();
    if (!filesystem::exists(fontPathAbs)) {
        fail(fmt::format("Error: Could not open font file \"{}\"\n", fontPathAbs));
    }

    m_io->Fonts->Clear();
    m_font = m_io->Fonts->AddFontFromFileTTF(fontPathRel.c_str(), sizePixels); // imgui doesn't like non-ascii
    assert(m_font != nullptr);
}

//------------------------------------------------------------------------

void AppBase::increaseUIScale()
{
    setUIScale(m_ui_scale * 1.1);
}

//------------------------------------------------------------------------

void AppBase::decreaseUIScale()
{
    setUIScale(m_ui_scale / 1.1);
}

//------------------------------------------------------------------------

void AppBase::setUIScale(float scale)
{
    m_ui_scale = scale;

    // ScaleAllSizes affects too manty things, e.g. paddings and spacings
    //ImGui::GetStyle().ScaleAllSizes(ui_scale_);

    // io.FontGlobalScale just scales previously generated textures
    loadFont(CS3100_TTF_PATH, 14.0f * m_ui_scale);
    m_font_atlas_dirty = true;
}

//------------------------------------------------------------------------
// Static function, does not have access to member functions except through static_instance_-> ...

void AppBase::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // First check for exit
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    // Always forward events to ImGui
    default_key_callback(window, key, scancode, action, mods);

    // Already handled?
    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    static_instance->handleKeypress(window, key, scancode, action, mods);
}

//------------------------------------------------------------------------
// Static function, does not have access to member functions except through static_instance_-> ...

void AppBase::mousebutton_callback(GLFWwindow* window, int button, int action, int mods)
{
    // Always forward events to ImGui
    default_mouse_button_callback(window, button, action, mods);

    // Already handled?
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    static_instance->handleMouseButton(window, button, action, mods);
}

//------------------------------------------------------------------------
// Static function, does not have access to member functions except through static_instance_-> ...

void AppBase::cursorpos_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Always forward events to ImGui
    default_cursor_pos_callback(window, xpos, ypos);

    // Already handled?
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    static_instance->handleMouseMovement(window, xpos, ypos);
}

//------------------------------------------------------------------------
// Static function, does not have access to member functions except through static_instance_-> ...

void AppBase::drop_callback(GLFWwindow* window, int count, const char** paths)
{
    static_instance->handleDrop(window, count, paths);
}

//------------------------------------------------------------------------

void AppBase::error_callback(int error, const char* description)
{
    fail(fmt::format("Error {}: {}\n", error, description));
}

//------------------------------------------------------------------------

void AppBase::gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
    if (type == GL_DEBUG_TYPE_ERROR) {
        cerr << fmt::format("OpenGL error {}: {}\n", id, message);
#ifdef WIN32
        _CrtDbgBreak();
#endif
    }
}

//------------------------------------------------------------------------

filesystem::path AppBase::absoluteToCwdRelativePath(const filesystem::path& p)
{
    auto relpath = filesystem::path(p).lexically_relative(filesystem::current_path());
    return relpath;
}
