#define _USE_MATH_DEFINES

#include "app.h"
#include "subdiv.h"

#include <fmt/core.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include "im3d.h"
#include <imgui_internal.h> // for ImGui::PushItemFlag / ImGuiItemFlags_Disabled

#ifndef CS3100_TTF_PATH
#define CS3100_TTF_PATH "roboto_mono.ttf"
#endif

//------------------------------------------------------------------------

// defined in im3d_opengl33.cpp
bool Im3d_Init();
void Im3d_NewFrame(GLFWwindow* window,
    int width,
    int height,
    const Matrix4f& world_to_view,
    const Matrix4f& view_to_clip,
    float dt,
    float mousex,
    float mousey);
void Im3d_EndFrame();
namespace Im3d
{
    inline void Vertex(const Vector3f& v) { Im3d::Vertex(v(0), v(1), v(2)); }
    inline void Vertex(const Vector4f& v) { Im3d::Vertex(v(0), v(1), v(2), v(3)); }
}

//------------------------------------------------------------------------

App::App(void)
{
    if (static_instance != 0)
        fail("Attempting to create a second instance of App!");

    static_instance = this;
}

//------------------------------------------------------------------------

App::~App()
{
    static_instance = 0;
}

//------------------------------------------------------------------------

void App::run(const filesystem::path savePNGAndTerminate)
{
    // Warn about cwd problems
    std::filesystem::path cwd = std::filesystem::current_path();
    if (!std::filesystem::is_directory(cwd/"assets")) {
        std::cout << fmt::format("Current working directory \"{}\" does not contain an \"assets\" folder.\n", cwd.string())
             << "Make sure the executable gets run relative to the project root.\n";
        return;
    }
    
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
    m_window = glfwCreateWindow(window_width, window_height, "CS-C3100 Computer Graphics, Assignment 2", NULL, NULL);
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
    m_io->IniFilename = nullptr;                                // Disable generation of imgui.ini for consistent behavior


    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Keep track of previous callbacks for chaining
    default_key_callback = glfwSetKeyCallback(m_window, App::key_callback);
    default_mouse_button_callback = glfwSetMouseButtonCallback(m_window, App::mousebutton_callback);
    default_cursor_pos_callback = glfwSetCursorPosCallback(m_window, App::cursorpos_callback);
    default_drop_callback = glfwSetDropCallback(m_window, App::drop_callback);

    if (!Im3d_Init())
        fail("Error initializing Im3d!");

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

        // First, render our own 3D scene using OpenGL
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);

        // set the internal state of the camera and store the relevant parts in our state
        setupViewportAndProjection(window_width, window_height);

        render(m_state, width, height, vecStatusMessages);

        // check if we are to grab the pixels to a file and terminate
        if (!savePNGAndTerminate.empty())
        {
            auto tmp = takeScreenShot();
            tmp->exportPNG(savePNGAndTerminate);
            break;
        }

        // Begin GUI window
        ImGui::Begin("Controls", 0, ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::RadioButton("Curve Mode (1)", m_state.mode == DrawMode::Curves))
            m_state.mode = DrawMode::Curves;
        if (ImGui::RadioButton("Subdivision Mode - full (2)", m_state.mode == DrawMode::Subdivision))
            m_state.mode = DrawMode::Subdivision;
        if (ImGui::RadioButton("Subdivision Mode - R3 only (3)", m_state.mode == DrawMode::Subdivision_R3))
            m_state.mode = DrawMode::Subdivision_R3;
        if (ImGui::RadioButton("Subdivision Mode - R3 & R4 only (4)", m_state.mode == DrawMode::Subdivision_R3_R4))
            m_state.mode = DrawMode::Subdivision_R3_R4;


        if (m_state.mode == DrawMode::Curves)
        {
            if (ImGui::Button("Load JSON curve file (L)"))
                handleLoading();
            ImGui::SliderInt("Tessellation steps", (int*) & m_state.spline_tessellation, 1, 32);
            if (m_render_cache.surfaces.size() > 0) {
                ImGui::Checkbox("Draw surface (S)", &m_state.show_surface);
                ImGui::Checkbox("Render wireframe (W)", &m_state.wireframe);
                ImGui::Checkbox("Render curve frames (F)", &m_state.draw_frames);
            }

            // Curve editor UI
            ImGui::Separator();
            ImGui::Text("Curve Editor");
            ImGui::Checkbox("Edit mode", &m_curve_edit_mode);
            if (m_curve_edit_mode) {
                auto applyTypeDefaults = [&](SplineCurve& c)
                {
                    if (c.type == "catmull-rom") {
                        if (c.control_points.size() < 2)
                            c.control_points = { {-0.5f, 0.0f, 0.0f}, {0.5f, 0.0f, 0.0f} };
                    } else if (c.type == "bezier") {
                        if (c.control_points.size() < 4 || (c.control_points.size() - 1) % 3 != 0)
                            c.control_points = { {0,0,0}, {1,0,0}, {2,0,0}, {3,0,0} };
                    } else if (c.type == "bspline") {
                        if (c.control_points.size() < 4)
                            c.control_points = { {-1,0,0}, {-0.3f,0.6f,0}, {0.3f,0.6f,0}, {1,0,0} };
                    } else if (c.type == "circle") {
                        if (c.control_points.size() != 2)
                            c.control_points = { {0.3f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
                    } else if (c.type == "kappa") {
                        if (c.control_points.size() < 3)
                            c.control_points = { {0.0f, 0.0f, 0.0f}, {0.4f, 0.0f, 0.0f}, {0.2f, 0.35f, 0.0f} };
                    }
                };

                int numCurves = (int)m_render_cache.spline_curves.size();
                if (numCurves == 0) {
                    if (ImGui::Button("New curve")) {
                        SplineCurve c; c.type = "catmull-rom";
                        c.control_points = { {-0.5f, 0.0f, 0.0f}, {0.0f, 0.5f, 0.0f}, {0.5f, 0.0f, 0.0f} };
                        m_render_cache.spline_curves.push_back(c);
                        m_edit_curve_idx = 0; m_edit_point_idx = -1;
                        retriangulateAfterEdit();
                    }
                } else {
                    if (m_edit_curve_idx < 0 || m_edit_curve_idx >= numCurves) m_edit_curve_idx = 0;
                    ImGui::SliderInt("Active curve", &m_edit_curve_idx, 0, numCurves-1);
                    auto &cur = m_render_cache.spline_curves[m_edit_curve_idx];

                    const char* types[] = {"bezier","bspline","catmull-rom","circle","kappa"};
                    int tsel = 0; for (int i=0;i<5;++i) if (cur.type==types[i]) tsel=i;
                    if (ImGui::Combo("Type", &tsel, types, 5)) {
                        cur.type = types[tsel];
                        applyTypeDefaults(cur);
                        m_edit_point_idx = std::min(m_edit_point_idx, (int)cur.control_points.size()-1);
                        retriangulateAfterEdit();
                    }

                    ImGui::Text("Points: %d", (int)cur.control_points.size());
                    bool allowAddRemove = (cur.type == "catmull-rom");
                    // Compatibility for older ImGui versions lacking BeginDisabled/EndDisabled
                    bool __cg_disabled_scope = !allowAddRemove;
                    if (__cg_disabled_scope) {
                        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                    }
                    if (ImGui::Button("Add point at mouse")) {
                        double mx,my; glfwGetCursorPos(m_window,&mx,&my);
                        int fbw, fbh; glfwGetFramebufferSize(m_window,&fbw,&fbh);
                        Vector3f o = screenToRayOrigin(m_state.camera);
                        Vector3f d = screenToRayDir(m_state.camera, fbw, fbh, mx, my);
                        Vector3f hit; if (intersectPlaneZ0(o,d,hit)) {
                            cur.control_points.push_back(hit);
                            m_edit_point_idx = (int)cur.control_points.size()-1;
                            retriangulateAfterEdit();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Delete selected")) {
                        if (m_edit_point_idx>=0 && m_edit_point_idx < (int)cur.control_points.size()) {
                            cur.control_points.erase(cur.control_points.begin()+m_edit_point_idx);
                            m_edit_point_idx = -1;
                            retriangulateAfterEdit();
                        }
                    }
                    if (__cg_disabled_scope) {
                        ImGui::PopStyleVar();
                        ImGui::PopItemFlag();
                    }
                    if (!allowAddRemove)
                        ImGui::TextDisabled("Add/Delete available only for Catmull-Rom curves.");

                    static char savePath[256] = "assignment2/assets/curves/extra/edited.json";
                    ImGui::InputText("Save to", savePath, IM_ARRAYSIZE(savePath));
                    if (ImGui::Button("Export JSON")) {
                        nlohmann::json j; j["curves"] = m_render_cache.spline_curves; j["surfaces"] = m_render_cache.surfaces; std::ofstream out(savePath); out << j.dump(2);
                    }
                    ImGui::Text("Tip: drag points with LMB. Hold Shift while clicking to add (Catmull-Rom).");
                }
            }
        }
        if (m_state.mode >= DrawMode::Subdivision_R3)
        {
            if (ImGui::Button("Load OBJ mesh (L)"))
                handleLoading();
            float mid = 200.0f * m_ui_scale;
            if (ImGui::Button("Increase subdivision (KP+)"))
                m_state.subdivision++;
            ImGui::SameLine(mid);
            if (ImGui::Button("Decrease subdivision (KP-)"))
                m_state.subdivision = (m_state.subdivision > 0) ? (m_state.subdivision - 1) : 0;

            ImGui::Checkbox("Render wireframe (W)", &m_state.wireframe);
            ImGui::Checkbox("Show connectivity (D)", &m_debug_subdivision);
            ImGui::Checkbox("Crude boundary handling (B)", &m_state.crude_boundaries);
        }


        if (ImGui::Button("Take screenshot"))
        {
            auto tmp = takeScreenShot();
            auto png_path = filesystem::current_path() / "debug.png";
            tmp->exportPNG(png_path);
            std::cerr << "Wrote screenshot to " << png_path << "\n";
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
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

//------------------------------------------------------------------------

// w, h - width and height of the window in pixels.
void App::setupViewportAndProjection(int w, int h)
{
    m_state.camera.SetDimensions(w, h);
    m_state.camera.SetViewport(0, 0, w, h);
    m_state.camera.SetPerspective(50);
}

//------------------------------------------------------------------------

void App::update_render_cache(const AppState& state) const {

    auto& cache = m_render_cache;

    // this is the general pattern: we check if a value has changed and is valid, and then set the cached value to match.
    // this way we act on the frame following the change, and loading/computation only has to happen once instead of every frame.
    bool file_changed = (cache.filename != state.filename) && (state.filename.size() > 0);
    cache.filename = state.filename;

    if (state.mode == DrawMode::Curves) {

        // spline should change if the file does OR if the tessellation slider is moved
        bool spline_changed = file_changed || (cache.tessellation_steps != state.spline_tessellation);
        cache.tessellation_steps = state.spline_tessellation;

        if (file_changed)
        {
            //loadSWP(state.filename);
            std::ifstream f(state.filename);
            auto parsed = nlohmann::json::parse(f);
            parsed.at("curves").get_to(cache.spline_curves);
            parsed.at("surfaces").get_to(cache.surfaces);
        }


        if (spline_changed) {
            tessellateCurves(state.spline_tessellation);
            generateSurfaces(state.spline_tessellation);
        }
    }
    else {

        // subdivision surfaces change if the mode or edge handling changes
        bool surface_changed = file_changed || (cache.crude_boundaries != state.crude_boundaries) || (cache.mode != state.mode);
        // the mesh to be displayed changes if the subdivision surface itself changes or if a different level is chosen
        bool mesh_changed = surface_changed || (cache.subdivision != state.subdivision);

        // set cached values to match
        cache.crude_boundaries = state.crude_boundaries;
        cache.mode = state.mode;
        cache.subdivision = state.subdivision;

        if (file_changed)
            loadOBJ(state.filename);

        // if load fails, we'll have zero meshes
        if (cache.subdivided_meshes.size() > 0) {

            if (surface_changed)
                cache.subdivided_meshes.resize(1);

            while (cache.subdivided_meshes.size() <= state.subdivision)
                addSubdivisionLevel(state.mode, state.crude_boundaries);

            if (mesh_changed) {
                uploadGeometryToGPU(*cache.subdivided_meshes[state.subdivision]);
            }
        }
    }
}

// This function is responsible for displaying the object.
void App::render(const AppState& state, int window_width, int window_height, vector<string>& vecStatusMessages) const
{
    // Before rendering, we check if any cached objects need to be reloaded/recomputed.
    update_render_cache(state);

    if (m_surfaces_dirty && !m_render_cache.surfaces.empty()) {
        generateSurfaces(m_state.spline_tessellation);
        m_surfaces_dirty = false;
    }

    // The cached values are now up to date, and we can render the frame.
    auto& cache = m_render_cache;

    // Remove any shader that may be in use.
    glUseProgram(0);

    // Clear the rendering window
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // tell OpenGL the size of the surface we're drawing into
    glViewport(0, 0, window_width, window_height);

    //Matrix4f model_to_clip = camera_.GetPerspective() * camera_.GetModelview();
    double mousex, mousey;
    glfwGetCursorPos(m_window, &mousex, &mousey);
    Im3d_NewFrame(m_window, window_width, window_height, state.camera.GetModelview(), state.camera.GetPerspective(), 0.01f, mousex, mousey);
    // Reset edit selection when mode changes to keep sane state
    if (state.mode != DrawMode::Curves) { m_curve_edit_mode = false; m_dragging_point = false; }

    switch(state.mode)
    {
    case DrawMode::Curves:
            renderCurves(state.draw_frames);
            if(cache.surfaces.size()>0 && state.show_surface)
                renderMesh(cache.surface_mesh, state.camera, state.wireframe, -1, -1);
            break;

    case DrawMode::Subdivision:
    case DrawMode::Subdivision_R3:
    case DrawMode::Subdivision_R3_R4:
        if (cache.subdivided_meshes.size() > 0)
        {
            const MeshWithConnectivity& m = *cache.subdivided_meshes[state.subdivision];

            // check if mouse is on top of a triangle and show debug info if requested
            int highlight_triangle = -1;
            int highlight_vertex = -1;
            if (m_debug_subdivision)
            {
                double mx, my;
                glfwGetCursorPos(m_window, &mx, &my);
                ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale; // Mac Retina specific
                std::tuple<int,int> tri_vertex_ind = pickTriangle(m, state.camera, window_width, window_height, mx * fbScale.x, my * fbScale.y);
                highlight_triangle = std::get<0>(tri_vertex_ind);
                highlight_vertex = std::get<1>(tri_vertex_ind);

                Vector3f pos(Vector3f::Zero());
			    Vector3f norm(Vector3f::Zero());
			    Vector3f col(Vector3f::Zero()); // These are not used after modification in traverseOneRing below.
                m.traverseOneRing(highlight_triangle, highlight_vertex, pos, norm, col, &m_debug_indices);

                vecStatusMessages.push_back(fmt::format("Selected triangle: {}", highlight_triangle));
                if (highlight_triangle != -1)
                {
                    const Vector3i& i = m.indices[highlight_triangle];
                    const Vector3i& nt = m.neighborTris[highlight_triangle];
                    const Vector3i& ne = m.neighborEdges[highlight_triangle];
                    vecStatusMessages.push_back(fmt::format("             Indices: {:3d}, {:3d}, {:3d}", i(0), i(1), i(2)));
                    vecStatusMessages.push_back(fmt::format("  Neighbor triangles: {:3d}, {:3d}, {:3d}", nt(0), nt(1), nt(2)));
                    vecStatusMessages.push_back(fmt::format("      Neighbor edges: {:3d}, {:3d}, {:3d}", ne(0), ne(1), ne(2)));
                }
            }

            renderMesh(m, state.camera, state.wireframe, highlight_triangle, highlight_vertex);
        }
        break;
	}

    // this draws the accumulated buffers 
    Im3d_EndFrame();
}

void App::renderMesh(const MeshWithConnectivity& m, const Camera& cam, bool include_wireframe, int highlight_triangle, int highlight_vertex) const
{
    Matrix4f world_to_view = cam.GetModelview();
    Matrix4f view_to_clip = cam.GetPerspective();
    Matrix4f view_to_world = world_to_view.inverse();
    Vector3f camera_position = view_to_world.block(0, 3, 3, 1);

    glUseProgram(m_gl.shader_program);
    auto err = glGetError();
    glUniform1f(m_gl.shading_toggle_uniform, true ? 1.0f : 0.0f);
    err = glGetError();
    glUniformMatrix4fv(m_gl.view_to_clip_uniform, 1, GL_FALSE, view_to_clip.data());
    err = glGetError();
    glUniformMatrix4fv(m_gl.world_to_view_uniform, 1, GL_FALSE, world_to_view.data());
    err = glGetError();
    glUniform3fv(m_gl.camera_world_position_uniform, 1, camera_position.data());
    err = glGetError();
    glUniform1f(m_gl.ambient_strength_uniform, 0.2f);
    err = glGetError();
    glUniform1f(m_gl.specular_strength_uniform, 0.55f);
    err = glGetError();
    glUniform1f(m_gl.shininess_uniform, 48.0f);
    err = glGetError();
    glUniform1f(m_gl.rim_strength_uniform, 0.35f);
    err = glGetError();
    glUniform3f(m_gl.rim_color_uniform, 0.55f, 0.70f, 0.90f);
    err = glGetError();
    glUniform3f(m_gl.specular_color_uniform, 1.0f, 1.0f, 1.0f);
    err = glGetError();


    // Draw the model with your model-to-world transformation.
    // Force opaque rendering for solid look
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBindVertexArray(m_gl.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gl.vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gl.index_buffer);
    glDrawElements(GL_TRIANGLES, (GLsizei)3 * m.indices.size(), GL_UNSIGNED_INT, 0);

    // Undo our bindings.
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    if (include_wireframe || (highlight_triangle != -1 && m_debug_subdivision))
    {
        static vector<size_t> only_highlighted_triangle({0});
        static vector<size_t> all_triangles;
        if (all_triangles.size() != m.indices.size())
        {
            all_triangles.resize(m.indices.size());
            for (size_t i = 0; i < m.indices.size(); ++i)
                all_triangles[i] = i;
        }
        const vector<size_t>* index_list = &all_triangles;
        if (!include_wireframe && highlight_triangle != -1 && m_debug_subdivision)
        {
            only_highlighted_triangle[0] = highlight_triangle;
            index_list = &only_highlighted_triangle;
        }

        Im3d::BeginPoints();
        if(m_debug_indices.size() && m_toggle_onering && highlight_triangle != -1){
            Im3d::SetSize(16.0f);
            Im3d::SetColor(1.0f, 0.0f, 0.0f);
            auto v_current = m.positions[m.indices[highlight_triangle][highlight_vertex]];
            auto n_current = m.normals[m.indices[highlight_triangle][highlight_vertex]];
            const float t = 0.05f;
            Vector3f nv_current = v_current+t*n_current;

            Im3d::Vertex(nv_current);

            bool bad_vertices = false;
            for (size_t j = 0; j < m_debug_indices.size(); ++j){
                bad_vertices = bad_vertices || m_debug_indices[j] == -1; // check if on boundary. If so, don't render the collected vertices.
            }

            Im3d::SetColor(1.0f, 1.0f, 1.0f);
            for (size_t j = 0; j < m_debug_indices.size(); ++j){
                if (bad_vertices){
                    break;
                }
                auto v0 = m.positions[m_debug_indices[j]];
                auto n0 = m.normals[m_debug_indices[j]];
                Vector3f nv0 = v0+t*n0;

                Im3d::Vertex(nv0);
            }
        }
        Im3d::End();

        Im3d::BeginLines();
        for (size_t j = 0; j < index_list->size(); ++j)
        {
            size_t i = (*index_list)[j];

            const Vector3i& f = m.indices[i];

            if (i == highlight_triangle)
                Im3d::SetSize(8.0f);
            else
                Im3d::SetSize(2.0f);

            // prepare a slightly smaller triangle so that we can code its edges with colors
            auto v0 = m.positions[f[0]];
            auto v1 = m.positions[f[1]];
            auto v2 = m.positions[f[2]];
            auto n0 = m.normals[f[0]];
            auto n1 = m.normals[f[1]];
            auto n2 = m.normals[f[2]];
            Vector3f tn = (v1 - v0).cross(v2 - v0).normalized() * 0.01f;
            Vector3f c = (v0 + v1 + v2) / 3;
            const float t = 0.95f;
            Vector3f nv0 = t * v0 + (1.0f - t) * c + tn;
            Vector3f nv1 = t * v1 + (1.0f - t) * c + tn;
            Vector3f nv2 = t * v2 + (1.0f - t) * c + tn;

            Im3d::SetColor(1.0f, 0.0f, 0.0f);   // 1st edge: red
            Im3d::Vertex(nv0);
            Im3d::Vertex(nv1);
            Im3d::SetColor(0.0f, 1.0f, 0.0f);   // 2nd edge: green
            Im3d::Vertex(nv1);
            Im3d::Vertex(nv2);
            Im3d::SetColor(0.0f, 0.0f, 1.0f);   // 3rd edge: blue
            Im3d::Vertex(nv2);
            Im3d::Vertex(nv0);
        }
        Im3d::End();
    }
}

//------------------------------------------------------------------------

// Initialize OpenGL's rendering modes
void App::initRendering()
{
    glEnable(GL_DEPTH_TEST);   // Depth testing must be turned on
    // Cull back faces to avoid seeing the inside surfaces and improve solidity
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Create vertex attribute objects and buffers for vertex and index data.
    glGenVertexArrays(1, &m_gl.vao);
    glGenBuffers(1, &m_gl.vertex_buffer);
    glGenBuffers(1, &m_gl.index_buffer);

    // Set up vertex attribute object
    glBindVertexArray(m_gl.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_gl.vertex_buffer);
    glEnableVertexAttribArray(ATTRIB_POSITION);
    glVertexAttribPointer(ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNC), (GLvoid*)0);
    glEnableVertexAttribArray(ATTRIB_NORMAL);
    glVertexAttribPointer(ATTRIB_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNC), (GLvoid*)offsetof(VertexPNC, normal));
    glEnableVertexAttribArray(ATTRIB_COLOR);
    glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, sizeof(VertexPNC), (GLvoid*)offsetof(VertexPNC, color));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gl.index_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gl.index_buffer);
    glBindVertexArray(0);


    auto shader_program = new ShaderProgram(
        "#version 330\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        FW_GL_SHADER_SOURCE(
            layout(location = 0) in vec4 aPosition;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec3 aColor;

            layout(location = 0) out vec3 vWorldPos;
            layout(location = 1) out vec3 vNormal;
            layout(location = 2) out vec4 vColor;

            uniform mat4 uWorldToView;
            uniform mat4 uViewToClip;
            uniform float uShading;

            void main()
            {
                gl_Position = uViewToClip * uWorldToView * aPosition;
                vNormal = aNormal;
                vColor = vec4(aColor, 1.0);
                vWorldPos = aPosition.xyz;
            }
        ),
        "#version 330\n"
        "#extension GL_ARB_separate_shader_objects : enable\n"
        FW_GL_SHADER_SOURCE(
            layout(location = 0) in vec3 vWorldPos;
            layout(location = 1) in vec3 vNormal;
            layout(location = 2) in vec4 vColor;

            uniform vec3 uCameraWorldPosition;
            uniform float uAmbientStrength;
            uniform float uSpecularStrength;
            uniform float uShininess;
            uniform float uRimStrength;
            uniform vec3 uRimColor;
            uniform vec3 uSpecularColor;

            const vec3 cLightDirection1 = normalize(vec3(0.5, 0.5, 0.6));
            const vec3 cLightDirection2 = normalize(vec3(-1, 0, 0));
            const vec3 cLightColor1 = vec3(1, 1, 1);
            const vec3 cLightColor2 = vec3(0.4, 0.3, 0.4);

            out vec4 fColor;

            void main()
            {
                vec3 n = normalize(vNormal);
                vec3 viewDir = normalize(uCameraWorldPosition - vWorldPos);
                vec3 baseColor = vColor.rgb;

                vec3 l1 = cLightDirection1;
                vec3 l2 = cLightDirection2;

                float diff1 = max(dot(n, l1), 0.0);
                float diff2 = max(dot(n, l2), 0.0);

                float spec1 = 0.0;
                float spec2 = 0.0;
                if (diff1 > 0.0)
                {
                    vec3 h1 = normalize(l1 + viewDir);
                    spec1 = pow(max(dot(n, h1), 0.0), uShininess);
                }
                if (diff2 > 0.0)
                {
                    vec3 h2 = normalize(l2 + viewDir);
                    spec2 = pow(max(dot(n, h2), 0.0), uShininess);
                }

                vec3 ambient = baseColor * uAmbientStrength;
                vec3 diffuse = baseColor * (diff1 * cLightColor1 + diff2 * cLightColor2);
                vec3 specular = uSpecularColor * (spec1 + spec2) * uSpecularStrength;

                float rim = pow(clamp(1.0 - max(dot(n, viewDir), 0.0), 0.0, 1.0), 2.0) * uRimStrength;
                vec3 rimLight = uRimColor * rim;

                vec3 color = ambient + diffuse + specular + rimLight;
                color = clamp(color, 0.0, 1.0);
                fColor = vec4(color, 1.0);
            }
        ));

    // Get the IDs of the shader program and its uniform input locations from OpenGL.
    m_gl.shader_program = shader_program->getHandle();
    m_gl.view_to_clip_uniform = glGetUniformLocation(m_gl.shader_program, "uViewToClip");
    m_gl.world_to_view_uniform = glGetUniformLocation(m_gl.shader_program, "uWorldToView");
    m_gl.shading_toggle_uniform = glGetUniformLocation(m_gl.shader_program, "uShading");
    m_gl.camera_world_position_uniform = glGetUniformLocation(m_gl.shader_program, "uCameraWorldPosition");
    m_gl.ambient_strength_uniform = glGetUniformLocation(m_gl.shader_program, "uAmbientStrength");
    m_gl.specular_strength_uniform = glGetUniformLocation(m_gl.shader_program, "uSpecularStrength");
    m_gl.shininess_uniform = glGetUniformLocation(m_gl.shader_program, "uShininess");
    m_gl.rim_strength_uniform = glGetUniformLocation(m_gl.shader_program, "uRimStrength");
    m_gl.rim_color_uniform = glGetUniformLocation(m_gl.shader_program, "uRimColor");
    m_gl.specular_color_uniform = glGetUniformLocation(m_gl.shader_program, "uSpecularColor");
}

//------------------------------------------------------------------------

// Load in objects from standard input into the class member variables: 
// spline_curves_, tessellated_curves_, curve_names_, surfaces_, m_surfaceNames.  If
// loading fails, this will exit the program.
//void App::loadSWP(const string& filename) const
//{
//    std::cout << "\n*** loading and constructing curves and surfaces ***\n";
//	
//    if (!parseSWP(filename, m_render_cache.spline_curves, m_render_cache.surfaces)) {
//        std::cerr << "\aerror in file format\a\n";
//        exit(-1);
//    }
//
//    std::cerr << "\n*** done ***\n";
//}

//------------------------------------------------------------------------

void App::loadOBJ(const string& filename) const
{
    // get rid of the old meshes, if any
    m_render_cache.subdivided_meshes.clear();

    auto pNewMesh = MeshWithConnectivity::loadOBJ(filename, m_state.crude_boundaries);

    // hoist it into GPU memory...
    uploadGeometryToGPU(*pNewMesh);

    // and write result down for later use.
    m_render_cache.subdivided_meshes.push_back(unique_ptr<MeshWithConnectivity>(pNewMesh));
}

//------------------------------------------------------------------------

void App::addSubdivisionLevel(DrawMode mode, bool crude_boundaries) const
{
    // copy constuct finest mesh
    MeshWithConnectivity* pNewMesh = new MeshWithConnectivity(*m_render_cache.subdivided_meshes.back());
    pNewMesh->LoopSubdivision(mode, crude_boundaries);
    pNewMesh->computeConnectivity();
    pNewMesh->computeVertexNormals();
    m_render_cache.subdivided_meshes.push_back(unique_ptr<MeshWithConnectivity>(pNewMesh));
}

//------------------------------------------------------------------------

void App::uploadGeometryToGPU(const MeshWithConnectivity& m) const
{
    static vector<VertexPNC> v;
    v.resize(m.positions.size());
    for (size_t i = 0; i < v.size(); ++i)
    {
        v[i].position = m.positions[i];
        v[i].color = m.colors[i];
        v[i].normal = m.normals[i];
    }

    // Load the vertex buffer to GPU.
    glBindBuffer(GL_ARRAY_BUFFER, m_gl.vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VertexPNC) * v.size(), v.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_gl.index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(Vector3i) * m.indices.size(), m.indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

//------------------------------------------------------------------------

void MeshWithConnectivity::computeVertexNormals()
{
    // Calculate average normal for each vertex position by
    // looping over triangles, computing their normals,
    // and adding them to each of its vertices' normal.

    typedef std::map<Vector3f, Vector3f, CompareVector3f> postonormal_t;
    postonormal_t posToNormal;
    Vector3f v[3];

    for (size_t j = 0; j < indices.size(); j++)
    {
        const Vector3i& tri = indices[j];
        for (int k = 0; k < 3; k++)
            v[k] = positions[tri[k]];

        Vector3f triNormal = (v[1] - v[0]).cross(v[2] - v[0]);
        for (int k = 0; k < 3; k++)
        {
            if (posToNormal.find(v[k]) != posToNormal.end())
                posToNormal[v[k]] += triNormal;
            else
                posToNormal[v[k]] = triNormal;
        }
    }

    // Output normals. Normalization yields the average of
    // the normals of all the triangles that share each vertex.
    for ( size_t i = 0; i < positions.size(); ++i )
    {
        if (posToNormal.find(positions[i]) != posToNormal.end())
            normals[i] = posToNormal[positions[i]].normalized();
    }
}

//------------------------------------------------------------------------

void App::writeObjects(const string& prefix) {
    std::cerr << "\n*** writing obj files ***\n";

    //for (auto i = 0u; i < surface_names_.size(); ++i) {
    //    if (surface_names_[i] != ".") {
    //        string filename = prefix + "_" + surface_names_[i] + ".obj";
    //        ofstream out(filename);
    //        if (!out) {
    //            cerr << "\acould not open file " << filename << ", skipping"<< endl;
    //            out.close();
    //            continue;
    //        } else {
    //            outputObjFile(out, surfaces_[i]);
    //            cerr << "wrote " << filename <<  endl;
    //        }
    //    }
    //}
}

//------------------------------------------------------------------------

void App::tessellateCurves(int tessellation_steps) const
{
    auto& cache = m_render_cache;
    cache.tessellated_curves.resize(cache.spline_curves.size());
    for (size_t i = 0; i < cache.spline_curves.size(); ++i)
    {
        const auto& curve = cache.spline_curves[i];
        auto& dest = cache.tessellated_curves[i];
        dest.clear();

        if (curve.type == "bezier")
        {
            if (curve.control_points.size() >= 4 && (curve.control_points.size() - 1) % 3 == 0)
                tessellateBezier(curve.control_points, dest, tessellation_steps);
        }
        else if (curve.type == "bspline")
        {
            if (curve.control_points.size() >= 4)
                tessellateBspline(curve.control_points, dest, tessellation_steps);
        }
        else if (curve.type == "bezier-piecewise")
        {
            if (!curve.segments.empty())
                tessellateBezierPiecewise(curve.segments, dest, tessellation_steps, /*connect*/false);
        }
        else if (curve.type == "bspline-piecewise")
        {
            if (!curve.segments.empty())
                tessellateBsplinePiecewise(curve.segments, dest, tessellation_steps, /*connect*/false);
        }
        else if (curve.type == "circle")
        {
            if (curve.control_points.size() == 2)
                tessellateCircle(curve.control_points, dest, tessellation_steps);
        }
        else if (curve.type == "catmull-rom")
        {
            if (curve.control_points.size() >= 2)
                tessellateCatmullRom(curve.control_points, dest, tessellation_steps);
        }
        else if (curve.type == "kappa")
        {
            if (curve.control_points.size() >= 3)
                tessellateKappaClosed(curve.control_points, dest, tessellation_steps);
        }
    }
}

void App::generateSurfaces(int tessellation_steps) const
{
    auto& cache = m_render_cache;

    MeshWithConnectivity& m = cache.surface_mesh;
    
    m.positions.clear();
    m.normals.clear();
    m.colors.clear();
    m.indices.clear();
    for (auto& surf : cache.surfaces) {
        GeneratedSurface s;
        //switch (surf.type) {
        if ( surf.type == "revolution")
        //case SurfaceType::Revolution:
            s = makeSurfRev(cache.tessellated_curves[surf.curve_indices[0]], tessellation_steps);
            //break;
        //case SurfaceType::GeneralizedCylinder:
        else if (surf.type == "gen_cyl") {
            const auto& profile = cache.tessellated_curves[surf.curve_indices[0]];
            const auto& sweep = cache.tessellated_curves[surf.curve_indices[1]];
            // If a scale curve is provided, use scaled generalized cylinder
            if (surf.curve_indices.size() >= 3 && surf.curve_indices[2] < cache.tessellated_curves.size()) {
                const auto& scale = cache.tessellated_curves[surf.curve_indices[2]];
                s = makeGenCyl(profile, sweep, scale);
            } else {
                // Segment sweep at gaps or sharp corners to allow piecewise surfaces
                std::vector<std::vector<CurvePoint>> segments;
                const float gap2 = 1e-4f; // position jump threshold^2
                const float angleCos = std::cos(50.0f * float(M_PI) / 180.0f); // sharp corner if angle > 50 deg
                std::vector<CurvePoint> current;
                for (size_t i = 0; i < sweep.size(); ++i) {
                    if (current.empty()) { current.push_back(sweep[i]); continue; }
                    bool newSeg = false;
                    Vector3f d = sweep[i].position - current.back().position;
                    if (d.squaredNorm() > gap2) newSeg = true;
                    else {
                        Vector3f t0 = current.back().tangent; float n0 = t0.norm(); if (n0>1e-8f) t0/=n0; else t0=Vector3f::UnitY();
                        Vector3f t1 = sweep[i].tangent; float n1 = t1.norm(); if (n1>1e-8f) t1/=n1; else t1=Vector3f::UnitY();
                        float c = t0.dot(t1);
                        if (c < angleCos) newSeg = true;
                    }
                    if (newSeg) { if (current.size()>=2) segments.push_back(current); current.clear(); }
                    current.push_back(sweep[i]);
                }
                if (current.size()>=2) segments.push_back(current);
                if (segments.size()<=1) s = makeGenCyl(profile, sweep);
                else s = makeGenCylPiecewise(profile, segments);
            }
        }
        else if (surf.type == "isosurface")
            s = makeIsoSurfaceRAW(surf.volume_file, surf.dims, surf.iso, surf.spacing, surf.origin, surf.dtype);
            //break;
        //}
        size_t offset = m.positions.size();
        m.positions.resize(offset + s.positions.size());
        m.normals.resize(offset + s.normals.size());
        m.colors.resize(offset + s.normals.size());
        for (int i = 0; i < s.positions.size(); ++i) {
            m.positions[offset + i] = s.positions[i];
            m.normals[offset + i] = s.normals[i];
            m.colors[offset + i] = Vector3f(.7f, .7f, .7f);
        }
        size_t ind_offset = m.indices.size();
        m.indices.resize(m.indices.size() + s.indices.size());
        for (int i = 0; i < s.indices.size(); ++i)
            m.indices[ind_offset + i] = Vector3i(int(offset), int(offset), int(offset)) + s.indices[i];
    }

    if (!m.positions.empty()) {
        m.colorizeByCurvature();
    }

    uploadGeometryToGPU(m);
}


//------------------------------------------------------------------------

void App::renderCurves(bool draw_frames) const
{    
    // draw the tessellated curves
    Im3d::SetColor(1.0f, 1.0f, 1.0f);
    Im3d::SetSize(2.0f);
    for (size_t i = 0; i < m_render_cache.tessellated_curves.size(); ++i)
        drawCurve(m_render_cache.tessellated_curves[i], draw_frames);

    // draw control point sequences
    for (auto i = 0u; i < m_render_cache.spline_curves.size(); ++i)
    {
        const SplineCurve& c = m_render_cache.spline_curves[i];

        if(c.type == "bezier")
            Im3d::SetColor(1.0f, 1.0f, 0.0f);
        else if (c.type == "bspline")
            Im3d::SetColor(0.0f, 1.0f, 0.0f);
        else if (c.type == "circle")
            Im3d::SetColor(.6f, .6f, .6f);

        // draw larger points
        Im3d::PushSize(16.0f);
        Im3d::BeginPoints();
        for (const auto& cpt : c.control_points)
            Im3d::Vertex(cpt(0), cpt(1), cpt(2));
        Im3d::End();
        Im3d::PopSize();

        Im3d::BeginLineStrip();
        for (const auto& cpt : c.control_points)
            Im3d::Vertex(cpt(0), cpt(1), cpt(2));
        Im3d::End();
    }

    // highlight selection when editing
    if (m_curve_edit_mode && m_edit_curve_idx >= 0 && m_edit_curve_idx < (int)m_render_cache.spline_curves.size())
    {
        const SplineCurve& cur = m_render_cache.spline_curves[m_edit_curve_idx];
        if (m_edit_point_idx >= 0 && m_edit_point_idx < (int)cur.control_points.size())
        {
            Vector3f p = cur.control_points[m_edit_point_idx];
            Im3d::PushSize(20.0f); Im3d::SetColor(1.0f, 0.2f, 0.2f); Im3d::BeginPoints(); Im3d::Vertex(p); Im3d::End(); Im3d::PopSize();
        }
    }
}

std::tuple<int,int> App::pickTriangle(const MeshWithConnectivity& m, const Camera& cam, int window_width, int window_height, float mousex, float mousey) const
{
    Matrix4f view_to_world = cam.GetModelview().inverse();
    Matrix4f clip_to_view = cam.GetPerspective().inverse();
    Vector3f o = view_to_world.block(0, 3, 3, 1); // ray origin

    Vector4f clipxy{  2.0f * float(mousex) / window_width - 1.0f,
                     -2.0f * float(mousey) / window_height + 1.0f,
                      1.0f,
                      1.0f };

    Vector4f e4 = view_to_world * clip_to_view * clipxy;    // end point
    Vector3f d = e4({ 0, 1, 2 }) / e4(3);   // project

    d -= o;

    return m.pickTriangle(o, d);
}

void App::screenshot (const string& name) {
    //// Capture image.
    //const Vec2i& size = window_.getGL()->getViewSize();
    //Image image(size, ImageFormat::R8_G8_B8_A8);
    //glUseProgram(0);
    //glWindowPos2i(0, 0);
    //glReadPixels(0, 0, size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, image.getMutablePtr());

    //// Display the captured image immediately.
    //for (int i = 0; i < 3; ++i) {
    //    glDrawPixels(size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, image.getPtr());
    //    window_.getGL()->swapBuffers();
    //}
    //glDrawPixels(size.x, size.y, GL_RGBA, GL_UNSIGNED_BYTE, image.getPtr());

    //// Export.
    //image.flipY();
    //exportImage(name, &image);
    //printf("Saved screenshot to '%s'", name.getPtr());
}

//------------------------------------------------------------------------

void App::handleDrop(GLFWwindow* window, int count, const char** paths) {}
void App::handleKeypress(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        if (key == GLFW_KEY_O)
            decreaseUIScale();
        else if (key == GLFW_KEY_P)
            increaseUIScale();
        else if (key == GLFW_KEY_W)
            m_state.wireframe = !m_state.wireframe;
        else if (key == GLFW_KEY_F)
            m_state.draw_frames = !m_state.draw_frames;
        else if (key == GLFW_KEY_D)
            m_debug_subdivision = !m_debug_subdivision;
        else if (key == GLFW_KEY_S)
            m_state.show_surface = !m_state.show_surface;
        else if (key == GLFW_KEY_B)
            m_state.crude_boundaries = !m_state.crude_boundaries;
        else if (key == GLFW_KEY_1)
            m_state.mode = DrawMode::Curves;
        else if (key == GLFW_KEY_2)
            m_state.mode = DrawMode::Subdivision;
        else if (key == GLFW_KEY_3)
            m_state.mode = DrawMode::Subdivision_R3;
        else if (key == GLFW_KEY_4)
            m_state.mode = DrawMode::Subdivision_R3_R4;
        else if (key == GLFW_KEY_SPACE)
            std::cout << nlohmann::json(m_state).dump(4) << "\n";
        else if (key == GLFW_KEY_KP_ADD)
            m_state.subdivision++;
        else if (key == GLFW_KEY_KP_SUBTRACT)
            m_state.subdivision = (m_state.subdivision>0) ? (m_state.subdivision-1) : 0;
        else if (key == GLFW_KEY_L)
            handleLoading();
        else if (key == GLFW_KEY_LEFT_ALT)
            m_toggle_onering = true;
    }
    if (action == GLFW_RELEASE){
        if (key == GLFW_KEY_LEFT_ALT){
            m_toggle_onering = false;
        }
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

void App::handleLoading()
{
    string filename;

    if (m_state.mode == DrawMode::Curves)
        filename = fileOpenDialog("JSON curve specification file", "json");
        //filename = fileOpenDialog("SWP curve specification file", "swp");
    else {
        filename = fileOpenDialog("OBJ mesh file", "obj");
        m_state.subdivision = 0;
    }

    if (filename != "")
        m_state.filename = filename;
}

void App::handleMouseButton(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        if (m_state.mode == DrawMode::Curves && m_curve_edit_mode && button == GLFW_MOUSE_BUTTON_LEFT)
        {
            int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
            if (m_edit_curve_idx >= 0 && m_edit_curve_idx < (int)m_render_cache.spline_curves.size())
            {
                bool allowAddRemove = (m_render_cache.spline_curves[m_edit_curve_idx].type == "catmull-rom");
                // pick existing point; if none and Shift pressed, add new point
                int picked = pickControlPointScreen(m_edit_curve_idx, fbw, fbh, x, y);
                if (picked >= 0) { m_edit_point_idx = picked; m_dragging_point = true; }
                else if ((mods & GLFW_MOD_SHIFT) != 0 && allowAddRemove)
                {
                    Vector3f o = screenToRayOrigin(m_state.camera);
                    Vector3f d = screenToRayDir(m_state.camera, fbw, fbh, x, y);
                    Vector3f hit; if (intersectPlaneZ0(o, d, hit)) {
                        m_render_cache.spline_curves[m_edit_curve_idx].control_points.push_back(hit);
                        m_edit_point_idx = (int)m_render_cache.spline_curves[m_edit_curve_idx].control_points.size()-1;
                        retriangulateAfterEdit();
                        m_dragging_point = true;
                    }
                }
            }
        }
        else
        {
            m_state.camera.MouseClick(button, x, y);
        }
    }
    else if (action == GLFW_RELEASE)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        if (m_state.mode == DrawMode::Curves && m_curve_edit_mode && button == GLFW_MOUSE_BUTTON_LEFT)
        {
            m_dragging_point = false;
        }
        else
        {
            m_state.camera.MouseRelease(x, y);
        }
    }

}

void App::handleMouseMovement(GLFWwindow* window, double xpos, double ypos)
{
    if (m_state.mode == DrawMode::Curves && m_curve_edit_mode && m_dragging_point && m_edit_curve_idx >= 0 && m_edit_point_idx >= 0)
    {
        int fbw, fbh; glfwGetFramebufferSize(window, &fbw, &fbh);
        Vector3f o = screenToRayOrigin(m_state.camera);
        Vector3f d = screenToRayDir(m_state.camera, fbw, fbh, xpos, ypos);
        Vector3f hit; if (intersectPlaneZ0(o, d, hit))
        {
            auto &cp = m_render_cache.spline_curves[m_edit_curve_idx].control_points[m_edit_point_idx];
            cp = hit;
            retriangulateAfterEdit();
        }
    }
    else
    {
        m_state.camera.MouseDrag(xpos, ypos);
    }
}

// ---------- Curve editor helpers ----------
Vector3f App::screenToRayOrigin(const Camera& cam) const
{
    Matrix4f view_to_world = cam.GetModelview().inverse();
    return view_to_world.block(0, 3, 3, 1);
}

Vector3f App::screenToRayDir(const Camera& cam, int fbWidth, int fbHeight, double mousex, double mousey) const
{
    Matrix4f clip_to_view = cam.GetPerspective().inverse();
    Matrix4f view_to_world = cam.GetModelview().inverse();
    Vector4f clipxy{  2.0f * float(mousex) / float(fbWidth) - 1.0f,
                     -2.0f * float(mousey) / float(fbHeight) + 1.0f,
                      1.0f,
                      1.0f };
    Vector4f e4 = view_to_world * clip_to_view * clipxy;    // end point in world
    Vector3f o = view_to_world.block(0, 3, 3, 1);
    Vector3f d = e4.head<3>() / e4(3) - o;
    return d;
}

bool App::intersectPlaneZ0(const Vector3f& o, const Vector3f& d, Vector3f& hit) const
{
    if (std::abs(d.z()) < 1e-12f) return false;
    float t = -o.z() / d.z();
    if (t < 0.0f) return false;
    hit = o + t * d;
    return std::isfinite(hit.x()) && std::isfinite(hit.y()) && std::isfinite(hit.z());
}

int App::pickControlPointScreen(int curveIdx, int fbWidth, int fbHeight, double mousex, double mousey) const
{
    if (curveIdx < 0 || curveIdx >= (int)m_render_cache.spline_curves.size()) return -1;
    const auto& cur = m_render_cache.spline_curves[curveIdx];
    if (cur.control_points.empty()) return -1;
    Matrix4f world_to_view = m_state.camera.GetModelview();
    Matrix4f view_to_clip = m_state.camera.GetPerspective();
    float bestDist2 = (m_pick_radius_pixels * m_pick_radius_pixels);
    int bestIdx = -1;
    for (int i = 0; i < (int)cur.control_points.size(); ++i)
    {
        Vector4f w; w << cur.control_points[i], 1.0f;
        Vector4f clip = view_to_clip * world_to_view * w;
        if (std::abs(clip.w()) < 1e-12f) continue;
        Vector3f ndc = clip.head<3>() / clip.w();
        float sx = (ndc.x() * 0.5f + 0.5f) * float(fbWidth);
        float sy = (-ndc.y() * 0.5f + 0.5f) * float(fbHeight);
        float dx = float(mousex) - sx;
        float dy = float(mousey) - sy;
        float d2 = dx*dx + dy*dy;
        if (d2 <= bestDist2) { bestDist2 = d2; bestIdx = i; }
    }
    return bestIdx;
}

void App::retriangulateAfterEdit() const
{
    tessellateCurves(m_state.spline_tessellation);
    if (!m_render_cache.surfaces.empty())
        m_surfaces_dirty = true;
}
