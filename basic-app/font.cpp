#include "font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "thirdparty/stb_truetype.h"

#include <fstream>
#include <algorithm>

const glyph_data * font::get_glyph(int codepoint) const
{
    auto it = glyphs.find(codepoint);
    if(it == end(glyphs)) return nullptr;
    return &it->second;
}

int font::get_text_width(const std::string & text) const
{         
    int width = 0;
    for(auto ch : text)
    {
        auto it = glyphs.find(ch);
        if(it == end(glyphs)) continue;
        width += it->second.advance;
    }
    return width;
}

std::string::size_type font::get_cursor_pos(const std::string & text, int x) const
{
    for(std::string::size_type i=0; i<text.size(); ++i)
    {
        auto it = glyphs.find(text[i]);
        if(it == end(glyphs)) continue;
        if(x*2 < it->second.advance) return i;
        x -= it->second.advance;
    }
    return text.size();
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
    float scale = stbtt_ScaleForPixelHeight(&info, size);

    line_height = static_cast<int>(std::round((ascent - descent + line_gap) * scale));
    baseline = static_cast<int>(std::round(ascent * scale));

    for(auto codepoint : codepoints)
    {
        auto & data = glyphs[codepoint];
        data.pixels = std::shared_ptr<uint8_t>(stbtt_GetCodepointBitmap(&info, 0, scale, codepoint, &data.dims.x, &data.dims.y, &data.offset.x, &data.offset.y), [](uint8_t * p) { stbtt_FreeBitmap(p, 0); });

        int advance;
        stbtt_GetCodepointHMetrics(&info, codepoint, &advance, nullptr);
        data.offset.y += baseline;
        data.advance = static_cast<int>(std::floor(advance * scale));
    }
}

void compute_circle_quadrant_coverage(float coverage[], int radius)
{
    const float rr = radius * radius;
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
            const float cross_x = function(y0i); // X location where curve passes from pixel y0i to pixel y1i

            // Coverage for pixel at (i,y0i) is the area under the curve from x0 to cross_x
            if(y0i < radius) coverage[i*radius+y0i] = coverage[y0i*radius+i] = integral(x0, cross_x) - y0i*(cross_x-x0);

            // Coverage for pixel at (i,y1i) is the area of a rectangle from x0 to cross_x, and the area under the curve from cross_x to x1
            if(y1i == y0i - 1) coverage[i*radius+y1i] = coverage[y1i*radius+i] = (cross_x-x0) + integral(cross_x, x1) - y1i*(x1-cross_x);
            else break; // Stop after the first octant
        }
    }
}

void font::make_circle_quadrant(int radius)
{
    auto & data = glyphs[-radius];
    if(!data.pixels)
    {
        std::vector<float> coverage(radius*radius);
        compute_circle_quadrant_coverage(coverage.data(), radius);

        auto memory = reinterpret_cast<uint8_t *>(std::malloc(radius*radius));
        if(!memory) throw std::bad_alloc();
        std::shared_ptr<uint8_t> pixels(memory, std::free);
        for(int i=0; i<radius*radius; ++i) pixels.get()[i] = static_cast<uint8_t>(coverage[i] * 255);

        data.pixels = pixels;
        data.offset = {0, 0};
        data.dims = {radius, radius};
        data.advance = 0;
    }
}

void font::prepare_texture()
{
    // Sort glyphs by descending height, then descending width
    std::vector<glyph_data *> glyph_list;
    for(auto & g : glyphs) glyph_list.push_back(&g.second);
    std::sort(begin(glyph_list), end(glyph_list), [](const glyph_data * a, const glyph_data * b)
    {
        return std::make_tuple(a->dims.y, a->dims.x) > std::make_tuple(b->dims.y, b->dims.x);
    });

    const int border = 1;
    tex_size = {64, 64};
    while(true)
    {
        tex_pixels.resize(tex_size.x * tex_size.y);
        std::fill(begin(tex_pixels), end(tex_pixels), 0);
        tex_pixels[0] = 255;

        int2 used = {border, border};
        int next_line = 0;
        bool bad_pack = false;
        for(auto * g : glyph_list)
        {
            if(used.x + g->dims.x > tex_size.x)
            {
                used.x = border;
                used.y = next_line + border;
            }
            if(used.x + g->dims.x > tex_size.x || used.y + g->dims.y > tex_size.y)
            {
                bad_pack = true;
                break;
            }
            for(int y=0; y<g->dims.y; ++y)
            {
                memcpy(tex_pixels.data() + (used.y+y)*tex_size.x + used.x, g->pixels.get() + y*g->dims.x, g->dims.x);
            }
            g->s0 = static_cast<float>(used.x) / tex_size.x;
            g->t0 = static_cast<float>(used.y) / tex_size.y;
            g->s1 = static_cast<float>(used.x + g->dims.x) / tex_size.x;
            g->t1 = static_cast<float>(used.y + g->dims.y) / tex_size.y;
            used.x += g->dims.x + border;
            next_line = std::max(next_line, used.y + g->dims.y);
        }
        if(bad_pack)
        {
            if(tex_size.x == tex_size.y) tex_size.x *= 2;
            else tex_size.y *= 2;
        }
        else break;
    }
}