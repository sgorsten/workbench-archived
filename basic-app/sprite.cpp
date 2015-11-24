// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "sprite.h"
#include <algorithm> // For std::sort
#include <tuple> // For std::make_tuple

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
            g->s0 = static_cast<float>(used.x) / tex_dims.x;
            g->t0 = static_cast<float>(used.y) / tex_dims.y;
            g->s1 = static_cast<float>(used.x + g->dims.x) / tex_dims.x;
            g->t1 = static_cast<float>(used.y + g->dims.y) / tex_dims.y;
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

sprite make_circle_quadrant(int radius)
{
    sprite s;
    std::vector<float> coverage(radius*radius);
    compute_circle_quadrant_coverage(coverage.data(), radius);

    auto memory = reinterpret_cast<uint8_t *>(std::malloc(radius*radius));
    if(!memory) throw std::bad_alloc();
    std::shared_ptr<uint8_t> pixels(memory, std::free);
    for(int i=0; i<radius*radius; ++i) pixels.get()[i] = static_cast<uint8_t>(coverage[i] * 255);

    s.pixels = pixels;
    s.dims = {radius, radius};
    return s;
}