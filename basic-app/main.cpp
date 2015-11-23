// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

#include <cstdlib>
#include <set>
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

int main(int argc, char * argv[])
{
    gui g;
    g.cam.yfov = 1.0f;
    g.cam.near_clip = 0.1f;
    g.cam.far_clip = 16.0f;
    g.cam.position = {0,1.5f,4};

    const auto box = make_box_geometry({-0.4f,0.0f,-0.4f}, {0.4f,0.8f,0.4f});
    const auto cylinder = make_cylinder_geometry({0,1,0}, {0,0,0.4f}, {0.4f,0,0}, 24);
    std::vector<scene_object> objects = {{&box, {-1,0,0}}, {&cylinder, {0,0,0}}, {&box, {+1,0,0}}};
    std::set<scene_object *> selection;

    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Basic Workbench App", nullptr, nullptr);
    glfwSetWindowUserPointer(win, &g);
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
        const float2 cursor = {static_cast<float>(x), static_cast<float>(y)};
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


    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        g.delta = {0,0};
        g.ml_down = g.ml_up = false;
        glfwPollEvents();

        int w,h;
        glfwGetWindowSize(win, &w, &h);
        g.begin_frame({w, h});

        const double t1 = glfwGetTime();
        g.timestep = static_cast<float>(t1-t0);
        t0 = t1;

        if(g.mr) do_mouselook(g, 0.01f);
        move_wasd(g, 8.0f);

        if(!selection.empty())
        {
            float3 com = get_center_of_mass(selection), new_com = com;
            position_gizmo(g, new_com);
            if(new_com != com) for(auto obj : selection) obj->position += new_com - com;
        }

        if(g.ml_down)
        {
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

        draw_rounded_rect(g, {20,20,200,50}, 4, {0.5f,0.5f,0.5f,1}, {0.3f,0.3f,0.3f,1});
        draw_text(g, {33,33}, {0,0,0,1}, "Hello world!");
        draw_text(g, {32,32}, {1,1,1,1}, "Hello world!");
        g.end_frame();

        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPushAttrib(GL_ALL_ATTRIB_BITS);

        glMatrixMode(GL_PROJECTION);
        gl_load_matrix(perspective_matrix(1.0f, (float)w/h, 0.1f, 16.0f));

        glMatrixMode(GL_MODELVIEW);
        gl_load_matrix(g.cam.get_view_matrix());
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

        glPopAttrib();

        glPushAttrib(GL_ALL_ATTRIB_BITS);
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