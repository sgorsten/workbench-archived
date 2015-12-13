// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef UI_H
#define UI_H

#include "draw2D.h"
#include "input.h"

enum class cursor_icon { arrow, ibeam, hresize, vresize };
enum class clipboard_event { none, cut, copy, paste };
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

struct gui
{
    // Output state
    sprite_library sprites;                         // Permanent repository of sprites, including font glyphs, icons, and antialiased shapes
    draw_buffer_2d buffer;                          // Buffer which receives 2D drawing commands, reset to empty on begin_frame(...)
    cursor_icon icon;                               // The current icon to display for the cursor, defaults to cursor_icon::arrow on begin_frame(...)

    // Input state
    input_event in;                                 // Any event which occurs during this frame, may be none, but cursor and mods are always available
    clipboard_event clip_event;                     // Was a clipboard event requested during this frame?
    std::string clipboard;                          // Buffer used to receive or send information to the clipboard

    // Focus state
    widget_id current_id;                           // The prefix of the ID of the current widget, managed by begin_children(...)/end_children(...) calls
    widget_id pressed_id;                           // The ID of the widget which has been clicked and is currently being held
    widget_id focused_id;                           // The ID of the last focusable widget which was clicked on

    // Widget state
    float2 click_offset;                            // Offset from top-left of widget to clicked point, used for sliders, scrollbars, and draggables
    std::vector<menu_stack_frame> menu_stack;       // Information about expanded popup menus
    std::string::size_type text_cursor, text_mark;  // The bounds of the current selection in a text-edit widget

    gui();

    // Scope API
    void begin_frame(const int2 & window_size, const input_event & e);
    void end_frame() { buffer.end_frame(); }
    void begin_overlay() { buffer.begin_overlay(); }
    void end_overlay() { buffer.end_overlay(); }
    void begin_transform(const transform_2d & t) { buffer.begin_transform(t); }
    void end_transform() { buffer.end_transform(); }
    void begin_scissor(const rect & r) { buffer.begin_scissor(r); }
    void end_scissor() { buffer.end_scissor(); }

    // Output API
    void draw_text(int2 p, utf8::string_view text, const float4 & color) { buffer.draw_text(p, text, color); }
    void draw_shadowed_text(int2 p, utf8::string_view text, const float4 & color) { buffer.draw_shadowed_text(p, text, color); }
    void draw_circle(const int2 & center, int radius, const float4 & color) { buffer.draw_circle(center, radius, color); }
    void draw_rect(const rect & r, const float4 & color) { buffer.draw_rect(r, color); }
    void draw_rounded_rect(const rect & r, int radius, const float4 & color) { buffer.draw_rounded_rect(r, radius, color); }
    void draw_partial_rounded_rect(rect r, int radius, const float4 & color, bool tl, bool tr, bool bl, bool br) { buffer.draw_partial_rounded_rect(r, radius, color, tl, tr, bl, br); }
    void draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color) { buffer.draw_line(p0, p1, width, color); }
    void draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color) { buffer.draw_bezier_curve(p0, p1, p2, p3, width, color); }

    // Input API
    float2 get_cursor() const { return buffer.detransform_point(in.cursor); }
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
};

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