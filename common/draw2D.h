// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>
//
// draw2D.h - A simple drawing toolkit for 2D interfaces, capable of rectangles, circles, rounded rectangles, lines, bezier curves, text and icons.
//
// This module can be used to produce flat arrays of vertex, index, and texture data, which can be rendered via a single glDrawElements call in
// vanilla OpenGL 1.1. However, no actual graphics API calls are made by this module, the user is free to use whatever API suits them the most.

#ifndef DRAW_2D_H
#define DRAW_2D_H

#include "linalg.h"
using namespace linalg::aliases;

#include <memory>   // For std::shared_ptr<T>
#include <vector>   // For std::vector<T>
#include <array>    // For std::array<T,N>
#include <map>      // For std::map<K,V>

struct sprite
{
    std::shared_ptr<const uint8_t> pixels; int2 dims; // The bitmap of per-pixel alpha values
    float s0, t0, s1, t1;                             // The subrect of this sprite within the texture atlas
};

class sprite_sheet
{
    std::vector<sprite> sprites;
    std::vector<uint8_t> tex_pixels;
    int2 tex_dims;
public:
    sprite_sheet();

    const sprite & get_sprite(size_t index) const { return sprites[index]; }
    const void * get_texture_data() const { return tex_pixels.data(); }
    const int2 & get_texture_dims() const { return tex_dims; }

    size_t insert_sprite(sprite s);
    void prepare_texture();
};

sprite make_circle_quadrant(int radius);

namespace utf8
{
    const char * prev(const char * units); // Assumes units points just past the end of a valid utf-8 sequence of code units
    const char * next(const char * units); // Assumes units points to the start of a valid utf-8 sequence of code units
    uint32_t code(const char * units); // Assumes units points to the start of a valid utf-8 sequence of code units
    std::array<char,5> units(uint32_t code); // Assumes code < 0x110000
    bool is_valid(const char * units, size_t count); // Return true if the given sequence of code units is valid utf-8

    struct codepoint_iterator 
    { 
        const char * p;
        uint32_t operator * () const { return code(p); }
        codepoint_iterator & operator ++ () { p = next(p); return *this; }
        bool operator != (const codepoint_iterator & r) const { return p != r.p; }
    };

    struct string_view
    {
        const char * first, * last;
        string_view() : first(), last() {}
        string_view(const std::string & s) : first(s.data()), last(s.data() + s.size()) {}
        template<int N> string_view(const char (& s)[N]) : first(s), last(s + N) {}
        codepoint_iterator begin() const { return {first}; }
        codepoint_iterator end() const { return {last}; }
    };
}

struct glyph_data
{
    size_t sprite_index;
    int2 offset;
    int advance;
};

class font
{
    sprite_sheet * sprites;
    std::map<int, glyph_data> glyphs;
public:
    font() : sprites() {}
    font(sprite_sheet * sprites) : sprites(sprites) {}

    int line_height, baseline;

    const glyph_data * get_glyph(int codepoint) const;
    int get_text_width(utf8::string_view text) const;
    std::string::size_type get_cursor_pos(utf8::string_view text, int x) const;

    void load_glyphs(const std::string & path, int size, const std::vector<int> & codepoints);
};

struct sprite_library
{
    sprite_sheet sheet;
    font default_font;
    std::map<int, size_t> corner_sprites;
    std::map<int, size_t> line_sprites;

    sprite_library();
};

// The purpose of this class is to support a handful of basic 2D drawing operations (rectangles, circles, rounded rectangles, lines, bezier curves, and text)
// The resultant geometry is coalesced into a single vertex/index buffer which can be rendered via a single draw call.

struct transform_2d 
{ 
    float scale; float2 translate; 
    const float2 transform_point(const float2 & point) const { return point * scale + translate; }
    const float2 detransform_point(const float2 & point) const { return (point - translate) / scale; }

    friend transform_2d operator * (const transform_2d & a, const transform_2d & b) { return {a.scale*b.scale, a.transform_point(b.translate)}; }
    friend transform_2d & operator *= (transform_2d & a, const transform_2d & b) { return a = a * b; }

    static transform_2d translation(const float2 & offset) { return {1,offset}; }
    static transform_2d scaling(float factor) { return {factor,{0,0}}; }
    static transform_2d scaling(float factor, const float2 & center) { return translation(center) * scaling(factor) * translation(-center); }
};

class draw_buffer_2d
{
public:
    struct vertex { float2 position, texcoord; float4 color; };

    const sprite_library & get_library() const { return *library; }
    const std::vector<vertex> & get_vertices() const { return vertices; }
    const std::vector<uint16_t> & get_indices() const { return out_indices; }
    const rect & get_scissor_rect() const { return scissor.back(); }

    const float transform_length(float length) const { return length * transforms.back().scale; }
    const float detransform_length(float length) const { return length / transforms.back().scale; }
    const float2 transform_point(const float2 & point) const { return transforms.back().transform_point(point); }
    const float2 detransform_point(const float2 & point) const { return transforms.back().detransform_point(point); }

    void begin_frame(const sprite_library & library, const int2 & window_size);
    void end_frame();
    void begin_overlay();
    void end_overlay();
    void begin_transform(const transform_2d & t) { transforms.push_back(transforms.back() * t); }
    void end_transform() { transforms.pop_back(); }
    void begin_scissor(const rect & r);
    void end_scissor();

    void draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color);
    void draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color);

    void draw_rect(const rect & r, const float4 & color);
    void draw_partial_rounded_rect(rect r, int radius, const float4 & color, bool tl, bool tr, bool bl, bool br);
    void draw_rounded_rect(const rect & r, int radius, const float4 & color);
    void draw_circle(const int2 & center, int radius, const float4 & color);

    void draw_sprite(const rect & r, float s0, float t0, float s1, float t1, const float4 & color);
    void draw_text(int2 p, utf8::string_view text, const float4 & color);
    void draw_shadowed_text(int2 p, utf8::string_view text, const float4 & color);
private:
    void draw_quad(const vertex & v0, const vertex & v1, const vertex & v2, const vertex & v3);

    struct list { size_t level,first,last; };
    const sprite_library * library;
    std::vector<vertex> vertices;
    std::vector<uint16_t> indices, out_indices;
    std::vector<list> lists;
    std::vector<rect> scissor;
    std::vector<transform_2d> transforms;
    float2 a, b;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following reference functions can be used to render the output of the sprite_sheet and draw_buffer_2d classes //
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
#include <gl/GL.h>
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
#endif

#endif