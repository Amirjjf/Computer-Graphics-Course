
#include "app.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <regex>
#include <fmt/core.h>
#include <cmath>
#include <unordered_map>

//------------------------------------------------------------------------

using namespace std;

// Vertex data for a quadrilateral reference plane at y = -1, with normals pointing up.
const App::Vertex reference_plane_data[] = {
    { Vector3f(-1, -1, -1), Vector3f(0, 1, 0) },
    { Vector3f(1, -1, -1), Vector3f(0, 1, 0) },
    { Vector3f(1, -1,  1), Vector3f(0, 1, 0) },
    { Vector3f(-1, -1, -1), Vector3f(0, 1, 0) },
    { Vector3f(1, -1,  1), Vector3f(0, 1, 0) },
    { Vector3f(-1, -1,  1), Vector3f(0, 1, 0) }
};

//------------------------------------------------------------------------

App::App(void)
{ 
    if (static_instance != 0)
        fail("Attempting to create a second instance of App!");

    static_instance = this;

    static_assert(is_standard_layout<Vertex>::value, "struct Vertex must be standard layout to use offsetof");
}

//------------------------------------------------------------------------

App::~App()
{
    static_instance = 0;
}

//------------------------------------------------------------------------

void App::run(const filesystem::path savePNGAndTerminate)
{
#ifndef SOLUTION
    // Warn about cwd problems
    filesystem::path cwd = filesystem::current_path();
    cerr << "Current working directory is " << cwd << endl;
    if (!filesystem::is_directory(cwd/"assets")) {
        cerr << fmt::format("Current working directory \"{}\" does not contain an \"assets\" folder.\n", cwd.string())
             << "Make sure the executable gets run relative to the project root.\n";
    }
#endif

    // Initialize GLFW
    if (!glfwInit()) {
        fail("glfwInit() failed");
    }

    glfwSetErrorCallback(error_callback);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_REFRESH_RATE, 0);

    int window_width = 1920;
    int window_height = 1080;

#ifdef __APPLE__
    // On macOS, adjust for monitor content scale to handle Retina displays properly
    // when we're saving the screenshot. Otherwise our frame buffer will not actually be 1920x1080.
    if (!savePNGAndTerminate.empty()) {
        GLFWmonitor* primary_monitor = glfwGetPrimaryMonitor();
        if (primary_monitor) {
            float xscale, yscale;
            glfwGetMonitorContentScale(primary_monitor, &xscale, &yscale);
            window_width = int(window_width / xscale);
            window_height = int(window_height / yscale);
        }
    }
#endif

    // Create a windowed mode window and its OpenGL context
    m_window = glfwCreateWindow(window_width, window_height, "Aalto CS-C3100 Computer Graphics, Fall 2025, Assignment 1", NULL, NULL);
    if (!m_window) {
        glfwTerminate();
        fail("glfwCreateWindow() failed");
    }

    // Make the window's context current
    glfwMakeContextCurrent(m_window);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(0);

    if (glfwExtensionSupported("GL_KHR_debug")) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(gl_debug_callback, nullptr);
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_io = &ImGui::GetIO();
    //m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //m_io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    m_io->IniFilename = nullptr;                                 // Disable generation of imgui.ini for consistent behavior

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Keep track of previous callbacks for chaining
    default_key_callback = glfwSetKeyCallback(m_window, App::key_callback);
    default_mouse_button_callback = glfwSetMouseButtonCallback(m_window, App::mousebutton_callback);
    default_cursor_pos_callback = glfwSetCursorPosCallback(m_window, App::cursorpos_callback);
    default_drop_callback = glfwSetDropCallback(m_window, App::drop_callback);
    default_scroll_callback = glfwSetScrollCallback(m_window, App::scroll_callback);

    // generate vertex buffer objects, load shaders, etc.
    initRendering();

    // also loads corresponding font
    setUIScale(1.5f);

    vector<string> vecStatusMessages;

    unsigned int uFrameNumber = 0;

    // MAIN LOOP
    while (!glfwWindowShouldClose(m_window))
    {
        // This vector holds the strings that are printed out.
        // It's also passed to render(...) so you can output
        // your own debug information from there if you like.
        vecStatusMessages.clear();
        vecStatusMessages.push_back("Use arrow keys, PgUp/PgDn to move the model (R1), Home/End to rotate camera.");

        glfwPollEvents();

        // Rebuild font atlas if necessary
        if (m_font_atlas_dirty)
        {
            m_io->Fonts->Build();
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            ImGui_ImplOpenGL3_CreateFontsTexture();
            m_font_atlas_dirty = false;
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        // Optional animation: update camera yaw if rotating
        if (m_state.is_rotating)
        {
            float dt = m_timer.end(); // seconds since last frame
            m_state.camera_rotation_angle += dt * (EIGEN_PI / 6.0f); // ~30 deg/sec
        }

        // First, render our own 3D scene using OpenGL
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        render(m_state, width, height, vecStatusMessages);

        // check if we are to grab the pixels to a file and terminate
        if (!savePNGAndTerminate.empty())
        {
            auto tmp = takeScreenShot();
            tmp->exportPNG(savePNGAndTerminate);
            break;
        }

        // Begin GUI window
        ImGui::Begin("Controls");
        // Model switching UI buttons
        if (ImGui::Button("Load Triangle Model"))
            m_state.scene_mode = "triangle";
        ImGui::SameLine(m_ui_scale * 150.f);
        if (ImGui::Button("Load Indexed Model"))
            m_state.scene_mode = "tetrahedron";
        if (ImGui::Button("Load Generated Cone"))
            m_state.scene_mode = "cone";
        ImGui::SameLine(m_ui_scale * 150.f);
        if (ImGui::Button("Load OBJ model (L)"))
            showObjLoadDialog();
        ImGui::SameLine();
        if (ImGui::Button("Load PLY model"))
            showPlyLoadDialog();
        if (ImGui::Button("Reload shaders"))
            initRendering();
        if (ImGui::Button("Take screenshot"))
        {
            auto tmp = takeScreenShot();
            auto png_path = filesystem::current_path() / "debug.png";
            tmp->exportPNG(png_path);
            cerr << "Wrote screenshot to " << png_path << endl;
        }
        ImGui::Checkbox("Fancy shading (S)", &(bool&)m_state.shading_toggle);
    ImGui::SliderFloat("FOV X (deg)", &m_state.fovx_degrees, 10.0f, 170.0f);

        // Simplification UI
        size_t triCount = m_indexed_mesh.triangles.size();
        if (m_simplify_target == 0) m_simplify_target = triCount;
        ImGui::Separator();
        ImGui::Text("Triangles: %zu", triCount);
        int target = (int)m_simplify_target;
        int minT = triCount > 10 ? 10 : (int)triCount;
        if (triCount > 0) {
            ImGui::SliderInt("Target triangles", &target, minT, (int)triCount);
            m_simplify_target = (size_t)std::max(minT, target);
            if (ImGui::Button("Simplify (QEM)")) {
                auto out = simplify::simplifyQEM(m_indexed_mesh, m_simplify_target);
                setMeshFromIndexed(out);
            }
        }

        ImGui::Text("Use function keys F1..F12 to load pre-saved states,");
        ImGui::Text("    Shift-F1..F12 for saving state snapshots, and");
        ImGui::Text("    Ctrl-F1..F12 for loading reference states.");

        // Draw the status messages in the current list
        // Note that you can add them from inside render(...)
        vecStatusMessages.push_back(fmt::format("Application average {:.3f} ms/frame ({:.1f} FPS)", 1000.0f / m_io->Framerate, m_io->Framerate));
        for (const string& msg : vecStatusMessages)
            ImGui::Text("%s", msg.c_str());

        // end the GUI window
        ImGui::End();

        // This creates the command buffer that is rendered below after our own drawing stuff
        ImGui::Render();

        // ..and this actually draws it using OpenGL
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // This shows the image that was just rendered in the window.
        glfwSwapBuffers(m_window);

        // make sure keyboard input goes to the main window at startup
        if ( uFrameNumber == 0 )
            ImGui::SetWindowFocus(nullptr);

        ++uFrameNumber;
    }

    // Cleanup
    m_shader_program.reset(nullptr);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

//------------------------------------------------------------------------

void App::render(const AppState& state, int window_width, int window_height, vector<string>& vecStatusMessages) const
{
    // handle scene change
    if (m_current_scene_mode != state.scene_mode)
    {
        if ( state.scene_mode == "triangle" )
            setMeshFromFlat(generateSingleTriangleMesh());
        else if (state.scene_mode == "tetrahedron" )
            setMeshFromFlat(generateIndexedTetrahedronMesh());
        else if (state.scene_mode == "cone")
            setMeshFromFlat(generateConeMesh());
        else if (state.scene_mode.substr(0,4) == "obj(")
        {
            string filename = state.scene_mode.substr(4, state.scene_mode.length() - 5);
            setMeshFromFlat(loadObjFile(filename));
        }
        else if (state.scene_mode.substr(0,4) == "ply(")
        {
            string filename = state.scene_mode.substr(4, state.scene_mode.length() - 5);
            setMeshFromFlat(loadPlyFile(filename));
        }
        m_current_scene_mode = state.scene_mode;
    }

    vecStatusMessages.push_back(fmt::format("Current scene: {}", m_current_scene_mode));

    // Clear screen.
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Enable depth testing.
    glEnable(GL_DEPTH_TEST);

    // Tell OpenGL the size of the buffer we're rendering into.
    glViewport(0, 0, window_width, window_height);

    // Set up a matrix to transform from world space to clip space.
    // Clip space is a [-1, 1]^3 space where OpenGL expects things to be
    // when it starts drawing them.
    // We piece the transformation together by first constructing
    // a mapping from the camera to world space, inverting it to get
    // a world-to-camera mapping and following that by the camera-to-clip projection.

    // Trackball camera centered at camera target; combine HOME/END yaw with trackball rotation
    Matrix3f camR = Matrix3f(AngleAxis<float>(-state.camera_rotation_angle, Vector3f(0, 1, 0))) * state.trackball_current_rotation;
    Matrix4f camera_to_world(Matrix4f::Identity());
    camera_to_world.block(0, 0, 3, 3) = camR.transpose(); // basis vectors as columns
    // Camera looks at target from distance along local -Z (do not follow model position)
    Vector3f camPos = camR.transpose() * Vector3f(0.0f, 0.0f, -state.camera_distance) + state.camera_target;
    camera_to_world.block(0, 3, 3, 1) = camPos;

    // Perspective from FOV X; adjust Y to match aspect
    float aspect = float(window_width) / float(window_height);
    static const float fnear = 0.1f, ffar = 4.0f;

    // Construct projection matrix (mapping from camera to clip space).
    Matrix4f camera_to_clip(Matrix4f::Identity());
    // Given horizontal FOV (deg), compute focal lengths
    float fovx_rad = std::clamp(m_state.fovx_degrees, 10.0f, 170.0f) * (EIGEN_PI / 180.0f);
    float fx = 1.0f / std::tan(fovx_rad * 0.5f);
    float fy = fx * aspect; // match vertical to preserve aspect
    camera_to_clip(0, 0) = fx;
    camera_to_clip(1, 1) = fy;
    camera_to_clip.col(2) = Vector4f(0, 0, (ffar + fnear) / (ffar - fnear), 1);
    camera_to_clip.col(3) = Vector4f(0, 0, -2 * ffar * fnear / (ffar - fnear), 0);

    Matrix4f world_to_clip = camera_to_clip * camera_to_world.inverse();

    // Set active shader program.
    m_shader_program->use();
    //glUniform1i(m_gl.shading_toggle_uniform, shading_toggle_);
    m_shader_program->setUniform("bShading", state.shading_toggle ? 1 : 0);
    // Pass transforms and camera/time uniforms for shading
    m_shader_program->setUniform("uWorldToClip", world_to_clip);
    m_shader_program->setUniform("uTime", (float)glfwGetTime());
    m_shader_program->setUniform("uCameraPos", camPos);

    // Draw the reference plane. It is already in world coordinates.
    Matrix4f identity = Matrix4f::Identity();
    //glUniformMatrix4fv(m_gl.world_to_view_uniform, 1, GL_FALSE, identity.data());
    m_shader_program->setUniform("uModelToWorld", identity);
    Matrix3f identity3 = Matrix3f::Identity();
    m_shader_program->setUniform("uNormalMatrix", identity3);
    glBindVertexArray(m_gl.static_vao);
    glDrawArrays(GL_TRIANGLES, 0, SIZEOF_ARRAY(reference_plane_data));

    // YOUR CODE HERE (R1)
    // Set the model space -> world space transform to translate the model according to user input.
    // Compose model transform: Model -> World = Translation * RotationY * Scale (non-uniform)
    Matrix4f T = Matrix4f::Identity();
    T.block(0, 3, 3, 1) = state.model_translation;

    Matrix3f Ry3 = Matrix3f(AngleAxis<float>(state.model_rotation_angle_y, Vector3f(0, 1, 0)));
    Matrix4f R = Matrix4f::Identity();
    R.block(0, 0, 3, 3) = Ry3;

    Matrix4f S = Matrix4f::Identity();
    S(0,0) = state.model_scale.x();
    S(1,1) = state.model_scale.y();
    S(2,2) = state.model_scale.z();

    Matrix4f modelToWorld = T * R * S;

    // Draw the model with your model-to-world transformation.
    //glUniformMatrix4fv(m_gl.world_to_view_uniform, 1, GL_FALSE, modelToWorld.data());
    m_shader_program->setUniform("uModelToWorld", modelToWorld);
    // Normal matrix = inverse transpose of upper-left 3x3 of modelToWorld
    Matrix3f normalMat = modelToWorld.block<3,3>(0,0).inverse().transpose();
    m_shader_program->setUniform("uNormalMatrix", normalMat);
    glBindVertexArray(m_gl.dynamic_vao);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertex_count);

    // Undo our bindings.
    glBindVertexArray(0);
    glUseProgram(0);

    // Show status messages. You may find it useful to show some debug information in a message.
    Vector3f camPosDbg = camera_to_world.block(0, 3, 3, 1);
    vecStatusMessages.push_back(fmt::format("Camera is at ({:.2f} {:.2f} {:.2f}) targeting model.",
        camPosDbg(0), camPosDbg(1), camPosDbg(2)));
}

//------------------------------------------------------------------------

void App::handleKeypress(GLFWwindow * window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT )
    {
        // YOUR CODE HERE (R1)
        // React to user input and move the model.
        // See https://www.glfw.org/docs/3.4/group__keys.html for more key codes.
        // Visual Studio tip: you can right-click an identifier like GLFW_KEY_HOME
        // and "Go to definition" to jump directly to where the identifier is defined.
    const float step = 0.03f; // match reference.exe movement increment
    const float rot_step = 0.05f * EIGEN_PI; // ~9 degrees per press
    const float scale_step = 0.05f; // 5% per press
        // Arrow keys move in XZ-plane. Convention: LEFT:-x, RIGHT:+x, PAGE_UP:-z (towards camera), PAGE_DOWN:+z.
        if (key == GLFW_KEY_LEFT)
            m_state.model_translation.x() -= step;
        else if (key == GLFW_KEY_RIGHT)
            m_state.model_translation.x() += step;
        else if (key == GLFW_KEY_PAGE_UP)
            m_state.model_translation.z() -= step;
        else if (key == GLFW_KEY_PAGE_DOWN)
            m_state.model_translation.z() += step;
        else if (key == GLFW_KEY_UP)
            m_state.model_translation.y() += step; 
        else if (key == GLFW_KEY_DOWN )
            m_state.model_translation.y() -= step; 
        else if (key == GLFW_KEY_Q)
            m_state.model_rotation_angle_y -= rot_step; // rotate -Y
        else if (key == GLFW_KEY_E)
            m_state.model_rotation_angle_y += rot_step; // rotate +Y
        else if (key == GLFW_KEY_Z)
        {
            m_state.model_scale.x() = max(0.01f, m_state.model_scale.x() - scale_step);
        }
        else if (key == GLFW_KEY_X)
        {
            m_state.model_scale.x() += scale_step;
        }
        else if (key == GLFW_KEY_O)
            decreaseUIScale();
        else if (key == GLFW_KEY_P)
            increaseUIScale();
        else if (key == GLFW_KEY_HOME)
            m_state.camera_rotation_angle -= 0.05 * EIGEN_PI;
        else if (key == GLFW_KEY_END)
            m_state.camera_rotation_angle += 0.05 * EIGEN_PI;
        else if (key == GLFW_KEY_SPACE)
            cout << nlohmann::json(m_state).dump(4) << "\n";
        else if (key == GLFW_KEY_L)
            showObjLoadDialog();
        else if (key == GLFW_KEY_S)
            m_state.shading_toggle = !m_state.shading_toggle;
        else if (key == GLFW_KEY_R && action == GLFW_PRESS)
            m_state.is_rotating = !m_state.is_rotating;
        else if (key == GLFW_KEY_EQUAL)
        {
            m_state.model_translation = Vector3f::Zero();
            m_state.model_rotation_angle_y = 0.0f;
            m_state.model_scale = Vector3f(1.0f, 1.0f, 1.0f);
            m_state.camera_target = Vector3f::Zero();
        }
        else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12)
        {
            bool bShift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            bool bCtrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            bool bAlt = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
            unsigned int slot = key - GLFW_KEY_F1 + 1;
            filesystem::path state_path = filesystem::path("saved_states");
            if (!bShift && !bCtrl && !bAlt)         // no modifiers -> load
                m_state.load(state_path / fmt::format("state_{:02d}.json", slot));
            else if (bShift && !bCtrl && !bAlt)     // just shift -> save
                m_state.save(state_path / fmt::format("state_{:02d}.json", slot));
            else if (!bShift && bCtrl && !bAlt)     // just ctrl -> load reference
                m_state.load(state_path / fmt::format("reference_state_{:02d}.json", slot));

        }
    }
}

//------------------------------------------------------------------------

void App::handleMouseButton(GLFWwindow* window, int button, int action, int mods)
{
    // EXTRA: you can put your mouse controls here.
    // See GLFW Input documentation for details.
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        m_trackball_dragging = true;
        double x, y; glfwGetCursorPos(window, &x, &y);
        int w, h; glfwGetWindowSize(window, &w, &h);
        // Map to arcball sphere in NDC [-1,1]
    // Invert directions: dragging down rotates up; dragging left rotates right
    float nx = float((w - 2.0 * x) / w);
    float ny = float((2.0 * y - h) / h);
        float z2 = max(0.0f, 1.0f - nx * nx - ny * ny);
        m_arcball_last = Vector3f(nx, ny, sqrtf(z2)).normalized();
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    {
        m_trackball_dragging = false;
    }
}

void App::handleMouseMovement(GLFWwindow* window, double xpos, double ypos)
{
    // EXTRA: you can put your mouse controls here.
    // See GLFW Input documentation for details.
    if (!m_trackball_dragging)
        return;

    int w, h; glfwGetWindowSize(window, &w, &h);
    // Invert directions: dragging down rotates up; dragging left rotates right
    float nx = float((w - 2.0 * xpos) / w);
    float ny = float((2.0 * ypos - h) / h);
    float z2 = max(0.0f, 1.0f - nx * nx - ny * ny);
    Vector3f curr = Vector3f(nx, ny, sqrtf(z2)).normalized();

    // Compute rotation from last to current
    Vector3f axis = m_arcball_last.cross(curr);
    float dotp = std::clamp(m_arcball_last.dot(curr), -1.0f, 1.0f);
    float angle = acosf(dotp);
    if (axis.squaredNorm() > 1e-6f && angle != 0.0f)
    {
        AngleAxisf aa(angle, axis.normalized());
        // Pre-multiply so new rotation is applied relative to camera movement
        m_state.trackball_current_rotation = aa.toRotationMatrix() * m_state.trackball_current_rotation;
    }
    m_arcball_last = curr;
}

void App::handleScroll(GLFWwindow* window, double xoffset, double yoffset)
{
    // Mouse wheel zoom disabled per request.
    (void)window; (void)xoffset; (void)yoffset;
}

//------------------------------------------------------------------------

void App::handleDrop(GLFWwindow* window, int count, const char** paths)
{
    // only look at the last thing to be dropped
    if (strlen(paths[count-1]) > 0)
    {
        string f(paths[count - 1]);
        for (unsigned int i = 0; i < f.length(); ++i)
            f[i] = tolower(f[i]);

        if (filesystem::path(f).extension() == ".obj")
            m_state.scene_mode = fmt::format("obj({})", absoluteToCwdRelativePath(filesystem::path(paths[count - 1])).generic_string());
        else if (filesystem::path(f).extension() == ".ply")
            m_state.scene_mode = fmt::format("ply({})", absoluteToCwdRelativePath(filesystem::path(paths[count - 1])).generic_string());
    }
}

//------------------------------------------------------------------------

void App::showObjLoadDialog()
{
    string filename = fileOpenDialog("OBJ model file", "obj");
    if (filename != "")
        m_state.scene_mode = fmt::format("obj({})", absoluteToCwdRelativePath(filesystem::path(filename)).generic_string());
}

void App::showPlyLoadDialog()
{
    string filename = fileOpenDialog("PLY model file", "ply");
    if (filename != "")
        m_state.scene_mode = fmt::format("ply({})", absoluteToCwdRelativePath(filesystem::path(filename)).generic_string());
}

vector<App::Vertex> App::generateSingleTriangleMesh()
{
    static const Vertex triangle_data[] = {
        { Vector3f(0.0f,  0.5f, 0), Vector3f(0.0f, 0.0f, -1.0f) },
        { Vector3f(-0.5f, -0.5f, 0), Vector3f(0.0f, 0.0f, -1.0f) },
        { Vector3f(0.5f, -0.5f, 0), Vector3f(0.0f, 0.0f, -1.0f) }
    };
    vector<Vertex> vertices;
    for (auto const& v : triangle_data)
        vertices.push_back(v);
    return vertices;
}

vector<App::Vertex> App::unpackIndexedData(
    const vector<Vector3f>& positions,
    const vector<Vector3f>& normals,
    const vector<array<unsigned, 6>>& faces)
{
    vector<Vertex> vertices;
    vertices.reserve(faces.size() * 3);

    // This is a 'range-for' loop which goes through all objects in the container 'faces'.
    // '&' gives us a reference to the object inside the container; if we omitted '&',
    // 'f' would be a copy of the object instead.
    // The compiler already knows the type of objects inside the container, so we can
    // just write 'auto' instead of having to spell out 'array<unsigned, 6>'.
    for (auto& f : faces) {
        // Each face provides position/normal index pairs for three vertices
        Vertex v0, v1, v2;
        v0.position = positions[f[0]]; v0.normal = normals[f[1]];
        v1.position = positions[f[2]]; v1.normal = normals[f[3]];
        v2.position = positions[f[4]]; v2.normal = normals[f[5]];
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
    }

    return vertices;
}

// This is for testing your unpackIndexedData implementation.
// You should get a tetrahedron like in example.exe.
vector<App::Vertex> App::generateIndexedTetrahedronMesh() {
    static const Vector3f point_data[] = {
        Vector3f(0.0f, 0.407f, 0.0f),
        Vector3f(0.0f, -0.3f, -0.5f),
        Vector3f(0.433f, -0.3f, 0.25f),
        Vector3f(-0.433f, -0.3f, 0.25f),
    };
    static const Vector3f normal_data[] = {
        Vector3f(0.8165f, 0.3334f, -0.4714f),
        Vector3f(0.0f, 0.3334f, 0.9428f),
        Vector3f(-0.8165f, 0.3334f, -0.4714f),
        Vector3f(0.0f, -1.0f, 0.0f)
    };
    static const unsigned face_data[][6] = {
        {0, 0, 1, 0, 2, 0},
        {0, 2, 3, 2, 1, 2},
        {0, 1, 2, 1, 3, 1},
        {1, 3, 3, 3, 2, 3}
    };
    vector<Vector3f> points(point_data, point_data + SIZEOF_ARRAY(point_data));
    vector<Vector3f> normals(normal_data, normal_data + SIZEOF_ARRAY(normal_data));
    vector<array<unsigned, 6>> faces;
    for (auto arr : face_data) {
        array<unsigned, 6> f;
        copy(arr, arr + 6, f.begin());
        faces.push_back(f);
    }
    return unpackIndexedData(points, normals, faces);
}

// Generate an upright cone with tip at (0, 0, 0), a radius of 0.25 and a height of 1.0.
// You can leave the base of the cone open, like it is in example.exe.
vector<App::Vertex> App::generateConeMesh()
{
    static const float radius = 0.25f;
    static const float height = 1.0f;
    static const unsigned faces = 40;
    static const float angle_increment = 2 * EIGEN_PI / faces;

    vector<Vertex> vertices;

    Vertex v0(Vertex::Zero());
    Vertex v1(Vertex::Zero());
    Vertex v2(Vertex::Zero());

    for (auto i = 0u; i < faces; ++i) {
        float a0 = angle_increment * static_cast<float>(i);
        float a1 = angle_increment * static_cast<float>(i + 1);

        // Tip at 0, base at -height
        const Vector3f tip(0.0f, 0.0f, 0.0f);
        const Vector3f p0(radius * std::cos(a0), -height, radius * std::sin(a0));
        const Vector3f p1(radius * std::cos(a1), -height, radius * std::sin(a1));

        v0.position = tip;
        v1.position = p0;
        v2.position = p1;

        Vector3f e1 = p0 - tip;
        Vector3f e2 = p1 - tip;
        Vector3f n = e1.cross(e2).normalized();
        v0.normal = v1.normal = v2.normal = n;

        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
    }
    return vertices;
}

void App::uploadGeometryToGPU(const std::vector<Vertex>& vertices) const {
    // Load the vertex buffer to GPU.
    glBindBuffer(GL_ARRAY_BUFFER, m_gl.dynamic_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
    m_vertex_count = vertices.size();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void App::setMeshFromFlat(const std::vector<Vertex>& vertices) const
{
    // Upload as-is
    uploadGeometryToGPU(vertices);

    // Build an indexed mesh by welding identical positions
    m_indexed_mesh.positions.clear();
    m_indexed_mesh.triangles.clear();
    m_indexed_mesh.positions.reserve(vertices.size());

    struct Key { float x,y,z; };
    struct KeyHash { size_t operator()(Key const& k) const noexcept { size_t h1 = std::hash<float>()(k.x); size_t h2 = std::hash<float>()(k.y); size_t h3 = std::hash<float>()(k.z); return h1 ^ (h2<<1) ^ (h3<<2);} };
    struct KeyEq { bool operator()(Key const& a, Key const& b) const noexcept { return a.x==b.x && a.y==b.y && a.z==b.z; } };
    std::unordered_map<Key, uint32_t, KeyHash, KeyEq> mapIdx;
    mapIdx.reserve(vertices.size());

    auto getIndex = [&](const Vector3f& p){
        Key k{p.x(), p.y(), p.z()};
        auto it = mapIdx.find(k);
        if (it != mapIdx.end()) return it->second;
        uint32_t idx = (uint32_t)m_indexed_mesh.positions.size();
        m_indexed_mesh.positions.push_back(p);
        mapIdx.emplace(k, idx);
        return idx;
    };

    for (size_t i=0;i+2<vertices.size(); i+=3){
        uint32_t a = getIndex(vertices[i+0].position);
        uint32_t b = getIndex(vertices[i+1].position);
        uint32_t c = getIndex(vertices[i+2].position);
        if (a!=b && b!=c && a!=c)
            m_indexed_mesh.triangles.push_back({a,b,c});
    }

    m_simplify_target = m_indexed_mesh.triangles.size();
}

void App::setMeshFromIndexed(const simplify::IndexedMesh& mesh) const
{
    m_indexed_mesh = mesh;
    // Build flat list with flat normals per triangle
    std::vector<Vertex> verts;
    verts.reserve(mesh.triangles.size()*3);
    for (auto const& t : mesh.triangles){
        const Vector3f& a = mesh.positions[t[0]];
        const Vector3f& b = mesh.positions[t[1]];
        const Vector3f& c = mesh.positions[t[2]];
        Vector3f n = (b-a).cross(c-a);
        if (n.norm() > 0) n.normalize(); else n = Vector3f(0,0,1);
        verts.push_back({a,n});
        verts.push_back({b,n});
        verts.push_back({c,n});
    }
    uploadGeometryToGPU(verts);
    m_simplify_target = m_indexed_mesh.triangles.size();
}


void App::initRendering()
{
    // are we doing this for the first time at program startup?
    // if yes, errors are fatal. Otherwise try to fail gracefully without crashing.
    bool bFirstTime = m_gl.dynamic_vao == 0;

    // Compile and link the shader program.
    // This shader program will be used to draw everything except the user interface.
    // It consists of one vertex shader and one fragment shader.
    string vertex_shader, pixel_shader;
    try
    {
        vertex_shader = loadTextFile("src/vertex_shader.glsl");
        pixel_shader = loadTextFile("src/pixel_shader.glsl");
    }
    catch (std::runtime_error& e)
    {
        if (bFirstTime)
            fail(e.what());
        else
        {
            cerr << "Error loading shaders: " << e.what() << endl;
            return;
        }
    }
    vector<string> vecErrors;
    auto tentative_shader = compileAndLinkShaders(vertex_shader, pixel_shader, vecErrors, m_vertex_input_mapping);
    if (!tentative_shader) {
        string err = "Shader compilation failed:\n";
        for (auto& s : vecErrors)
            err += s + "\n";
        if (bFirstTime)
            fail(err.c_str());
        else
        {
            cerr << "Error compiling or linking shaders:" << endl << err << endl;
            return;
        }
    }

    // take ownership of newly compiled shader
    m_shader_program.swap(tentative_shader);
    // force reload of scene on next render if shader was changed, just in case vertex formatting has changed
    m_current_scene_mode = "";   

    if (m_gl.static_vao != 0)
    {
        assert(m_gl.dynamic_vao != 0);
        assert(m_gl.static_vertex_buffer != 0);
        assert(m_gl.dynamic_vertex_buffer != 0);

        glDeleteVertexArrays(1, &m_gl.static_vao);
        glDeleteVertexArrays(1, &m_gl.dynamic_vao);
        glDeleteBuffers(1, &m_gl.static_vertex_buffer);
        glDeleteBuffers(1, &m_gl.dynamic_vertex_buffer);
    }

    // Create vertex attribute objects and buffers for vertex data.
    glGenVertexArrays(1, &m_gl.static_vao);
    glGenVertexArrays(1, &m_gl.dynamic_vao);
    glGenBuffers(1, &m_gl.static_vertex_buffer);
    glGenBuffers(1, &m_gl.dynamic_vertex_buffer);

    // Set up vertex attribute object for static data.
    glBindVertexArray(m_gl.static_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gl.static_vertex_buffer);
    glEnableVertexAttribArray(m_vertex_input_mapping["aPosition"]);
    glVertexAttribPointer(m_vertex_input_mapping["aPosition"], 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
    glEnableVertexAttribArray(m_vertex_input_mapping["aNormal"]);
    glVertexAttribPointer(m_vertex_input_mapping["aNormal"], 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, normal));

    // Load the static data to the GPU; needs to be done only once.
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex)* SIZEOF_ARRAY(reference_plane_data), reference_plane_data, GL_STATIC_DRAW);

    // Set up vertex attribute object for dynamic data. We'll load the actual data later, whenever the model changes.
    glBindVertexArray(m_gl.dynamic_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gl.dynamic_vertex_buffer);
    glEnableVertexAttribArray(m_vertex_input_mapping["aPosition"]);
    glVertexAttribPointer(m_vertex_input_mapping["aPosition"], 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
    glEnableVertexAttribArray(m_vertex_input_mapping["aNormal"]);
    glVertexAttribPointer(m_vertex_input_mapping["aNormal"], 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, normal));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//------------------------------------------------------------------------

vector<App::Vertex> App::loadObjFile(const filesystem::path& filename) {
    vector<Vector3f> positions, normals;
    vector<array<unsigned, 6>> faces;

    // Open input file stream for reading.
    ifstream input(filename, ios::in);

    if (!input.is_open())
    {
        cerr << "Error opening " << filename << "!" << endl;
        return vector<App::Vertex>();
    }

    // Read the file line by line.
    string line;
    while (getline(input, line)) {
        // Replace any '/' characters with spaces ' ' so that all of the
        // values we wish to read are separated with whitespace.
        for (auto& c : line)
            if (c == '/')
                c = ' ';

        // Temporary objects to read data into.
        array<unsigned, 6>  f; // Face index array
        Vector3f               v;
        string              s;

        // Create a stream from the string to pick out one value at a time.
        istringstream        iss(line);

        // Read the first token from the line into string 's'.
        // It identifies the type of object (vertex or normal or ...)
        iss >> s;

        if (s == "v") { // vertex position
            // Read the three vertex coordinates (x, y, z) into 'v'.
            // Store a copy of 'v' in 'positions'.
            iss >> v.x() >> v.y() >> v.z();
            positions.push_back(v);
        }
        else if (s == "vn") { // normal
            // Read the three normal coordinates (nx, ny, nz) into 'v'.
            iss >> v.x() >> v.y() >> v.z();
            normals.push_back(v);
        }
        else if (s == "f") { // face
            // Read the indices representing a face and store it in 'faces'.
            // After replacing '/', the format becomes:
            // f v1 vt1 vn1 v2 vt2 vn2 v3 vt3 vn3
            // We'll ignore vt* (texture) by reading into a sink.
            unsigned sink; // Temporary variable for reading the unused texture indices.

            // Read position/normal index pairs for three vertices
            if (iss >> f[0] >> sink >> f[1]
                    >> f[2] >> sink >> f[3]
                    >> f[4] >> sink >> f[5])
            {
                // Convert from 1-based OBJ indices to 0-based C++ indices
                for (int i = 0; i < 6; ++i) {
                    // Guard against malformed files where index might be 0
                    if (f[i] > 0) f[i] -= 1;
                    else f[i] = 0; // clamp to 0 to avoid underflow
                }
                faces.push_back(f);
                // Optional debug:
                // cerr << f[0] << " " << f[1] << " " << f[2] << " " << f[3] << " " << f[4] << " " << f[5] << endl;
            }
        }
    }
    //common_ctrl_.message(("Loaded mesh from " + filename).c_str());
    return unpackIndexedData(positions, normals, faces);
}

// Minimal ASCII PLY loader: supports vertex positions (x y z), optional normals (nx ny nz),
// and triangular faces (f 3 idx0 idx1 idx2 or just '3 i j k' following 'face' element).
vector<App::Vertex> App::loadPlyFile(const filesystem::path& filename) {
    ifstream input(filename);
    if (!input.is_open()) {
        cerr << "Error opening " << filename << "!" << endl;
        return {};
    }

    string line;
    bool ascii = false;
    size_t vertexCount = 0, faceCount = 0;
    bool hasNormals = false;
    vector<Vector3f> positions;
    vector<Vector3f> normals;
    vector<array<unsigned,6>> faces; // pos/normal index pairs

    // Parse header
    if (!getline(input, line) || line != "ply") {
        cerr << "Not a PLY file" << endl; return {};
    }
    while (getline(input, line)) {
        if (line == "end_header") break;
        istringstream iss(line);
        string tok; iss >> tok;
        if (tok == "format") {
            string fmt; iss >> fmt; ascii = (fmt == "ascii");
            if (!ascii) { cerr << "Only ASCII PLY supported" << endl; return {}; }
        } else if (tok == "element") {
            string what; size_t count; iss >> what >> count;
            if (what == "vertex") vertexCount = count;
            else if (what == "face") faceCount = count;
        } else if (tok == "property") {
            string type, name; iss >> type >> name;
            if (name == "nx" || name == "ny" || name == "nz") hasNormals = true;
        }
    }

    positions.reserve(vertexCount);
    if (hasNormals) normals.reserve(vertexCount);

    // Read vertices
    for (size_t i = 0; i < vertexCount; ++i) {
        if (!getline(input, line)) { cerr << "Unexpected EOF in vertices" << endl; return {}; }
        istringstream iss(line);
        float x,y,z; iss >> x >> y >> z;
        positions.emplace_back(x,y,z);
        if (hasNormals) { float nx,ny,nz; iss >> nx >> ny >> nz; normals.emplace_back(nx,ny,nz); }
    }

    // Read faces (triangles)
    for (size_t i = 0; i < faceCount; ++i) {
        if (!getline(input, line)) { cerr << "Unexpected EOF in faces" << endl; return {}; }
        istringstream iss(line);
        int n; iss >> n; // number of indices
        if (n != 3) { cerr << "Non-tri face encountered; skipping" << endl; continue; }
        unsigned i0,i1,i2; iss >> i0 >> i1 >> i2;
        array<unsigned,6> f;
        f[0] = i0; f[2] = i1; f[4] = i2;
        if (hasNormals) { f[1] = i0; f[3] = i1; f[5] = i2; }
        else { f[1] = f[3] = f[5] = 0; }
        faces.push_back(f);
    }

    // If normals not provided, compute per-face flat normals
    if (!hasNormals) {
        normals.resize(positions.size(), Vector3f::Zero());
        // accumulate face normals to vertices, then normalize
        for (auto& f : faces) {
            Vector3f p0 = positions[f[0]];
            Vector3f p1 = positions[f[2]];
            Vector3f p2 = positions[f[4]];
            Vector3f n = (p1 - p0).cross(p2 - p0).normalized();
            normals[f[0]] += n; normals[f[2]] += n; normals[f[4]] += n;
            f[1] = f[0]; f[3] = f[2]; f[5] = f[4];
        }
        for (auto& n : normals) if (n.norm() > 0) n.normalize();
    }

    return unpackIndexedData(positions, normals, faces);
}

