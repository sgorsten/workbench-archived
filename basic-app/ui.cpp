// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

#include <cassert>
#include <cctype>
#include <algorithm>

ray camera::get_ray_from_pixel(const float2 & pixel, const int2 & viewport) const
{
    const float x = 2 * pixel.x / viewport.x - 1, y = 1 - 2 * pixel.y / viewport.y;
    const float4x4 inv_view_proj = inverse(get_viewproj_matrix((float)viewport.x/viewport.y));
    const float4 p0 = inv_view_proj * float4(x, y, -1, 1), p1 = inv_view_proj * float4(x, y, +1, 1);
    return {position, p1.xyz()*p0.w - p0.xyz()*p1.w};
}

gui::gui() : bf(), bl(), bb(), br(), ml(), mr(), ml_down(), ml_up(), timestep(), cam({}), gizmode() 
{
    std::vector<int> codepoints;
    for(int i=0; i<128; ++i) if(isprint(i)) codepoints.push_back(i);
    default_font.load_glyphs("c:/windows/fonts/arialbd.ttf", 14, codepoints);
    for(int i=1; i<=32; ++i) default_font.make_circle_quadrant(i);
    default_font.prepare_texture();

    gizmo_meshes[0] = make_cylinder_geometry({1,0,0}, {0,0.05f,0}, {0,0,0.05f}, 12);
    gizmo_meshes[1] = make_cylinder_geometry({0,1,0}, {0,0,0.05f}, {0.05f,0,0}, 12);
    gizmo_meshes[2] = make_cylinder_geometry({0,0,1}, {0.05f,0,0}, {0,0.05f,0}, 12);
    gizmo_meshes[3] = make_box_geometry({-0.01f,0,0}, {0.01f,0.4f,0.4f});
    gizmo_meshes[4] = make_box_geometry({0,-0.01f,0}, {0.4f,0.01f,0.4f});
    gizmo_meshes[5] = make_box_geometry({0,0,-0.01f}, {0.4f,0.4f,0.01f});
}

void gui::begin_frame()
{
    vertices.clear();
    lists = {{0, 0}};
}

void gui::end_frame()
{
    lists.back().last = vertices.size();
    std::stable_sort(begin(lists), end(lists), [](const list & a, const list & b) { return a.level < b.level; });
}

void gui::begin_overlay()
{
    lists.back().last = vertices.size();
    lists.push_back({lists.back().level + 1, vertices.size()});
}

void gui::end_overlay()
{
    lists.back().last = vertices.size();
    lists.push_back({lists.back().level - 1, vertices.size()});
}

void gui::add_glyph(const rect & r, float s0, float t0, float s1, float t1, const float4 & top_color, const float4 & bottom_color)
{
    const auto c0 = byte4(top_color * 255.0f), c1 = byte4(bottom_color * 255.0f);
    vertices.push_back({short2(r.x0, r.y0), c0, {s0, t0}});
    vertices.push_back({short2(r.x1, r.y0), c0, {s1, t0}});
    vertices.push_back({short2(r.x1, r.y1), c1, {s1, t1}});
    vertices.push_back({short2(r.x0, r.y1), c1, {s0, t1}});
}

void draw_rect(gui & g, const rect & r, const float4 & color) { draw_rect(g, r, color, color); }
void draw_rect(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const float s = 0.5f / g.default_font.tex_size.x, t = 0.5f / g.default_font.tex_size.y;
    g.add_glyph(r, s, t, s, t, top_color, bottom_color);
}

void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & color) { draw_rounded_rect(g, r, radius, color, color); }
void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & top_color, const float4 & bottom_color)
{
    assert(radius >= 0);

    auto c0 = top_color;
    auto c1 = lerp(top_color, bottom_color, (float)radius/r.height());
    auto c2 = lerp(bottom_color, top_color, (float)radius/r.height());
    auto c3 = bottom_color;

    draw_rect(g, {r.x0+radius, r.y0, r.x1-radius, r.y0+radius}, c0, c1);
    draw_rect(g, {r.x0, r.y0+radius, r.x1, r.y1-radius}, c1, c2);        
    draw_rect(g, {r.x0+radius, r.y1-radius, r.x1-radius, r.y1}, c2, c3);

    if(auto * glyph = g.default_font.get_glyph(-radius))
    {
        g.add_glyph({r.x0, r.y0, r.x0+radius, r.y0+radius}, glyph->s1, glyph->t1, glyph->s0, glyph->t0, c0, c1);
        g.add_glyph({r.x1-radius, r.y0, r.x1, r.y0+radius}, glyph->s0, glyph->t1, glyph->s1, glyph->t0, c0, c1);
        g.add_glyph({r.x0, r.y1-radius, r.x0+radius, r.y1}, glyph->s1, glyph->t0, glyph->s0, glyph->t1, c2, c3);
        g.add_glyph({r.x1-radius, r.y1-radius, r.x1, r.y1}, glyph->s0, glyph->t0, glyph->s1, glyph->t1, c2, c3);
    }
}

void draw_text(gui & g, int2 p, const float4 & c, const std::string & text)
{         
    for(auto ch : text)
    {
        if(auto * glyph = g.default_font.get_glyph(ch))
        {
            const int2 p0 = p + glyph->offset, p1 = p0 + glyph->dims;
            g.add_glyph({p0.x, p0.y, p1.x, p1.y}, glyph->s0, glyph->t0, glyph->s1, glyph->t1, c, c);
            p.x += glyph->advance;
        }
    }
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
        const ray ray = g.get_ray_from_cursor();

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
        auto ray = g.get_ray_from_cursor();
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