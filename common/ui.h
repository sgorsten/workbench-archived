// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef UI_H
#define UI_H

#include "draw2D.h"
#include "input.h"

enum class cursor_icon { arrow, ibeam, hresize, vresize };
enum class gizmo_mode { none, translate_x, translate_y, translate_z, translate_yz, translate_zx, translate_xy, rotate_yz, rotate_zx, rotate_xy };
struct menu_stack_frame { rect r; bool open, clicked; };

class widget_id
{
    std::vector<int> values;
public:
    bool is_equal_to(const widget_id & r, int id) const;
    bool is_parent_of(const widget_id & r, int id) const;
    void push(int id) { values.push_back(id); }
    void pop() { values.pop_back(); }
};

enum class clipboard_event { none, cut, copy, paste };

struct gui
{
    const sprite_library & sprites;
    draw_buffer_2d buffer;

    int2 window_size;               // Size in pixels of the current window
    bool bf, bl, bb, br, ml, mr;    // Instantaneous state of WASD keys and left/right mouse buttons
    input_event in;
    float timestep;                 // Timestep between the last frame and this one
    clipboard_event clip_event;
    std::string clipboard;

    cursor_icon icon;

    widget_id current_id;
    widget_id pressed_id;
    widget_id focused_id;

    std::string::size_type text_cursor, text_mark;

    float3 original_position;       // Original position of an object being manipulated with a gizmo
    float4 original_orientation;    // Original orientation of an object being manipulated with a gizmo
    float3 click_offset;            // Offset from position of grabbed object to coordinates of clicked point

    std::vector<menu_stack_frame> menu_stack;

    gui(sprite_library & sprites);

    const float2 & get_cursor() const { return buffer.detransform_point(in.cursor); }

    bool is_control_held() const { return (in.mods & GLFW_MOD_CONTROL) != 0; }
    bool is_shift_held() const { return (in.mods & GLFW_MOD_SHIFT) != 0; }

    // API for determining clicked status
    bool is_pressed(int id) const { return pressed_id.is_equal_to(current_id, id); }
    bool is_focused(int id) const { return focused_id.is_equal_to(current_id, id); }
    bool is_child_pressed(int id) const { return pressed_id.is_parent_of(current_id, id); }
    bool is_child_focused(int id) const { return focused_id.is_parent_of(current_id, id); }
    void set_pressed(int id) { pressed_id = current_id; pressed_id.push(id); }
    void begin_children(int id) { current_id.push(id); }
    void end_children() { current_id.pop(); }

    bool is_cursor_over(const rect & r) const;
    bool check_click(int id, const rect & r); // Returns true if the item was clicked during this frame
    bool check_pressed(int id); // Returns true if the item with the specified ID was clicked and has not yet been released
    bool check_release(int id); // Returns true if the item with the specified ID was released during this frame

    // API for rendering 2D glyphs
    void begin_frame(const int2 & window_size);
    void end_frame();
    void begin_overlay();
    void end_overlay();
    void begin_scissor(const rect & r);
    void end_scissor();
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

// Menu support
void begin_menu(gui & g, int id, const rect & r);
void begin_popup(gui & g, int id, const std::string & caption);
bool menu_item(gui & g, const std::string & caption, int mods=0, int key=0, uint32_t icon=0);
void menu_seperator(gui & g);
void end_popup(gui & g);
void end_menu(gui & g);

#endif