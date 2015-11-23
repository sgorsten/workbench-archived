// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

#include <cassert>
#include <cctype>
#include <algorithm>
#include <sstream>

ray camera::get_ray_from_pixel(const float2 & pixel, const rect & viewport) const
{
    const float x = 2 * (pixel.x - viewport.x0) / viewport.width() - 1, y = 1 - 2 * (pixel.y - viewport.y0) / viewport.height();
    const float4x4 inv_view_proj = inverse(get_viewproj_matrix(viewport));
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

    std::initializer_list<float2> points = {{0, 0.05f}, {1, 0.05f}, {1, 0.10f}, {1.2f, 0}};
    gizmo_meshes[0] = make_lathed_geometry({1,0,0}, {0,1,0}, {0,0,1}, 12, points);
    gizmo_meshes[1] = make_lathed_geometry({0,1,0}, {0,0,1}, {1,0,0}, 12, points);
    gizmo_meshes[2] = make_lathed_geometry({0,0,1}, {1,0,0}, {0,1,0}, 12, points);
    gizmo_meshes[3] = make_box_geometry({-0.01f,0,0}, {0.01f,0.4f,0.4f});
    gizmo_meshes[4] = make_box_geometry({0,-0.01f,0}, {0.4f,0.01f,0.4f});
    gizmo_meshes[5] = make_box_geometry({0,0,-0.01f}, {0.4f,0.4f,0.01f});
}

bool gui::is_pressed(int id) const
{
    if(pressed_id.size() != current_id.size() + 1) return false;
    for(size_t i=0; i<current_id.size(); ++i) if(pressed_id[i] != current_id[i]) return false;
    return pressed_id.back() == id;
}

bool gui::is_focused(int id) const
{
    if(focused_id.size() != current_id.size() + 1) return false;
    for(size_t i=0; i<current_id.size(); ++i) if(focused_id[i] != current_id[i]) return false;
    return focused_id.back() == id;
}

bool gui::is_child_pressed(int id) const
{
    if(pressed_id.size() < current_id.size() + 2) return false;
    for(size_t i=0; i<current_id.size(); ++i) if(pressed_id[i] != current_id[i]) return false;
    return pressed_id[current_id.size()] == id;
}

void gui::set_pressed(int id)
{
    pressed_id = current_id;
    pressed_id.push_back(id);
}

void gui::clear_pressed()
{
    pressed_id.clear();
}

void gui::begin_childen(int id)
{
    current_id.push_back(id);
}

void gui::end_children()
{
    current_id.pop_back();
}

bool gui::is_cursor_over(const rect & r) const
{
    const auto & s = scissor.back();
    return cursor.x >= r.x0 && cursor.y >= r.y0 && cursor.x < r.x1 && cursor.y < r.y1
        && cursor.x >= s.x0 && cursor.y >= s.y0 && cursor.x < s.x1 && cursor.y < s.y1;
}

bool gui::check_click(int id, const rect & r)
{
    if(ml_down && is_cursor_over(r))
    {
        click_offset.x = cursor.x - r.x0;
        click_offset.y = cursor.y - r.y0;
        set_pressed(id);
        return true;
    }
    return false;
}

bool gui::check_pressed(int id)
{
    if(is_pressed(id))
    {
        if(ml_up) clear_pressed();
        else return true;
    }
    return false;
}

bool gui::check_release(int id)
{
    if(ml_up && is_pressed(id))
    {
        clear_pressed();
        return true;
    }
    return false;
}

void gui::begin_frame(const int2 & window_size)
{
    this->window_size = window_size;
    vertices.clear();
    lists = {{0, 0}};
    scissor.push_back({0, 0, window_size.x, window_size.y});
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
    scissor.push_back(scissor.front()); // Overlays are not constrained by parent scissor rect
}

void gui::end_overlay()
{
    scissor.pop_back();
    lists.back().last = vertices.size();
    lists.push_back({lists.back().level - 1, vertices.size()});
}

void gui::begin_scissor(const rect & r)
{
    const auto & s = scissor.back();
    scissor.push_back({std::max(s.x0, r.x0), std::max(s.y0, r.y0), std::min(s.x1, r.x1), std::min(s.y1, r.y1)});
}

void gui::end_scissor()
{
    scissor.pop_back();
}

float lerp(float a, float b, float t) { return a*(1-t) + b*t; }

void gui::add_glyph(const rect & r_, float s0, float t0, float s1, float t1, const float4 & top_color, const float4 & bottom_color)
{
    // Discard fully clipped glyphs
    const auto & s = scissor.back();
    if(r_.x0 >= s.x1) return;
    if(r_.x1 <= s.x0) return;
    if(r_.y0 >= s.y1) return;
    if(r_.y1 <= s.y0) return;

    auto r = r_;
    auto c0 = top_color, c1 = bottom_color;
        
    // Clip glyph against left edge of scissor rect
    if(r.x0 < s.x0)
    {
        float f = (float)(s.x0 - r.x0) / r.width();
        r.x0 = s.x0;
        s0 = lerp(s0, s1, f);
    }

    // Clip glyph against top edge of scissor rect
    if(r.y0 < s.y0)
    {
        float f = (float)(s.y0 - r.y0) / r.height();
        r.y0 = s.y0;
        t0 = lerp(t0, t1, f);
        c0 = lerp(c0, c1, f);            
    }

    // Clip glyph against right edge of scissor rect
    if(r.x1 > s.x1)
    {
        float f = (float)(r.x1 - s.x1) / r.width();
        r.x1 = s.x1;
        s1 = lerp(s1, s0, f);
    }

    // Clip glyph against bottom edge of scissor rect
    if(r.y1 > s.y1)
    {
        float f = (float)(r.y1 - s.y1) / r.height();
        r.y1 = s.y1;
        t1 = lerp(t1, t0, f);
        c1 = lerp(c1, c0, f);            
    }

    // Add clipped glyph
    vertices.push_back({short2(r.x0, r.y0), byte4(c0 * 255.0f), {s0, t0}});
    vertices.push_back({short2(r.x1, r.y0), byte4(c0 * 255.0f), {s1, t0}});
    vertices.push_back({short2(r.x1, r.y1), byte4(c1 * 255.0f), {s1, t1}});
    vertices.push_back({short2(r.x0, r.y1), byte4(c1 * 255.0f), {s0, t1}});
}

void draw_rect(gui & g, const rect & r, const float4 & color) { draw_rect(g, r, color, color); }
void draw_rect(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const float s = 0.5f / g.default_font.tex_size.x, t = 0.5f / g.default_font.tex_size.y;
    g.add_glyph(r, s, t, s, t, top_color, bottom_color);
}

void draw_rounded_rect_top(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const int radius = r.height();
    draw_rect(g, {r.x0+radius, r.y0, r.x1-radius, r.y0+radius}, top_color, bottom_color);
    if(auto * glyph = g.default_font.get_glyph(-radius))
    {
        g.add_glyph({r.x0, r.y0, r.x0+radius, r.y0+radius}, glyph->s1, glyph->t1, glyph->s0, glyph->t0, top_color, bottom_color);
        g.add_glyph({r.x1-radius, r.y0, r.x1, r.y0+radius}, glyph->s0, glyph->t1, glyph->s1, glyph->t0, top_color, bottom_color);
    }
}

void draw_rounded_rect_bottom(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const int radius = r.height();
    draw_rect(g, {r.x0+radius, r.y1-radius, r.x1-radius, r.y1}, top_color, bottom_color);
    if(auto * glyph = g.default_font.get_glyph(-radius))
    {
        g.add_glyph({r.x0, r.y1-radius, r.x0+radius, r.y1}, glyph->s1, glyph->t0, glyph->s0, glyph->t1, top_color, bottom_color);
        g.add_glyph({r.x1-radius, r.y1-radius, r.x1, r.y1}, glyph->s0, glyph->t0, glyph->s1, glyph->t1, top_color, bottom_color);
    }
}

void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & color) { draw_rounded_rect(g, r, radius, color, color); }
void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & top_color, const float4 & bottom_color)
{
    assert(radius >= 0);
    const float4 c1 = lerp(top_color, bottom_color, (float)radius/r.height());
    const float4 c2 = lerp(bottom_color, top_color, (float)radius/r.height());
    draw_rounded_rect_top(g, {r.x0, r.y0, r.x1, r.y0+radius}, top_color, c1);
    draw_rect(g, {r.x0, r.y0+radius, r.x1, r.y1-radius}, c1, c2);        
    draw_rounded_rect_bottom(g, {r.x0, r.y1-radius, r.x1, r.y1}, c2, bottom_color);
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

void draw_shadowed_text(gui & g, int2 p, const float4 & c, const std::string & text)
{
    draw_text(g, p+1, {0,0,0,c.w}, text);
    draw_text(g, p, c, text);
}

bool edit(gui & g, int id, const rect & r, std::string & text)
{
    if(g.check_click(id, r))
    {
        g.text_cursor = g.text_mark = g.default_font.get_cursor_pos(text, g.click_offset.x - 5);
        g.focused_id = g.pressed_id;
    }
    g.check_release(id);
    if(g.is_pressed(id)) g.text_cursor = g.default_font.get_cursor_pos(text, g.cursor.x - r.x0 - 5);

    bool changed = false;
    if(g.is_focused(id))
    {
        g.text_cursor = std::min(g.text_cursor, text.size());
        g.text_mark = std::min(g.text_mark, text.size());

        if(g.codepoint)
        {
            if(g.text_cursor != g.text_mark)
            {
                auto lo = std::min(g.text_cursor, g.text_mark);
                auto hi = std::max(g.text_cursor, g.text_mark);
                text.erase(begin(text)+lo, begin(text)+hi);
                g.text_cursor = g.text_mark = lo;
            }

            assert(isprint(g.codepoint)); // Only support printable ASCII for now, later, we will encode other characters in utf-8
            text.insert(begin(text) + g.text_cursor++, (char)g.codepoint);
            g.text_mark = g.text_cursor;
            changed = true;
        }

        if(g.pressed_key != key::none)
        {
            switch(g.pressed_key)
            {
            case key::left:
                if(g.text_cursor > 0) --g.text_cursor;
                if(!g.shift) g.text_mark = g.text_cursor;
                break;
            case key::right:
                if(g.text_cursor < text.size()) ++g.text_cursor;
                if(!g.shift) g.text_mark = g.text_cursor;
                break;
            case key::home:
                g.text_cursor = 0;
                if(!g.shift) g.text_mark = g.text_cursor;
                break;
            case key::end:
                g.text_cursor = text.size();
                if(!g.shift) g.text_mark = g.text_cursor;
                break;
            case key::backspace: 
                if(g.text_cursor != g.text_mark)
                {
                    auto lo = std::min(g.text_cursor, g.text_mark);
                    auto hi = std::max(g.text_cursor, g.text_mark);
                    text.erase(begin(text)+lo, begin(text)+hi);
                    g.text_cursor = g.text_mark = lo;
                    changed = true;
                }
                else if(g.text_cursor > 0)
                {
                    text.erase(begin(text) + --g.text_cursor); 
                    g.text_mark = g.text_cursor;
                    changed = true;
                }
                break;
            case key::delete_: 
                if(g.text_cursor != g.text_mark)
                {
                    auto lo = std::min(g.text_cursor, g.text_mark);
                    auto hi = std::max(g.text_cursor, g.text_mark);
                    text.erase(begin(text)+lo, begin(text)+hi);
                    g.text_cursor = g.text_mark = lo;   
                    changed = true;
                }
                if(g.text_cursor < text.size())
                {
                    text.erase(begin(text) + g.text_cursor);
                    changed = true;
                }
                break;
            }
        }
    }
    
    const rect tr = {r.x0+5, r.y0+2, r.x1-5, r.y1-2};
    draw_rounded_rect(g, r, 4, {1,1,1,1});
    if(g.is_focused(id))
    {
        auto lo = std::min(g.text_cursor, g.text_mark), hi = std::max(g.text_cursor, g.text_mark);
        draw_rect(g, {tr.x0 + g.default_font.get_text_width(text.substr(0, lo)), tr.y0, tr.x0 + g.default_font.get_text_width(text.substr(0, hi)), tr.y1}, {1,1,0,1}, {1,1,0,1});
    }
    draw_text(g, {tr.x0, tr.y0}, {0,0,0,1}, text);
    if(g.is_focused(id))
    {
        int w = g.default_font.get_text_width(text.substr(0, g.text_cursor));
        draw_rect(g, {tr.x0+w, tr.y0, tr.x0+w+1, tr.y1}, {0,0,0,1});
    }
    return changed;
}

bool edit(gui & g, int id, const rect & r, float & number)
{
    std::ostringstream ss;
    ss << number;
    std::string text = ss.str();
    if(!edit(g, id, r, text)) return false;
    if(text.empty() && number != 0)
    {
        number = 0;
        return true;
    }
    return !!(std::istringstream(text) >> number);
}

template<class T, int N> bool edit_vector(gui & g, int id, const rect & r, linalg::vec<T,N> & vec)
{
    bool changed = false;
    g.begin_childen(id);
    const int w = r.width() - (N-1)*2;    
    for(int i=0; i<N; ++i) changed |= edit(g, i, {r.x0 + w*i/N + i*2, r.y0, r.x0 + w*(i+1)/N + i*2, r.y1}, vec[i]);
    g.end_children();
    return changed;
}

bool edit(gui & g, int id, const rect & r, float2 & vec) { return edit_vector(g, id, r, vec); }
bool edit(gui & g, int id, const rect & r, float3 & vec) { return edit_vector(g, id, r, vec); }
bool edit(gui & g, int id, const rect & r, float4 & vec) { return edit_vector(g, id, r, vec); }

const float4 frame_color = {0.5f,0.5f,0.5f,1}, cap_color = {0.3f,0.3f,0.3f,1};
const int scrollbar_width = 12;
const int splitbar_width = 6;

rect tabbed_frame(gui & g, rect r, const std::string & caption)
{
    const int cap_width = g.default_font.get_text_width(caption)+24, cap_height = g.default_font.line_height + 4;

    draw_rounded_rect_top(g, {r.x0, r.y0, r.x0 + cap_width, r.y0 + 10}, frame_color, frame_color);
    draw_rect(g, {r.x0, r.y0 + 10, r.x0 + cap_width, r.y0 + cap_height}, frame_color);
    draw_rect(g, {r.x0, r.y0 + cap_height, r.x1, r.y0 + cap_height + 1}, frame_color);
    draw_rect(g, {r.x0, r.y0 + cap_height + 1, r.x0 + 1, r.y1 - 1}, frame_color);
    draw_rect(g, {r.x1 - 1, r.y0 + cap_height + 1, r.x1, r.y1 - 1}, frame_color);
    draw_rect(g, {r.x0, r.y1 - 1, r.x1, r.y1}, frame_color);

    draw_rounded_rect_top(g, {r.x0 + 1, r.y0 + 1, r.x0 + cap_width - 1, r.y0 + 10}, cap_color, cap_color);
    draw_rect(g, {r.x0 + 1, r.y0 + 10, r.x0 + cap_width - 1, r.y0 + cap_height + 1}, cap_color);

    draw_shadowed_text(g, {r.x0 + 11, r.y0 + 3}, {1,1,1,1}, caption);

    return {r.x0 + 1, r.y0 + cap_height + 1, r.x1 - 1, r.y1 - 1};
}

rect vscroll_panel(gui & g, int id, const rect & r, int client_height, int & offset)
{
    if(g.check_pressed(id)) offset = (static_cast<int>(g.cursor.y - g.click_offset.y) - r.y0) * client_height / r.height();
    if(g.is_cursor_over(r)) offset -= static_cast<int>(g.scroll.y * 20);
    offset = std::min(offset, client_height - r.height());
    offset = std::max(offset, 0);

    if(client_height <= r.height()) return r;
    const rect tab = {r.x1-scrollbar_width, r.y0 + offset * r.height() / client_height, r.x1, r.y0 + (offset + r.height()) * r.height() / client_height};
    g.check_click(id, tab);

    draw_rect(g, {r.x1-scrollbar_width, r.y0, r.x1, r.y1}, {0.5f,0.5f,0.5f,1}); // Track
    draw_rounded_rect(g, tab, (scrollbar_width-2)/2, {0.8f,0.8f,0.8f,1}); // Tab
    return {r.x0, r.y0, r.x1-scrollbar_width, r.y1};
}

std::pair<rect, rect> hsplitter(gui & g, int id, const rect & r, int & split)
{
    if(g.check_pressed(id)) split = static_cast<int>(g.cursor.x - g.click_offset.x) - r.x0;
    split = std::min(split, r.x1 - 10 - splitbar_width);
    split = std::max(split, r.x0 + 10);

    const rect splitbar = {r.x0 + split, r.y0, r.x0 + split + splitbar_width, r.y1};
    g.check_click(id, splitbar);

    //draw_rect(g, splitbar, {0.5f,0.5f,0.5f,1});
    return {{r.x0, r.y0, splitbar.x0, r.y1}, {splitbar.x1, r.y0, r.x1, r.y1}};
}

std::pair<rect, rect> vsplitter(gui & g, int id, const rect & r, int & split)
{
    if(g.check_pressed(id)) split = static_cast<int>(g.cursor.y - g.click_offset.y) - r.y0;
    split = std::min(split, r.y1 - 10 - splitbar_width);
    split = std::max(split, r.y0 + 10);

    const rect splitbar = {r.x0, r.y0 + split, r.x1, r.y0 + split + splitbar_width};
    g.check_click(id, splitbar);

    //draw_rect(g, splitbar, {0.5f,0.5f,0.5f,1});
    return {{r.x0, r.y0, r.x1, splitbar.y0}, {r.x0, splitbar.y1, r.x1, r.y1}};
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

void plane_translation_dragger(gui & g, const float3 & plane_normal, float3 & point)
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

void axis_translation_dragger(gui & g, const float3 & axis, float3 & point)
{
    if(g.ml)
    {
        // First apply a plane translation dragger with a plane that contains the desired axis and is oriented to face the camera
        const float3 plane_tangent = cross(axis, point - g.cam.position);
        const float3 plane_normal = cross(axis, plane_tangent);
        plane_translation_dragger(g, plane_normal, point);

        // Constrain object motion to be along the desired axis
        point = g.original_position + axis * dot(point - g.original_position, axis);
    }
}

void position_gizmo(gui & g, int id, float3 & position)
{
    // On click, set the gizmo mode based on which component the user clicked on
    if(g.ml_down)
    {
        g.gizmode = gizmo_mode::none;
        auto ray = g.get_ray_from_cursor();
        ray.origin -= position;
        float best_t = std::numeric_limits<float>::infinity(), t;           
        if(intersect_ray_mesh(ray, g.gizmo_meshes[0], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_x; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_meshes[1], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_y; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_meshes[2], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_z; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_meshes[3], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_yz; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_meshes[4], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_zx; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_meshes[5], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_xy; best_t = t; }
        if(g.gizmode != gizmo_mode::none)
        {
            g.click_offset = ray.origin + ray.direction*t;
            g.set_pressed(id);
        }
    }

    // If the user has previously clicked on a gizmo component, allow the user to interact with that gizmo
    if(g.is_pressed(id))
    {
        position += g.click_offset;
        switch(g.gizmode)
        {
        case gizmo_mode::translate_x: axis_translation_dragger(g, {1,0,0}, position); break;
        case gizmo_mode::translate_y: axis_translation_dragger(g, {0,1,0}, position); break;
        case gizmo_mode::translate_z: axis_translation_dragger(g, {0,0,1}, position); break;
        case gizmo_mode::translate_yz: plane_translation_dragger(g, {1,0,0}, position); break;
        case gizmo_mode::translate_zx: plane_translation_dragger(g, {0,1,0}, position); break;
        case gizmo_mode::translate_xy: plane_translation_dragger(g, {0,0,1}, position); break;
        }        
        position -= g.click_offset;
    }

    // On release, deactivate the current gizmo mode
    if(g.check_release(id)) g.gizmode = gizmo_mode::none;
}