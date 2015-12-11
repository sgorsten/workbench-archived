// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <fstream>
#include <algorithm>

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
