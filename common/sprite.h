// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef SPRITE_H
#define SPRITE_H

#include "linalg.h"
using namespace linalg::aliases;

#include <memory>
#include <vector>

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

#endif