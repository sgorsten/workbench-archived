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

class font
{
    std::map<int, glyph_data> glyphs;
public:
    int line_height, baseline;

    std::vector<uint8_t> tex_pixels;
    int2 tex_size;

    const glyph_data * get_glyph(int codepoint) const;
    int get_text_width(const std::string & text) const;
    std::string::size_type get_cursor_pos(const std::string & text, int x) const;

    void load_glyphs(const std::string & path, int size, const std::vector<int> & codepoints);
    void make_circle_quadrant(int radius);
    void prepare_texture();
};

#endif