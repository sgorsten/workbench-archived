#include <vector>
#include <GLFW\glfw3.h>
#include "linalg.h"
using namespace linalg::aliases;

#include "sprite.h"
#include "font.h"

#include <cassert>

struct sprite_library
{
    sprite_sheet sheet;
    font default_font;
    std::map<int, size_t> corner_sprites;

    sprite_library() : default_font(&sheet)
    {
        std::vector<int> codepoints;
        for(int i=32; i<256; ++i) codepoints.push_back(i);
        default_font.load_glyphs("c:/windows/fonts/arialbd.ttf", 14, codepoints);
        for(int i=1; i<=32; ++i) corner_sprites[i] = sheet.insert_sprite(make_circle_quadrant(i));
        sheet.prepare_texture();
    }
};

struct draw_buffer_2d
{
    struct vertex { float2 position, texcoord; float4 color; };
    struct list { size_t level,first,last; };

    std::vector<vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<list> lists;
    float sx,sy;

    void begin_frame(const int2 & window_size)
    {
        vertices.clear();
        indices.clear();
        lists.clear();
        sx = 2.0f / window_size.x;
        sy = 2.0f / window_size.y;
    }

    void emit_polygon(const vertex * verts, size_t vert_count)
    {
        if(vertices.size() + vert_count > 0x10000) throw std::runtime_error("draw buffer overflow");
        const uint16_t base = vertices.size();
        for(uint16_t i=2; i<vert_count; ++i)
        {
            indices.push_back(base);
            indices.push_back(base+i-1);
            indices.push_back(base+i);
        }
        for(auto end = verts + vert_count; verts != end; ++verts)
        {
            vertices.push_back({{verts->position.x*sx-1, 1-verts->position.y*sy}, verts->texcoord, verts->color});
        }
    }

    void emit_rect(int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, const float4 & color)
    {
        const vertex verts[] = {
            {{(float)x0,(float)y0}, {s0,t0}, color},
            {{(float)x1,(float)y0}, {s1,t0}, color},
            {{(float)x1,(float)y1}, {s1,t1}, color},
            {{(float)x0,(float)y1}, {s0,t1}, color}
        };
        emit_polygon(verts, 4);
    }
};

GLuint make_sprite_texture_opengl(const sprite_sheet & sprites);
void render_draw_buffer_opengl(const draw_buffer_2d & buffer, GLuint sprite_texture);

void draw_line(draw_buffer_2d & buffer, const sprite_library & sprites, const float2 & p0, const float2 & p1, float width, const float4 & color)
{
    const auto & sprite = sprites.sheet.get_sprite(0);
    const float2 perp = normalize(cross(float3(p1-p0,0), float3(0,0,1)).xy());
    const draw_buffer_2d::vertex verts[] = {
        {p0+perp*(width/2), {sprite.s0,sprite.t0}, color},
        {p1+perp*(width/2), {sprite.s1,sprite.t0}, color},
        {p1-perp*(width/2), {sprite.s1,sprite.t1}, color},
        {p0-perp*(width/2), {sprite.s0,sprite.t1}, color}
    };
    buffer.emit_polygon(verts, 4);
}

void draw_bezier_curve(draw_buffer_2d & buffer, const sprite_library & sprites, const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, float width, const float4 & color)
{
    const auto & sprite = sprites.sheet.get_sprite(0);
    const float2 d01 = p1-p0, d12 = p2-p1, d23 = p3-p2;
    float2 v0, v1;
    for(float i=0; i<=32; ++i)
    {
        float t = i/32, s = (1-t);
        const float2 p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
        const float2 d = normalize(d01*(3*s*s) + d12*(6*s*t) + d23*(3*t*t)) * (width/2);
        const float2 v2 = {p.x-d.y, p.y+d.x}, v3 = {p.x+d.y, p.y-d.x};
        if(i)
        {
            const draw_buffer_2d::vertex verts[] = {
                {v0, {sprite.s0,sprite.t0}, color},
                {v1, {sprite.s1,sprite.t0}, color},
                {v2, {sprite.s1,sprite.t1}, color},
                {v3, {sprite.s0,sprite.t1}, color}
            };
            buffer.emit_polygon(verts, 4);
        }
        v0 = v3;
        v1 = v2;
    }
}

void draw_rect(draw_buffer_2d & buffer, const sprite_library & sprites, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const auto & sprite = sprites.sheet.get_sprite(0);
    buffer.emit_rect(r.x0, r.y0, r.x1, r.y1, sprite.s0, sprite.t0, sprite.s1, sprite.t1, top_color); //, bottom_color);
}
void draw_rect(draw_buffer_2d & buffer, const sprite_library & sprites, const rect & r, const float4 & color) { draw_rect(buffer, sprites, r, color, color); }

void draw_rounded_rect_top(draw_buffer_2d & buffer, const sprite_library & sprites, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const int radius = r.height();
    draw_rect(buffer, sprites, {r.x0+radius, r.y0, r.x1-radius, r.y0+radius}, top_color, bottom_color);
    auto it = sprites.corner_sprites.find(radius);
    if(it == end(sprites.corner_sprites)) return;
    auto & sprite = sprites.sheet.get_sprite(it->second);
    buffer.emit_rect(r.x0, r.y0, r.x0+radius, r.y0+radius, sprite.s1, sprite.t1, sprite.s0, sprite.t0, top_color); //, bottom_color);
    buffer.emit_rect(r.x1-radius, r.y0, r.x1, r.y0+radius, sprite.s0, sprite.t1, sprite.s1, sprite.t0, top_color); //, bottom_color);
}

void draw_rounded_rect_bottom(draw_buffer_2d & buffer, const sprite_library & sprites, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const int radius = r.height();
    draw_rect(buffer, sprites, {r.x0+radius, r.y1-radius, r.x1-radius, r.y1}, top_color, bottom_color);
    auto it = sprites.corner_sprites.find(radius);
    if(it == end(sprites.corner_sprites)) return;
    auto & sprite = sprites.sheet.get_sprite(it->second);
    buffer.emit_rect(r.x0, r.y1-radius, r.x0+radius, r.y1, sprite.s1, sprite.t0, sprite.s0, sprite.t1, top_color); //, bottom_color);
    buffer.emit_rect(r.x1-radius, r.y1-radius, r.x1, r.y1, sprite.s0, sprite.t0, sprite.s1, sprite.t1, top_color); //, bottom_color);
}

void draw_rounded_rect(draw_buffer_2d & buffer, const sprite_library & sprites, const rect & r, int radius, const float4 & top_color, const float4 & bottom_color)
{
    assert(radius >= 0);
    const float4 c1 = lerp(top_color, bottom_color, (float)radius/r.height());
    const float4 c2 = lerp(bottom_color, top_color, (float)radius/r.height());
    draw_rounded_rect_top(buffer, sprites, {r.x0, r.y0, r.x1, r.y0+radius}, top_color, c1);
    draw_rect(buffer, sprites, {r.x0, r.y0+radius, r.x1, r.y1-radius}, c1, c2);        
    draw_rounded_rect_bottom(buffer, sprites, {r.x0, r.y1-radius, r.x1, r.y1}, c2, bottom_color);
}
void draw_rounded_rect(draw_buffer_2d & buffer, const sprite_library & sprites, const rect & r, int radius, const float4 & color) { draw_rounded_rect(buffer, sprites, r, radius, color, color); }

void draw_circle(draw_buffer_2d & buffer, const sprite_library & sprites, const int2 & center, int radius, const float4 & color) { draw_rounded_rect(buffer, sprites, {center.x-radius, center.y-radius, center.x+radius, center.y+radius}, radius, color, color); }

void draw_text(draw_buffer_2d & buffer, const sprite_library & sprites, int2 p, utf8::string_view text, const float4 & color)
{
    for(auto codepoint : text)
    {
        if(auto * g = sprites.default_font.get_glyph(codepoint))
        {
            auto & s = sprites.sheet.get_sprite(g->sprite_index);
            const int2 p0 = p + g->offset, p1 = p0 + s.dims;
            buffer.emit_rect(p0.x, p0.y, p1.x, p1.y, s.s0, s.t0, s.s1, s.t1, color);
            p.x += g->advance;
        }
    }
}

void draw_shadowed_text(draw_buffer_2d & buffer, const sprite_library & sprites, int2 p, utf8::string_view text, const float4 & color)
{
    draw_text(buffer, sprites, p+1, text, {0,0,0,color.w});
    draw_text(buffer, sprites, p, text, color);
}

void draw_list(draw_buffer_2d & buffer, const sprite_library & sprites, int2 p, std::initializer_list<utf8::string_view> list)
{
    for(auto s : list)
    {
        draw_shadowed_text(buffer, sprites, p, s, {1,1,1,1});
        p.y += sprites.default_font.line_height;
    }
}

struct node_type
{
    static const int corner_radius = 10;
    static const int title_height = 25;

    std::string caption;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    int2 get_input_location(const rect & r, size_t index) const { return {r.x0, r.y0 + title_height + 18 + 24 * index}; }
    int2 get_output_location(const rect & r, size_t index) const { return {r.x1, r.y0 + title_height + 18 + 24 * index}; }

    void draw(draw_buffer_2d & buffer, const sprite_library & sprites, const rect & r) const
    {
        const float4 title_color = {0.5f,0.5f,0.5f,1}, node_color = {0.3f,0.3f,0.3f,1};
        draw_rounded_rect_top(buffer, sprites, {r.x0, r.y0, r.x1, r.y0+corner_radius}, title_color, title_color);
        draw_rect(buffer, sprites, {r.x0, r.y0+corner_radius, r.x1, r.y0+title_height}, title_color, title_color);        
        draw_rect(buffer, sprites, {r.x0, r.y0+title_height, r.x1, r.y1-corner_radius}, node_color, node_color);        
        draw_rounded_rect_bottom(buffer, sprites, {r.x0, r.y1-corner_radius, r.x1, r.y1}, node_color, node_color);
        draw_shadowed_text(buffer, sprites, {r.x0+8, r.y0+6}, caption, {1,1,1,1});

        for(size_t i=0; i<inputs.size(); ++i)
        {
            const auto loc = get_input_location(r,i);
            draw_circle(buffer, sprites, loc, 8, {1,1,1,1});
            draw_circle(buffer, sprites, loc, 6, {0,0,0,1});
            draw_shadowed_text(buffer, sprites, loc + int2(12, -sprites.default_font.line_height/2), inputs[i], {1,1,1,1});
        }
        for(size_t i=0; i<outputs.size(); ++i)
        {
            const auto loc = get_output_location(r,i);
            draw_circle(buffer, sprites, loc, 8, {1,1,1,1});
            draw_circle(buffer, sprites, loc, 6, {0,0,0,1});
            draw_shadowed_text(buffer, sprites, loc + int2(-12 - sprites.default_font.get_text_width(outputs[i]), -sprites.default_font.line_height/2), outputs[i], {1,1,1,1});
        }
    }
};

struct node
{
    const node_type * type;
    rect placement;

    int2 get_input_location(size_t index) const { return type->get_input_location(placement, index); }
    int2 get_output_location(size_t index) const { return type->get_output_location(placement, index); }

    void draw(draw_buffer_2d & buffer, const sprite_library & sprites) const
    {
        type->draw(buffer, sprites, placement);
    }
};

struct edge
{
    const node * output_node;
    int output_index;
    const node * input_node;
    int input_index;

    void draw(draw_buffer_2d & buffer, const sprite_library & sprites) const
    {
        const auto p0 = float2(output_node->get_output_location(output_index));
        const auto p3 = float2(input_node->get_input_location(input_index));
        const auto p1 = float2((p0.x+p3.x)/2, p0.y), p2 = float2((p0.x+p3.x)/2, p3.y);
        draw_circle(buffer, sprites, output_node->get_output_location(output_index), 7, {1,1,1,1});
        draw_circle(buffer, sprites, input_node->get_input_location(input_index), 7, {1,1,1,1});
        draw_bezier_curve(buffer, sprites, p0, p1, p2, p3, 3, {1,1,1,1});
    }
};

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
        {&nodes[0], 1, &nodes[1], 0}
    };

    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        int w, h;
        glfwGetWindowSize(win, &w, &h);

        buffer.begin_frame({w, h});
        for(auto & n : nodes) n.draw(buffer, sprites);
        for(auto & e : edges) e.draw(buffer, sprites);


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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
    glVertexPointer(2, GL_FLOAT, sizeof(draw_buffer_2d::vertex), &buffer.vertices.data()->position);
    glTexCoordPointer(2, GL_FLOAT, sizeof(draw_buffer_2d::vertex), &buffer.vertices.data()->texcoord);
    glColorPointer(4, GL_FLOAT, sizeof(draw_buffer_2d::vertex), &buffer.vertices.data()->color);
    glDrawElements(GL_TRIANGLES, buffer.indices.size(), GL_UNSIGNED_SHORT, buffer.indices.data());
    for(GLenum array : {GL_VERTEX_ARRAY, GL_TEXTURE_COORD_ARRAY, GL_COLOR_ARRAY}) glDisableClientState(array);
    glPopAttrib();
}