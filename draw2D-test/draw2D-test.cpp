#include <vector>
#include <GLFW\glfw3.h>
#include "draw2D.h"
#include <cassert>

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

    void draw(draw_buffer_2d & buffer, const rect & r) const
    {
        buffer.draw_partial_rounded_rect({r.x0, r.y0, r.x1, r.y0+title_height}, corner_radius, {0.5f,0.5f,0.5f,1}, true, true, false, false);
        buffer.draw_partial_rounded_rect({r.x0, r.y0+title_height, r.x1, r.y1}, corner_radius, {0.3f,0.3f,0.3f,1}, false, false, true, true);
        buffer.draw_shadowed_text({r.x0+8, r.y0+6}, caption, {1,1,1,1});

        for(size_t i=0; i<inputs.size(); ++i)
        {
            const auto loc = get_input_location(r,i);
            buffer.draw_circle(loc, 8, {1,1,1,1});
            buffer.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
            buffer.draw_shadowed_text(loc + int2(12, -buffer.get_library().default_font.line_height/2), inputs[i], {1,1,1,1});
        }
        for(size_t i=0; i<outputs.size(); ++i)
        {
            const auto loc = get_output_location(r,i);
            buffer.draw_circle(loc, 8, {1,1,1,1});
            buffer.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
            buffer.draw_shadowed_text(loc + int2(-12 - buffer.get_library().default_font.get_text_width(outputs[i]), -buffer.get_library().default_font.line_height/2), outputs[i], {1,1,1,1});

            if(i == 1) draw_tooltip(buffer, loc, "Tooltip in an overlay");
        }
    }
};

struct node
{
    const node_type * type;
    rect placement;

    int2 get_input_location(size_t index) const { return type->get_input_location(placement, index); }
    int2 get_output_location(size_t index) const { return type->get_output_location(placement, index); }
    void draw(draw_buffer_2d & buffer) const { type->draw(buffer, placement); }
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
    draw_buffer_2d buffer;

    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Draw2D Test", nullptr, nullptr);
    glfwMakeContextCurrent(win);

    GLuint tex = make_sprite_texture_opengl(sprites.sheet);

    const node_type type = {"Graph Node", {"Input 1", "Input 2"}, {"Output 1", "Output 2", "Output 3"}};
    const node nodes[] = {
        {&type, {50,50,300,250}},
        {&type, {650,150,900,350}}
    };
    const edge edges[] = {
        {&nodes[0], 0, &nodes[1], 0, false},
        {&nodes[0], 2, &nodes[1], 1, true}
    };

    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        int w, h;
        glfwGetWindowSize(win, &w, &h);
        buffer.begin_frame(sprites, {w, h});
        for(auto & n : nodes) n.draw(buffer);
        for(auto & e : edges) e.draw(buffer);
        buffer.end_frame();

        glClear(GL_COLOR_BUFFER_BIT);
        render_draw_buffer_opengl(buffer, tex);
        glfwSwapBuffers(win);
    }

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