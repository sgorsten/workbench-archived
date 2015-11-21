#include "geometry.h"

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

struct camera
{
    float yfov, near_clip, far_clip;
    float3 position;
    float pitch, yaw;
    float4 get_orientation() const { return qmul(rotation_quat(float3(0,1,0), yaw), rotation_quat(float3(1,0,0), pitch)); }
    float4x4 get_view_matrix() const { return rotation_matrix(qconj(get_orientation())) * translation_matrix(-position); }
    float4x4 get_projection_matrix(float aspect) const { return perspective_matrix(yfov, aspect, near_clip, far_clip); }
    float4x4 get_viewproj_matrix(float aspect) const { return get_projection_matrix(aspect) * get_view_matrix(); }
};

enum class gizmo_mode { none, translate_x, translate_y, translate_z, translate_yz, translate_zx, translate_xy };

struct gui
{
    geometry_mesh gizmo_meshes[6];

    int2 window_size;               // Size in pixels of the current window
    bool bf, bl, bb, br, ml, mr;    // Instantaneous state of WASD keys and left/right mouse buttons
    bool ml_down, ml_up;            // Whether the left mouse was pressed or released during this frame
    float2 cursor, delta;           // Current pixel coordinates of cursor, as well as the amount by which the cursor has moved
    float timestep;                 // Timestep between the last frame and this one

    camera cam;                     // Current 3D camera used to render the scene
    int focus_id;                   // ID of object which has the focus
    float3 gizmo_position;          // Position in which the gizmo will be drawn
    gizmo_mode gizmode;             // Mode that the gizmo is currently in
    float3 original_position;       // Original position of an object being manipulated with a gizmo
    float3 click_offset;            // Offset from position of grabbed object to coordinates of clicked point

    float4x4 get_viewproj_matrix() const { return cam.get_viewproj_matrix((float)window_size.x/window_size.y); }

    ray get_ray_from_pixel(const float2 & coord) const
    {
        const float x = 2 * cursor.x / window_size.x - 1, y = 1 - 2 * cursor.y / window_size.y;
        const float4x4 inv_view_proj = inverse(get_viewproj_matrix());
        const float4 p0 = inv_view_proj * float4(x, y, -1, 1), p1 = inv_view_proj * float4(x, y, +1, 1);
        return {cam.position, p1.xyz()*p0.w - p0.xyz()*p1.w};
    }
};

void do_mouselook(gui & g, float sensitivity)
{
    g.cam.yaw -= g.delta.x * sensitivity;
    g.cam.pitch -= g.delta.y * sensitivity;
}

void move_wasd(gui & g, float speed)
{
    const float4 orientation = g.cam.get_orientation();
    float3 move;
    if(g.bf) move -= qzdir(orientation);
    if(g.bl) move -= qxdir(orientation);
    if(g.bb) move += qzdir(orientation);
    if(g.br) move += qxdir(orientation);
    if(mag2(move) > 0) g.cam.position += normalize(move) * (speed * g.timestep);
}

void plane_translation_gizmo(gui & g, const float3 & plane_normal, float3 & point)
{
    if(g.ml_down) { g.original_position = point; }

    if(g.ml)
    {
        // Define the plane to contain the original position of the object
        const float3 plane_point = g.original_position;

        // Define a ray emitting from the camera underneath the cursor
        const ray ray = g.get_ray_from_pixel(g.cursor);

        // If an intersection exists between the ray and the plane, place the object at that point
        const float denom = dot(ray.direction, plane_normal);
        if(std::abs(denom) == 0) return;
        const float t = dot(plane_point - ray.origin, plane_normal) / denom;
        if(t < 0) return;
        point = ray.origin + ray.direction * t;
    }
}

void axis_translation_gizmo(gui & g, const float3 & axis, float3 & point)
{
    if(g.ml)
    {
        // First apply a plane translation gizmo with a plane that contains the desired axis and is oriented to face the camera
        const float3 plane_tangent = cross(axis, point - g.cam.position);
        const float3 plane_normal = cross(axis, plane_tangent);
        plane_translation_gizmo(g, plane_normal, point);

        // Constrain object motion to be along the desired axis
        point = g.original_position + axis * dot(point - g.original_position, axis);
    }
}

void position_gizmo(gui & g, float3 & position)
{
    // On click, set the gizmo mode based on which component the user clicked on
    if(g.ml_down)
    {
        g.gizmode = gizmo_mode::none;
        auto ray = g.get_ray_from_pixel(g.cursor);
        ray.origin -= position;
        float t;           
        if(intersect_ray_mesh(ray, g.gizmo_meshes[0], &t)) g.gizmode = gizmo_mode::translate_x;
        if(intersect_ray_mesh(ray, g.gizmo_meshes[1], &t)) g.gizmode = gizmo_mode::translate_y;
        if(intersect_ray_mesh(ray, g.gizmo_meshes[2], &t)) g.gizmode = gizmo_mode::translate_z;
        if(intersect_ray_mesh(ray, g.gizmo_meshes[3], &t)) g.gizmode = gizmo_mode::translate_yz;
        if(intersect_ray_mesh(ray, g.gizmo_meshes[4], &t)) g.gizmode = gizmo_mode::translate_zx;
        if(intersect_ray_mesh(ray, g.gizmo_meshes[5], &t)) g.gizmode = gizmo_mode::translate_xy;
        if(g.gizmode != gizmo_mode::none) g.click_offset = ray.origin + ray.direction*t;
    }

    // On release, deactivate the current gizmo mode
    if(g.ml_up) g.gizmode = gizmo_mode::none;

    // If the user has previously clicked on a gizmo component, allow the user to interact with that gizmo
    if(g.gizmode != gizmo_mode::none)
    {
        position += g.click_offset;
        switch(g.gizmode)
        {
        case gizmo_mode::translate_x: axis_translation_gizmo(g, {1,0,0}, position); break;
        case gizmo_mode::translate_y: axis_translation_gizmo(g, {0,1,0}, position); break;
        case gizmo_mode::translate_z: axis_translation_gizmo(g, {0,0,1}, position); break;
        case gizmo_mode::translate_yz: plane_translation_gizmo(g, {1,0,0}, position); break;
        case gizmo_mode::translate_zx: plane_translation_gizmo(g, {0,1,0}, position); break;
        case gizmo_mode::translate_xy: plane_translation_gizmo(g, {0,0,1}, position); break;
        }        
        position -= g.click_offset;
    }

    // Store the gizmo position for later use by the renderer
    g.gizmo_position = position;
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

gui g;

#include <iostream>

int main(int argc, char * argv[])
{
    g.window_size = {1280,720};
    g.cam.yfov = 1.0f;
    g.cam.near_clip = 0.1f;
    g.cam.far_clip = 16.0f;
    g.cam.position = {0,0,4};

    g.gizmo_meshes[0] = make_cylinder_geometry({1,0,0}, {0,0.05f,0}, {0,0,0.05f}, 12);
    g.gizmo_meshes[1] = make_cylinder_geometry({0,1,0}, {0,0,0.05f}, {0.05f,0,0}, 12);
    g.gizmo_meshes[2] = make_cylinder_geometry({0,0,1}, {0.05f,0,0}, {0,0.05f,0}, 12);
    g.gizmo_meshes[3] = make_box_geometry({-0.01f,0,0}, {0.01f,0.4f,0.4f});
    g.gizmo_meshes[4] = make_box_geometry({0,-0.01f,0}, {0.4f,0.01f,0.4f});
    g.gizmo_meshes[5] = make_box_geometry({0,0,-0.01f}, {0.4f,0.4f,0.01f});

    auto box_mesh = make_box_geometry({-0.5f,-0.5f,-0.5f}, {0.5f,0.5f,0.5f});
    std::vector<float3> boxes = {{-1,0,0}, {+1,0,0}};

    glfwInit();
    auto win = glfwCreateWindow(g.window_size.x, g.window_size.y, "Basic Workbench App", nullptr, nullptr);
    glfwSetWindowSizeCallback(win, [](GLFWwindow *, int w, int h) { g.window_size = {w,h}; });
    glfwSetKeyCallback(win, [](GLFWwindow *, int key, int scancode, int action, int mods)
    {
        switch(key)
        {
        case GLFW_KEY_W: g.bf = action != GLFW_RELEASE; break;
        case GLFW_KEY_A: g.bl = action != GLFW_RELEASE; break;
        case GLFW_KEY_S: g.bb = action != GLFW_RELEASE; break;
        case GLFW_KEY_D: g.br = action != GLFW_RELEASE; break;
        }
    });
    glfwSetMouseButtonCallback(win, [](GLFWwindow *, int button, int action, int mods)
    {
        switch(button)
        {
        case GLFW_MOUSE_BUTTON_LEFT: g.ml = action != GLFW_RELEASE; (g.ml ? g.ml_down : g.ml_up) = true; break;
        case GLFW_MOUSE_BUTTON_RIGHT: g.mr = action != GLFW_RELEASE; break;
        }
    });
    glfwSetCursorPosCallback(win, [](GLFWwindow *, double x, double y)
    {
        const float2 cursor = {static_cast<float>(x), static_cast<float>(y)};
        g.delta = cursor - g.cursor;
        g.cursor = cursor;
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