// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

#include <cstdlib>
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

void mesh_position_gizmo(gui & g, int id, const geometry_mesh & mesh, float3 & position)
{
    if(g.focus_id == id)
    {
        position_gizmo(g, position);
    }
    else if(g.ml_down)
    {
        auto ray = g.get_ray_from_pixel(g.cursor);
        ray.origin -= position;
        if(intersect_ray_mesh(ray, mesh))
        {
            g.focus_id = id;
            g.gizmode = gizmo_mode::none;
        }
    }
}

#include <iostream>

int main(int argc, char * argv[])
{
    gui g;
    g.window_size = {1280,720};
    g.cam.yfov = 1.0f;
    g.cam.near_clip = 0.1f;
    g.cam.far_clip = 16.0f;
    g.cam.position = {0,0,4};

    auto box_mesh = make_box_geometry({-0.5f,-0.5f,-0.5f}, {0.5f,0.5f,0.5f});
    std::vector<float3> boxes = {{-1,0,0}, {+1,0,0}};

    glfwInit();
    auto win = glfwCreateWindow(g.window_size.x, g.window_size.y, "Basic Workbench App", nullptr, nullptr);
    glfwSetWindowUserPointer(win, &g);
    glfwSetWindowSizeCallback(win, [](GLFWwindow * win, int w, int h)
    { 
        auto * g = reinterpret_cast<gui *>(glfwGetWindowUserPointer(win));
        g->window_size = {w,h}; 
    });
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


    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        g.delta = {0,0};
        g.ml_down = g.ml_up = false;
        glfwPollEvents();

        const double t1 = glfwGetTime();
        g.timestep = static_cast<float>(t1-t0);
        t0 = t1;

        if(g.mr) do_mouselook(g, 0.01f);
        move_wasd(g, 8.0f);
        for(auto & box : boxes) mesh_position_gizmo(g, &box - boxes.data() + 1, box_mesh, box);

        int w,h;
        glfwGetFramebufferSize(win, &w, &h);
        glfwMakeContextCurrent(win);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        gl_load_matrix(perspective_matrix(1.0f, (float)w/h, 0.1f, 16.0f));

        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, begin(normalize(float4(0.1f, 0.9f, 0.3f, 0))));

        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glMatrixMode(GL_MODELVIEW);
        for(auto & box : boxes)
        {            
            gl_load_matrix(g.cam.get_view_matrix() * translation_matrix(box));
            glColor3f(1,1,1); render_geometry(box_mesh);
        }

        if(g.focus_id)
        {
            glClear(GL_DEPTH_BUFFER_BIT);
            gl_load_matrix(g.cam.get_view_matrix() * translation_matrix(g.gizmo_position));
            gl_color(g.gizmode == gizmo_mode::translate_x ? float3(1,0.5f,0.5f) : float3(1,0,0)); render_geometry(g.gizmo_meshes[0]);
            gl_color(g.gizmode == gizmo_mode::translate_y ? float3(0.5f,1,0.5f) : float3(0,1,0)); render_geometry(g.gizmo_meshes[1]);
            gl_color(g.gizmode == gizmo_mode::translate_z ? float3(0.5f,0.5f,1) : float3(0,0,1)); render_geometry(g.gizmo_meshes[2]);
            gl_color(g.gizmode == gizmo_mode::translate_yz ? float3(0.5f,1,1) : float3(0,1,1)); render_geometry(g.gizmo_meshes[3]);
            gl_color(g.gizmode == gizmo_mode::translate_zx ? float3(1,0.5f,1) : float3(1,0,1)); render_geometry(g.gizmo_meshes[4]);
            gl_color(g.gizmode == gizmo_mode::translate_xy ? float3(1,1,0.5f) : float3(1,1,0)); render_geometry(g.gizmo_meshes[5]);
        }

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}