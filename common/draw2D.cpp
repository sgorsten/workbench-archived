// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw2D.h"

#include <cassert>  // For assert(...)
#include <fstream>  // For std::ifstream

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

sprite_sheet::sprite_sheet()
{
    // Sprite index 0 will always be a single solid pixel, suitable for doing solid color fills
    sprites.push_back({std::make_shared<uint8_t>(255), {1,1}});
}

size_t sprite_sheet::insert_sprite(sprite s)
{
    const size_t index = sprites.size();
    sprites.push_back(s);
    tex_pixels.clear();
    tex_dims = {0,0};
    return index;
}

void sprite_sheet::prepare_texture()
{
    // Sort glyphs by descending height, then descending width
    std::vector<sprite *> glyph_list;
    for(auto & g : sprites) glyph_list.push_back(&g);
    std::sort(begin(glyph_list), end(glyph_list), [](const sprite * a, const sprite * b)
    {
        return std::make_tuple(a->dims.y, a->dims.x) > std::make_tuple(b->dims.y, b->dims.x);
    });

    const int border = 1;
    tex_dims = {64, 64};
    while(true)
    {
        tex_pixels.resize(tex_dims.x * tex_dims.y);
        std::fill(begin(tex_pixels), end(tex_pixels), 0);

        int2 used = {border, border};
        int next_line = 0;
        bool bad_pack = false;
        for(auto * g : glyph_list)
        {
            if(used.x + g->dims.x > tex_dims.x)
            {
                used.x = border;
                used.y = next_line + border;
            }
            if(used.x + g->dims.x > tex_dims.x || used.y + g->dims.y > tex_dims.y)
            {
                bad_pack = true;
                break;
            }
            for(int y=0; y<g->dims.y; ++y)
            {
                memcpy(tex_pixels.data() + (used.y+y)*tex_dims.x + used.x, g->pixels.get() + y*g->dims.x, g->dims.x);
            }
            g->s0 = static_cast<float>(used.x + g->border) / tex_dims.x;
            g->t0 = static_cast<float>(used.y + g->border) / tex_dims.y;
            g->s1 = static_cast<float>(used.x + g->dims.x - g->border) / tex_dims.x;
            g->t1 = static_cast<float>(used.y + g->dims.y - g->border) / tex_dims.y;
            used.x += g->dims.x + border;
            next_line = std::max(next_line, used.y + g->dims.y);
        }
        if(bad_pack)
        {
            if(tex_dims.x == tex_dims.y) tex_dims.x *= 2;
            else tex_dims.y *= 2;
        }
        else break;
    }
}

static void compute_circle_quadrant_coverage(float coverage[], int radius)
{
    const float rr = static_cast<float>(radius * radius);
    auto function = [rr](float x) { return sqrt(rr - x*x); };
    auto antiderivative = [rr, function](float x) { return (x * function(x) + rr * atan(x/function(x))) / 2; };
    auto integral = [antiderivative](float x0, float x1) { return antiderivative(x1) - antiderivative(x0); };

    for(int i=0; i<radius; ++i)
    {
        const float x0 = i+0.0f, x1 = i+1.0f;
        const float y0 = function(x0);
        const float y1 = function(x1);
        const int y0i = (int)y0, y1i = (int)y1;

        for(int j=i; j<y1i; ++j)
        {
            coverage[i*radius+j] = coverage[j*radius+i] = 1.0f;
        }

        if(y0i == y1i)
        {
            float c = integral(x0, x1) - y1i*(x1-x0);
            coverage[i*radius+y1i] = c;
            coverage[y1i*radius+i] = c;
        }
        else
        {
            const float cross_x = function(static_cast<float>(y0i)); // X location where curve passes from pixel y0i to pixel y1i

            // Coverage for pixel at (i,y0i) is the area under the curve from x0 to cross_x
            if(y0i < radius) coverage[i*radius+y0i] = coverage[y0i*radius+i] = integral(x0, cross_x) - y0i*(cross_x-x0);

            // Coverage for pixel at (i,y1i) is the area of a rectangle from x0 to cross_x, and the area under the curve from cross_x to x1
            if(y1i == y0i - 1) coverage[i*radius+y1i] = coverage[y1i*radius+i] = (cross_x-x0) + integral(cross_x, x1) - y1i*(x1-cross_x);
            else break; // Stop after the first octant
        }
    }
}

sprite make_circle_quadrant(int radius)
{
    std::vector<float> coverage(radius*radius);
    compute_circle_quadrant_coverage(coverage.data(), radius);

    const int width = radius+2;
    auto memory = reinterpret_cast<uint8_t *>(std::malloc(width*width));
    if(!memory) throw std::bad_alloc();
    std::shared_ptr<uint8_t> pixels(memory, std::free);
    auto out = pixels.get(); auto in = coverage.data();
    *out++ = 255;
    for(int i=0; i<radius; ++i) *out++ = 255;
    *out++ = 0;
    for(int i=0; i<radius; ++i)
    {
        *out++ = 255;
        for(int i=0; i<radius; ++i) *out++ = static_cast<uint8_t>(*in++ * 255);
        *out++ = 0;
    }
    for(int i=0; i<width; ++i) *out++ = 0;

    return {pixels, {width,width}, true};
}

namespace utf8
{
    int get_code_length(uint8_t byte)
    { 
        if(byte < 0x80) return 1;
        if(byte < 0xC0) return 0;
        if(byte < 0xE0) return 2;
        if(byte < 0xF0) return 3;
        if(byte < 0xF8) return 4;
        return 0;
    }

    bool is_continuation_byte(uint8_t byte)
    { 
        return byte >= 0x80 && byte < 0xC0; 
    }

    const char * prev(const char * units)
    {
        do { --units; } while(is_continuation_byte(*units));
        return units;
    }

    const char * next(const char * units)
    {
        return units + get_code_length(*units);
    }

    uint32_t code(const char * units)
    {
        static const uint8_t masks[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
        auto length = get_code_length(*units);
        uint32_t codepoint = units[0] & masks[length];
        for(int i=1; i<length; ++i)
        {
            codepoint = (codepoint << 6) | (units[i] & 0x3F);
        }
        return codepoint;
    }

    std::array<char,5> units(uint32_t code)
    {
        if(code < 0x80) return {{code}};
        if(code < 0x800) return {{0xC0|((code>>6)&0x1F), 0x80|(code&0x3F)}};
        if(code < 0x10000) return {{0xE0|((code>>12)&0x0F), 0x80|((code>>6)&0x3F), 0x80|(code&0x3F)}};
        return {{0xF0|((code>>18)&0x07), 0x80|((code>>12)&0x3F), 0x80|((code>>6)&0x3F), 0x80|(code&0x3F)}};
    }

    bool is_valid(const char * units, size_t count)
    {
        auto end = units + count;
        while(units != end)
        {
            auto length = get_code_length(*units++);
            if(length == 0) return false;
            for(int i=1; i<length; ++i)
            {
                if(units == end) return false;
                if(!is_continuation_byte(*units++)) return false;
            }
        }
        return true;
    }
}

const glyph_data * font::get_glyph(int codepoint) const
{
    auto it = glyphs.find(codepoint);
    if(it == end(glyphs)) return nullptr;
    return &it->second;
}

int font::get_text_width(utf8::string_view text) const
{         
    int width = 0;
    for(auto codepoint : text)
    {
        auto g = glyphs.find(codepoint);
        if(g == end(glyphs)) continue;
        width += g->second.advance;
    }
    return width;
}

std::string::size_type font::get_cursor_pos(utf8::string_view text, int x) const
{
    for(auto it = text.begin(); it != text.end(); ++it)
    {
        auto g = glyphs.find(*it);
        if(g == end(glyphs)) continue;
        if(x*2 < g->second.advance) return it.p - text.first;
        x -= g->second.advance;
    }
    return text.last - text.first;
}

void font::load_glyphs(const std::string & path, int size, const std::vector<int> & codepoints)
{
    std::ifstream in(path, std::ifstream::binary);
    if(!in) throw std::runtime_error("failed to open file " + path);
    in.seekg(0, std::ifstream::end);
    std::vector<uint8_t> buffer(in.tellg());
    in.seekg(0, std::ifstream::beg);
    in.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
    in.close();

    stbtt_fontinfo info;
    stbtt_InitFont(&info, buffer.data(), stbtt_GetFontOffsetForIndex(buffer.data(), 0));
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    float scale = stbtt_ScaleForPixelHeight(&info, static_cast<float>(size));

    line_height = static_cast<int>(std::round((ascent - descent + line_gap) * scale));
    baseline = static_cast<int>(std::round(ascent * scale));

    for(auto codepoint : codepoints)
    {
        glyph_data & g = glyphs[codepoint];

        sprite s = {};
        s.pixels = std::shared_ptr<uint8_t>(stbtt_GetCodepointBitmap(&info, 0, scale, codepoint, &s.dims.x, &s.dims.y, &g.offset.x, &g.offset.y), [](uint8_t * p) { stbtt_FreeBitmap(p, 0); });
        g.sprite_index = sprites->insert_sprite(s);

        int advance;
        stbtt_GetCodepointHMetrics(&info, codepoint, &advance, nullptr);
        g.offset.y += baseline;
        g.advance = static_cast<int>(std::floor(advance * scale));
    }
}

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
    lists = {{0, 0}};
    scissor = {{0, 0, window_size.x, window_size.y}};
    transforms = {{1}};
    a = {2.0f / window_size.x, -2.0f / window_size.y};
    b = {-1, +1};
}

void draw_buffer_2d::end_frame()
{
    lists.back().last = indices.size();
    std::stable_sort(begin(lists), end(lists), [](const list & a, const list & b) { return a.level < b.level; });

    out_indices.clear();
    out_indices.reserve(indices.size());
    for(auto & list : lists) out_indices.insert(end(out_indices), indices.data() + list.first, indices.data() + list.last);
}

void draw_buffer_2d::begin_overlay()
{
    lists.back().last = indices.size();
    lists.push_back({lists.back().level + 1, indices.size()});
    scissor.push_back(scissor.front()); // Overlays are not constrained by parent scissor rect
}

void draw_buffer_2d::end_overlay()
{
    scissor.pop_back();
    lists.back().last = indices.size();
    lists.push_back({lists.back().level - 1, indices.size()});
}

void draw_buffer_2d::begin_scissor(const rect & r)
{
    const auto & t = transforms.back();
    const auto p0 = int2(round(t.transform_point(float2(r.x0, r.y0))));
    const auto p1 = int2(round(t.transform_point(float2(r.x1, r.y1))));

    const auto & s = scissor.back();
    scissor.push_back({std::max(s.x0, p0.x), std::max(s.y0, p0.y), std::min(s.x1, p1.x), std::min(s.y1, p1.y)});
}

void draw_buffer_2d::end_scissor()
{
    scissor.pop_back();
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

void draw_buffer_2d::draw_quad(const vertex & v0, const vertex & v1, const vertex & v2, const vertex & v3)
{
    vertex a[8] = {v0, v1, v2, v3}, b[8]; size_t n = 4;
    for(size_t i=0; i<n; ++i) a[i].position = transform_point(a[i].position);
    n = clip_polygon(b, a, n, float3(+1,  0, static_cast<float>(-scissor.back().x0)));
    n = clip_polygon(a, b, n, float3( 0, +1, static_cast<float>(-scissor.back().y0)));
    n = clip_polygon(b, a, n, float3(-1,  0, static_cast<float>(+scissor.back().x1)));
    n = clip_polygon(a, b, n, float3( 0, -1, static_cast<float>(+scissor.back().y1)));
    if(vertices.size() + n > 0x10000) throw std::runtime_error("draw buffer overflow");
    const uint16_t base = static_cast<uint16_t>(vertices.size());
    for(uint16_t i=2; i<n; ++i)
    {
        indices.push_back(base);
        indices.push_back(base+i-1);
        indices.push_back(base+i);
    }
    for(auto it = a, end = a + n; it != end; ++it) vertices.push_back({it->position * this->a + this->b, it->texcoord, it->color});
}

void draw_buffer_2d::draw_sprite(const rect & r, float s0, float t0, float s1, float t1, const float4 & color)
{
    draw_quad({float2(r.x0, r.y0), {s0,t0}, color},
              {float2(r.x1, r.y0), {s1,t0}, color},
              {float2(r.x1, r.y1), {s1,t1}, color},
              {float2(r.x0, r.y1), {s0,t1}, color});
}

void draw_buffer_2d::draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color)
{
    int adjusted_width = std::min(std::max(static_cast<int>(std::round(transform_length(width))), 1), 8);
    auto it = library->line_sprites.find(adjusted_width);
    if(it == end(library->line_sprites)) return;
    const auto & sprite = library->sheet.get_sprite(it->second);
    const float2 perp = normalize(cross(float3(p1-p0,0), float3(0,0,1)).xy()) * (width*0.5f + detransform_length(1));
    draw_quad({p0+perp, {sprite.s0, (sprite.t0+sprite.t1)/2}, color},
              {p0-perp, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
              {p1-perp, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
              {p1+perp, {sprite.s0, (sprite.t0+sprite.t1)/2}, color});
}

void draw_buffer_2d::draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color)
{
    int adjusted_width = std::min(std::max(static_cast<int>(std::round(transform_length(width))), 1), 8);
    auto it = library->line_sprites.find(adjusted_width);
    if(it == end(library->line_sprites)) return;
    const auto & sprite = library->sheet.get_sprite(it->second);
    const float2 d01 = p1-p0, d12 = p2-p1, d23 = p3-p2;
    float2 v0, v1;
    for(float i=0; i<=32; ++i)
    {
        float t = i/32, s = (1-t);
        const float2 p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
        const float2 d = normalize(d01*(3*s*s) + d12*(6*s*t) + d23*(3*t*t)) * (width*0.5f + detransform_length(1));
        const float2 v2 = {p.x-d.y, p.y+d.x}, v3 = {p.x+d.y, p.y-d.x};
        if(i) draw_quad({v0, {sprite.s0, (sprite.t0+sprite.t1)/2}, color},
                        {v1, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
                        {v2, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
                        {v3, {sprite.s0, (sprite.t0+sprite.t1)/2}, color});
        v0 = v3;
        v1 = v2;
    }
}

void draw_buffer_2d::draw_rect(const rect & r, const float4 & color)
{
    const auto & sprite = library->sheet.get_sprite(0);
    float s = (sprite.s0+sprite.s1)/2, t = (sprite.t0+sprite.t1)/2;
    draw_sprite({r.x0, r.y0, r.x1, r.y1}, s, t, s, t, color);
}

static rect take_x0(rect & r, int x) { rect r2 = {r.x0, r.y0, r.x0+x, r.y1}; r.x0 = r2.x1; return r2; }
static rect take_x1(rect & r, int x) { rect r2 = {r.x1-x, r.y0, r.x1, r.y1}; r.x1 = r2.x0; return r2; }
static rect take_y0(rect & r, int y) { rect r2 = {r.x0, r.y0, r.x1, r.y0+y}; r.y0 = r2.y1; return r2; }
static rect take_y1(rect & r, int y) { rect r2 = {r.x0, r.y1-y, r.x1, r.y1}; r.y1 = r2.y0; return r2; }
void draw_buffer_2d::draw_partial_rounded_rect(rect r, int radius, const float4 & color, bool tl, bool tr, bool bl, bool br)
{
    int adjusted_radius = std::min(std::max(static_cast<int>(std::ceilf(radius * transforms.back().scale)), 1), 32);
    auto it = library->corner_sprites.find(adjusted_radius);
    if(it == end(library->corner_sprites)) return;
    auto & sprite = library->sheet.get_sprite(it->second);
    
    if(tl || tr)
    {
        rect r2 = take_y0(r, radius);
        if(tl) draw_sprite(take_x0(r2, radius), sprite.s1, sprite.t1, sprite.s0, sprite.t0, color);    
        if(tr) draw_sprite(take_x1(r2, radius), sprite.s0, sprite.t1, sprite.s1, sprite.t0, color);
        draw_rect(r2, color);
    }

    if(bl || br)
    {
        rect r2 = take_y1(r, radius);
        if(bl) draw_sprite(take_x0(r2, radius), sprite.s1, sprite.t0, sprite.s0, sprite.t1, color);
        if(br) draw_sprite(take_x1(r2, radius), sprite.s0, sprite.t0, sprite.s1, sprite.t1, color);
        draw_rect(r2, color);
    }

    draw_rect(r, color);
}

void draw_buffer_2d::draw_rounded_rect(const rect & r, int radius, const float4 & color)
{
    draw_partial_rounded_rect(r, radius, color, true, true, true, true);
}

void draw_buffer_2d::draw_circle(const int2 & center, int radius, const float4 & color) 
{ 
    draw_rounded_rect({center.x-radius, center.y-radius, center.x+radius, center.y+radius}, radius, color);
}

void draw_buffer_2d::draw_text(int2 p, utf8::string_view text, const float4 & color)
{
    for(auto codepoint : text)
    {
        if(auto * g = library->default_font.get_glyph(codepoint))
        {
            auto & s = library->sheet.get_sprite(g->sprite_index);
            const int2 p0 = p + g->offset, p1 = p0 + s.dims;
            draw_sprite({p0.x, p0.y, p1.x, p1.y}, s.s0, s.t0, s.s1, s.t1, color);
            p.x += g->advance;
        }
    }
}

void draw_buffer_2d::draw_shadowed_text(int2 p, utf8::string_view text, const float4 & color)
{
    draw_text(p+1, text, {0,0,0,color.w});
    draw_text(p, text, color);
}
