// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef UI_H
#define UI_H

#include "geometry.h"
#include "font.h"

struct rect 
{ 
    int x0, y0, x1, y1; 
    int width() const { return x1 - x0; }
    int height() const { return y1 - y0; }
    int2 dims() const { return {width(), height()}; }
    float aspect_ratio() const { return (float)width()/height(); }
};

struct camera
{
    float yfov, near_clip, far_clip;
    float3 position;
    float pitch, yaw;
    float4 get_orientation() const { return qmul(rotation_quat(float3(0,1,0), yaw), rotation_quat(float3(1,0,0), pitch)); }
    float4x4 get_view_matrix() const { return rotation_matrix(qconj(get_orientation())) * translation_matrix(-position); }
    float4x4 get_projection_matrix(const rect & viewport) const { return perspective_matrix(yfov, viewport.aspect_ratio(), near_clip, far_clip); }
    float4x4 get_viewproj_matrix(const rect & viewport) const { return get_projection_matrix(viewport) * get_view_matrix(); }
    ray get_ray_from_pixel(const float2 & pixel, const rect & viewport) const;
};

enum class cursor_icon { arrow, ibeam, hresize, vresize };
enum class key { none, left, right, up, down, home, end, page_up, page_down, backspace, delete_, enter, escape };
enum class gizmo_mode { none, translate_x, translate_y, translate_z, translate_yz, translate_zx, translate_xy };

struct gui
{
    struct vertex { short2 position; byte4 color; float2 texcoord; };    
    struct list { size_t level,first,last; };
    std::vector<vertex> vertices;
    std::vector<list> lists;
    std::vector<rect> scissor;

    font default_font;
    geometry_mesh gizmo_meshes[6];

    int2 window_size;               // Size in pixels of the current window
    bool ctrl, shift;               // Whether at least one control or shift key is being held down
    bool bf, bl, bb, br, ml, mr;    // Instantaneous state of WASD keys and left/right mouse buttons
    bool ml_down, ml_up;            // Whether the left mouse was pressed or released during this frame
    unsigned int codepoint;         // Codepoint of unicode character typed during this frame
    key pressed_key;                // Special key pressed during this frame
    float2 cursor, delta;           // Current pixel coordinates of cursor, as well as the amount by which the cursor has moved
    float2 scroll;                  // Scroll amount in current frame
    float timestep;                 // Timestep between the last frame and this one

    cursor_icon icon;

    std::vector<int> current_id;
    std::vector<int> pressed_id;
    std::vector<int> focused_id;

    std::string::size_type text_cursor, text_mark;

    rect viewport3d;                // Current 3D viewport used to render the scene
    camera cam;                     // Current 3D camera used to render the scene
    gizmo_mode gizmode;             // Mode that the gizmo is currently in
    float3 original_position;       // Original position of an object being manipulated with a gizmo
    float3 click_offset;            // Offset from position of grabbed object to coordinates of clicked point

    gui();

    // API for determining clicked status
    bool is_pressed(int id) const;
    bool is_focused(int id) const;
    bool is_child_pressed(int id) const;
    void set_pressed(int id);
    void clear_pressed();
    void begin_childen(int id);
    void end_children();

    bool is_cursor_over(const rect & r) const;
    bool check_click(int id, const rect & r);
    bool check_pressed(int id); // Returns true if the item with the specified ID was clicked and has not yet been released
    bool check_release(int id);

    // API for rendering 2D glyphs
    void begin_frame(const int2 & window_size);
    void end_frame();
    void begin_overlay();
    void end_overlay();
    void begin_scissor(const rect & r);
    void end_scissor();
    void add_glyph(const rect & r, float s0, float t0, float s1, float t1, const float4 & top_color, const float4 & bottom_color);

    // API for doing computations in 3D space
    float4x4 get_view_matrix() const { return cam.get_view_matrix(); }
    float4x4 get_projection_matrix() const { return cam.get_projection_matrix(viewport3d); }
    float4x4 get_viewproj_matrix() const { return cam.get_viewproj_matrix(viewport3d); }
    ray get_ray_from_cursor() const { return cam.get_ray_from_pixel(cursor, viewport3d); }
};

// Basic 2D gui output
void draw_rect(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color);
void draw_rect(gui & g, const rect & r, const float4 & color);
void draw_rounded_rect_top(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color);
void draw_rounded_rect_bottom(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color);
void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & top_color, const float4 & bottom_color);
void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & color);
void draw_text(gui & g, int2 p, const float4 & c, const std::string & text);
void draw_shadowed_text(gui & g, int2 p, const float4 & c, const std::string & text);

// 2D gui widgets
bool edit(gui & g, int id, const rect & r, std::string & text);
bool edit(gui & g, int id, const rect & r, float & number);
bool edit(gui & g, int id, const rect & r, float2 & vec);
bool edit(gui & g, int id, const rect & r, float3 & vec);
bool edit(gui & g, int id, const rect & r, float4 & vec);

// These layout controls return rects defining their client regions
rect tabbed_frame(gui & g, rect r, const std::string & caption);
rect vscroll_panel(gui & g, int id, const rect & r, int client_height, int & offset);
std::pair<rect, rect> hsplitter(gui & g, int id, const rect & r, int & split);
std::pair<rect, rect> vsplitter(gui & g, int id, const rect & r, int & split);

// 3D camera interactions
void do_mouselook(gui & g, float sensitivity);
void move_wasd(gui & g, float speed);

// 3D manipulation interactions
void plane_translation_dragger(gui & g, const float3 & plane_normal, float3 & point);
void axis_translation_dragger(gui & g, const float3 & axis, float3 & point);
void position_gizmo(gui & g, int id, float3 & position);

#endif