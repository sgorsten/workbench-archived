#ifndef FONT_H
#define FONT_H

#include "linalg.h"
using namespace linalg::aliases;

#include <cstdint>
#include <memory>
#include <vector>
#include <map>

struct glyph_data
{
    int2 dims, offset; int advance;
    float s0, t0, s1, t1;
    std::shared_ptr<const uint8_t> pixels;
};

class glyph_map
{
    std::vector<glyph_data> glyphs;
    std::vector<uint8_t> tex_pixels;
    int2 tex_size;
public:
    const glyph_data & get_glyph(size_t index) const { return glyphs[index]; }
    const void * get_texture_data() const { return tex_pixels.data(); }
    const int2 & get_texture_size() const { return tex_size; }

    size_t insert_glyph(glyph_data glyph);
    void prepare_texture();
};

class font
{
    glyph_map map;
    std::map<int, size_t> glyphs;
public:
    int line_height, baseline;

    const void * get_texture_data() const { return map.get_texture_data(); }
    const int2 & get_texture_size() const { return map.get_texture_size(); }

    const glyph_data * get_glyph(int codepoint) const;
    int get_text_width(const std::string & text) const;
    std::string::size_type get_cursor_pos(const std::string & text, int x) const;

    void load_glyphs(const std::string & path, int size, const std::vector<int> & codepoints);
    void make_circle_quadrant(int radius);
    void prepare_texture() { map.prepare_texture(); }
};

#endif