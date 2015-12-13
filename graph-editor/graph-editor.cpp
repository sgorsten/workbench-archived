// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

void draw_tooltip(draw_buffer_2d & buffer, const int2 & loc, utf8::string_view text)
{
    int w = buffer.get_library().default_font.get_text_width(text), h = buffer.get_library().default_font.line_height;

    buffer.begin_overlay();
    buffer.draw_partial_rounded_rect({loc.x+10, loc.y, loc.x+w+20, loc.y+h+10}, 8, {0.5f,0.5f,0.5f,1}, 0, 1, 1, 1);
    buffer.draw_partial_rounded_rect({loc.x+11, loc.y+1, loc.x+w+19, loc.y+h+9}, 7, {0.3f,0.3f,0.3f,1}, 0, 1, 1, 1);
    buffer.draw_shadowed_text({loc.x+15, loc.y+5}, text, {1,1,1,1});
    buffer.end_overlay();
}

struct node_type
{
    static const int corner_radius = 10;
    static const int title_height = 25;

    std::string caption;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    int2 get_input_location(const rect & r, size_t index) const { return {r.x0, r.y0 + title_height + 18 + 24 * (int)index}; }
    int2 get_output_location(const rect & r, size_t index) const { return {r.x1, r.y0 + title_height + 18 + 24 * (int)index}; }

    void draw(gui & g, const rect & r) const
    {
        g.buffer.draw_partial_rounded_rect({r.x0, r.y0, r.x1, r.y0+title_height}, corner_radius, {0.5f,0.5f,0.5f,1}, true, true, false, false);
        g.buffer.draw_partial_rounded_rect({r.x0, r.y0+title_height, r.x1, r.y1}, corner_radius, {0.3f,0.3f,0.3f,1}, false, false, true, true);
        g.buffer.begin_scissor({r.x0, r.y0, r.x1, r.y0+title_height});
        g.buffer.draw_shadowed_text({r.x0+8, r.y0+6}, caption, {1,1,1,1});
        g.buffer.end_scissor();

        for(size_t i=0; i<inputs.size(); ++i)
        {
            const auto loc = get_input_location(r,i);
            g.buffer.draw_circle(loc, 8, {1,1,1,1});
            g.buffer.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
            g.buffer.draw_shadowed_text(loc + int2(12, -g.buffer.get_library().default_font.line_height/2), inputs[i], {1,1,1,1});

            if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
            {
                draw_tooltip(g.buffer, loc, "This is an input");
            }
        }
        for(size_t i=0; i<outputs.size(); ++i)
        {
            const auto loc = get_output_location(r,i);
            g.buffer.draw_circle(loc, 8, {1,1,1,1});
            g.buffer.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
            g.buffer.draw_shadowed_text(loc + int2(-12 - g.buffer.get_library().default_font.get_text_width(outputs[i]), -g.buffer.get_library().default_font.line_height/2), outputs[i], {1,1,1,1});

            if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
            {
                draw_tooltip(g.buffer, loc, "This is an output");
            }
        }
    }
};

#include <iostream>

struct node
{
    const node_type * type;
    rect placement;

    int2 get_input_location(size_t index) const { return type->get_input_location(placement, index); }
    int2 get_output_location(size_t index) const { return type->get_output_location(placement, index); }
    void on_gui(gui & g, int id) 
    { 
        g.check_click(id, placement);
        if(g.check_pressed(id))
        {
            int2 delta = int2(g.get_cursor() - g.click_offset) - int2(placement.x0, placement.y0);
            placement.x0 += delta.x;
            placement.y0 += delta.y;
            placement.x1 += delta.x;
            placement.y1 += delta.y;
        }
        type->draw(g, placement); 
    }
};

struct edge
{
    const node * output_node;
    int output_index;
    const node * input_node;
    int input_index;
    bool curved;

    void draw(draw_buffer_2d & buffer) const
    {
        const auto p0 = float2(output_node->get_output_location(output_index));
        const auto p3 = float2(input_node->get_input_location(input_index));
        const auto p1 = float2((p0.x+p3.x)/2, p0.y), p2 = float2((p0.x+p3.x)/2, p3.y);
        buffer.draw_circle(output_node->get_output_location(output_index), 7, {1,1,1,1});
        buffer.draw_circle(input_node->get_input_location(input_index), 7, {1,1,1,1});
        if(curved) buffer.draw_bezier_curve(p0, p1, p2, p3, 2, {1,1,1,1});
        else buffer.draw_line(p0, p3, 2, {1,1,1,1});
    }
};

GLuint make_sprite_texture_opengl(const sprite_sheet & sprites);
void render_draw_buffer_opengl(const draw_buffer_2d & buffer, GLuint sprite_texture);

int main()
{
    sprite_library sprites;
    gui g(sprites);

    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Draw2D Test", nullptr, nullptr);
    std::vector<input_event> events;
    install_input_callbacks(win, events);
    glfwMakeContextCurrent(win);

    GLuint tex = make_sprite_texture_opengl(sprites.sheet);

    const node_type type = {"Graph Node with a long title that will clip the edge of the node", {"Input 1", "Input 2"}, {"Output 1", "Output 2", "Output 3"}};
    node nodes[] = {
        {&type, {50,50,300,250}},
        {&type, {650,150,900,350}}
    };
    const edge edges[] = {
        {&nodes[0], 0, &nodes[1], 0, false},
        {&nodes[0], 2, &nodes[1], 1, true}
    };

    transform_2d cam = {1,{0,0}};
    bool ml = false, mr = false;
    while(!glfwWindowShouldClose(win))
    {
        g.icon = cursor_icon::arrow;
        glfwPollEvents();

        if(events.empty()) emit_empty_event(win);
        g.in = events.front();
        events.erase(begin(events));

        switch(g.in.type)
        {
        case input::scroll:
            if(g.in.scroll.y > 0) cam = transform_2d::scaling(1.25f, g.in.cursor) * cam;
            if(g.in.scroll.y < 0) cam = transform_2d::scaling(0.80f, g.in.cursor) * cam;
            if(cam.scale > 0.85f && cam.scale < 1.20f) cam = transform_2d::scaling(1/cam.scale, g.in.cursor) * cam;
            break;
        case input::mouse_down: case input::mouse_up:
            switch(g.in.button)
            {
            case GLFW_MOUSE_BUTTON_LEFT: ml = g.in.is_down(); break;
            case GLFW_MOUSE_BUTTON_RIGHT: mr = g.in.is_down(); break;
            }
            break;
        case input::cursor_motion:
            if(mr) cam = transform_2d::translation(g.in.motion) * cam;
            break;
        }

        int2 window_size;
        glfwGetWindowSize(win, &window_size.x, &window_size.y);
        g.begin_frame(window_size);
        g.buffer.begin_transform(cam);    
        for(int i=0; i<2; ++i) nodes[i].on_gui(g, i+1);
        for(auto & e : edges) e.draw(g.buffer);
        g.buffer.end_transform();

        g.end_frame();

        glClear(GL_COLOR_BUFFER_BIT);
        render_draw_buffer_opengl(g.buffer, tex);
        glfwSwapBuffers(win);
    }

    uninstall_input_callbacks(win);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}

GLuint make_sprite_texture_opengl(const sprite_sheet & sprites)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, sprites.get_texture_dims().x, sprites.get_texture_dims().y, 0, GL_ALPHA, GL_UNSIGNED_BYTE, sprites.get_texture_data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void render_draw_buffer_opengl(const draw_buffer_2d & buffer, GLuint sprite_texture)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_TEXTURE_2D); 
    glBindTexture(GL_TEXTURE_2D, sprite_texture);
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for(GLenum array : {GL_VERTEX_ARRAY, GL_TEXTURE_COORD_ARRAY, GL_COLOR_ARRAY}) glEnableClientState(array);
    glVertexPointer(2, GL_FLOAT, sizeof(draw_buffer_2d::vertex), &buffer.get_vertices().data()->position);
    glTexCoordPointer(2, GL_FLOAT, sizeof(draw_buffer_2d::vertex), &buffer.get_vertices().data()->texcoord);
    glColorPointer(4, GL_FLOAT, sizeof(draw_buffer_2d::vertex), &buffer.get_vertices().data()->color);
    glDrawElements(GL_TRIANGLES, buffer.get_indices().size(), GL_UNSIGNED_SHORT, buffer.get_indices().data());
    for(GLenum array : {GL_VERTEX_ARRAY, GL_TEXTURE_COORD_ARRAY, GL_COLOR_ARRAY}) glDisableClientState(array);
    glPopAttrib();
}