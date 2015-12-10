#include <vector>
#include <GLFW\glfw3.h>
#include "linalg.h"
using namespace linalg::aliases;

#include "sprite.h"
#include "font.h"

struct draw_buffer_2d
{
    struct vertex { short2 position; byte4 color; float2 texcoord; };    
    struct list { size_t level,first,last; };

    std::vector<vertex> vertices;
    std::vector<list> lists;

    void begin_frame()
    {
        vertices.clear();
        lists.clear();
    }

    void emit_rect(int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1, const float4 & color)
    {
        auto c = byte4(color * 255.0f);
        vertices.push_back({{(short)x0,(short)y0}, c, {s0,t0}});
        vertices.push_back({{(short)x1,(short)y0}, c, {s1,t0}});
        vertices.push_back({{(short)x1,(short)y1}, c, {s1,t1}});
        vertices.push_back({{(short)x0,(short)y1}, c, {s0,t1}});
    }
};

GLuint make_sprite_texture_opengl(const sprite_sheet & sprites);
void render_draw_buffer_opengl(const draw_buffer_2d & buffer, GLuint sprite_texture);

void draw_text(draw_buffer_2d & buffer, const sprite_sheet & sprites, const font & f, int2 p, utf8::string_view text)
{
    for(auto codepoint : text)
    {
        if(auto * g = f.get_glyph(codepoint))
        {
            auto & s = sprites.get_sprite(g->sprite_index);
            const int2 p0 = p + g->offset, p1 = p0 + s.dims;
            buffer.emit_rect(p0.x, p0.y, p1.x, p1.y, s.s0, s.t0, s.s1, s.t1, {1,1,1,1});
            p.x += g->advance;
        }
    }
}

void draw_list(draw_buffer_2d & buffer, const sprite_sheet & sprites, const font & f, int2 p, std::initializer_list<utf8::string_view> list)
{
    for(auto s : list)
    {
        draw_text(buffer, sprites, f, p, s);
        p.y += f.line_height;
    }
}

int main()
{
    sprite_sheet sprites;
    font fn = font(&sprites);
    std::vector<int> codepoints;
    for(int i=32; i<256; ++i) codepoints.push_back(i);
    fn.load_glyphs("c:/windows/fonts/arialbd.ttf", 14, codepoints);
    sprites.prepare_texture();

    draw_buffer_2d buffer;
    draw_text(buffer, sprites, fn, {200,200}, "This is a test of some \xC2\xBD finished techniques");
    draw_list(buffer, sprites, fn, {200,250}, {"Red", "Green", "Blue", "White"});

    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Draw2D Test", nullptr, nullptr);
    glfwMakeContextCurrent(win);

    GLuint tex = make_sprite_texture_opengl(sprites);

    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        int w, h;
        glfwGetWindowSize(win, &w, &h);

        glClear(GL_COLOR_BUFFER_BIT);
        glPushMatrix();
        glOrtho(0, w, h, 0, -1, +1);
        render_draw_buffer_opengl(buffer, tex);
        glPopMatrix();
        glfwSwapBuffers(win);
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}

GLuint make_sprite_texture_opengl(const sprite_sheet & sprites)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, sprites.get_texture_dims().x, sprites.get_texture_dims().y, 0, GL_ALPHA, GL_UNSIGNED_BYTE, sprites.get_texture_data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void render_draw_buffer_opengl(const draw_buffer_2d & buffer, GLuint sprite_texture)
{
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_TEXTURE_2D); 
    glBindTexture(GL_TEXTURE_2D, sprite_texture);
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for(auto & v : buffer.vertices)
    {
        glTexCoord2fv(&v.texcoord.x);
        glColor4ubv(&v.color.x);
        glVertex2sv(&v.position.x);
    }
    glEnd();
    glPopAttrib();
}