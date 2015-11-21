// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

gui::gui() : bf(), bl(), bb(), br(), ml(), mr(), ml_down(), ml_up(), timestep(), cam({}), gizmode() 
{
    gizmo_meshes[0] = make_cylinder_geometry({1,0,0}, {0,0.05f,0}, {0,0,0.05f}, 12);
    gizmo_meshes[1] = make_cylinder_geometry({0,1,0}, {0,0,0.05f}, {0.05f,0,0}, 12);
    gizmo_meshes[2] = make_cylinder_geometry({0,0,1}, {0.05f,0,0}, {0,0.05f,0}, 12);
    gizmo_meshes[3] = make_box_geometry({-0.01f,0,0}, {0.01f,0.4f,0.4f});
    gizmo_meshes[4] = make_box_geometry({0,-0.01f,0}, {0.4f,0.01f,0.4f});
    gizmo_meshes[5] = make_box_geometry({0,0,-0.01f}, {0.4f,0.4f,0.01f});
}

ray gui::get_ray_from_pixel(const float2 & coord) const
{
    const float x = 2 * cursor.x / window_size.x - 1, y = 1 - 2 * cursor.y / window_size.y;
    const float4x4 inv_view_proj = inverse(get_viewproj_matrix());
    const float4 p0 = inv_view_proj * float4(x, y, -1, 1), p1 = inv_view_proj * float4(x, y, +1, 1);
    return {cam.position, p1.xyz()*p0.w - p0.xyz()*p1.w};
}

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
}