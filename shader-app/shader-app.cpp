// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw.h"
#include "input.h"
#include "ui.h"

#include <cassert>

#include <iostream>
#include <sstream>
#include <thread>
#include <algorithm>
#pragma comment(lib, "opengl32.lib")

const char * vert_shader_source = R"(#version 420
layout(shared, binding=1) uniform PerScene 
{
    mat4 u_viewProj; 
    vec3 u_eyePos;
    vec3 u_lightDir;
};

layout(binding=2) uniform PerObject
{
    mat4 u_model, u_modelIT;
    vec3 u_diffuseMtl;
};

layout(location = 0) in vec3 v_position; 
layout(location = 1) in vec3 v_normal; 
layout(location = 2) in vec3 v_tangent; 
layout(location = 3) in vec3 v_bitangent; 
layout(location = 4) in vec2 v_texCoord; 
out vec3 position, normal, tangent, bitangent;
out vec2 texCoord;
void main() 
{
    position = (u_model * vec4(v_position,1)).xyz;
    normal = (u_modelIT * vec4(v_normal,0)).xyz;
    tangent = (u_modelIT * vec4(v_tangent,0)).xyz;
    bitangent = (u_modelIT * vec4(v_bitangent,0)).xyz;
    texCoord = v_texCoord;
    gl_Position = u_viewProj * vec4(position,1);
})";

const char * frag_shader_source = R"(#version 420
layout(shared, binding=1) uniform PerScene 
{
    mat4 u_viewProj; 
    vec3 u_eyePos;
    vec3 u_lightDir;
};

layout(binding=2) uniform PerObject
{
    mat4 u_model, u_modelIT;
    vec3 u_diffuseMtl;
};

uniform sampler2D u_diffuseTex;
uniform sampler2D u_normalTex;
in vec3 position, normal, tangent, bitangent;
in vec2 texCoord;
void main() 
{ 
    vec3 tsNormal = texture2D(u_normalTex, texCoord).xyz * 2 - 1;
    vec3 normalVec = normalize(normalize(tangent) * tsNormal.x + normalize(bitangent) * -tsNormal.y + normalize(normal) * tsNormal.z);
    vec3 eyeVec = normalize(u_eyePos - position);
    vec3 halfVec = normalize(normalVec + eyeVec);

    vec3 diffuseMtl = texture2D(u_diffuseTex, texCoord).rgb * u_diffuseMtl;
    float diffuseLight = max(dot(u_lightDir, normalVec), 0);
    float specularLight = max(pow(dot(u_lightDir, halfVec), 1024), 0);
    gl_FragColor = vec4(diffuseMtl * (diffuseLight + 0.1f) + vec3(0.5) * specularLight, 1);
})";

std::shared_ptr<gfx::mesh> make_draw_mesh(std::shared_ptr<gfx::context> ctx, const geometry_mesh & mesh)
{
    const geometry_vertex * vertex = 0;
    auto m = create_mesh(ctx);
    gfx::set_vertices(*m, mesh.vertices.data(), mesh.vertices.size() * sizeof(geometry_vertex));
    gfx::set_attribute(*m, 0, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &vertex->position);
    gfx::set_attribute(*m, 1, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &vertex->normal);
    gfx::set_attribute(*m, 2, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &vertex->tangent);
    gfx::set_attribute(*m, 3, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &vertex->bitangent);
    gfx::set_attribute(*m, 4, 2, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &vertex->texcoords);
    gfx::set_indices(*m, GL_TRIANGLES, (const unsigned int *)mesh.triangles.data(), mesh.triangles.size() * 3);
    return m;
}

struct scene_object
{
    std::string name;
    const geometry_mesh * mesh;
    std::shared_ptr<const gfx::mesh> dmesh;
    float3 position, diffuse;
};

int main(int argc, char * argv[]) try
{
    camera cam = {};
    cam.yfov = 1.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 16.0f;
    cam.position = {0,1.5f,4};

    auto ground = make_box_geometry({-4,-0.1f,-4}, {4,0,4});
    generate_texcoords_cubic(ground, 0.5);
    const auto box = make_box_geometry({-0.4f,0.0f,-0.4f}, {0.4f,0.8f,0.4f});
    const auto cylinder = make_cylinder_geometry({0,1,0}, {0,0,0.4f}, {0.4f,0,0}, 24);

    auto ctx = gfx::create_context();
    auto vert_shader = compile_shader(ctx, GL_VERTEX_SHADER, vert_shader_source);
    auto frag_shader = compile_shader(ctx, GL_FRAGMENT_SHADER, frag_shader_source);
    auto program = gfx::link_program(ctx, {vert_shader, frag_shader});
    auto diffuse_tex = load_texture(ctx, "pattern_191_diffuse.png");
    auto normal_tex = load_texture(ctx, "pattern_191_normal.png");

    auto win = gfx::create_window(*ctx, {1280, 720}, "Shader App");
    std::vector<input_event> events;
    install_input_callbacks(win, events);

    const auto g_ground = make_draw_mesh(ctx, ground);
    const auto g_box = make_draw_mesh(ctx, box);
    const auto g_cylinder = make_draw_mesh(ctx, cylinder);

    std::vector<scene_object> objects = {
        {"Ground", &ground, g_ground, {0,0,0}, {0.8,0.8,0.8}},
        {"Box", &box, g_box, {-1,0,0}, {1,0.5f,0.5f}},
        {"Cylinder", &cylinder, g_cylinder, {0,0,0}, {0.5f,1,0.5f}},
        {"Box 2", &box, g_box, {+1,0,0}, {0.5f,0.5f,1}}
    };

    renderer the_renderer;

    bool ml=0, mr=0, bf=0, bl=0, bb=0, br=0;
    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        const double t1 = glfwGetTime();
        const float timestep = static_cast<float>(t1-t0);
        if(timestep < 1.0f/60)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(1000000 * (1.0f/60 - timestep))));
            continue;
        }
        t0 = t1;

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
        events.clear();

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

        int w, h;
        glfwGetWindowSize(win, &w, &h);
        const auto * per_scene = get_desc(*program).get_block_desc("PerScene");
        std::vector<byte> scene_buffer(per_scene->data_size);
        per_scene->set_uniform(scene_buffer.data(), "u_viewProj", cam.get_viewproj_matrix({0, 0, w, h}));
        per_scene->set_uniform(scene_buffer.data(), "u_eyePos", cam.position);
        per_scene->set_uniform(scene_buffer.data(), "u_lightDir", normalize(float3(0.2f,1,0.1f)));   

        draw_list list;
        for(auto & obj : objects)
        {   
            const float4x4 model = translation_matrix(obj.position);
            list.begin_object(obj.dmesh, program);
            list.set_uniform("u_model", model);
            list.set_uniform("u_modelIT", inverse(transpose(model)));
            list.set_uniform("u_diffuseMtl", obj.diffuse);
            list.set_sampler("u_diffuseTex", diffuse_tex);
            list.set_sampler("u_normalTex", normal_tex);
        }

        the_renderer.draw_scene(win, per_scene, scene_buffer.data(), list);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}