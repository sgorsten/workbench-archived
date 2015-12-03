// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "input.h"

struct input_buffer
{
    std::vector<input_event> & events;
    float2 cursor;
    int entered, mods;
};

bool is_cursor_entered(GLFWwindow * window)
{
    return reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(window))->entered;
}

void install_input_callbacks(GLFWwindow * window, std::vector<input_event> & events)
{
    double2 cursor;
    glfwGetCursorPos(window, &cursor.x, &cursor.y);
    glfwSetWindowUserPointer(window, new input_buffer{events, float2(cursor), 0, 0});
    glfwSetCursorPosCallback(window, [](GLFWwindow * win, double x, double y)
    {
        auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(win));
        const float2 cursor(double2(x,y));
        buffer->events.push_back({input::cursor_motion, cursor, buffer->mods, cursor - buffer->cursor, 0, 0, {0,0}, 0});
        buffer->cursor = cursor;
    });
    glfwSetCursorEnterCallback(window, [](GLFWwindow * win, int entered)
    {
        auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(win));
        buffer->entered = entered;
    });
    glfwSetKeyCallback(window, [](GLFWwindow * win, int key, int scancode, int action, int mods)
    {
        auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(win));
        switch(action)
        {
        case GLFW_PRESS: buffer->events.push_back({input::key_down, buffer->cursor, mods, {0,0}, key, 0, {0,0}, 0}); break;
        case GLFW_REPEAT: buffer->events.push_back({input::key_repeat, buffer->cursor, mods, {0,0}, key, 0, {0,0}, 0}); break;
        case GLFW_RELEASE: buffer->events.push_back({input::key_up, buffer->cursor, mods, {0,0}, key, 0, {0,0}, 0}); break;
        }
        buffer->mods = mods;
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow * win, int button, int action, int mods)
    {
        auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(win));
        switch(action)
        {
        case GLFW_PRESS: buffer->events.push_back({input::mouse_down, buffer->cursor, mods, {0,0}, 0, button, {0,0}, 0}); break;
        case GLFW_RELEASE: buffer->events.push_back({input::mouse_up, buffer->cursor, mods, {0,0}, 0, button, {0,0}, 0}); break;
        }
        buffer->mods = mods;
    });
    glfwSetScrollCallback(window, [](GLFWwindow * win, double x, double y)
    {
        auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(win));
        buffer->events.push_back({input::scroll, buffer->cursor, buffer->mods, {0,0}, 0, 0, float2(double2(x,y)), 0});
    });
    glfwSetCharCallback(window, [](GLFWwindow * win, unsigned codepoint)
    {
        auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(win));
        buffer->events.push_back({input::character, buffer->cursor, buffer->mods, {0,0}, 0, 0, {0,0}, codepoint});
    });
}

void emit_empty_event(GLFWwindow * window)
{
    auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(window));
    buffer->events.push_back({input::none, buffer->cursor, buffer->mods});
}

void uninstall_input_callbacks(GLFWwindow * window)
{
    if(auto * buffer = reinterpret_cast<input_buffer *>(glfwGetWindowUserPointer(window)))
    {
        glfwSetCursorPosCallback(window, nullptr);
        glfwSetKeyCallback(window, nullptr);
        glfwSetMouseButtonCallback(window, nullptr);
        glfwSetScrollCallback(window, nullptr);
        glfwSetCharCallback(window, nullptr);
        glfwSetWindowUserPointer(window, nullptr);
        delete buffer;
    }
}
