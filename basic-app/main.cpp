#include "linalg.h"

#include <cmath>

using namespace linalg::aliases;

float4 rotation_quat(const float3 & axis, float angle)                          { return {axis*std::sin(angle/2), std::cos(angle/2)}; }
float4x4 translation_matrix(const float3 & translation)                         { return {{1,0,0,0},{0,1,0,0},{0,0,1,0},{translation,1}}; }
float4x4 rotation_matrix(const float4 & rotation)                               { return {{qxdir(rotation),0}, {qydir(rotation),0}, {qzdir(rotation),0}, {0,0,0,1}}; }
float4x4 frustum_matrix(float l, float r, float b, float t, float n, float f)   { return {{2*n/(r-l),0,0,0}, {0,2*n/(t-b),0,0}, {(r+l)/(r-l),(t+b)/(t-b),-(f+n)/(f-n),-1}, {0,0,-2*f*n/(f-n),0}}; }
float4x4 perspective_matrix(float fovy, float aspect, float n, float f)         { float y = n*std::tan(fovy/2), x=y*aspect; return frustum_matrix(-x,x,-y,y,n,f); }

struct ray { float3 origin, direction; };

bool intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2, float * hit_t = 0, float2 * hit_uv = 0)
{
    auto e1 = v1 - v0, e2 = v2 - v0, h = cross(ray.direction, e2);
    auto a = dot(e1, h);
    if (a > -0.0001f && a < 0.0001f) return {false};

    float f = 1 / a;
    auto s = ray.origin - v0;
	auto u = f * dot(s, h);
	if (u < 0 || u > 1) return false;

	auto q = cross(s, e1);
	auto v = f * dot(ray.direction, q);
	if (v < 0 || u + v > 1) return false;

    auto t = f * dot(e2, q);
    if(t < 0) return false;

    if(hit_t) *hit_t = t;
    if(hit_uv) *hit_uv = {u,v};
    return true;
}

#include <vector>

struct geometry_vertex { float3 position, normal; float2 texcoords; };
struct geometry_mesh { std::vector<geometry_vertex> vertices; std::vector<int3> triangles; };

bool intersect_ray_mesh(const ray & ray, const geometry_mesh & mesh, float * hit_t = 0, int * hit_tri = 0, float2 * hit_uv = 0)
{
    float best_t = std::numeric_limits<float>::infinity(), t;
    float2 best_uv, uv;
    int best_tri = -1;
    for(auto & tri : mesh.triangles)
    {
        if(intersect_ray_triangle(ray, mesh.vertices[tri[0]].position, mesh.vertices[tri[1]].position, mesh.vertices[tri[2]].position, &t, &uv) && t < best_t)
        {
            best_t = t;
            best_uv = uv;
            best_tri = &tri - mesh.triangles.data();
        }
    }
    if(best_tri == -1) return false;
    if(hit_t) *hit_t = best_t;
    if(hit_tri) *hit_tri = best_tri;
    if(hit_uv) *hit_uv = best_uv;
    return true;
}

geometry_mesh make_box_geometry(const float3 & min_bounds, const float3 & max_bounds)
{
    const auto a = min_bounds, b = max_bounds;
    geometry_mesh mesh;
    mesh.vertices = {
        {{a.x, a.y, a.z}, {-1,0,0}, {0,0}},
        {{a.x, a.y, b.z}, {-1,0,0}, {1,0}},
        {{a.x, b.y, b.z}, {-1,0,0}, {1,1}},
        {{a.x, b.y, a.z}, {-1,0,0}, {0,1}},
        {{b.x, a.y, a.z}, {+1,0,0}, {0,0}},
        {{b.x, b.y, a.z}, {+1,0,0}, {1,0}},
        {{b.x, b.y, b.z}, {+1,0,0}, {1,1}},
        {{b.x, a.y, b.z}, {+1,0,0}, {0,1}},
        {{a.x, a.y, a.z}, {0,-1,0}, {0,0}},
        {{b.x, a.y, a.z}, {0,-1,0}, {1,0}},
        {{b.x, a.y, b.z}, {0,-1,0}, {1,1}},
        {{a.x, a.y, b.z}, {0,-1,0}, {0,1}},
        {{a.x, b.y, a.z}, {0,+1,0}, {0,0}},
        {{a.x, b.y, b.z}, {0,+1,0}, {1,0}},
        {{b.x, b.y, b.z}, {0,+1,0}, {1,1}},
        {{b.x, b.y, a.z}, {0,+1,0}, {0,1}},
        {{a.x, a.y, a.z}, {0,0,-1}, {0,0}},
        {{a.x, b.y, a.z}, {0,0,-1}, {1,0}},
        {{b.x, b.y, a.z}, {0,0,-1}, {1,1}},
        {{b.x, a.y, a.z}, {0,0,-1}, {0,1}},
        {{a.x, a.y, b.z}, {0,0,+1}, {0,0}},
        {{b.x, a.y, b.z}, {0,0,+1}, {1,0}},
        {{b.x, b.y, b.z}, {0,0,+1}, {1,1}},
        {{a.x, b.y, b.z}, {0,0,+1}, {0,1}},
    };
    mesh.triangles = {{0,1,2}, {0,2,3}, {4,5,6}, {4,6,7}, {8,9,10}, {8,10,11}, {12,13,14}, {12,14,15}, {16,17,18}, {16,18,19}, {20,21,22}, {20,22,23}};
    return mesh;
}

#include <cstdlib>
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")

void gl_load_matrix(const float4x4 & m) { glLoadMatrixf(&m.x.x); }
void gl_tex_coord(const float2 & v) { glTexCoord2fv(begin(v)); }
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

struct gui
{
    int2 window_size;               // Size in pixels of the current window
    bool bf, bl, bb, br, ml, mr;    // Instantaneous state of WASD keys and left/right mouse buttons
    bool ml_down, ml_up;            // Whether the left mouse was pressed or released during this frame
    float2 cursor, delta;           // Current pixel coordinates of cursor, as well as the amount by which the cursor has moved
    float timestep;                 // Timestep between the last frame and this one

    camera cam;                     // Current 3D camera used to render the scene
    int focus_id;                   // ID of object which has the focus
    float3 original_position;       // Original position of an object being manipulated with a gizmo

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
    if(g.ml_down) g.original_position = point;

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
        const float3 forward = -qzdir(g.cam.get_orientation());
        const float3 plane_tangent = cross(axis, forward);
        const float3 plane_normal = cross(axis, plane_tangent);
        plane_translation_gizmo(g, plane_normal, point);

        // Constrain object motion to be along the desired axis
        point = g.original_position + axis * dot(point - g.original_position, axis);
    }
}

gui g;
float3 box_position = {-1,0,0};

#include <iostream>

int main(int argc, char * argv[])
{
    g.window_size = {1280,720};
    g.cam.yfov = 1.0f;
    g.cam.near_clip = 0.1f;
    g.cam.far_clip = 16.0f;
    g.cam.position = {0,0,4};

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

    auto box = make_box_geometry({-1,-1,-1}, {1,1,1});

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
        move_wasd(g, 4.0f);
        if(g.ml_down)
        {
            auto ray = g.get_ray_from_pixel(g.cursor);
            ray.origin -= box_position;
            if(intersect_ray_mesh(ray, box)) g.focus_id = 1;
            else g.focus_id = 0;
        }
        if(g.focus_id == 1)
        {
            axis_translation_gizmo(g, {1,0,0}, box_position);
        }

        int w,h;
        glfwGetFramebufferSize(win, &w, &h);
        glfwMakeContextCurrent(win);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        gl_load_matrix(perspective_matrix(1.0f, (float)w/h, 0.1f, 16.0f));

        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, begin(normalize(float4(0.1f, 0.9f, 0.3f, 0))));
        glEnable(GL_CULL_FACE);
        gl_load_matrix(g.cam.get_view_matrix() * translation_matrix(box_position));
        render_geometry(box);

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}