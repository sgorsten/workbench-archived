// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "load.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

void load_texture(std::shared_ptr<gfx::texture> tex, const char * filename)
{
    int2 dims; int comp;
    auto image = stbi_load(filename, &dims.x, &dims.y, &comp, 0);
    switch(comp)
    {
    case 1: gfx::set_mip_image(tex, 0, GL_RGBA, dims, GL_LUMINANCE,       GL_UNSIGNED_BYTE, image); break;
    case 2: gfx::set_mip_image(tex, 0, GL_RGBA, dims, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, image); break;
    case 3: gfx::set_mip_image(tex, 0, GL_RGBA, dims, GL_RGB,             GL_UNSIGNED_BYTE, image); break;
    case 4: gfx::set_mip_image(tex, 0, GL_RGBA, dims, GL_RGBA,            GL_UNSIGNED_BYTE, image); break;
    }
    gfx::generate_mips(tex);
    stbi_image_free(image);
}
