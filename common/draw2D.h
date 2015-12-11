// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef DRAW_2D_H
#define DRAW_2D_H

#include "font.h"

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
class draw_buffer_2d
{
public:
    struct vertex { float2 position, texcoord; float4 color; };

    const sprite_library & get_library() const { return *library; }
    const std::vector<vertex> & get_vertices() const { return vertices; }
    const std::vector<uint16_t> & get_indices() const { return out_indices; }
    const rect & get_scissor_rect() const { return scissor.back(); }

    void begin_frame(const sprite_library & library, const int2 & window_size);
    void end_frame();
    void begin_overlay();
    void end_overlay();
    void begin_scissor(const rect & r);
    void end_scissor();

    void draw_quad(const vertex & v0, const vertex & v1, const vertex & v2, const vertex & v3);

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
    struct list { size_t level,first,last; };
    const sprite_library * library;
    std::vector<vertex> vertices;
    std::vector<uint16_t> indices, out_indices;
    std::vector<list> lists;
    std::vector<rect> scissor;
    float2 scale, translate;
};

#endif