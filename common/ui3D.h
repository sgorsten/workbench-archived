// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef UI_3D_H
#define UI_3D_H

#include "draw.h"
#include "ui.h"
#include "geometry.h"

struct camera
{
    float yfov, near_clip, far_clip;
    float3 position;
    float pitch, yaw;
    float4 get_orientation() const { return qmul(rotation_quat(float3(0,1,0), yaw), rotation_quat(float3(1,0,0), pitch)); }
    float4x4 get_view_matrix() const { return mul(rotation_matrix(qconj(get_orientation())), translation_matrix(-position)); }
    float4x4 get_projection_matrix(const rect & viewport) const { return linalg::perspective_matrix(yfov, viewport.aspect_ratio(), near_clip, far_clip); }
    float4x4 get_viewproj_matrix(const rect & viewport) const { return mul(get_projection_matrix(viewport), get_view_matrix()); }
    ray get_ray_from_pixel(const float2 & pixel, const rect & viewport) const;
};

struct gizmo_resources
{
    geometry_mesh geomeshes[9];
    std::shared_ptr<const gfx::program> program;
    std::shared_ptr<const gfx::mesh> meshes[9];
};

enum class gizmo_mode { none, translate_x, translate_y, translate_z, translate_yz, translate_zx, translate_xy, rotate_yz, rotate_zx, rotate_xy };

struct gui3D
{
    gui & g;

    gizmo_resources gizmo_res;
    bool bf, bl, bb, br, ml, mr;    // Instantaneous state of WASD keys and left/right mouse buttons
    float timestep;                 // Timestep between the last frame and this one

    draw_list draw;                 // Draw list for 3D gizmos and the like
    rect viewport3d;                // Current 3D viewport used to render the scene
    camera cam;                     // Current 3D camera used to render the scene
    gizmo_mode gizmode;             // Mode that the gizmo is currently in

    float3 original_position;       // Original position of an object being manipulated with a gizmo
    float4 original_orientation;    // Original orientation of an object being manipulated with a gizmo
    float3 click_offset;            // Offset from position of grabbed object to coordinates of clicked point

    gui3D(gui & g);

    void begin_frame();

    // API for doing computations in 3D space
    float4x4 get_view_matrix() const { return cam.get_view_matrix(); }
    float4x4 get_projection_matrix() const { return cam.get_projection_matrix(viewport3d); }
    float4x4 get_viewproj_matrix() const { return cam.get_viewproj_matrix(viewport3d); }
    ray get_ray_from_cursor() const { return cam.get_ray_from_pixel(g.in.cursor, viewport3d); }
};

// 3D manipulation interactions
void plane_translation_dragger(gui3D & g, const float3 & plane_normal, float3 & point);
void axis_translation_dragger(gui3D & g, const float3 & axis, float3 & point);
void position_gizmo(gui3D & g, int id, float3 & position);
void orientation_gizmo(gui3D & g, int id, const float3 & center, float4 & orientation);

#endif