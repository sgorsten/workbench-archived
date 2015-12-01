// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw.h"
#include "input.h"
#include "ui.h"

#include <cassert>

#include <iostream>
#include <sstream>
#include <algorithm>
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

int main(int argc, char * argv[]) try
{
    camera cam = {};
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
    std::vector<input_event> events;
    install_input_callbacks(win, events);
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
    
    bool ml=0, mr=0, bf=0, bl=0, bb=0, br=0;
    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        events.clear();
        glfwPollEvents();
        for(const auto & e : events) switch(e.type)
        {
        case input::key_down: case input::key_up:            
            switch(e.key)
            {
            case GLFW_KEY_W: bf = e.is_down(); break;
            case GLFW_KEY_A: bl = e.is_down(); break;
            case GLFW_KEY_S: bb = e.is_down(); break;
            case GLFW_KEY_D: br = e.is_down(); break;
            }
            break;
        case input::mouse_down: case input::mouse_up:
            switch(e.button)
            {
            case GLFW_MOUSE_BUTTON_LEFT: ml = e.is_down(); break;
            case GLFW_MOUSE_BUTTON_RIGHT: mr = e.is_down(); break;
            }
            break;
        case input::cursor_motion:
            if(mr)
            {
                cam.yaw -= e.motion.x * 0.01f;
                cam.pitch -= e.motion.y * 0.01f;
            }
            break;
        }

        const double t1 = glfwGetTime();
        const float timestep = static_cast<float>(t1-t0);
        t0 = t1;

        if(mr)
        {
            const float4 orientation = cam.get_orientation();
            float3 move;
            if(bf) move -= qzdir(orientation);
            if(bl) move -= qxdir(orientation);
            if(bb) move += qzdir(orientation);
            if(br) move += qxdir(orientation);
            if(mag2(move) > 0) cam.position += normalize(move) * (timestep * 8);
        }

        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        glViewport(0, 0, fw, fh);
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