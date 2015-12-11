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

bool widget_id::is_equal_to(const widget_id & r, int id) const
{
    if(values.size() != r.values.size() + 1) return false;
    for(size_t i=0; i<r.values.size(); ++i) if(values[i] != r.values[i]) return false;
    return values.back() == id;
}

bool widget_id::is_parent_of(const widget_id & r, int id) const
{
    if(values.size() < r.values.size() + 2) return false;
    for(size_t i=0; i<r.values.size(); ++i) if(values[i] != r.values[i]) return false;
    return values[r.values.size()] == id;
}

gui::gui(sprite_library & sprites) : sprites(sprites), bf(), bl(), bb(), br(), ml(), mr(), in({}), timestep(), cam({}), gizmode() 
{
    std::vector<int> codepoints;
    for(int i=0xf000; i<=0xf295; ++i) codepoints.push_back(i);
    sprites.default_font.load_glyphs("fontawesome-webfont.ttf", 14, codepoints);
    sprites.sheet.prepare_texture();

    std::initializer_list<float2> arrow_points = {{0, 0.05f}, {1, 0.05f}, {1, 0.10f}, {1.2f, 0}};
    std::initializer_list<float2> ring_points = {{+0.05f, 1}, {-0.05f, 1}, {-0.05f, 1}, {-0.05f, 1.2f}, {-0.05f, 1.2f}, {+0.05f, 1.2f}, {+0.05f, 1.2f}, {+0.05f, 1}};
    gizmo_res.geomeshes[0] = make_lathed_geometry({1,0,0}, {0,1,0}, {0,0,1}, 12, arrow_points);
    gizmo_res.geomeshes[1] = make_lathed_geometry({0,1,0}, {0,0,1}, {1,0,0}, 12, arrow_points);
    gizmo_res.geomeshes[2] = make_lathed_geometry({0,0,1}, {1,0,0}, {0,1,0}, 12, arrow_points);
    gizmo_res.geomeshes[3] = make_box_geometry({-0.01f,0,0}, {0.01f,0.4f,0.4f});
    gizmo_res.geomeshes[4] = make_box_geometry({0,-0.01f,0}, {0.4f,0.01f,0.4f});
    gizmo_res.geomeshes[5] = make_box_geometry({0,0,-0.01f}, {0.4f,0.4f,0.01f});
    gizmo_res.geomeshes[6] = make_lathed_geometry({1,0,0}, {0,1,0}, {0,0,1}, 24, ring_points);
    gizmo_res.geomeshes[7] = make_lathed_geometry({0,1,0}, {0,0,1}, {1,0,0}, 24, ring_points);
    gizmo_res.geomeshes[8] = make_lathed_geometry({0,0,1}, {1,0,0}, {0,1,0}, 24, ring_points);
}

bool gui::is_cursor_over(const rect & r) const
{
    const auto & s = buffer.get_scissor_rect();
    return in.cursor.x >= r.x0 && in.cursor.y >= r.y0 && in.cursor.x < r.x1 && in.cursor.y < r.y1
        && in.cursor.x >= s.x0 && in.cursor.y >= s.y0 && in.cursor.x < s.x1 && in.cursor.y < s.y1;
}

bool gui::check_click(int id, const rect & r)
{
    if(in.type == input::mouse_down && in.button == GLFW_MOUSE_BUTTON_LEFT && is_cursor_over(r))
    {
        click_offset.x = in.cursor.x - r.x0;
        click_offset.y = in.cursor.y - r.y0;
        set_pressed(id);
        return true;
    }
    return false;
}

bool gui::check_pressed(int id)
{
    if(is_pressed(id))
    {
        if(in.type == input::mouse_up && in.button == GLFW_MOUSE_BUTTON_LEFT) pressed_id = {};
        else return true;
    }
    return false;
}

bool gui::check_release(int id)
{
    if(in.type == input::mouse_up && in.button == GLFW_MOUSE_BUTTON_LEFT && is_pressed(id))
    {
        pressed_id = {};
        return true;
    }
    return false;
}

void gui::begin_frame(const int2 & window_size)
{
    this->window_size = window_size;
    buffer.begin_frame(sprites, window_size);
    draw = {};
    clip_event = clipboard_event::none;
    clipboard.clear();
}
void gui::end_frame() { buffer.end_frame(); }
void gui::begin_overlay() { buffer.begin_overlay(); }
void gui::end_overlay() { buffer.end_overlay(); }
void gui::begin_scissor(const rect & r) { buffer.begin_scissor(r); }
void gui::end_scissor() { buffer.end_scissor(); }

void draw_rect(gui & g, const rect & r, const float4 & color) { draw_rect(g, r, color, color); }
void draw_rect(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    g.buffer.draw_rect(r, top_color);
}

void draw_rounded_rect_top(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    g.buffer.draw_partial_rounded_rect(r, r.height(), top_color, 1, 1, 0, 0);
}

void draw_rounded_rect_bottom(gui & g, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    g.buffer.draw_partial_rounded_rect(r, r.height(), top_color, 0, 0, 1, 1);
}

void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & color) { draw_rounded_rect(g, r, radius, color, color); }
void draw_rounded_rect(gui & g, const rect & r, int radius, const float4 & top_color, const float4 & bottom_color)
{
    g.buffer.draw_rounded_rect(r, radius, top_color);
}

void draw_text(gui & g, int2 p, const float4 & c, const std::string & text)
{     
    g.buffer.draw_text(p, text, c);
}

void draw_shadowed_text(gui & g, int2 p, const float4 & c, const std::string & text)
{
    g.buffer.draw_shadowed_text(p, text, c);
}

static void prev_char(const std::string & text, std::string::size_type & pos) { pos = utf8::prev(text.data() + pos) - text.data(); }
static void next_char(const std::string & text, std::string::size_type & pos) { pos = utf8::next(text.data() + pos) - text.data(); }

bool edit(gui & g, int id, const rect & r, std::string & text)
{
    if(g.is_cursor_over(r)) g.icon = cursor_icon::ibeam;
    if(g.check_click(id, r))
    {
        g.text_cursor = g.text_mark = g.sprites.default_font.get_cursor_pos(text, g.click_offset.x - 5);
        g.focused_id = g.pressed_id;
    }
    g.check_release(id);
    if(g.is_pressed(id)) g.text_cursor = g.sprites.default_font.get_cursor_pos(text, g.in.cursor.x - r.x0 - 5);

    bool changed = false;
    if(g.is_focused(id))
    {
        g.text_cursor = std::min(g.text_cursor, text.size());
        g.text_mark = std::min(g.text_mark, text.size());

        if(g.in.type == input::character)
        {
            if(g.text_cursor != g.text_mark)
            {
                auto lo = std::min(g.text_cursor, g.text_mark);
                auto hi = std::max(g.text_cursor, g.text_mark);
                text.erase(begin(text)+lo, begin(text)+hi);
                g.text_cursor = g.text_mark = lo;
            }

            auto units = utf8::units(g.in.codepoint);
            text.insert(g.text_cursor, units.data());
            next_char(text, g.text_cursor);
            g.text_mark = g.text_cursor;
            changed = true;
        }

        if(g.in.type == input::key_down) switch(g.in.key)
        {
        case GLFW_KEY_LEFT:
            if(g.text_cursor > 0) prev_char(text, g.text_cursor);
            if(!g.is_shift_held()) g.text_mark = g.text_cursor;
            break;
        case GLFW_KEY_RIGHT:
            if(g.text_cursor < text.size()) next_char(text, g.text_cursor);
            if(!g.is_shift_held()) g.text_mark = g.text_cursor;
            break;
        case GLFW_KEY_HOME:
            g.text_cursor = 0;
            if(!g.is_shift_held()) g.text_mark = g.text_cursor;
            break;
        case GLFW_KEY_END:
            g.text_cursor = text.size();
            if(!g.is_shift_held()) g.text_mark = g.text_cursor;
            break;
        case GLFW_KEY_BACKSPACE: 
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
                prev_char(text, g.text_cursor);
                text.erase(begin(text) + g.text_cursor, begin(text) + g.text_mark);
                g.text_mark = g.text_cursor;
                changed = true;
            }
            break;
        case GLFW_KEY_DELETE: 
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
                next_char(text, g.text_mark);
                text.erase(begin(text) + g.text_cursor, begin(text) + g.text_mark);
                g.text_mark = g.text_cursor;
                changed = true;
            }
            break;
        }

        switch(g.clip_event)
        {
        case clipboard_event::cut:
            if(g.text_cursor != g.text_mark)
            {
                auto lo = std::min(g.text_cursor, g.text_mark);
                auto hi = std::max(g.text_cursor, g.text_mark);
                g.clipboard = text.substr(lo, hi-lo);
                text.erase(begin(text)+lo, begin(text)+hi);
                g.text_cursor = g.text_mark = lo;
                changed = true;
            }
            break;
        case clipboard_event::copy:
            if(g.text_cursor != g.text_mark)
            {
                auto lo = std::min(g.text_cursor, g.text_mark);
                auto hi = std::max(g.text_cursor, g.text_mark);
                g.clipboard = text.substr(lo, hi-lo);
            }
            break;
        case clipboard_event::paste:
            if(g.text_cursor != g.text_mark)
            {
                auto lo = std::min(g.text_cursor, g.text_mark);
                auto hi = std::max(g.text_cursor, g.text_mark);
                text.erase(begin(text)+lo, begin(text)+hi);
                g.text_cursor = g.text_mark = lo;
            }
            text.insert(begin(text)+g.text_cursor, begin(g.clipboard), end(g.clipboard));
            changed = true;
            break;
        }
    }
    
    const rect tr = {r.x0+5, r.y0+2, r.x1-5, r.y1-2};
    draw_rounded_rect(g, r, 4, {1,1,1,1});
    if(g.is_focused(id))
    {
        auto lo = std::min(g.text_cursor, g.text_mark), hi = std::max(g.text_cursor, g.text_mark);
        draw_rect(g, {tr.x0 + g.sprites.default_font.get_text_width(text.substr(0, lo)), tr.y0, tr.x0 + g.sprites.default_font.get_text_width(text.substr(0, hi)), tr.y1}, {1,1,0,1}, {1,1,0,1});
    }
    draw_text(g, {tr.x0, tr.y0}, {0,0,0,1}, text);
    if(g.is_focused(id))
    {
        int w = g.sprites.default_font.get_text_width(text.substr(0, g.text_cursor));
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
    g.begin_children(id);
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
    const int cap_width = g.sprites.default_font.get_text_width(caption)+24, cap_height = g.sprites.default_font.line_height + 4;

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
    if(g.check_pressed(id)) offset = (static_cast<int>(g.in.cursor.y - g.click_offset.y) - r.y0) * client_height / r.height();
    if(g.is_cursor_over(r)) offset -= static_cast<int>(g.in.scroll.y * 20);
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
    if(g.check_pressed(id)) split = static_cast<int>(g.in.cursor.x - g.click_offset.x) - r.x0;
    split = std::min(split, r.x1 - 10 - splitbar_width);
    split = std::max(split, r.x0 + 10);

    const rect splitbar = {r.x0 + split, r.y0, r.x0 + split + splitbar_width, r.y1};
    if(g.is_cursor_over(splitbar)) g.icon = cursor_icon::hresize;
    g.check_click(id, splitbar);
    return {{r.x0, r.y0, splitbar.x0, r.y1}, {splitbar.x1, r.y0, r.x1, r.y1}};
}

std::pair<rect, rect> vsplitter(gui & g, int id, const rect & r, int & split)
{
    if(g.check_pressed(id)) split = static_cast<int>(g.in.cursor.y - g.click_offset.y) - r.y0;
    split = std::min(split, r.y1 - 10 - splitbar_width);
    split = std::max(split, r.y0 + 10);

    const rect splitbar = {r.x0, r.y0 + split, r.x1, r.y0 + split + splitbar_width};
    if(g.is_cursor_over(splitbar)) g.icon = cursor_icon::vresize;
    g.check_click(id, splitbar);
    return {{r.x0, r.y0, r.x1, splitbar.y0}, {r.x0, splitbar.y1, r.x1, r.y1}};
}

///////////
// Menus //
///////////

void begin_menu(gui & g, int id, const rect & r)
{
    draw_rect(g, r, cap_color);

    g.menu_stack.clear();
    g.menu_stack.push_back({{r.x0+10, r.y0, r.x0+10, r.y1}, true});

    g.begin_children(id);
}

static rect get_next_menu_item_rect(gui & g, rect & r, const std::string & caption)
{
    if(g.menu_stack.size() == 1)
    {
        const rect item = {r.x1, r.y0 + (r.height() - g.sprites.default_font.line_height) / 2, r.x1 + g.sprites.default_font.get_text_width(caption), r.y0 + (r.height() + g.sprites.default_font.line_height) / 2};
        r.x1 = item.x1 + 30;
        return item;
    }
    else
    {
        const rect item = {r.x0 + 4, r.y1, r.x0 + 190, r.y1 + g.sprites.default_font.line_height};
        r.x1 = std::max(r.x1, item.x1);
        r.y1 = item.y1 + 4;
        return item;    
    }
}

void begin_popup(gui & g, int id, const std::string & caption)
{
    auto & f = g.menu_stack.back();
    const rect item = get_next_menu_item_rect(g, f.r, caption);

    if(f.open)
    {
        if(g.is_cursor_over(item)) draw_rect(g, item, {0.5f,0.5f,0,1});

        if(g.menu_stack.size() > 1)
        {
            draw_shadowed_text(g, {item.x0+20, item.y0}, {1,1,1,1}, caption);
            draw_shadowed_text(g, {item.x0 + 180, item.y0}, {1,1,1,1}, utf8::units(0xf0da).data());
        }
        else draw_shadowed_text(g, {item.x0, item.y0}, {1,1,1,1}, caption);

        if(g.check_click(id, item))
        {
            g.focused_id = g.pressed_id;
            f.clicked = true;
        }
    }
    
    if(g.menu_stack.size() == 1) g.menu_stack.push_back({{item.x0, item.y1, item.x0+200, item.y1+4}, g.is_focused(id) || g.is_child_focused(id)});
    else g.menu_stack.push_back({{item.x1, item.y0-1, item.x1+200, item.y0+3}, g.is_focused(id) || g.is_child_focused(id)});
    g.begin_overlay();
    g.begin_overlay();
    g.begin_children(id);
}

void menu_seperator(gui & g)
{
    if(g.menu_stack.size() < 2) return;
    auto & f = g.menu_stack.back();
    if(f.open) draw_rect(g, {f.r.x0 + 4, f.r.y1 + 1, f.r.x0 + 196, f.r.y1 + 2}, {0.5,0.5,0.5,1});
    f.r.y1 += 6;
}

bool menu_item(gui & g, const std::string & caption, int mods, int key, uint32_t icon)
{
    if(key && (mods & g.in.mods) == mods && g.in.type == input::key_down && g.in.key == key) return true;

    auto & f = g.menu_stack.back();
    const rect item = get_next_menu_item_rect(g, f.r, caption);

    if(f.open)
    {
        if(g.is_cursor_over(item)) draw_rect(g, item, {0.5f,0.5f,0,1});
        if(icon) draw_shadowed_text(g, {item.x0, item.y0}, {1,1,1,1}, utf8::units(icon).data());
        draw_shadowed_text(g, {item.x0+20, item.y0}, {1,1,1,1}, caption);

        if(key)
        {
            std::ostringstream ss;
            if(mods & GLFW_MOD_CONTROL) ss << "Ctrl+";
            if(mods & GLFW_MOD_SHIFT) ss << "Shift+";
            if(mods & GLFW_MOD_ALT) ss << "Alt+";
            if(mods & GLFW_MOD_SUPER) ss << "Super+";
            if(key >= GLFW_KEY_A && key <= GLFW_KEY_Z) ss << (char)('A' + key - GLFW_KEY_A);
            else if(key >= GLFW_KEY_0 && key <= GLFW_KEY_9) ss << (key - GLFW_KEY_0);
            else if(key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) ss << 'F' << (1 + key - GLFW_KEY_F1);
            else switch(key)
            {
            case GLFW_KEY_SPACE:            ss << "Space";       break;
            case GLFW_KEY_APOSTROPHE:       ss << '\'';          break;
            case GLFW_KEY_COMMA:            ss << ',';           break;
            case GLFW_KEY_MINUS:            ss << '-';           break;
            case GLFW_KEY_PERIOD:           ss << '.';           break;
            case GLFW_KEY_SLASH:            ss << '/';           break;
            case GLFW_KEY_SEMICOLON:        ss << ';';           break;
            case GLFW_KEY_EQUAL:            ss << '=';           break;
            case GLFW_KEY_LEFT_BRACKET:     ss << '[';           break;
            case GLFW_KEY_BACKSLASH:        ss << '\\';          break;
            case GLFW_KEY_RIGHT_BRACKET:    ss << ']';           break;
            case GLFW_KEY_GRAVE_ACCENT:     ss << '`';           break;
            case GLFW_KEY_ESCAPE:           ss << "Escape";      break;
            case GLFW_KEY_ENTER:            ss << "Enter";       break;
            case GLFW_KEY_TAB:              ss << "Tab";         break;
            case GLFW_KEY_BACKSPACE:        ss << "Backspace";   break;
            case GLFW_KEY_INSERT:           ss << "Insert";      break;
            case GLFW_KEY_DELETE:           ss << "Delete";      break;
            case GLFW_KEY_RIGHT:            ss << "Right";       break;
            case GLFW_KEY_LEFT:             ss << "Left";        break;
            case GLFW_KEY_DOWN:             ss << "Down";        break;
            case GLFW_KEY_UP:               ss << "Up";          break;
            case GLFW_KEY_PAGE_UP:          ss << "PageUp";      break;
            case GLFW_KEY_PAGE_DOWN:        ss << "PageDown";    break;
            case GLFW_KEY_HOME:             ss << "Home";        break;
            case GLFW_KEY_END:              ss << "End";         break;
            case GLFW_KEY_CAPS_LOCK:        ss << "CapsLock";    break;
            case GLFW_KEY_SCROLL_LOCK:      ss << "ScrollLock";  break;
            case GLFW_KEY_NUM_LOCK:         ss << "NumLock";     break;
            case GLFW_KEY_PRINT_SCREEN:     ss << "PrintScreen"; break;
            case GLFW_KEY_PAUSE:            ss << "Pause";       break;
            default: throw std::logic_error("unsupported hotkey");
            }
            draw_shadowed_text(g, {item.x0 + 100, item.y0}, {1,1,1,1}, ss.str());
        }
        if(g.is_cursor_over(item) && g.in.type == input::mouse_down && g.in.button == GLFW_MOUSE_BUTTON_LEFT) return true;
    }

    return false;
}

void end_popup(gui & g)
{
    g.end_children();
    g.end_overlay();
    if(g.menu_stack.back().open)
    {
        const auto & r = g.menu_stack.back().r;
        draw_rect(g, r, {0.5f,0.5f,0.5f,1});
        draw_rect(g, {r.x0+1, r.y0+1, r.x1-1, r.y1-1}, {0.2f,0.2f,0.2f,1});
    }
    g.end_overlay();
    auto clicked = g.menu_stack.back().clicked;
    g.menu_stack.pop_back();
    g.menu_stack.back().clicked |= clicked;
}

void end_menu(gui & g)
{
    g.end_children();
    if(g.in.type == input::mouse_down && g.in.button == GLFW_MOUSE_BUTTON_LEFT && !g.menu_stack.back().clicked) g.focused_id = {};
}

/////////////////
// 3D controls //
/////////////////

void plane_translation_dragger(gui & g, const float3 & plane_normal, float3 & point)
{
    if(g.in.type == input::mouse_down && g.in.button == GLFW_MOUSE_BUTTON_LEFT) { g.original_position = point; }

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
    if(g.in.type == input::mouse_down && g.in.button == GLFW_MOUSE_BUTTON_LEFT)
    {
        g.gizmode = gizmo_mode::none;
        auto ray = g.get_ray_from_cursor();
        ray.origin -= position;
        float best_t = std::numeric_limits<float>::infinity(), t;           
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[0], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_x; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[1], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_y; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[2], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_z; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[3], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_yz; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[4], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_zx; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[5], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_xy; best_t = t; }
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

    // Add the gizmo to our 3D draw list
    const float3 colors[] = {
        g.gizmode == gizmo_mode::translate_x ? float3(1,0.5f,0.5f) : float3(1,0,0),
        g.gizmode == gizmo_mode::translate_y ? float3(0.5f,1,0.5f) : float3(0,1,0),
        g.gizmode == gizmo_mode::translate_z ? float3(0.5f,0.5f,1) : float3(0,0,1),
        g.gizmode == gizmo_mode::translate_yz ? float3(0.5f,1,1) : float3(0,1,1),
        g.gizmode == gizmo_mode::translate_zx ? float3(1,0.5f,1) : float3(1,0,1),
        g.gizmode == gizmo_mode::translate_xy ? float3(1,1,0.5f) : float3(1,1,0),   
    };

    auto model = translation_matrix(position), modelIT = inverse(transpose(model));
    for(int i=0; i<6; ++i)
    {
        g.draw.begin_object(g.gizmo_res.meshes[i], g.gizmo_res.program);
        g.draw.set_uniform("u_model", model);
        g.draw.set_uniform("u_modelIT", modelIT);
        g.draw.set_uniform("u_diffuseMtl", colors[i]);
    }
}

void axis_rotation_dragger(gui & g, const float3 & axis, const float3 & center, float4 & orientation)
{
    if(g.ml)
    {
        pose original_pose = {g.original_orientation, g.original_position};
        float3 the_axis = original_pose.transform_vector(axis);
        float4 the_plane = {the_axis, -dot(the_axis, g.click_offset)};
        const ray the_ray = g.get_ray_from_cursor();

        float t;
        if(intersect_ray_plane(the_ray, the_plane, &t))
        {
            float3 center_of_rotation = g.original_position + the_axis * dot(the_axis, g.click_offset - g.original_position);
            float3 arm1 = normalize(g.click_offset - center_of_rotation);
            float3 arm2 = normalize(the_ray.origin + the_ray.direction * t - center_of_rotation); 

            float d = dot(arm1, arm2);
            if(d > 0.999f) { orientation = g.original_orientation; return; }
            float angle = std::acos(d);
            if(angle < 0.001f) { orientation = g.original_orientation; return; }
            auto a = normalize(cross(arm1, arm2));
            orientation = qmul(rotation_quat(a, angle), g.original_orientation);
        }
    }
}

void orientation_gizmo(gui & g, int id, const float3 & center, float4 & orientation)
{
    auto p = pose(orientation, center);
    // On click, set the gizmo mode based on which component the user clicked on
    if(g.in.type == input::mouse_down && g.in.button == GLFW_MOUSE_BUTTON_LEFT)
    {
        g.gizmode = gizmo_mode::none;
        auto ray = detransform(p, g.get_ray_from_cursor());
        float best_t = std::numeric_limits<float>::infinity(), t;           
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[6], &t) && t < best_t) { g.gizmode = gizmo_mode::rotate_yz; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[7], &t) && t < best_t) { g.gizmode = gizmo_mode::rotate_zx; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[8], &t) && t < best_t) { g.gizmode = gizmo_mode::rotate_xy; best_t = t; }
        if(g.gizmode != gizmo_mode::none)
        {
            g.original_position = center;
            g.original_orientation = orientation;
            g.click_offset = p.transform_point(ray.origin + ray.direction*t);
            g.set_pressed(id);
        }
    }

    // If the user has previously clicked on a gizmo component, allow the user to interact with that gizmo
    if(g.is_pressed(id))
    {
        switch(g.gizmode)
        {
        case gizmo_mode::rotate_yz: axis_rotation_dragger(g, {1,0,0}, center, orientation); break;
        case gizmo_mode::rotate_zx: axis_rotation_dragger(g, {0,1,0}, center, orientation); break;
        case gizmo_mode::rotate_xy: axis_rotation_dragger(g, {0,0,1}, center, orientation); break;
        }
    }

    // On release, deactivate the current gizmo mode
    if(g.check_release(id)) g.gizmode = gizmo_mode::none;

    // Add the gizmo to our 3D draw list
    const float3 colors[] = {
        g.gizmode == gizmo_mode::rotate_yz ? float3(0.5f,1,1) : float3(0,1,1),
        g.gizmode == gizmo_mode::rotate_zx ? float3(1,0.5f,1) : float3(1,0,1),
        g.gizmode == gizmo_mode::rotate_xy ? float3(1,1,0.5f) : float3(1,1,0),   
    };

    const auto model = p.matrix();
    for(int i=6; i<9; ++i)
    {
        g.draw.begin_object(g.gizmo_res.meshes[i], g.gizmo_res.program);
        g.draw.set_uniform("u_model", model);
        g.draw.set_uniform("u_modelIT", model); // No scaling, no need to inverse-transpose
        g.draw.set_uniform("u_diffuseMtl", colors[i-6]);
    }
}