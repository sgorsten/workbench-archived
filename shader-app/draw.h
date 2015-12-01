// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef DRAW_H
#define DRAW_H

#include <cstdlib>
#include <initializer_list>
#include <GL\glew.h>

#include "linalg.h"
using namespace linalg::aliases;

void set_uniform(GLuint program, const char * name, int scalar);
void set_uniform(GLuint program, const char * name, float scalar);
void set_uniform(GLuint program, const char * name, const float2 & vec);
void set_uniform(GLuint program, const char * name, const float3 & vec);
void set_uniform(GLuint program, const char * name, const float4 & vec);
void set_uniform(GLuint program, const char * name, const float4x4 & mat);

GLuint compile_shader(GLenum type, const char * source);
GLuint link_program(std::initializer_list<GLuint> shaders);

#endif