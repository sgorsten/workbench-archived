// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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
        glyph_data & g = glyphs[codepoint];

        sprite s;
        s.pixels = std::shared_ptr<uint8_t>(stbtt_GetCodepointBitmap(&info, 0, scale, codepoint, &s.dims.x, &s.dims.y, &g.offset.x, &g.offset.y), [](uint8_t * p) { stbtt_FreeBitmap(p, 0); });
        g.sprite_index = sprites->insert_sprite(s);

        int advance;
        stbtt_GetCodepointHMetrics(&info, codepoint, &advance, nullptr);
        g.offset.y += baseline;
        g.advance = static_cast<int>(std::floor(advance * scale));
    }
}
