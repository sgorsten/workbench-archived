// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw.h"

void load_texture(std::shared_ptr<gfx::texture> tex, const char * filename);

inline std::shared_ptr<gfx::texture> load_texture(std::shared_ptr<gfx::context> ctx, const char * filename)
{
    auto tex = create_texture(ctx);
    load_texture(tex, filename);
    return tex;
}
