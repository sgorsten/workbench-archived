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
    lists = {{0, 0}};
    scissor = {{0, 0, window_size.x, window_size.y}};
    scale = {2.0f / window_size.x, -2.0f / window_size.y};
    translate = {-1, +1};
}

void draw_buffer_2d::end_frame()
{
    lists.back().last = indices.size();
    std::stable_sort(begin(lists), end(lists), [](const list & a, const list & b) { return a.level < b.level; });
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
    const auto & s = scissor.back();
    scissor.push_back({std::max(s.x0, r.x0), std::max(s.y0, r.y0), std::min(s.x1, r.x1), std::min(s.y1, r.y1)});
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
    vertex a[8] = {v0, v1, v2, v3}, b[8];
    size_t n = clip_polygon(b, a, 4, float3(+1,  0, -scissor.back().x0));
    n = clip_polygon(a, b, n, float3( 0, +1, -scissor.back().y0));
    n = clip_polygon(b, a, n, float3(-1,  0, +scissor.back().x1));
    n = clip_polygon(a, b, n, float3( 0, -1, +scissor.back().y1));
    if(vertices.size() + n > 0x10000) throw std::runtime_error("draw buffer overflow");
    const uint16_t base = vertices.size();
    for(uint16_t i=2; i<n; ++i)
    {
        indices.push_back(base);
        indices.push_back(base+i-1);
        indices.push_back(base+i);
    }
    for(auto it = a, end = a + n; it != end; ++it) vertices.push_back({it->position * scale + translate, it->texcoord, it->color});
}

void draw_buffer_2d::draw_sprite(const rect & r, float s0, float t0, float s1, float t1, const float4 & color)
{
    draw_quad({{static_cast<float>(r.x0), static_cast<float>(r.y0)}, {s0,t0}, color},
              {{static_cast<float>(r.x1), static_cast<float>(r.y0)}, {s1,t0}, color},
              {{static_cast<float>(r.x1), static_cast<float>(r.y1)}, {s1,t1}, color},
              {{static_cast<float>(r.x0), static_cast<float>(r.y1)}, {s0,t1}, color});
}

void draw_buffer_2d::draw_line(const float2 & p0, const float2 & p1, int width, const float4 & color)
{
    auto it = library->line_sprites.find(width);
    if(it == end(library->line_sprites)) return;
    const auto & sprite = library->sheet.get_sprite(it->second);
    const float2 perp = normalize(cross(float3(p1-p0,0), float3(0,0,1)).xy()) * ((width+2) * 0.5f);
    draw_quad({p0+perp, {sprite.s0, (sprite.t0+sprite.t1)/2}, color},
        {p0-perp, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
        {p1-perp, {sprite.s1, (sprite.t0+sprite.t1)/2}, color},
        {p1+perp, {sprite.s0, (sprite.t0+sprite.t1)/2}, color});
}

void draw_buffer_2d::draw_bezier_curve(const float2 & p0, const float2 & p1, const float2 & p2, const float2 & p3, int width, const float4 & color)
{
    auto it = library->line_sprites.find(width);
    if(it == end(library->line_sprites)) return;
    const auto & sprite = library->sheet.get_sprite(it->second);
    const float2 d01 = p1-p0, d12 = p2-p1, d23 = p3-p2;
    float2 v0, v1;
    for(float i=0; i<=32; ++i)
    {
        float t = i/32, s = (1-t);
        const float2 p = p0*(s*s*s) + p1*(3*s*s*t) + p2*(3*s*t*t) + p3*(t*t*t);
        const float2 d = normalize(d01*(3*s*s) + d12*(6*s*t) + d23*(3*t*t)) * ((width+2) * 0.5f);
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
    auto it = library->corner_sprites.find(radius);
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
