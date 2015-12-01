// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include <cstdlib>
#include <GL\glew.h>

#include "ui.h"

#include <cassert>

#include <iostream>
#include <sstream>
#include <algorithm>
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")

const char * vert_shader_source = R"(#version 330
uniform mat4 u_viewProj; 
uniform mat4 u_model, u_modelIT;
layout(location = 0) in vec3 v_position; 
layout(location = 1) in vec3 v_normal; 
layout(location = 2) in vec2 v_texCoord; 
out vec3 position, normal;
out vec2 texCoord;
void main() 
{
    position = (u_model * vec4(v_position,1)).xyz;
    normal = (u_modelIT * vec4(v_normal,0)).xyz;
    texCoord = v_texCoord;
    gl_Position = u_viewProj * vec4(position,1);
})";

const char * frag_shader_source = R"(#version 330
uniform vec3 u_lightDir;
uniform sampler2D u_diffuse;
in vec3 position, normal;
in vec2 texCoord;
void main() 
{ 
    vec4 diffuseMtl = texture2D(u_diffuse, texCoord);
    float diffuseLight = max(dot(normal, u_lightDir), 0);
    gl_FragColor = diffuseMtl * diffuseLight;
})";

void render_geometry(const geometry_mesh & mesh)
{
    for(GLuint i=0; i<3; ++i) glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->position);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->normal);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->texcoords);
    glDrawElements(GL_TRIANGLES, mesh.triangles.size()*3, GL_UNSIGNED_INT, mesh.triangles.data());
    for(GLuint i=0; i<3; ++i) glDisableVertexAttribArray(i);
}

struct scene_object
{
    std::string name;
    const geometry_mesh * mesh;
    float3 position, diffuse;
};

camera cam;
bool ml, mr, bf, bl, bb, br;
float2 cursor, delta;

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

int main(int argc, char * argv[]) try
{
    cam.yfov = 1.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 16.0f;
    cam.position = {0,1.5f,4};

    const auto box = make_box_geometry({-0.4f,0.0f,-0.4f}, {0.4f,0.8f,0.4f});
    const auto cylinder = make_cylinder_geometry({0,1,0}, {0,0,0.4f}, {0.4f,0,0}, 24);
    std::vector<scene_object> objects = {
        {"Box", &box, {-1,0,0}, {1,0.5f,0.5f}},
        {"Cylinder", &cylinder, {0,0,0}, {0.5f,1,0.5f}},
        {"Box 2", &box, {+1,0,0}, {0.5f,0.5f,1}}
    };
    
    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Shader App", nullptr, nullptr);
    glfwSetKeyCallback(win, [](GLFWwindow * win, int key, int scancode, int action, int mods)
    {
        switch(key)
        {
        case GLFW_KEY_W: bf = action != GLFW_RELEASE; break;
        case GLFW_KEY_A: bl = action != GLFW_RELEASE; break;
        case GLFW_KEY_S: bb = action != GLFW_RELEASE; break;
        case GLFW_KEY_D: br = action != GLFW_RELEASE; break;
        }
    });
    glfwSetMouseButtonCallback(win, [](GLFWwindow * win, int button, int action, int mods)
    {
        switch(button)
        {
        case GLFW_MOUSE_BUTTON_LEFT: ml = action != GLFW_RELEASE; break;
        case GLFW_MOUSE_BUTTON_RIGHT: mr = action != GLFW_RELEASE; break;
        }
    });
    glfwSetCursorPosCallback(win, [](GLFWwindow * win, double x, double y)
    {
        const float2 new_cursor = float2(double2(x,y));
        delta = new_cursor - cursor;
        cursor = new_cursor;
    });

    glfwMakeContextCurrent(win);

    glewInit();

    GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert_shader_source);
    GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_shader_source);
    GLuint program = link_program({vert_shader, frag_shader});

    float image[16][16];
    for(int i=0; i<16; ++i)
    {
        for(int j=0; j<16; ++j)
        {
            image[i][j] = ((i/4 + j/4) % 2) 
                ? ((i+j) % 2 ? 0.8f : 0.6f)
                : ((i+j) % 2 ? 0.2f : 0.1f);
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_LUMINANCE, GL_FLOAT, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    int split1 = 1080, split2 = 358, offset0 = 0, offset1 = 0;
    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        delta = {0,0};
        glfwPollEvents();

        int fw, fh, w, h;
        glfwGetFramebufferSize(win, &fw, &fh);
        glfwGetWindowSize(win, &w, &h);

        const double t1 = glfwGetTime();
        const float timestep = static_cast<float>(t1-t0);
        t0 = t1;

        if(mr)
        {
            cam.yaw -= delta.x * 0.01f;
            cam.pitch -= delta.y * 0.01f;

            const float4 orientation = cam.get_orientation();
            float3 move;
            if(bf) move -= qzdir(orientation);
            if(bl) move -= qxdir(orientation);
            if(bb) move += qzdir(orientation);
            if(br) move += qxdir(orientation);
            if(mag2(move) > 0) cam.position += normalize(move) * (timestep * 8);
        }

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glViewport(0, 0, fw, fh);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        set_uniform(program, "u_viewProj", cam.get_viewproj_matrix({0, 0, fw, fh}));    
        set_uniform(program, "u_lightDir", normalize(float3(0.3f,1,0.2f)));

        glEnable(GL_DEPTH_TEST);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_CULL_FACE);
        for(auto & obj : objects)
        {   
            const float4x4 model = translation_matrix(obj.position);
            set_uniform(program, "u_model", model);
            set_uniform(program, "u_modelIT", inverse(transpose(model)));
            render_geometry(*obj.mesh);
        }

        glPopAttrib();

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}