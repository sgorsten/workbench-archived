// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef INPUT_H
#define INPUT_H

#include "linalg.h"
using namespace linalg::aliases;

#include <vector>
#include <GLFW\glfw3.h>

struct input_event
{
    enum { CURSOR_MOTION, KEY_DOWN, KEY_REPEAT, KEY_UP, MOUSE_DOWN, MOUSE_UP, SCROLL, CHARACTER } type;
    float2 cursor;      // The cursor location, specified in pixels, relative to the top left corner of the window
    int mods;           // Mod flags in play during the current event
    int key;            // Which key was pressed during KEY_DOWN events, held during KEY_REPEAT events, or released during KEY_UP events
    int button;         // Which mouse button was pressed during MOUSE_DOWN events, or released during MOUSE_UP events
    float2 scroll;      // The amount of scrolling which occurred during SCROLL events
    unsigned codepoint; // The unicode codepoint of the character typed during CHARACTER events
};

void install_input_callbacks(GLFWwindow * window, std::vector<input_event> & events);
void uninstall_input_callbacks(GLFWwindow * window);

#endif