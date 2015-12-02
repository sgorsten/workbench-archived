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

struct GLFWwindow;
struct GLFWmonitor;

namespace gfx
{
    struct context;
    struct mesh;

    std::shared_ptr<context>    create_context  ();
    GLuint                      compile_shader  (context & ctx, GLenum type, const char * source);
    GLuint                      link_program    (context & ctx, std::initializer_list<GLuint> shaders);
    GLuint                      load_texture    (context & ctx, const char * filename);

    std::shared_ptr<mesh>       create_mesh     (std::shared_ptr<context> ctx);
    void                        set_vertices    (mesh & m, const void * data, size_t size);
    void                        set_attribute   (mesh & m, int index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid * pointer);
    void                        set_indices     (mesh & m, GLenum mode, const unsigned int * data, size_t count);

    GLFWwindow *                create_window   (context & ctx,  const int2 & dims, const char * title, GLFWmonitor * monitor = nullptr);
}

enum class byte : uint8_t {};
enum class native_type { float_, double_, int_, unsigned_int, bool_ };
struct gl_data_type { GLenum gl_type; native_type component_type; int num_rows, num_cols; };
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

const gl_data_type * get_gl_data_type(GLenum gl_type);
uniform_block_desc get_uniform_block_description(GLuint program, const char * name);
std::ostream & operator << (std::ostream & o, const gl_data_type & t);
std::ostream & operator << (std::ostream & o, const uniform_desc & u);

GLuint load_texture(const char * filename);

// This type does not make any OpenGL calls. Lists can be freely composited in parallel, from background threads, etc.
class draw_list
{
    struct object { std::shared_ptr<const gfx::mesh> mesh; GLuint program; const uniform_block_desc * block; size_t buffer_offset; };
    std::vector<byte> buffer;
    std::vector<object> objects;
public:
    const std::vector<byte> & get_buffer() const { return buffer; }
    const std::vector<object> & get_objects() const { return objects; }

    void begin_object(std::shared_ptr<const gfx::mesh> mesh, GLuint program, const uniform_block_desc * block);
    template<class T> void set_uniform(const char * name, const T & value)
    {
        const auto & object = objects.back();
        object.block->set_uniform(buffer.data() + object.buffer_offset, name, value);
    }
};

class renderer
{
    GLuint scene_ubo, object_ubo;
public:
    renderer();

    void set_scene_uniforms(const uniform_block_desc & block, const void * data);
    void draw_objects(const draw_list & list);
};

#endif