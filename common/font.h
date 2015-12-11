// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef FONT_H
#define FONT_H

#include "sprite.h"

#include <cstdint>
#include <array>
#include <map>

namespace utf8
{
    const char * prev(const char * units); // Assumes units points just past the end of a valid utf-8 sequence of code units
    const char * next(const char * units); // Assumes units points to the start of a valid utf-8 sequence of code units
    uint32_t code(const char * units); // Assumes units points to the start of a valid utf-8 sequence of code units
    std::array<char,5> units(uint32_t code); // Assumes code < 0x110000
    bool is_valid(const char * units, size_t count); // Return true if the given sequence of code units is valid utf-8

    struct codepoint_iterator 
    { 
        const char * p;
        uint32_t operator * () const { return code(p); }
        codepoint_iterator & operator ++ () { p = next(p); return *this; }
        bool operator != (const codepoint_iterator & r) const { return p != r.p; }
    };

    struct string_view
    {
        const char * first, * last;
        string_view() : first(), last() {}
        string_view(const std::string & s) : first(s.data()), last(s.data() + s.size()) {}
        template<int N> string_view(const char (& s)[N]) : first(s), last(s + N) {}
        codepoint_iterator begin() const { return {first}; }
        codepoint_iterator end() const { return {last}; }
    };
}

struct glyph_data
{
    size_t sprite_index;
    int2 offset;
    int advance;
};

class font
{
    sprite_sheet * sprites;
    std::map<int, glyph_data> glyphs;
public:
    font() : sprites() {}
    font(sprite_sheet * sprites) : sprites(sprites) {}

    int line_height, baseline;

    const glyph_data * get_glyph(int codepoint) const;
    int get_text_width(utf8::string_view text) const;
    std::string::size_type get_cursor_pos(utf8::string_view text, int x) const;

    void load_glyphs(const std::string & path, int size, const std::vector<int> & codepoints);
};

#endif