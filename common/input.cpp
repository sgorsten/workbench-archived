#include "input.h"

static void push_event(GLFWwindow * window, const input_event & event) 
{ 
    auto events = reinterpret_cast<std::vector<input_event> *>(glfwGetWindowUserPointer(window));
    events->push_back(event); 
}

static float2 get_cursor_pos(GLFWwindow * window)
{ 
    double2 p;
    glfwGetCursorPos(window, &p.x, &p.y);
    return float2(p); 
}

static int get_mods(GLFWwindow * window)
{
    int mods;
    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT  ) || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT  )) mods |= GLFW_MOD_SHIFT;
    if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL)) mods |= GLFW_MOD_CONTROL;
    if(glfwGetKey(window, GLFW_KEY_LEFT_ALT    ) || glfwGetKey(window, GLFW_KEY_RIGHT_ALT    )) mods |= GLFW_MOD_ALT;
    if(glfwGetKey(window, GLFW_KEY_LEFT_SUPER  ) || glfwGetKey(window, GLFW_KEY_RIGHT_SUPER  )) mods |= GLFW_MOD_SUPER;
    return mods;
}

void install_input_callbacks(GLFWwindow * window, std::vector<input_event> & events)
{
    glfwSetWindowUserPointer(window, &events);
    glfwSetCursorPosCallback(window, [](GLFWwindow * win, double x, double y)
    {
        push_event(win, {input_event::CURSOR_MOTION, float2(double2(x,y)), get_mods(win), 0, 0, {0,0}, 0});
    });
    glfwSetKeyCallback(window, [](GLFWwindow * win, int key, int scancode, int action, int mods)
    {
        input_event e = {};
        switch(action)
        {
        case GLFW_PRESS: push_event(win, {input_event::KEY_DOWN, get_cursor_pos(win), mods, key, 0, {0,0}, 0}); break;
        case GLFW_REPEAT: push_event(win, {input_event::KEY_REPEAT, get_cursor_pos(win), mods, key, 0, {0,0}, 0}); break;
        case GLFW_RELEASE: push_event(win, {input_event::KEY_UP, get_cursor_pos(win), mods, key, 0, {0,0}, 0}); break;
        }
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow * win, int button, int action, int mods)
    {
        switch(action)
        {
        case GLFW_PRESS: push_event(win, {input_event::MOUSE_DOWN, get_cursor_pos(win), mods, 0, button, {0,0}, 0}); break;
        case GLFW_RELEASE: push_event(win, {input_event::MOUSE_UP, get_cursor_pos(win), mods, 0, button, {0,0}, 0}); break;
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow * win, double x, double y)
    {
        push_event(win, {input_event::SCROLL, get_cursor_pos(win), get_mods(win), 0, 0, float2(double2(x,y)), 0});
    });
    glfwSetCharCallback(window, [](GLFWwindow * win, unsigned codepoint)
    {
        push_event(win, {input_event::CHARACTER, get_cursor_pos(win), get_mods(win), 0, 0, {0,0}, codepoint});
    });
}

void uninstall_input_callbacks(GLFWwindow * window)
{
    glfwSetWindowUserPointer(window, nullptr);
    glfwSetCursorPosCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetMouseButtonCallback(window, nullptr);
    glfwSetScrollCallback(window, nullptr);
    glfwSetCharCallback(window, nullptr);
}
