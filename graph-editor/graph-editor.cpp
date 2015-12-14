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
        g.draw_partial_rounded_rect({r.x0, r.y0, r.x1, r.y0+title_height}, corner_radius, {0.5f,0.5f,0.5f,0.85f}, true, true, false, false);
        g.draw_partial_rounded_rect({r.x0, r.y0+title_height, r.x1, r.y1}, corner_radius, {0.3f,0.3f,0.3f,0.85f}, false, false, true, true);
        g.begin_scissor({r.x0, r.y0, r.x1, r.y0+title_height});
        g.draw_shadowed_text({r.x0+8, r.y0+6}, caption, {1,1,1,1});
        g.end_scissor();

        for(size_t i=0; i<inputs.size(); ++i)
        {
            const auto loc = get_input_location(r,i);
            g.draw_circle(loc, 8, {1,1,1,1});
            g.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
            g.draw_shadowed_text(loc + int2(12, -g.buffer.get_library().default_font.line_height/2), inputs[i], {1,1,1,1});

            if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
            {
                draw_tooltip(g.buffer, loc, "This is an input");
            }
        }
        for(size_t i=0; i<outputs.size(); ++i)
        {
            const auto loc = get_output_location(r,i);
            g.draw_circle(loc, 8, {1,1,1,1});
            g.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
            g.draw_shadowed_text(loc + int2(-12 - g.buffer.get_library().default_font.get_text_width(outputs[i]), -g.buffer.get_library().default_font.line_height/2), outputs[i], {1,1,1,1});

            if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
            {
                draw_tooltip(g.buffer, loc, "This is an output");
            }
        }
    }
};

struct editor_state;

struct node;

struct edge
{
    const node * other;
    size_t pin;

    edge() : other(), pin() {}
    edge(const node * other, size_t pin) : other(other), pin(pin) {}
};

struct node
{
    const node_type * type;
    rect placement;

    std::vector<edge> input_edges;

    node(const node_type * type, const rect & placement) : type(type), placement(placement), input_edges(type->inputs.size()) {}

    int2 get_input_location(size_t index) const { return type->get_input_location(placement, index); }
    int2 get_output_location(size_t index) const { return type->get_output_location(placement, index); }
    rect get_input_rect(size_t index) const { auto loc = get_input_location(index); return {loc.x-8, loc.y-8, loc.x+8, loc.y+8}; }
    rect get_output_rect(size_t index) const { auto loc = get_output_location(index); return {loc.x-8, loc.y-8, loc.x+8, loc.y+8}; }
};

struct graph
{
    transform_2d view;
    std::vector<node> nodes;

    node * link_input_node, * link_output_node;
    size_t link_input_pin, link_output_pin;

    graph() { reset_link(); }

    void reset_link()
    {
        link_input_node = link_output_node = nullptr;
    }

    void on_gui(gui & g)
    {
        int id = 2;
        g.begin_transform(view);

        // Draw wires
        for(auto & n : nodes)
        {
            for(size_t i=0; i<n.input_edges.size(); ++i)
            {
                if(!n.input_edges[i].other) continue;
                const auto p0 = float2(n.input_edges[i].other->get_output_location(n.input_edges[i].pin)), p3 = float2(n.get_input_location(i));
                const auto p1 = float2(p0.x + abs(p3.x - p0.x)*0.7f, p0.y), p2 = float2(p3.x - abs(p3.x - p0.x)*0.7f, p3.y);
                g.draw_bezier_curve(p0, p1, p2, p3, 2, {1,1,1,1});
            }
        }

        // Clickable + draggable nodes
        for(auto & n : nodes)
        {
            // Input pin interactions
            for(size_t i=0; i<n.type->inputs.size(); ++i)
            {
                if(g.check_click(1, n.get_input_rect(i)))
                {
                    n.input_edges[i].other = nullptr;
                    link_input_node = &n;
                    link_input_pin = i;
                    g.consume_input();
                }

                if(g.is_cursor_over(n.get_input_rect(i)) && link_output_node && g.check_release(1))
                {
                    n.input_edges[i] = {link_output_node, link_output_pin};
                    reset_link();
                }
            }
            
            // Output pin interactions
            for(size_t i=0; i<n.type->outputs.size(); ++i)
            {
                if(g.check_click(1, n.get_output_rect(i)))
                {
                    if(g.is_alt_held()) for(auto & other : nodes) for(auto & edge : other.input_edges) if(edge.other == &n) edge.other = nullptr;
                    link_output_node = &n;
                    link_output_pin = i;
                    g.consume_input();
                }

                if(g.is_cursor_over(n.get_output_rect(i)) && link_input_node && g.check_release(1))
                {
                    link_input_node->input_edges[link_input_pin] = {&n, i};
                    reset_link();
                }
            }

            // Drag the body of the node
            if(g.check_click(id, n.placement)) g.consume_input();
            if(g.check_pressed(id))
            {
                int2 delta = int2(g.get_cursor() - g.click_offset) - int2(n.placement.x0, n.placement.y0);
                n.placement.x0 += delta.x;
                n.placement.y0 += delta.y;
                n.placement.x1 += delta.x;
                n.placement.y1 += delta.y;
            }

            // Draw the node
            n.type->draw(g, n.placement);
            ++id;
        }

        // Fill in pins with connected wires
        for(auto & n : nodes)
        {
            for(size_t i=0; i<n.input_edges.size(); ++i)
            {
                if(!n.input_edges[i].other) continue;
                g.draw_circle(n.input_edges[i].other->get_output_location(n.input_edges[i].pin), 7, {1,1,1,1});
                g.draw_circle(n.get_input_location(i), 7, {1,1,1,1});
            }
        }

        // If the user is currently dragging a wire between two pins, draw it
        if(g.is_pressed(1))
        {
            auto p0 = g.get_cursor(), p3 = p0;
            if(link_output_node)
            {
                auto loc = link_output_node->get_output_location(link_output_pin);
                g.draw_circle(loc, 7, {1,1,1,1});
                p0 = float2(loc);
            }
            if(link_input_node)
            {
                auto loc = link_input_node->get_input_location(link_input_pin);
                g.draw_circle(loc, 7, {1,1,1,1});
                p3 = float2(loc);
            }
            const auto p1 = float2((p0.x+p3.x)/2, p0.y), p2 = float2((p0.x+p3.x)/2, p3.y);
            g.draw_bezier_curve(p0, p1, p2, p3, 2, {1,1,1,1});

            if(g.check_release(1)) reset_link();
        }

        // Do the scrollable, zoomable background
        scrollable_zoomable_background(g, id, view);

        g.end_transform();

    }
};

GLuint make_sprite_texture_opengl(const sprite_sheet & sprites);
void render_draw_buffer_opengl(const draw_buffer_2d & buffer, GLuint sprite_texture);

int main()
{
    gui g;

    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Graph Editor", nullptr, nullptr);
    std::vector<input_event> events;
    install_input_callbacks(win, events);
    glfwMakeContextCurrent(win);

    GLuint tex = make_sprite_texture_opengl(g.sprites.sheet);

    const node_type type = {"Graph Node with a long title that will clip the edge of the node", {"Input 1", "Input 2"}, {"Output 1", "Output 2", "Output 3"}};
    graph gr;
    gr.view = {1,{0,0}};
    gr.nodes = {
        {&type, {50,50,300,250}},
        {&type, {650,150,900,350}}
    };
    gr.nodes[1].input_edges[1] = edge(&gr.nodes[0], 1);

    while(!glfwWindowShouldClose(win))
    {
        g.icon = cursor_icon::arrow;
        glfwPollEvents();

        int2 window_size;
        glfwGetWindowSize(win, &window_size.x, &window_size.y);
        if(events.empty()) emit_empty_event(win);
        g.begin_frame(window_size, events.front());
        events.erase(begin(events));

        gr.on_gui(g);
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