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
struct draw_buffer_2d
{
    struct vertex { float2 position, texcoord; float4 color; };
    struct list { size_t level,first,last; };

    const sprite_library * library;
    std::vector<vertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<list> lists;
    float sx,sy;
    rect scissor;

    void begin_frame(const sprite_library & library, const int2 & window_size);

    void emit_polygon(const vertex * verts, size_t vert_count);

    void emit_rect(int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, const float4 & color);
};

void draw_line(draw_buffer_2d & buffer, const float2 & p0, const float2 & p1, float width, const float4 & color);
void draw_bezier_curve(draw_buffer_2d & buffer, const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color);
void draw_rect(draw_buffer_2d & buffer, const rect & r, const float4 & top_color, const float4 & bottom_color);
void draw_rect(draw_buffer_2d & buffer, const rect & r, const float4 & color);
void draw_rounded_rect_top(draw_buffer_2d & buffer, const rect & r, const float4 & top_color, const float4 & bottom_color);
void draw_rounded_rect_bottom(draw_buffer_2d & buffer, const rect & r, const float4 & top_color, const float4 & bottom_color);
void draw_rounded_rect(draw_buffer_2d & buffer, const rect & r, int radius, const float4 & top_color, const float4 & bottom_color);
void draw_rounded_rect(draw_buffer_2d & buffer, const rect & r, int radius, const float4 & color);
void draw_circle(draw_buffer_2d & buffer, const int2 & center, int radius, const float4 & color);
void draw_text(draw_buffer_2d & buffer, int2 p, utf8::string_view text, const float4 & color);
void draw_shadowed_text(draw_buffer_2d & buffer, int2 p, utf8::string_view text, const float4 & color);

#endif