// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw.h"

#include <string>

void set_uniform(GLuint program, const char * name, int scalar) { glUniform1i(glGetUniformLocation(program, name), scalar); }
void set_uniform(GLuint program, const char * name, float scalar) { glUniform1f(glGetUniformLocation(program, name), scalar); }
void set_uniform(GLuint program, const char * name, const float2 & vec) { glUniform2fv(glGetUniformLocation(program, name), 1, &vec.x); }
void set_uniform(GLuint program, const char * name, const float3 & vec) { glUniform3fv(glGetUniformLocation(program, name), 1, &vec.x); }
void set_uniform(GLuint program, const char * name, const float4 & vec) { glUniform4fv(glGetUniformLocation(program, name), 1, &vec.x); }
void set_uniform(GLuint program, const char * name, const float4x4 & mat) { glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, &mat.x.x); }

GLuint compile_shader(GLenum type, const char * source)
{
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

GLuint link_program(std::initializer_list<GLuint> shaders)
{
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