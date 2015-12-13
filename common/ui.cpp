// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

#include <cassert>
#include <cctype>
#include <algorithm>
#include <sstream>

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

gui::gui(sprite_library & sprites) : sprites(sprites), in({})
{
    std::vector<int> codepoints;
    for(int i=0xf000; i<=0xf295; ++i) codepoints.push_back(i);
    sprites.default_font.load_glyphs("fontawesome-webfont.ttf", 14, codepoints);
    sprites.sheet.prepare_texture();
}

bool gui::is_cursor_over(const rect & r) const
{
    const auto & s = buffer.get_scissor_rect();
    return get_cursor().x >= r.x0 && get_cursor().y >= r.y0 && get_cursor().x < r.x1 && get_cursor().y < r.y1
        && in.cursor.x >= s.x0 && in.cursor.y >= s.y0 && in.cursor.x < s.x1 && in.cursor.y < s.y1;
}

bool gui::check_click(int id, const rect & r)
{
    if(in.type == input::mouse_down && in.button == GLFW_MOUSE_BUTTON_LEFT && is_cursor_over(r))
    {
        click_offset.x = get_cursor().x - r.x0;
        click_offset.y = get_cursor().y - r.y0;
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
    if(g.is_pressed(id)) g.text_cursor = g.sprites.default_font.get_cursor_pos(text, g.get_cursor().x - r.x0 - 5);

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
    if(g.check_pressed(id)) offset = (static_cast<int>(g.get_cursor().y - g.click_offset.y) - r.y0) * client_height / r.height();
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
    if(g.check_pressed(id)) split = static_cast<int>(g.get_cursor().x - g.click_offset.x) - r.x0;
    split = std::min(split, r.x1 - 10 - splitbar_width);
    split = std::max(split, r.x0 + 10);

    const rect splitbar = {r.x0 + split, r.y0, r.x0 + split + splitbar_width, r.y1};
    if(g.is_cursor_over(splitbar)) g.icon = cursor_icon::hresize;
    g.check_click(id, splitbar);
    return {{r.x0, r.y0, splitbar.x0, r.y1}, {splitbar.x1, r.y0, r.x1, r.y1}};
}

std::pair<rect, rect> vsplitter(gui & g, int id, const rect & r, int & split)
{
    if(g.check_pressed(id)) split = static_cast<int>(g.get_cursor().y - g.click_offset.y) - r.y0;
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
        if(g.is_cursor_over(item) && g.in.type == input::mouse_down && g.in.button == GLFW_MOUSE_BUTTON_LEFT)
        {
            g.in.type = input::none;
            g.focused_id = {};
            return true;
        }
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
