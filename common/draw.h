// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef DRAW_H
#define DRAW_H

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <memory>

#include <GL\glew.h>

#include "linalg.h"
using namespace linalg::aliases;

enum class byte : uint8_t {};
enum class native_type { float_, double_, int_, unsigned_int, bool_ };
struct gl_data_type { GLenum gl_type; native_type component_type; int num_rows, num_cols; };
struct gl_sampler_type { GLenum gl_type; native_type component_type; GLenum target; bool shadow; };
struct sampler_desc { std::string name; const gl_sampler_type * sampler_type; int location, array_size; };
struct uniform_desc
{
    std::string name;
    const gl_data_type * data_type;
    int location, array_size;
    int3 stride; // row, column, element

    template<class T> void set_component_value(byte buffer[], int row, int column, int element, T value) const
    {
        switch(data_type->component_type)
        {
        case native_type::float_:       reinterpret_cast<float    &>(buffer[location + dot(stride, int3(row,column,element))]) = static_cast<float   >(value); break;
        case native_type::double_:      reinterpret_cast<double   &>(buffer[location + dot(stride, int3(row,column,element))]) = static_cast<double  >(value); break;
        case native_type::int_:         reinterpret_cast<int32_t  &>(buffer[location + dot(stride, int3(row,column,element))]) = static_cast<int32_t >(value); break;
        case native_type::unsigned_int: reinterpret_cast<uint32_t &>(buffer[location + dot(stride, int3(row,column,element))]) = static_cast<uint32_t>(value); break;
        case native_type::bool_:        reinterpret_cast<uint32_t &>(buffer[location + dot(stride, int3(row,column,element))]) = value ? 1 : 0; break;
        }
    }

    template<class T> void set_element_value(byte buffer[], int element, const T & value) const { set_component_value(buffer, 0, 0, element, value); }
    template<class T, int M> void set_element_value(byte buffer[], int element, const linalg::vec<T,M> & value) const { for(int i=0; i<M; ++i) set_component_value(buffer, i, 0, element, value[i]); }
    template<class T, int M, int N> void set_element_value(byte buffer[], int element, const linalg::mat<T,M,N> & value) const { for(int j=0; j<N; ++j) for(int i=0; i<M; ++i) set_component_value(buffer, i, j, element, value(i,j)); }

    template<class T> void set_value(byte buffer[], const T & value) const { set_element_value(buffer, 0, value); }
};
struct uniform_block_desc
{
    std::string name;
    GLuint index, binding;
    size_t data_size;
    std::vector<uniform_desc> uniforms;

    const uniform_desc * get_uniform_desc(const char * name) const { for(auto & u : uniforms) if(u.name == name) return &u; return nullptr; }
    template<class T> void set_uniform(byte buffer[], const char * name, const T & value) const { if(auto * u = get_uniform_desc(name)) u->set_value(buffer, value); }
};
struct program_desc
{
    std::vector<uniform_block_desc> blocks;
    std::vector<sampler_desc> samplers;
    const uniform_block_desc * get_block_desc(const char * name) const { for(auto & b : blocks) if(b.name == name) return &b; return nullptr; }
    const sampler_desc * get_sampler_desc(const char * name) const { for(auto & s : samplers) if(s.name == name) return &s; return nullptr; }
};

std::ostream & operator << (std::ostream & o, const gl_data_type & t);
std::ostream & operator << (std::ostream & o, const uniform_desc & u);

struct GLFWwindow;
struct GLFWmonitor;

// This namespace abstracts over OpenGL, and provides reasonable object-oriented access
namespace gfx
{
    struct context;
    struct shader;
    struct program;
    struct texture;
    struct mesh;

    std::shared_ptr<context>    create_context      ();

    std::shared_ptr<shader>     compile_shader      (std::shared_ptr<context> ctx, GLenum type, const char * source);
    std::shared_ptr<program>    link_program        (std::shared_ptr<context> ctx, std::initializer_list<std::shared_ptr<shader>> shaders);
    const program_desc &        get_desc            (const program & p);

    std::shared_ptr<texture>    create_texture(std::shared_ptr<context> ctx);
    void                        set_mip_image       (std::shared_ptr<texture> tex, int mip, GLenum internalformat, const int2 & dims, GLenum format, GLenum type, const void * pixels);
    void                        generate_mips       (std::shared_ptr<texture> tex);

    std::shared_ptr<mesh>       create_mesh         (std::shared_ptr<context> ctx);
    void                        set_vertices        (mesh & m, const void * data, size_t size);
    void                        set_attribute       (mesh & m, int index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
    void                        set_indices         (mesh & m, GLenum mode, const unsigned int * data, size_t count);

    GLFWwindow *                create_window       (context & ctx,  const int2 & dims, const char * title, GLFWmonitor * monitor = nullptr);

    template<class V, int N> void set_attribute(mesh & m, int index, linalg::vec<float,N> V::* attribute) { set_attribute(m, index, N, GL_FLOAT, GL_FALSE, sizeof(V), &(static_cast<V*>(0)->*attribute)); }
    template<class V, int N> void set_attribute(mesh & m, int index, linalg::vec<short,N> V::* attribute) { set_attribute(m, index, N, GL_SHORT, GL_FALSE, sizeof(V), &(static_cast<V*>(0)->*attribute)); }
    template<class V, int N> void set_attribute(mesh & m, int index, linalg::vec<uint8_t,N> V::* attribute) { set_attribute(m, index, N, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(V), &(static_cast<V*>(0)->*attribute)); }
}

// These types do not make any OpenGL calls. Lists can be freely composited in parallel, from background threads, etc.
struct rect 
{ 
    int x0, y0, x1, y1; 
    int width() const { return x1 - x0; }
    int height() const { return y1 - y0; }
    int2 dims() const { return {width(), height()}; }
    float aspect_ratio() const { return (float)width()/height(); }
};

class material
{
    std::shared_ptr<const gfx::program> program;
    const uniform_block_desc * block; 
    std::vector<byte> buffer;
    std::vector<std::shared_ptr<const gfx::texture>> textures;
public:
    material(std::shared_ptr<const gfx::program> program);

    std::shared_ptr<const gfx::program> get_program() const { return program; }
    const std::vector<byte> & get_buffer() const { return buffer; }
    const std::vector<std::shared_ptr<const gfx::texture>> & get_textures() const { return textures; }

    const uniform_block_desc * get_block_desc() const { return block; }
    byte * get_buffer_data() { return buffer.data(); }
    template<class T> void set_uniform(const char * name, const T & value) { block->set_uniform(get_buffer_data(), name, value); }
    void set_sampler(const char * name, std::shared_ptr<const gfx::texture> texture);
};

class draw_list
{
    struct object 
    { 
        std::shared_ptr<const gfx::mesh> mesh;
        std::shared_ptr<const gfx::program> program;
        const uniform_block_desc * block; size_t buffer_offset, texture_offset;
    };
    std::vector<byte> buffer;
    std::vector<std::shared_ptr<const gfx::texture>> textures;
    std::vector<object> objects;
public:
    const std::vector<byte> & get_buffer() const { return buffer; }
    const std::vector<std::shared_ptr<const gfx::texture>> & get_textures() const { return textures; }
    const std::vector<object> & get_objects() const { return objects; }

    void begin_object(std::shared_ptr<const gfx::mesh> mesh, const material & mat);
    void begin_object(std::shared_ptr<const gfx::mesh> mesh, std::shared_ptr<const gfx::program> program);
    template<class T> void set_uniform(const char * name, const T & value)
    {
        const auto & object = objects.back();
        object.block->set_uniform(buffer.data() + object.buffer_offset, name, value);
    }
    void set_sampler(const char * name, std::shared_ptr<const gfx::texture> texture);
};

class renderer
{
    GLuint scene_ubo=0, object_ubo=0;
public:
    void draw_scene(GLFWwindow * window, const rect & viewport, const uniform_block_desc * per_scene, const void * data, const draw_list & list);
};

#endif