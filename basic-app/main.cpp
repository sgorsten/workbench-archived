// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

#include <cassert>
#include <cstdlib>
#include <set>
#include <sstream>
#include <algorithm>
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")

void gl_load_matrix(const float4x4 & m) { glLoadMatrixf(&m.x.x); }
void gl_tex_coord(const float2 & v) { glTexCoord2fv(begin(v)); }
void gl_color(const float3 & v) { glColor3fv(begin(v)); }
void gl_normal(const float3 & v) { glNormal3fv(begin(v)); }
void gl_vertex(const float3 & v) { glVertex3fv(begin(v)); }

void render_geometry(const geometry_mesh & mesh)
{
    glBegin(GL_TRIANGLES);
    for(auto & tri : mesh.triangles)
    {
        for(auto index : tri)
        {
            auto & vert = mesh.vertices[index];
            gl_tex_coord(vert.texcoords);
            gl_normal(vert.normal);
            gl_vertex(vert.position);
        }
    }
    glEnd();
}

void render_gizmo(const gui & g)
{
    gl_color(g.gizmode == gizmo_mode::translate_x ? float3(1,0.5f,0.5f) : float3(1,0,0)); render_geometry(g.gizmo_meshes[0]);
    gl_color(g.gizmode == gizmo_mode::translate_y ? float3(0.5f,1,0.5f) : float3(0,1,0)); render_geometry(g.gizmo_meshes[1]);
    gl_color(g.gizmode == gizmo_mode::translate_z ? float3(0.5f,0.5f,1) : float3(0,0,1)); render_geometry(g.gizmo_meshes[2]);
    gl_color(g.gizmode == gizmo_mode::translate_yz ? float3(0.5f,1,1) : float3(0,1,1)); render_geometry(g.gizmo_meshes[3]);
    gl_color(g.gizmode == gizmo_mode::translate_zx ? float3(1,0.5f,1) : float3(1,0,1)); render_geometry(g.gizmo_meshes[4]);
    gl_color(g.gizmode == gizmo_mode::translate_xy ? float3(1,1,0.5f) : float3(1,1,0)); render_geometry(g.gizmo_meshes[5]);
}

struct scene_object
{
    std::string name;
    const geometry_mesh * mesh;
    float3 position;
};

scene_object * raycast(const ray & ray, std::vector<scene_object> & objects)
{
    scene_object * best = nullptr;
    float t, best_t;
    for(auto & obj : objects)
    {
        auto r = ray;
        r.origin -= obj.position;
        if(intersect_ray_mesh(r, *obj.mesh, &t) && (!best || t < best_t))
        {
            best = &obj;
            best_t = t;
        }
    }
    return best;
}

float3 get_center_of_mass(const std::set<scene_object *> & objects)
{
    float3 sum;
    for(auto obj : objects) sum += obj->position;
    return sum / (float)objects.size();
}

void object_list_ui(gui & g, int id, const rect & r, std::vector<scene_object> & objects, std::set<scene_object *> & selection, int & offset)
{
    auto panel = vscroll_panel(g, id, r, objects.size()*30+20, offset);
    g.begin_childen(id);
    g.begin_scissor(panel);
    int y0 = panel.y0 + 10 - offset;
    for(auto & obj : objects)
    {
        const rect list_entry = {panel.x0 + 10, y0, panel.x1 - 10, y0 + 30};
        if(g.check_click(&obj - objects.data(), list_entry))
        {
            if(!g.ctrl) selection.clear();
            auto it = selection.find(&obj);
            if(it == end(selection)) selection.insert(&obj);
            else selection.erase(it);
            g.gizmode = gizmo_mode::none;
        }

        bool selected = selection.find(&obj) != end(selection);
        draw_text(g, {list_entry.x0 + 1, list_entry.y0 + 1}, float4(0,0,0,1), obj.name);
        draw_text(g, {list_entry.x0, list_entry.y0}, selected ? float4(1,1,0,1) : float4(1,1,1,1), obj.name);
        y0 += 30;
    }
    g.end_scissor();
    g.end_children();
}

void object_properties_ui(gui & g, int id, const rect & r, std::set<scene_object *> & selection, int & offset)
{
    if(selection.size() != 1) return;

    auto & obj = **selection.begin();

    auto panel = vscroll_panel(g, id, r, 1000, offset); // TODO: Determine correct size
    g.begin_childen(id);
    g.begin_scissor(panel);
    int y0 = panel.y0 + 10 - offset;

    int line_height = g.default_font.line_height + 4;
    edit(g, 1, {panel.x0 + 10, y0, panel.x1 - 10, y0 + line_height}, obj.name);
    y0 += line_height + 2;
    edit(g, 2, {panel.x0 + 10, y0, panel.x1 - 10, y0 + line_height}, obj.position);
    y0 += line_height + 2;

    g.end_scissor();
    g.end_children();
}

void viewport_ui(gui & g, int id, const rect & r, std::vector<scene_object> & objects, std::set<scene_object *> & selection)
{
    if(!selection.empty())
    {
        g.begin_childen(id);
        float3 com = get_center_of_mass(selection), new_com = com;
        position_gizmo(g, 1, new_com);
        if(new_com != com) for(auto obj : selection) obj->position += new_com - com;
        g.end_children();
    }

    if(g.ml_down && g.is_cursor_over(r))
    {
        if(!g.is_child_pressed(id)) g.set_pressed(id);

        if(!selection.empty() && g.gizmode == gizmo_mode::none && !g.ctrl) selection.clear();

        if(selection.empty() || g.ctrl)
        {
            if(auto picked_object = raycast(g.get_ray_from_cursor(), objects))
            {
                auto it = selection.find(picked_object);
                if(it == end(selection)) selection.insert(picked_object);
                else selection.erase(it);
                g.gizmode = gizmo_mode::none;
            }
        }
    }

    if(g.is_pressed(id))
    {
        if(g.mr) do_mouselook(g, 0.01f);
        move_wasd(g, 8.0f);
    }
}

void begin_3d(GLFWwindow * win, const rect & r, gui & g)
{
    int fw, fh, w, h;
    glfwGetFramebufferSize(win, &fw, &fh);
    glfwGetWindowSize(win, &w, &h);
    const int multiplier = fw / w;
    assert(w * multiplier == fw);
    assert(h * multiplier == fh);

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glViewport(r.x0 * multiplier, r.y0 * multiplier, r.width() * multiplier, r.height() * multiplier);
    glEnable(GL_SCISSOR_TEST);
    glScissor(r.x0 * multiplier, r.y0 * multiplier, r.width() * multiplier, r.height() * multiplier);

    g.viewport3d = r;
}

void end_3d()
{
    glPopAttrib();
}

int main(int argc, char * argv[])
{
    gui g;
    g.cam.yfov = 1.0f;
    g.cam.near_clip = 0.1f;
    g.cam.far_clip = 16.0f;
    g.cam.position = {0,1.5f,4};

    const auto box = make_box_geometry({-0.4f,0.0f,-0.4f}, {0.4f,0.8f,0.4f});
    const auto cylinder = make_cylinder_geometry({0,1,0}, {0,0,0.4f}, {0.4f,0,0}, 24);
    std::vector<scene_object> objects = {{"Box", &box, {-1,0,0}}, {"Cylinder", &cylinder, {0,0,0}}, {"Box 2", &box, {+1,0,0}}};
    std::set<scene_object *> selection;
    
    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Basic Workbench App", nullptr, nullptr);
    glfwSetWindowUserPointer(win, &g);
    glfwSetCharCallback(win, [](GLFWwindow * win, unsigned int codepoint) { reinterpret_cast<gui *>(glfwGetWindowUserPointer(win))->codepoint = codepoint; });
    glfwSetScrollCallback(win, [](GLFWwindow * win, double x, double y) { reinterpret_cast<gui *>(glfwGetWindowUserPointer(win))->scroll = float2(double2(x,y)); });
    glfwSetKeyCallback(win, [](GLFWwindow * win, int key, int scancode, int action, int mods)
    {
        auto * g = reinterpret_cast<gui *>(glfwGetWindowUserPointer(win));
        switch(key)
        {
        case GLFW_KEY_W: g->bf = action != GLFW_RELEASE; break;
        case GLFW_KEY_A: g->bl = action != GLFW_RELEASE; break;
        case GLFW_KEY_S: g->bb = action != GLFW_RELEASE; break;
        case GLFW_KEY_D: g->br = action != GLFW_RELEASE; break;
        }
        if(action == GLFW_RELEASE) return;
        switch(key)
        {
        case GLFW_KEY_LEFT: g->pressed_key = ::key::left; break;
        case GLFW_KEY_RIGHT: g->pressed_key = ::key::right; break;
        case GLFW_KEY_UP: g->pressed_key = ::key::up; break;
        case GLFW_KEY_DOWN: g->pressed_key = ::key::down; break;
        case GLFW_KEY_HOME: g->pressed_key = ::key::home; break;
        case GLFW_KEY_END: g->pressed_key = ::key::end; break;
        case GLFW_KEY_PAGE_UP: g->pressed_key = ::key::page_up; break;
        case GLFW_KEY_PAGE_DOWN: g->pressed_key = ::key::page_down; break;
        case GLFW_KEY_BACKSPACE: g->pressed_key = ::key::backspace; break;
        case GLFW_KEY_DELETE: g->pressed_key = ::key::delete_; break;
        case GLFW_KEY_ENTER: g->pressed_key = ::key::enter; break;
        case GLFW_KEY_ESCAPE: g->pressed_key = ::key::escape; break;       
        }
    });
    glfwSetMouseButtonCallback(win, [](GLFWwindow * win, int button, int action, int mods)
    {
        auto * g = reinterpret_cast<gui *>(glfwGetWindowUserPointer(win));
        g->ctrl = (mods & GLFW_MOD_CONTROL) != 0;
        g->shift = (mods & GLFW_MOD_SHIFT) != 0;
        switch(button)
        {
        case GLFW_MOUSE_BUTTON_LEFT: g->ml = action != GLFW_RELEASE; (g->ml ? g->ml_down : g->ml_up) = true; break;
        case GLFW_MOUSE_BUTTON_RIGHT: g->mr = action != GLFW_RELEASE; break;
        }
    });
    glfwSetCursorPosCallback(win, [](GLFWwindow * win, double x, double y)
    {
        auto * g = reinterpret_cast<gui *>(glfwGetWindowUserPointer(win));
        const float2 cursor = float2(double2(x,y));
        g->delta = cursor - g->cursor;
        g->cursor = cursor;
    });

    glfwMakeContextCurrent(win);

    float image[16][16];
    for(int i=0; i<16; ++i)
    {
        for(int j=0; j<16; ++j)
        {
            image[i][j] = ((i/4 + j/4) % 2) 
                ? ((i+j) % 2 ? 0.8f : 0.6f)
                : ((i+j) % 2 ? 0.2f : 0.1f);
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_LUMINANCE, GL_FLOAT, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGenTextures(1, &g.font_tex);
    glBindTexture(GL_TEXTURE_2D, g.font_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, g.default_font.tex_size.x, g.default_font.tex_size.y, 0, GL_ALPHA, GL_UNSIGNED_BYTE, g.default_font.tex_pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    int split = 358, offset0 = 0, offset1 = 0;
    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        g.scroll = g.delta = {0,0};
        g.ml_down = g.ml_up = false;
        g.codepoint = 0;
        g.pressed_key = key::none;
        glfwPollEvents();

        int fw, fh, w, h;
        glfwGetFramebufferSize(win, &fw, &fh);
        glfwGetWindowSize(win, &w, &h);

        g.begin_frame({w, h});

        const double t1 = glfwGetTime();
        g.timestep = static_cast<float>(t1-t0);
        t0 = t1;

        viewport_ui(g, 1, {0, 0, w-200, h}, objects, selection);
        auto s = vsplitter(g, 2, {w-200, 0, w, h}, split);
        object_list_ui(g, 3, s.first, objects, selection, offset0);
        object_properties_ui(g, 4, s.second, selection, offset1);
        g.end_frame();

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glViewport(0, 0, fw, fh);
        glClearColor(0.2f, 0.2f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        begin_3d(win, {0, 0, w-200, h}, g);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        gl_load_matrix(g.get_projection_matrix());

        glMatrixMode(GL_MODELVIEW);
        gl_load_matrix(g.get_view_matrix());
        glEnable(GL_DEPTH_TEST);
        glBegin(GL_LINES);
        for(int i=-5; i<=5; ++i)
        {
            glVertex3i(i, 0, -5);
            glVertex3i(i, 0, +5);
            glVertex3i(-5, 0, i);
            glVertex3i(+5, 0, i);
        }
        glEnd();

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, begin(normalize(float4(0.1f, 0.9f, 0.3f, 0))));
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_CULL_FACE);
        for(auto & obj : objects)
        {            
            gl_load_matrix(g.cam.get_view_matrix() * translation_matrix(obj.position));
            gl_color(selection.find(&obj) == end(selection) ? float3(1,1,1) : float3(1,1,0.8f)); render_geometry(*obj.mesh);
        }

        if(!selection.empty())
        {
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_TEXTURE_2D);
            gl_load_matrix(g.cam.get_view_matrix() * translation_matrix(get_center_of_mass(selection)));
            render_gizmo(g);
        }

        end_3d();

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, w, h, 0, -1, +1);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glDisable(GL_DEPTH_TEST);

        glDisable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g.font_tex);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        const auto * vertex = g.vertices.data();
        glVertexPointer(2, GL_SHORT, sizeof(*vertex), &vertex->position);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(*vertex), &vertex->color);
        glTexCoordPointer(2, GL_FLOAT, sizeof(*vertex), &vertex->texcoord);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        for(const auto & list : g.lists) glDrawArrays(GL_QUADS, list.first, list.last - list.first);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        glPopAttrib();

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}