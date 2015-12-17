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
    std::string caption;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
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
    int2 placement;

    std::vector<edge> input_edges;

    node(const node_type * type, const int2 & placement) : type(type), placement(placement), input_edges(type->inputs.size()) {}
};

const int corner_radius = 10;
const int title_height = 25;
static int get_node_width(const gui & g, const node & n) 
{ 
    int l=0, r=0;
    for(auto & in : n.type->inputs) l = std::max(l, g.sprites.default_font.get_text_width(in));
    for(auto & out : n.type->outputs) r = std::max(r, g.sprites.default_font.get_text_width(out));
    return std::max(l + r + 50, g.sprites.default_font.get_text_width(n.type->caption) + 16);
}
static int get_in_pins(const node & n) { return static_cast<int>(n.type->inputs.size()); }
static int get_out_pins(const node & n) { return static_cast<int>(n.type->outputs.size()); }
static int get_node_body_height(const node & n) { return std::max(get_in_pins(n), get_out_pins(n)) * 24 + 12; }
static int get_node_height(const node & n) { return title_height + get_node_body_height(n); }
static rect get_node_rect(const gui & g, const node & n) { return {n.placement.x, n.placement.y, n.placement.x+get_node_width(g,n), n.placement.y+get_node_height(n)}; }
static int2 get_input_location (const gui & g, const node & n, size_t index) { auto r = get_node_rect(g,n); return {r.x0, r.y0 + title_height + 18 + 24 * (int)index + std::max(get_out_pins(n) - get_in_pins(n), 0) * 12}; }
static int2 get_output_location(const gui & g, const node & n, size_t index) { auto r = get_node_rect(g,n); return {r.x1, r.y0 + title_height + 18 + 24 * (int)index + std::max(get_in_pins(n) - get_out_pins(n), 0) * 12}; }
static rect get_input_rect (const gui & g, const node & n, size_t index) { auto loc = get_input_location(g,n,index); return {loc.x-8, loc.y-8, loc.x+8, loc.y+8}; }
static rect get_output_rect(const gui & g, const node & n, size_t index) { auto loc = get_output_location(g,n,index); return {loc.x-8, loc.y-8, loc.x+8, loc.y+8}; }

const node_type types[] = {
    {"Add", {"A", "B"}, {"A + B"}},
    {"Subtract", {"A", "B"}, {"A - B"}},
    {"Multiply", {"A", "B"}, {"A * B"}},
    {"Divide", {"A", "B"}, {"A / B"}},
    {"Make Float2", {"X", "Y"}, {"(X, Y)"}},
    {"Make Float3", {"X", "Y", "Z"}, {"(X, Y, Z)"}},
    {"Make Float4", {"X", "Y", "Z", "W"}, {"(X, Y, Z, W)"}},
    {"Break Float2", {"(X, Y)"}, {"X", "Y"}},
    {"Break Float3", {"(X, Y, Z)"}, {"X", "Y", "Z"}},
    {"Break Float4", {"(X, Y, Z, W)"}, {"X", "Y", "Z", "W"}},
    {"Normalize Vector", {"V"}, {"V / |V|"}},
};

bool is_subsequence(const std::string & seq, const std::string & sub)
{
    auto it = begin(seq);
    for(char ch : sub)
    {
        bool match = false;
        for(; it != end(seq); ++it)
        {
            if(toupper(ch) == toupper(*it))
            {
                match = true;
                break;
            }
        }
        if(!match) return false;
    }
    return true;
}

struct graph
{
    transform_2d view;
    std::vector<node *> nodes;

    node * link_input_node, * link_output_node;
    size_t link_input_pin, link_output_pin;

    int2 popup_loc;
    std::string node_filter;
    int node_scroll;

    graph() { reset_link(); }

    void reset_link()
    {
        link_input_node = link_output_node = nullptr;
    }

    void draw_wire(gui & g, const float2 & p0, const float2 & p1) const
    {
        g.draw_bezier_curve(p0, float2(p0.x + abs(p1.x - p0.x)*0.7f, p0.y), float2(p1.x - abs(p1.x - p0.x)*0.7f, p1.y), p1, 2, {1,1,1,1});    
    }

    void new_node_popup(gui & g, int id)
    {
        if(g.is_mouse_down(GLFW_MOUSE_BUTTON_RIGHT))
        {
            g.begin_children(id);
            g.set_pressed(1);
            g.focused_id = g.pressed_id;
            g.pressed_id = {};
            popup_loc = int2(g.in.cursor);            
            node_filter = "";
        }

        if(g.is_focused(id) || g.is_child_focused(id))
        {
            int w = 0;
            int n = 0;
            for(auto & type : types)
            {
                w = std::max(w, g.sprites.default_font.get_text_width(type.caption));
                if(is_subsequence(type.caption, node_filter)) ++n;
            }

            auto loc = popup_loc;
            const rect overlay = {loc.x, loc.y, loc.x + w + 30, loc.y + 200}; 
            g.begin_children(id);
            g.begin_overlay();      

            g.draw_rect(overlay, {0.7f,0.7f,0.7f,1});
            g.draw_rect({overlay.x0+1, overlay.y0+1, overlay.x1-1, overlay.y1-1}, {0.3f,0.3f,0.3f,1});
            edit(g, 1, {overlay.x0+4, overlay.y0+4, overlay.x1-4, overlay.y0 + g.sprites.default_font.line_height + 8}, node_filter);

            auto c = vscroll_panel(g, 2, {overlay.x0 + 1, overlay.y0 + g.sprites.default_font.line_height + 12, overlay.x1 - 1, overlay.y1 - 1}, (g.sprites.default_font.line_height+4) * n - 4, node_scroll);
            
            g.begin_scissor(c);
            g.begin_transform(transform_2d::translation(float2(0, -node_scroll)));
            int y = c.y0;
            for(auto & type : types)
            {
                if(!is_subsequence(type.caption, node_filter)) continue;

                rect r = {c.x0, y, c.x1, y + g.sprites.default_font.line_height};
                if(g.check_click(3, r))
                {
                    nodes.push_back(new node{&type, int2(view.detransform_point(float2(popup_loc)))});
                    g.focused_id = {};
                }
                if(g.is_cursor_over(r)) g.draw_rect(r, {0.7f,0.7f,0.3f,1});
                g.draw_shadowed_text({r.x0+4, r.y0}, type.caption, {1,1,1,1});                
                y = r.y1 + 4;
            }
            g.end_transform();
            g.end_scissor();

            g.end_overlay();
            g.end_children();
            if(overlay.contains(g.in.cursor)) g.consume_input();
            else if(g.in.type == input::mouse_down) g.focused_id = {};
        }
    }

    void on_gui(gui & g)
    {
        const int ID_NEW_WIRE = 1, ID_POPUP_MENU = 2, ID_DRAG_GRAPH = 3;
        int id = 4;

        new_node_popup(g, ID_POPUP_MENU);

        g.begin_transform(view);
        
        // Draw wires
        for(auto & n : nodes)
        {
            for(size_t i=0; i<n->input_edges.size(); ++i)
            {
                if(!n->input_edges[i].other) continue;
                const auto p0 = float2(get_output_location(g, *n->input_edges[i].other, n->input_edges[i].pin)), p3 = float2(get_input_location(g, *n, i));
                draw_wire(g, p0, p3);
            }
        }

        // Clickable + draggable nodes
        for(auto & n : nodes)
        {
            // Input pin interactions
            for(size_t i=0; i<n->type->inputs.size(); ++i)
            {
                if(g.check_click(ID_NEW_WIRE, get_input_rect(g,*n,i)))
                {
                    n->input_edges[i].other = nullptr;
                    link_input_node = n;
                    link_input_pin = i;
                    g.consume_input();
                }

                if(g.is_cursor_over(get_input_rect(g,*n,i)) && link_output_node && g.check_release(ID_NEW_WIRE))
                {
                    n->input_edges[i] = {link_output_node, link_output_pin};
                    reset_link();
                }
            }
            
            // Output pin interactions
            for(size_t i=0; i<n->type->outputs.size(); ++i)
            {
                if(g.check_click(ID_NEW_WIRE, get_output_rect(g,*n,i)))
                {
                    if(g.is_alt_held()) for(auto & other : nodes) for(auto & edge : other->input_edges) if(edge.other == n) edge.other = nullptr;
                    link_output_node = n;
                    link_output_pin = i;
                    g.consume_input();
                }

                if(g.is_cursor_over(get_output_rect(g,*n,i)) && link_input_node && g.check_release(ID_NEW_WIRE))
                {
                    link_input_node->input_edges[link_input_pin] = {n, i};
                    reset_link();
                }
            }

            // Drag the body of the node
            if(g.check_click(id, get_node_rect(g,*n))) g.consume_input();
            if(g.check_pressed(id))
            {
                n->placement = int2(g.get_cursor() - g.click_offset);
            }

            // Draw the node
            auto r = get_node_rect(g,*n);
            g.draw_partial_rounded_rect({r.x0, r.y0, r.x1, r.y0+title_height}, corner_radius, {0.5f,0.5f,0.5f,0.85f}, true, true, false, false);
            g.draw_partial_rounded_rect({r.x0, r.y0+title_height, r.x1, r.y1}, corner_radius, {0.3f,0.3f,0.3f,0.85f}, false, false, true, true);
            g.begin_scissor({r.x0, r.y0, r.x1, r.y0+title_height});
            g.draw_shadowed_text({r.x0+8, r.y0+6}, n->type->caption, {1,1,1,1});
            g.end_scissor();

            for(size_t i=0; i<n->type->inputs.size(); ++i)
            {
                const auto loc = get_input_location(g,*n,i);
                g.draw_circle(loc, 8, {1,1,1,1});
                g.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
                g.draw_shadowed_text(loc + int2(12, -g.buffer.get_library().default_font.line_height/2), n->type->inputs[i], {1,1,1,1});

                if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
                {
                    draw_tooltip(g.buffer, loc, "This is an input");
                }
            }
            for(size_t i=0; i<n->type->outputs.size(); ++i)
            {
                const auto loc = get_output_location(g,*n,i);
                g.draw_circle(loc, 8, {1,1,1,1});
                g.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
                g.draw_shadowed_text(loc + int2(-12 - g.buffer.get_library().default_font.get_text_width(n->type->outputs[i]), -g.buffer.get_library().default_font.line_height/2), n->type->outputs[i], {1,1,1,1});

                if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
                {
                    draw_tooltip(g.buffer, loc, "This is an output");
                }
            }
            ++id;
        }

        // Fill in pins with connected wires
        for(auto & n : nodes)
        {
            for(size_t i=0; i<n->input_edges.size(); ++i)
            {
                if(!n->input_edges[i].other) continue;
                g.draw_circle(get_output_location(g, *n->input_edges[i].other, n->input_edges[i].pin), 7, {1,1,1,1});
                g.draw_circle(get_input_location(g, *n, i), 7, {1,1,1,1});
            }
        }

        // If the user is currently dragging a wire between two pins, draw it
        if(g.is_pressed(ID_NEW_WIRE))
        {
            auto p0 = g.get_cursor(), p1 = p0;
            if(link_output_node)
            {
                auto loc = get_output_location(g, *link_output_node, link_output_pin);
                g.draw_circle(loc, 7, {1,1,1,1});
                p0 = float2(loc);
            }
            if(link_input_node)
            {
                auto loc = get_input_location(g, *link_input_node, link_input_pin);
                g.draw_circle(loc, 7, {1,1,1,1});
                p1 = float2(loc);
            }
            draw_wire(g, p0, p1);

            if(g.check_release(ID_NEW_WIRE)) reset_link();
        }

        // Do the scrollable, zoomable background
        scrollable_zoomable_background(g, ID_DRAG_GRAPH, view);

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

    graph gr;
    gr.view = {1,{0,0}};
    gr.nodes = {
        new node{&types[0], {50,50}},
        new node{&types[1], {650,150}}
    };
    gr.nodes[1]->input_edges[1] = edge(gr.nodes[0], 0);

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