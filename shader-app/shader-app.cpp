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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

void render_geometry(const geometry_mesh & mesh)
{
    for(GLuint i=0; i<5; ++i) glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->position);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->normal);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->tangent);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->bitangent);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(geometry_vertex), &mesh.vertices.data()->texcoords);
    glDrawElements(GL_TRIANGLES, mesh.triangles.size()*3, GL_UNSIGNED_INT, mesh.triangles.data());
    for(GLuint i=0; i<5; ++i) glDisableVertexAttribArray(i);
}

struct scene_object
{
    std::string name;
    const geometry_mesh * mesh;
    float3 position, diffuse;
};

GLuint load_texture(const char * filename)
{
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

struct draw_list
{
    struct object { const geometry_mesh * mesh; GLuint program; const uniform_block_desc * block; size_t buffer_offset; };
    std::vector<byte> buffer;
    std::vector<object> objects;

    void begin_object(const geometry_mesh * mesh, GLuint program, const uniform_block_desc * block)
    {
        objects.push_back({mesh, program, block, buffer.size()});
        buffer.resize(buffer.size() + block->data_size);
    }

    template<class T> void set_uniform(const char * name, const T & value)
    {
        const auto & object = objects.back();
        object.block->set_uniform(buffer.data() + object.buffer_offset, name, value);
    }

    void draw(GLuint ubo)
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        GLuint current_program = 0;
        for(auto & object : objects)
        {   
            if(object.program != current_program)
            {
                glUseProgram(object.program);
                // TODO: Bind textures as appropriate
            }

            glBindBuffer(GL_UNIFORM_BUFFER, ubo);
            glBufferData(GL_UNIFORM_BUFFER, object.block->data_size, buffer.data() + object.buffer_offset, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, object.block->binding, ubo);
            render_geometry(*object.mesh);
        }        
    }
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
    std::vector<scene_object> objects = {
        {"Ground", &ground, {0,0,0}, {0.8,0.8,0.8}},
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

    auto per_scene = get_uniform_block_description(program, "PerScene");
    auto per_object = get_uniform_block_description(program, "PerObject");

    GLuint diffuse_tex = load_texture("pattern_191_diffuse.png");
    GLuint normal_tex = load_texture("pattern_191_normal.png");
    
    GLuint ubos[2];
    glGenBuffers(2, ubos);

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

        draw_list list;
        for(auto & obj : objects)
        {   
            const float4x4 model = translation_matrix(obj.position);
            list.begin_object(obj.mesh, program, &per_object);
            list.set_uniform("u_model", model);
            list.set_uniform("u_modelIT", inverse(transpose(model)));
            list.set_uniform("u_diffuseMtl", obj.diffuse);
        }

        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        std::vector<byte> buffer(per_scene.data_size);
        per_scene.set_uniform(buffer.data(), "u_viewProj", cam.get_viewproj_matrix({0, 0, fw, fh}));
        per_scene.set_uniform(buffer.data(), "u_eyePos", cam.position);
        per_scene.set_uniform(buffer.data(), "u_lightDir", normalize(float3(0.2f,1,0.1f)));       
        glBindBuffer(GL_UNIFORM_BUFFER, ubos[0]);
        glBufferData(GL_UNIFORM_BUFFER, buffer.size(), buffer.data(), GL_STREAM_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, per_scene.binding, ubos[0]);

        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0 + 0);
        glBindTexture(GL_TEXTURE_2D, diffuse_tex);
        set_uniform(program, "u_diffuseTex", 0);
        glActiveTexture(GL_TEXTURE0 + 1);
        glBindTexture(GL_TEXTURE_2D, normal_tex);
        set_uniform(program, "u_normalTex", 1);

        list.draw(ubos[1]);

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