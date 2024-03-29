// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw.h"

#include <cassert>
#include <vector>
#include <string>
#include <map>

#include <GLFW\glfw3.h>

static const gl_data_type * get_gl_data_type(GLenum gl_type);
static const gl_sampler_type * get_gl_sampler_type(GLenum gl_type);



struct gfx::context
{
    GLFWwindow * hidden = nullptr;
    ~context()
    {
        if(hidden) glfwDestroyWindow(hidden);
        glfwTerminate();
    }
};

std::shared_ptr<gfx::context> gfx::create_context()
{
    glfwInit();
    auto ctx = std::make_shared<gfx::context>();
    glfwWindowHint(GLFW_VISIBLE, 0);
    ctx->hidden = glfwCreateWindow(1, 1, "OpenGL Context", nullptr, nullptr);
    glfwMakeContextCurrent(ctx->hidden);
    glewInit();    
    glfwDefaultWindowHints();
    return ctx;
}



struct gfx::shader
{
    std::shared_ptr<gfx::context> ctx;
    GLuint object_name;
    shader(std::shared_ptr<gfx::context> ctx) : ctx(move(ctx)), object_name() {}
    ~shader() { if(object_name) glDeleteShader(object_name); }
};

std::shared_ptr<gfx::shader> gfx::compile_shader(std::shared_ptr<context> ctx, GLenum type, const char * source)
{
    glfwMakeContextCurrent(ctx->hidden);
    auto s = std::make_shared<shader>(ctx);
    s->object_name = glCreateShader(type);
    glShaderSource(s->object_name, 1, &source, nullptr);
    glCompileShader(s->object_name);

    GLint status, length;
    glGetShaderiv(s->object_name, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE)
    {
        glGetShaderiv(s->object_name, GL_INFO_LOG_LENGTH, &length);
        std::string log(length-1, ' ');
        glGetShaderInfoLog(s->object_name, length, nullptr, &log[0]);
        throw std::runtime_error("GLSL compile error: " + log);
    }

    return s;
}



struct gfx::program
{
    std::shared_ptr<gfx::context> ctx;
    GLuint object_name;
    program_desc desc;
    program(std::shared_ptr<gfx::context> ctx) : ctx(move(ctx)), object_name() {}
    ~program() { if(object_name) glDeleteProgram(object_name); }
};

std::shared_ptr<gfx::program> gfx::link_program(std::shared_ptr<context> ctx, std::initializer_list<std::shared_ptr<shader>> shaders)
{
    glfwMakeContextCurrent(ctx->hidden);
    auto p = std::make_shared<gfx::program>(ctx);
    p->object_name = glCreateProgram();
    for(auto s : shaders) glAttachShader(p->object_name, s->object_name);
    glLinkProgram(p->object_name);

    GLint status, length, count;
    glGetProgramiv(p->object_name, GL_LINK_STATUS, &status);
    if(status == GL_FALSE)
    {
        glGetProgramiv(p->object_name, GL_INFO_LOG_LENGTH, &length);
        std::string log(length-1, ' ');
        glGetProgramInfoLog(p->object_name, length, nullptr, &log[0]);
        throw std::runtime_error("GLSL link error: " + log);
    }

    // Obtain uniform block metadata
    glGetProgramiv(p->object_name, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    glGetProgramiv(p->object_name, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &length);
    std::vector<char> name_buffer(length);
    for(GLuint i=0; i<count; ++i)
    {
        GLint binding, data_size;
        glGetActiveUniformBlockiv(p->object_name, i, GL_UNIFORM_BLOCK_BINDING, &binding);
        glGetActiveUniformBlockiv(p->object_name, i, GL_UNIFORM_BLOCK_DATA_SIZE, &data_size);
        glGetActiveUniformBlockName(p->object_name, i, length, nullptr, name_buffer.data());
        p->desc.blocks.push_back({name_buffer.data(), i, binding, data_size});
    }

    // Obtain uniform metadata
    glGetProgramiv(p->object_name, GL_ACTIVE_UNIFORMS, &count);
    glGetProgramiv(p->object_name, GL_ACTIVE_UNIFORM_MAX_LENGTH, &length);
    name_buffer.resize(length);
    for(GLuint i=0; i<count; ++i)
    {
        GLint type=0, size=0;
        glGetActiveUniformsiv(p->object_name, 1, &i, GL_UNIFORM_TYPE, &type);
        glGetActiveUniformsiv(p->object_name, 1, &i, GL_UNIFORM_SIZE, &size);
        glGetActiveUniformName(p->object_name, i, name_buffer.size(), nullptr, name_buffer.data());
        if(auto * s = get_gl_sampler_type(type))
        {
            p->desc.samplers.push_back({name_buffer.data(), s, glGetUniformLocation(p->object_name, name_buffer.data()), size});
            continue;
        }

        // Data uniforms must belong to a uniform block
        GLint block_index=GL_INVALID_INDEX, offset=0, array_stride=0, matrix_stride=0, is_row_major=0;
        glGetActiveUniformsiv(p->object_name, 1, &i, GL_UNIFORM_BLOCK_INDEX, &block_index);
        if(block_index == GL_INVALID_INDEX) continue;
        glGetActiveUniformsiv(p->object_name, 1, &i, GL_UNIFORM_OFFSET, &offset);
        glGetActiveUniformsiv(p->object_name, 1, &i, GL_UNIFORM_ARRAY_STRIDE, &array_stride);
        glGetActiveUniformsiv(p->object_name, 1, &i, GL_UNIFORM_MATRIX_STRIDE, &matrix_stride);
        glGetActiveUniformsiv(p->object_name, 1, &i, GL_UNIFORM_IS_ROW_MAJOR, &is_row_major);
        if(auto * t = get_gl_data_type(type))
        {
            uniform_desc u;
            u.name = name_buffer.data();
            u.data_type = t;
            u.location = offset;
            u.stride.x = t->component_type == native_type::double_ ? 8 : 4;
            u.stride.y = matrix_stride;
            u.stride.z = array_stride;
            if(is_row_major) std::swap(u.stride.x, u.stride.y);
            p->desc.blocks[block_index].uniforms.push_back(u);
        }
    }
    return p;
}

const program_desc & gfx::get_desc(const program & p) { return p.desc; }



struct gfx::texture
{
    std::shared_ptr<gfx::context> ctx;
    GLuint object_name;
    texture(std::shared_ptr<gfx::context> ctx) : ctx(move(ctx)), object_name() {}
    ~texture() { if(object_name) glDeleteTextures(1, &object_name); }
};

std::shared_ptr<gfx::texture> gfx::create_texture(std::shared_ptr<gfx::context> ctx) 
{
    return std::make_shared<texture>(ctx);
}

void gfx::set_mip_image(std::shared_ptr<texture> tex, int mip, GLenum internalformat, const int2 & dims, GLenum format, GLenum type, const void * pixels)
{
    glfwMakeContextCurrent(tex->ctx->hidden);
    if(!tex->object_name) glGenTextures(1, &tex->object_name);
    glBindTexture(GL_TEXTURE_2D, tex->object_name);
    glTexImage2D(GL_TEXTURE_2D, mip, internalformat, dims.x, dims.y, 0, format, type, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}
void gfx::generate_mips(std::shared_ptr<texture> tex)
{
    glfwMakeContextCurrent(tex->ctx->hidden);
    glBindTexture(GL_TEXTURE_2D, tex->object_name);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
}



struct gfx::mesh
{
    struct attribute { GLint size; GLenum type; GLboolean normalized; const GLvoid * pointer; };

    std::shared_ptr<gfx::context> ctx;
    attribute attributes[8];
    GLsizei vertex_stride;
    GLenum primitive_mode, index_type;
    GLsizei element_count;
    GLuint vbo, ibo;
};

std::shared_ptr<gfx::mesh> gfx::create_mesh(std::shared_ptr<context> ctx)
{
    return std::make_shared<mesh>(mesh{ctx});
}

void gfx::set_vertices(mesh & m, const void * data, size_t size)
{
    glfwMakeContextCurrent(m.ctx->hidden);
    if(!m.vbo) glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void gfx::set_attribute(mesh & m, int index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer)
{
    m.attributes[index] = {size, type, normalized, pointer};
    m.vertex_stride = stride; // TODO: Ensure consistency
}

void gfx::set_indices(mesh & m, GLenum mode, const uint16_t * data, size_t count)
{
    glfwMakeContextCurrent(m.ctx->hidden);
    if(!m.ibo) glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * count, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m.primitive_mode = mode;
    m.index_type = GL_UNSIGNED_SHORT;
    m.element_count = count;
}

void gfx::set_indices(mesh & m, GLenum mode, const uint32_t * data, size_t count)
{
    glfwMakeContextCurrent(m.ctx->hidden);
    if(!m.ibo) glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * count, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m.primitive_mode = mode;
    m.index_type = GL_UNSIGNED_INT;
    m.element_count = count;
}

GLFWwindow * gfx::create_window(context & ctx,  const int2 & dims, const char * title, GLFWmonitor * monitor)
{
    return glfwCreateWindow(dims.x, dims.y, title, monitor, ctx.hidden);
}            

std::ostream & operator << (std::ostream & o, const gl_data_type & t)
{
    if(t.num_cols > 1) // Matrix
    {
        if(t.component_type == native_type::double_) o << 'd';
        return o << "mat" << t.num_cols << 'x' << t.num_rows;
    }
    else if(t.num_rows > 1) // Vector
    {
        switch(t.component_type)
        {
        case native_type::double_: o << 'd'; break;
        case native_type::int_: o << 'i'; break;
        case native_type::unsigned_int: o << 'u'; break;
        case native_type::bool_: o << 'b'; break;
        }
        return o << "vec" << t.num_rows;
    }
    else // Scalar
    {
        switch(t.component_type)
        {
        case native_type::float_: return o << "float";
        case native_type::double_: return o << "double";
        case native_type::int_: return o << "int";
        case native_type::unsigned_int: return o << "unsigned int";
        case native_type::bool_: return o << "bool";
        default: throw std::logic_error("bad type");
        }    
    }
}

std::ostream & operator << (std::ostream & o, const uniform_desc & u)
{
    o << u.name << " : " << *u.data_type;
    if(u.array_size > 1) o << "[" << u.array_size << "]";
    return o << " @ " << u.location << ':' << u.stride.x << ',' << u.stride.y << ',' << u.stride.z;
}

material::material(std::shared_ptr<const gfx::program> program) : program(program), block(program->desc.get_block_desc("PerObject")), buffer(block->data_size), textures(program->desc.samplers.size()) {}

void material::set_sampler(const char * name, std::shared_ptr<const gfx::texture> texture)
{
    for(size_t i=0; i<program->desc.samplers.size(); ++i)
    {
        if(program->desc.samplers[i].name == name)
        {
            textures[i] = texture;
        }
    }
}

void draw_list::begin_object(std::shared_ptr<const gfx::mesh> mesh, const material & mat)
{
    objects.push_back({mesh, mat.get_program(), mat.get_block_desc(), buffer.size(), textures.size()});
    buffer.insert(end(buffer), begin(mat.get_buffer()), end(mat.get_buffer()));
    textures.insert(end(textures), begin(mat.get_textures()), end(mat.get_textures()));
}

void draw_list::begin_object(std::shared_ptr<const gfx::mesh> mesh, std::shared_ptr<const gfx::program> program)
{
    const uniform_block_desc * block = program->desc.get_block_desc("PerObject");
    objects.push_back({mesh, program, block, buffer.size(), textures.size()});
    buffer.resize(buffer.size() + block->data_size);
    textures.resize(textures.size() + program->desc.samplers.size());
}

void draw_list::set_sampler(const char * name, std::shared_ptr<const gfx::texture> texture)
{
    auto & o = objects.back();
    for(size_t i=0; i<o.program->desc.samplers.size(); ++i)
    {
        if(o.program->desc.samplers[i].name == name)
        {
            textures[o.texture_offset + i] = texture;
        }
    }
}



void renderer::draw_scene(GLFWwindow * window, const rect & r, const uniform_block_desc * per_scene, const void * data, const draw_list & list)
{
    int fw, fh, w, h;
    glfwGetFramebufferSize(window, &fw, &fh);
    glfwGetWindowSize(window, &w, &h);
    const int multiplier = fw / w;
    assert(w * multiplier == fw);
    assert(h * multiplier == fh);

    glfwMakeContextCurrent(window);
    glViewport(r.x0 * multiplier, fh - r.y1 * multiplier, r.width() * multiplier, r.height() * multiplier);
    glScissor(r.x0 * multiplier, fh - r.y1 * multiplier, r.width() * multiplier, r.height() * multiplier);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    if(per_scene)
    {
        if(!scene_ubo) glGenBuffers(1, &scene_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
        glBufferData(GL_UNIFORM_BUFFER, per_scene->data_size, data, GL_STREAM_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, per_scene->binding, scene_ubo);
    }

    if(!object_ubo) glGenBuffers(1, &object_ubo);
    const byte * buffer = list.get_buffer().data();
    const gfx::program * current_program = nullptr;
    const gfx::mesh * current_mesh = nullptr;

    for(auto & object : list.get_objects())
    {
        if(object.program.get() != current_program)
        {
            current_program = object.program.get();
            glUseProgram(current_program->object_name);
            // TODO: Bind scene textures as appropriate
        }

        if(object.mesh.get() != current_mesh)
        {
            current_mesh = object.mesh.get();
            
            glBindBuffer(GL_ARRAY_BUFFER, current_mesh->vbo);
            for(int i=0; i<8; ++i)
            {
                auto & a = current_mesh->attributes[i];
                if(a.size)
                {
                    glVertexAttribPointer(i, a.size, a.type, a.normalized, current_mesh->vertex_stride, a.pointer);
                    glEnableVertexAttribArray(i);
                }
                else glDisableVertexAttribArray(i);
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, current_mesh->ibo);           
        }

        glBindBuffer(GL_UNIFORM_BUFFER, object_ubo);
        glBufferData(GL_UNIFORM_BUFFER, object.block->data_size, buffer + object.buffer_offset, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, object.block->binding, object_ubo);

        for(int i=0; i<current_program->desc.samplers.size(); ++i)
        {
            const gfx::texture * tex = list.get_textures()[object.texture_offset+i].get();
            glUniform1i(current_program->desc.samplers[i].location, i);
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(current_program->desc.samplers[i].sampler_type->target, tex ? tex->object_name : 0);
        }

        glDrawElements(current_mesh->primitive_mode, current_mesh->element_count, current_mesh->index_type, 0);
    }

    // Reset all state
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    for(int i=0; i<8; ++i) glDisableVertexAttribArray(i);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_SCISSOR_TEST);
}

const gl_data_type * get_gl_data_type(GLenum gl_type)
{
    static const gl_data_type gl_data_types[] = 
    {
        {GL_FLOAT,             native_type::float_,       1, 1}, // float
        {GL_FLOAT_VEC2,        native_type::float_,       2, 1}, // vec2
        {GL_FLOAT_VEC3,        native_type::float_,       3, 1}, // vec3
        {GL_FLOAT_VEC4,        native_type::float_,       4, 1}, // vec4
        {GL_DOUBLE,            native_type::double_,      1, 1}, // double
        {GL_DOUBLE_VEC2,       native_type::double_,      2, 1}, // dvec2
        {GL_DOUBLE_VEC3,       native_type::double_,      3, 1}, // dvec3
        {GL_DOUBLE_VEC4,       native_type::double_,      4, 1}, // dvec4
        {GL_INT,               native_type::int_,         1, 1}, // int
        {GL_INT_VEC2,          native_type::int_,         2, 1}, // ivec2
        {GL_INT_VEC3,          native_type::int_,         3, 1}, // ivec3
        {GL_INT_VEC4,          native_type::int_,         4, 1}, // ivec4
        {GL_UNSIGNED_INT,      native_type::unsigned_int, 1, 1}, // unsigned int
        {GL_UNSIGNED_INT_VEC2, native_type::unsigned_int, 2, 1}, // uvec2
        {GL_UNSIGNED_INT_VEC3, native_type::unsigned_int, 3, 1}, // uvec3
        {GL_UNSIGNED_INT_VEC4, native_type::unsigned_int, 4, 1}, // uvec4
        {GL_BOOL,              native_type::bool_,        1, 1}, // bool
        {GL_BOOL_VEC2,         native_type::bool_,        2, 1}, // bvec2
        {GL_BOOL_VEC3,         native_type::bool_,        3, 1}, // bvec3
        {GL_BOOL_VEC4,         native_type::bool_,        4, 1}, // bvec4
        {GL_FLOAT_MAT2,        native_type::float_,       2, 2}, // mat2
        {GL_FLOAT_MAT3,        native_type::float_,       3, 3}, // mat3
        {GL_FLOAT_MAT4,        native_type::float_,       4, 4}, // mat4
        {GL_FLOAT_MAT2x3,      native_type::float_,       3, 2}, // mat2x3
        {GL_FLOAT_MAT2x4,      native_type::float_,       4, 2}, // mat2x4
        {GL_FLOAT_MAT3x2,      native_type::float_,       2, 3}, // mat3x2
        {GL_FLOAT_MAT3x4,      native_type::float_,       4, 3}, // mat3x4
        {GL_FLOAT_MAT4x2,      native_type::float_,       2, 4}, // mat4x2
        {GL_FLOAT_MAT4x3,      native_type::float_,       3, 4}, // mat4x3
        {GL_DOUBLE_MAT2,       native_type::double_,      2, 2}, // dmat2
        {GL_DOUBLE_MAT3,       native_type::double_,      3, 3}, // dmat3
        {GL_DOUBLE_MAT4,       native_type::double_,      4, 4}, // dmat4
        {GL_DOUBLE_MAT2x3,     native_type::double_,      3, 2}, // dmat2x3
        {GL_DOUBLE_MAT2x4,     native_type::double_,      4, 2}, // dmat2x4
        {GL_DOUBLE_MAT3x2,     native_type::double_,      2, 3}, // dmat3x2
        {GL_DOUBLE_MAT3x4,     native_type::double_,      4, 3}, // dmat3x4
        {GL_DOUBLE_MAT4x2,     native_type::double_,      2, 4}, // dmat4x2
        {GL_DOUBLE_MAT4x3,     native_type::double_,      3, 4}, // dmat4x3
    };
    for(auto & type : gl_data_types)
    {
        if(type.gl_type == gl_type) return &type;
    }
    return nullptr;
}

const gl_sampler_type * get_gl_sampler_type(GLenum gl_type)
{
    static const gl_sampler_type gl_sampler_types[] = 
    {
        {GL_SAMPLER_1D,                                native_type::float_,       GL_TEXTURE_1D, false},
        {GL_SAMPLER_2D,                                native_type::float_,       GL_TEXTURE_2D, false},
        {GL_SAMPLER_3D,                                native_type::float_,       GL_TEXTURE_3D, false},
        {GL_SAMPLER_CUBE,                              native_type::float_,       GL_TEXTURE_CUBE_MAP, false},
        {GL_SAMPLER_1D_SHADOW,                         native_type::float_,       GL_TEXTURE_1D, true},
        {GL_SAMPLER_2D_SHADOW,                         native_type::float_,       GL_TEXTURE_2D, true},
        {GL_SAMPLER_1D_ARRAY,                          native_type::float_,       GL_TEXTURE_1D_ARRAY, false},
        {GL_SAMPLER_2D_ARRAY,                          native_type::float_,       GL_TEXTURE_2D_ARRAY, false},
        {GL_SAMPLER_1D_ARRAY_SHADOW,                   native_type::float_,       GL_TEXTURE_1D_ARRAY, true},
        {GL_SAMPLER_2D_ARRAY_SHADOW,                   native_type::float_,       GL_TEXTURE_2D_ARRAY, true},
        {GL_SAMPLER_2D_MULTISAMPLE,                    native_type::float_,       GL_TEXTURE_2D_MULTISAMPLE, false},
        {GL_SAMPLER_2D_MULTISAMPLE_ARRAY,              native_type::float_,       GL_TEXTURE_2D_MULTISAMPLE_ARRAY, false},
        {GL_SAMPLER_CUBE_SHADOW,                       native_type::float_,       GL_TEXTURE_CUBE_MAP, true},
        {GL_SAMPLER_BUFFER,                            native_type::float_,       GL_TEXTURE_BUFFER, false},
        {GL_SAMPLER_2D_RECT,                           native_type::float_,       GL_TEXTURE_RECTANGLE, false},
        {GL_SAMPLER_2D_RECT_SHADOW,                    native_type::float_,       GL_TEXTURE_RECTANGLE, true},
        {GL_INT_SAMPLER_1D,                            native_type::int_,         GL_TEXTURE_1D, false},
        {GL_INT_SAMPLER_2D,                            native_type::int_,         GL_TEXTURE_2D, false},
        {GL_INT_SAMPLER_3D,                            native_type::int_,         GL_TEXTURE_3D, false},
        {GL_INT_SAMPLER_CUBE,                          native_type::int_,         GL_TEXTURE_CUBE_MAP, false},
        {GL_INT_SAMPLER_1D_ARRAY,                      native_type::int_,         GL_TEXTURE_1D_ARRAY, false},
        {GL_INT_SAMPLER_2D_ARRAY,                      native_type::int_,         GL_TEXTURE_2D_ARRAY, false},
        {GL_INT_SAMPLER_2D_MULTISAMPLE,                native_type::int_,         GL_TEXTURE_2D_MULTISAMPLE, false},
        {GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,          native_type::int_,         GL_TEXTURE_2D_MULTISAMPLE_ARRAY, false},
        {GL_INT_SAMPLER_BUFFER,                        native_type::int_,         GL_TEXTURE_BUFFER, false},
        {GL_INT_SAMPLER_2D_RECT,                       native_type::int_,         GL_TEXTURE_RECTANGLE, false},
        {GL_UNSIGNED_INT_SAMPLER_1D,                   native_type::unsigned_int, GL_TEXTURE_1D, false},
        {GL_UNSIGNED_INT_SAMPLER_2D,                   native_type::unsigned_int, GL_TEXTURE_2D, false},
        {GL_UNSIGNED_INT_SAMPLER_3D,                   native_type::unsigned_int, GL_TEXTURE_3D, false},
        {GL_UNSIGNED_INT_SAMPLER_CUBE,                 native_type::unsigned_int, GL_TEXTURE_CUBE_MAP, false},
        {GL_UNSIGNED_INT_SAMPLER_1D_ARRAY,             native_type::unsigned_int, GL_TEXTURE_1D_ARRAY, false},
        {GL_UNSIGNED_INT_SAMPLER_2D_ARRAY,             native_type::unsigned_int, GL_TEXTURE_2D_ARRAY, false},
        {GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE,       native_type::unsigned_int, GL_TEXTURE_2D_MULTISAMPLE, false},
        {GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, native_type::unsigned_int, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, false},
        {GL_UNSIGNED_INT_SAMPLER_BUFFER,               native_type::unsigned_int, GL_TEXTURE_BUFFER, false},
        {GL_UNSIGNED_INT_SAMPLER_2D_RECT,              native_type::unsigned_int, GL_TEXTURE_RECTANGLE, false},
    };
    for(auto & type : gl_sampler_types)
    {
        if(type.gl_type == gl_type) return &type;
    }
    return nullptr;
}