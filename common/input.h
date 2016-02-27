// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef INPUT_H
#define INPUT_H

#include "../thirdparty/linalg/linalg.h"
using namespace linalg::aliases;

#include <vector>
#include <GLFW\glfw3.h>

enum class input { none, cursor_motion, key_down, key_repeat, key_up, mouse_down, mouse_up, scroll, character };
struct input_event
{
    input    type;      // Which input occurred during this event?
    float2   cursor;    // The cursor location, specified in pixels, relative to the top left corner of the window
    int      mods;      // Mod flags in play during the current event
    float2   motion;    // The amount the cursor has moved, in pixels, during input::cursor_motion
    int      key;       // Which key was pressed during input::key_down, held during input::key_repeat, or released during input::key_up
    int      button;    // Which mouse button was pressed during input::mouse_down, or released during input::mouse_up
    float2   scroll;    // The amount of scrolling which occurred during input::scroll
    unsigned codepoint; // The unicode codepoint of the character typed during input::character

    bool is_down() const { return type == input::key_down || type == input::key_repeat || type == input::mouse_down; }
    bool is_up() const { return type == input::key_up || type == input::mouse_up; }
};

void install_input_callbacks(GLFWwindow * window, std::vector<input_event> & events);
void emit_empty_event(GLFWwindow * window);
void uninstall_input_callbacks(GLFWwindow * window);
bool is_cursor_entered(GLFWwindow * window);

#endif