// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw.h"

#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GLFW\glfw3.h>

opengl_context::opengl_context() : opengl_context(1)
{
    glfwInit();
    glfwWindowHint(GLFW_VISIBLE, 0);
    hidden = glfwCreateWindow(1, 1, "OpenGL Context", nullptr, nullptr);
    glfwMakeContextCurrent(hidden);
    glewInit();    
    glfwDefaultWindowHints();
}

opengl_context::~opengl_context()
{
    if(hidden) glfwDestroyWindow(hidden);
    glfwTerminate();
}

GLuint opengl_context::compile_shader(GLenum type, const char * source)
{
    glfwMakeContextCurrent(hidden);
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status, length;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE)
    {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(length-1, ' ');
        glGetShaderInfoLog(shader, length, nullptr, &log[0]);
        glDeleteShader(shader);
        throw std::runtime_error("GLSL compile error: " + log);
    }

    return shader;
}

GLuint opengl_context::link_program(std::initializer_list<GLuint> shaders)
{
    glfwMakeContextCurrent(hidden);
    GLuint program = glCreateProgram();
    for(auto shader : shaders) glAttachShader(program, shader);
    glLinkProgram(program);

    GLint status, length;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(status == GL_FALSE)
    {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(length-1, ' ');
        glGetProgramInfoLog(program, length, nullptr, &log[0]);
        glDeleteProgram(program);
        throw std::runtime_error("GLSL link error: " + log);
    }

    return program;
}

GLuint opengl_context::load_texture(const char * filename)
{
    glfwMakeContextCurrent(hidden);
    int x, y, comp;
    auto image = stbi_load(filename, &x, &y, &comp, 0);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    switch(comp)
    {
    case 1: glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_LUMINANCE,       GL_UNSIGNED_BYTE, image); break;
    case 2: glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, image); break;
    case 3: glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGB,             GL_UNSIGNED_BYTE, image); break;
    case 4: glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA,            GL_UNSIGNED_BYTE, image); break;
    }
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    stbi_image_free(image);  
    return tex;
}

GLFWwindow * opengl_context::create_window(const int2 & dims, const char * title, GLFWmonitor * monitor)
{
    return glfwCreateWindow(dims.x, dims.y, title, monitor, hidden);
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
const gl_data_type * get_gl_data_type(GLenum gl_type)
{
    for(auto & type : gl_data_types)
    {
        if(type.gl_type == gl_type)
        {
            return &type;
        }
    }
    return nullptr;
}

uniform_block_desc get_uniform_block_description(GLuint program, const char * name)
{
    GLuint block_index = glGetUniformBlockIndex(program, name);
    if(block_index == GL_INVALID_INDEX) throw std::logic_error("missing uniform block");

    GLint binding, data_size;
    glGetActiveUniformBlockiv(program, block_index, GL_UNIFORM_BLOCK_BINDING, &binding);
    glGetActiveUniformBlockiv(program, block_index, GL_UNIFORM_BLOCK_DATA_SIZE, &data_size);
    uniform_block_desc block = {name, block_index, binding, data_size};

    GLint num_uniforms, max_name_length;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &num_uniforms);
    glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
    std::vector<char> name_buffer(max_name_length);
    for(GLuint uniform_index=0; uniform_index<num_uniforms; ++uniform_index)
    {
        GLint uniform_block_index=GL_INVALID_INDEX;
        glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_BLOCK_INDEX, &uniform_block_index);
        if(uniform_block_index != block_index) continue;

        GLint type=0, size=0, offset=0, array_stride=0, matrix_stride=0, is_row_major=0;
        glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_TYPE, &type);
        glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_SIZE, &size);
        glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_OFFSET, &offset);
        glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_ARRAY_STRIDE, &array_stride);
        glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_MATRIX_STRIDE, &matrix_stride);
        glGetActiveUniformsiv(program, 1, &uniform_index, GL_UNIFORM_IS_ROW_MAJOR, &is_row_major);
        glGetActiveUniformName(program, uniform_index, name_buffer.size(), nullptr, name_buffer.data());
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
            block.uniforms.push_back(u);
        }
        //switch(type)
        //{
        //case GL_SAMPLER_1D:                                 std::cout << "sampler1D"; break;
        //case GL_SAMPLER_2D:                                 std::cout << "sampler2D"; break;
        //case GL_SAMPLER_3D:                                 std::cout << "sampler3D"; break;
        //case GL_SAMPLER_CUBE:                               std::cout << "samplerCube"; break;
        //case GL_SAMPLER_1D_SHADOW:                          std::cout << "sampler1DShadow"; break;
        //case GL_SAMPLER_2D_SHADOW:                          std::cout << "sampler2DShadow"; break;
        //case GL_SAMPLER_1D_ARRAY:                           std::cout << "sampler1DArray"; break;
        //case GL_SAMPLER_2D_ARRAY:                           std::cout << "sampler2DArray"; break;
        //case GL_SAMPLER_1D_ARRAY_SHADOW:                    std::cout << "sampler1DArrayShadow"; break;
        //case GL_SAMPLER_2D_ARRAY_SHADOW:                    std::cout << "sampler2DArrayShadow"; break;
        //case GL_SAMPLER_2D_MULTISAMPLE:                     std::cout << "sampler2DMS"; break;
        //case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:               std::cout << "sampler2DMSArray"; break;
        //case GL_SAMPLER_CUBE_SHADOW:                        std::cout << "samplerCubeShadow"; break;
        //case GL_SAMPLER_BUFFER:                             std::cout << "samplerBuffer"; break;
        //case GL_SAMPLER_2D_RECT:                            std::cout << "sampler2DRect"; break;
        //case GL_SAMPLER_2D_RECT_SHADOW:                     std::cout << "sampler2DRectShadow"; break;
        //case GL_INT_SAMPLER_1D:                             std::cout << "isampler1D"; break;
        //case GL_INT_SAMPLER_2D:                             std::cout << "isampler2D"; break;
        //case GL_INT_SAMPLER_3D:                             std::cout << "isampler3D"; break;
        //case GL_INT_SAMPLER_CUBE:                           std::cout << "isamplerCube"; break;
        //case GL_INT_SAMPLER_1D_ARRAY:                       std::cout << "isampler1DArray"; break;
        //case GL_INT_SAMPLER_2D_ARRAY:                       std::cout << "isampler2DArray"; break;
        //case GL_INT_SAMPLER_2D_MULTISAMPLE:                 std::cout << "isampler2DMS"; break;
        //case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:           std::cout << "isampler2DMSArray"; break;
        //case GL_INT_SAMPLER_BUFFER:                         std::cout << "isamplerBuffer"; break;
        //case GL_INT_SAMPLER_2D_RECT:                        std::cout << "isampler2DRect"; break;
        //case GL_UNSIGNED_INT_SAMPLER_1D:                    std::cout << "usampler1D"; break;
        //case GL_UNSIGNED_INT_SAMPLER_2D:                    std::cout << "usampler2D"; break;
        //case GL_UNSIGNED_INT_SAMPLER_3D:                    std::cout << "usampler3D"; break;
        //case GL_UNSIGNED_INT_SAMPLER_CUBE:                  std::cout << "usamplerCube"; break;
        //case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:              std::cout << "usampler2DArray"; break;
        //case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:              std::cout << "usampler2DArray"; break;
        //case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:        std::cout << "usampler2DMS"; break;
        //case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:  std::cout << "usampler2DMSArray"; break;
        //case GL_UNSIGNED_INT_SAMPLER_BUFFER:                std::cout << "usamplerBuffer"; break;
        //case GL_UNSIGNED_INT_SAMPLER_2D_RECT:               std::cout << "usampler2DRect"; break;
        //}
    }

    return block;
}

void draw_list::begin_object(const draw_mesh * mesh, GLuint program, const uniform_block_desc * block)
{
    objects.push_back({mesh, program, block, buffer.size()});
    buffer.resize(buffer.size() + block->data_size);
}

renderer::renderer()
{
    glGenBuffers(1, &scene_ubo);
    glGenBuffers(1, &object_ubo);
}

void renderer::set_scene_uniforms(const uniform_block_desc & block, const void * data)
{
    glBindBuffer(GL_UNIFORM_BUFFER, scene_ubo);
    glBufferData(GL_UNIFORM_BUFFER, block.data_size, data, GL_STREAM_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, block.binding, scene_ubo);
}

void renderer::draw_objects(const draw_list & list)
{
    const byte * buffer = list.get_buffer().data();
    GLuint current_program = 0;
    const draw_mesh * current_mesh = nullptr;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    for(auto & object : list.get_objects())
    {   
        if(object.program != current_program)
        {
            glUseProgram(object.program);
            // TODO: Bind textures as appropriate
            current_program = object.program;
        }

        if(object.mesh != current_mesh)
        {
            glBindVertexArray(object.mesh->vao);
            current_mesh = object.mesh;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, object_ubo);
        glBufferData(GL_UNIFORM_BUFFER, object.block->data_size, buffer + object.buffer_offset, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, object.block->binding, object_ubo);

        glDrawElements(current_mesh->mode, current_mesh->element_count, current_mesh->index_type, 0);
    }   
}