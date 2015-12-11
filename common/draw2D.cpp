#include "draw2D.h"
#include <cassert>

sprite_library::sprite_library() : default_font(&sheet)
{
    std::vector<int> codepoints;
    for(int i=32; i<256; ++i) codepoints.push_back(i);
    default_font.load_glyphs("c:/windows/fonts/arialbd.ttf", 14, codepoints);
    for(int i=1; i<=32; ++i) corner_sprites[i] = sheet.insert_sprite(make_circle_quadrant(i));
    for(int i=1; i<=8; ++i)
    {
        auto pixels = reinterpret_cast<uint8_t *>(std::malloc(i+2));
        if(!pixels) throw std::bad_alloc();
        sprite s = {std::shared_ptr<uint8_t>(pixels, std::free), {i+2,1}};
        memset(pixels+1, 255, i);
        pixels[0] = pixels[i+1] = 0;
        line_sprites[i] = sheet.insert_sprite(s);
    }
    sheet.prepare_texture();
}

void draw_buffer_2d::begin_frame(const sprite_library & library, const int2 & window_size)
{
    this->library = &library;
    vertices.clear();
    indices.clear();
    lists.clear();
    scissor = {0, 0, window_size.x, window_size.y};
    sx = 2.0f / window_size.x;
    sy = 2.0f / window_size.y;
}

static size_t clip_polygon(draw_buffer_2d::vertex (& out)[8], const draw_buffer_2d::vertex in[], size_t in_size, const float3 & plane)
{
    size_t out_size = 0;
    for(size_t i=0; i<in_size; ++i)
    {
        const auto & v0 = in[i], & v1 = in[(i+1)%in_size];
        float t0 = dot(float3(v0.position,1), plane);
        if(t0 >= 0)
        {
            assert(out_size < 8);
            out[out_size++] = v0;
        }
        float t1 = dot(float3(v1.position,1), plane);
        if(t0 * t1 < 0)
        {
            assert(out_size < 8);
            const float t = -t0/(t1-t0);
            out[out_size++] = {lerp(v0.position, v1.position, t), lerp(v0.texcoord, v1.texcoord, t), lerp(v0.color, v1.color, t)};
        }
    }
    return out_size;
}

void draw_buffer_2d::emit_polygon(const vertex * verts, size_t vert_count)
{
    assert(vert_count <= 4);
    vertex verts_a[8], verts_b[8];
    vert_count = clip_polygon(verts_a, verts,   vert_count, float3(+1,  0, -scissor.x0));
    vert_count = clip_polygon(verts_b, verts_a, vert_count, float3( 0, +1, -scissor.y0));
    vert_count = clip_polygon(verts_a, verts_b, vert_count, float3(-1,  0, +scissor.x1));
    vert_count = clip_polygon(verts_b, verts_a, vert_count, float3( 0, -1, +scissor.y1));
    if(vertices.size() + vert_count > 0x10000) throw std::runtime_error("draw buffer overflow");
    const uint16_t base = vertices.size();
    for(uint16_t i=2; i<vert_count; ++i)
    {
        indices.push_back(base);
        indices.push_back(base+i-1);
        indices.push_back(base+i);
    }
    for(auto it = verts_b, end = verts_b + vert_count; it != end; ++it)
    {
        vertices.push_back({{it->position.x*sx-1, 1-it->position.y*sy}, it->texcoord, it->color});
    }
}

void draw_buffer_2d::emit_rect(int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, const float4 & color)
{
    const vertex verts[] = {
        {{(float)x0,(float)y0}, {s0,t0}, color},
        {{(float)x1,(float)y0}, {s1,t0}, color},
        {{(float)x1,(float)y1}, {s1,t1}, color},
        {{(float)x0,(float)y1}, {s0,t1}, color}
    };
    emit_polygon(verts, 4);
}

void draw_line(draw_buffer_2d & buffer, const float2 & p0, const float2 & p1, float width, const float4 & color)
{
    auto it = buffer.library->line_sprites.find(width);
    if(it == end(buffer.library->line_sprites)) return;
    const auto & sprite = buffer.library->sheet.get_sprite(it->second);
    const float2 perp = normalize(cross(float3(p1-p0,0), float3(0,0,1)).xy()) * ((width+2) * 0.5f);
    const draw_buffer_2d::vertex verts[] = {
        {p0+perp, {sprite.s0, (sprite.t0+sprite.t1)/2}, color},
        {p1+perp, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
        {p1-perp, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
        {p0-perp, {sprite.s0, (sprite.t0+sprite.t1)/2}, color}
    };
    buffer.emit_polygon(verts, 4);
}

void draw_bezier_curve(draw_buffer_2d & buffer, const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color)
{
    auto it = buffer.library->line_sprites.find(width);
    if(it == end(buffer.library->line_sprites)) return;
    const auto & sprite = buffer.library->sheet.get_sprite(it->second);
    const float2 d01 = p1-p0, d12 = p2-p1, d23 = p3-p2;
    float2 v0, v1;
    for(float i=0; i<=32; ++i)
    {
        float t = i/32, s = (1-t);
        const float2 p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
        const float2 d = normalize(d01*(3*s*s) + d12*(6*s*t) + d23*(3*t*t)) * ((width+2) * 0.5f);
        const float2 v2 = {p.x-d.y, p.y+d.x}, v3 = {p.x+d.y, p.y-d.x};
        if(i)
        {
            const draw_buffer_2d::vertex verts[] = {
                {v0, {sprite.s0, (sprite.t0+sprite.t1)/2}, color},
                {v1, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
                {v2, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
                {v3, {sprite.s0, (sprite.t0+sprite.t1)/2}, color}
            };
            buffer.emit_polygon(verts, 4);
        }
        v0 = v3;
        v1 = v2;
    }
}

void draw_rect(draw_buffer_2d & buffer, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const auto & sprite = buffer.library->sheet.get_sprite(0);
    float s = (sprite.s0+sprite.s1)/2, t = (sprite.t0+sprite.t1)/2;
    buffer.emit_rect(r.x0, r.y0, r.x1, r.y1, s, t, s, t, top_color); //, bottom_color);
}
void draw_rect(draw_buffer_2d & buffer, const rect & r, const float4 & color) { draw_rect(buffer, r, color, color); }

void draw_rounded_rect_top(draw_buffer_2d & buffer, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const int radius = r.height();
    draw_rect(buffer, {r.x0+radius, r.y0, r.x1-radius, r.y0+radius}, top_color, bottom_color);
    auto it = buffer.library->corner_sprites.find(radius);
    if(it == end(buffer.library->corner_sprites)) return;
    auto & sprite = buffer.library->sheet.get_sprite(it->second);
    buffer.emit_rect(r.x0, r.y0, r.x0+radius, r.y0+radius, sprite.s1, sprite.t1, sprite.s0, sprite.t0, top_color); //, bottom_color);
    buffer.emit_rect(r.x1-radius, r.y0, r.x1, r.y0+radius, sprite.s0, sprite.t1, sprite.s1, sprite.t0, top_color); //, bottom_color);
}

void draw_rounded_rect_bottom(draw_buffer_2d & buffer, const rect & r, const float4 & top_color, const float4 & bottom_color)
{
    const int radius = r.height();
    draw_rect(buffer, {r.x0+radius, r.y1-radius, r.x1-radius, r.y1}, top_color, bottom_color);
    auto it = buffer.library->corner_sprites.find(radius);
    if(it == end(buffer.library->corner_sprites)) return;
    auto & sprite = buffer.library->sheet.get_sprite(it->second);
    buffer.emit_rect(r.x0, r.y1-radius, r.x0+radius, r.y1, sprite.s1, sprite.t0, sprite.s0, sprite.t1, top_color); //, bottom_color);
    buffer.emit_rect(r.x1-radius, r.y1-radius, r.x1, r.y1, sprite.s0, sprite.t0, sprite.s1, sprite.t1, top_color); //, bottom_color);
}

void draw_rounded_rect(draw_buffer_2d & buffer, const rect & r, int radius, const float4 & top_color, const float4 & bottom_color)
{
    assert(radius >= 0);
    const float4 c1 = lerp(top_color, bottom_color, (float)radius/r.height());
    const float4 c2 = lerp(bottom_color, top_color, (float)radius/r.height());
    draw_rounded_rect_top(buffer, {r.x0, r.y0, r.x1, r.y0+radius}, top_color, c1);
    draw_rect(buffer, {r.x0, r.y0+radius, r.x1, r.y1-radius}, c1, c2);        
    draw_rounded_rect_bottom(buffer, {r.x0, r.y1-radius, r.x1, r.y1}, c2, bottom_color);
}
void draw_rounded_rect(draw_buffer_2d & buffer, const rect & r, int radius, const float4 & color) { draw_rounded_rect(buffer, r, radius, color, color); }

void draw_circle(draw_buffer_2d & buffer, const int2 & center, int radius, const float4 & color) { draw_rounded_rect(buffer, {center.x-radius, center.y-radius, center.x+radius, center.y+radius}, radius, color, color); }

void draw_text(draw_buffer_2d & buffer, int2 p, utf8::string_view text, const float4 & color)
{
    for(auto codepoint : text)
    {
        if(auto * g = buffer.library->default_font.get_glyph(codepoint))
        {
            auto & s = buffer.library->sheet.get_sprite(g->sprite_index);
            const int2 p0 = p + g->offset, p1 = p0 + s.dims;
            buffer.emit_rect(p0.x, p0.y, p1.x, p1.y, s.s0, s.t0, s.s1, s.t1, color);
            p.x += g->advance;
        }
    }
}

void draw_shadowed_text(draw_buffer_2d & buffer, int2 p, utf8::string_view text, const float4 & color)
{
    draw_text(buffer, p+1, text, {0,0,0,color.w});
    draw_text(buffer, p, text, color);
}
