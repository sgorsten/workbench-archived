#include <linalg.h>
using namespace linalg::aliases;

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#pragma comment(lib, "opengl32.lib")

#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>

GLuint compile_shader(GLenum type, std::initializer_list<const char *> sources)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, sources.size(), const_cast<const char **>(sources.begin()), nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE)
    {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(length,' ');
        glGetShaderInfoLog(shader, log.size(), nullptr, &log[0]);
        glDeleteShader(shader);
        throw std::runtime_error("glCompileShader(...) failed: " + log);
    }

    return shader;
}

GLuint link_program(std::initializer_list<GLuint> shaders)
{
    GLuint program = glCreateProgram();
    for(auto s : shaders) glAttachShader(program, s);
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(status != GL_TRUE)
    {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(length,' ');
        glGetProgramInfoLog(program, log.size(), nullptr, &log[0]);
        glDeleteProgram(program);
        throw std::runtime_error("glLinkProgram(...) failed: " + log);
    }

    return program;
}
void set_uniform(GLuint program, const char * name, const float3 & value) { glProgramUniform3fv(program, glGetUniformLocation(program, name), 1, &value.x); }
void set_uniform(GLuint program, const char * name, const float4x4 & value) { glProgramUniformMatrix4fv(program, glGetUniformLocation(program, name), 1, GL_FALSE, &value.x.x); }
struct mesh { GLuint vao; GLenum mode; GLsizei count; GLenum type; };

GLuint make_beveled_box_vertex_shader()
{
    return compile_shader(GL_VERTEX_SHADER, {R"(#version 430
        uniform mat4 u_view_proj_matrix, u_model_matrix, u_normal_matrix;
        uniform vec3 u_part_offsets[2], u_part_scales[2];
        layout(location = 0) in vec3 v_normal;
        layout(location = 1) in int v_part;
        out vec3 position, normal;
        void main() 
        {
            vec3 offset = vec3(u_part_offsets[v_part&1].x, u_part_offsets[(v_part&2)>>1].y, u_part_offsets[(v_part&4)>>2].z);
            vec3 scale = vec3(u_part_scales[v_part&1].x, u_part_scales[(v_part&2)>>1].y, u_part_scales[(v_part&4)>>2].z);
            position = (u_model_matrix * vec4(v_normal*scale+offset,1)).xyz;
            normal = normalize((u_normal_matrix * vec4(v_normal/max(scale,0.001),0)).xyz);
            gl_Position = u_view_proj_matrix * vec4(position,1);                
        }
    )"});
}

// Produces a mesh with exactly (n+1)*(n+1)*24 vertices and 24*n*n + 24*n + 6 quads
mesh make_beveled_box_mesh(uint32_t n) 
{
    struct vertex { float3 normal; int part; bool operator == (const vertex & v) { return normal==v.normal && part==v.part; } };
    std::vector<vertex> vertices;
    std::vector<uint4> quads;

    // Generate vertices and quads for the eight corners
    const float fn = static_cast<float>(n);
    auto add_corner_grid = [&vertices, &quads, n, fn](const float3 & norm, const float3 & tan, const float3 & bitan, int part)
    {
        const uint32_t base = vertices.size();
        for(uint32_t i=0; i<=n; ++i) for(int j=0; j<=n; ++j)
        {
            const float3 normal = normalize(norm + tan*(i/fn) + bitan*(j/fn));
            vertices.push_back({normal, part});
        }
        for(uint32_t i=0; i<n; ++i) for(int j=0; j<n; ++j) quads.push_back(base + uint4((i+0)*(n+1)+j+0, (i+1)*(n+1)+j+0, (i+1)*(n+1)+j+1, (i+0)*(n+1)+j+1));    
    };
    auto make_part = [](int3 bits) { return bits.x<<0 | bits.y<<1 | bits.z<<2; };
    for(int x : {0,1}) for(int y : {0,1}) for(int z : {0,1})
    {
        add_corner_grid({1,0,0}, {0,1,0}, {0,0,1}, make_part({x,y,z}));
        add_corner_grid({0,1,0}, {0,0,1}, {1,0,0}, make_part({x,y,z}));
        add_corner_grid({0,0,1}, {1,0,0}, {0,1,0}, make_part({x,y,z}));
    } 

    // Generate quads for the six faces, using existing vertices
    auto find_vertex = [&vertices](const vertex & v) { return uint32_t(std::find(begin(vertices), end(vertices), v) - begin(vertices)); };
    const auto add_face = [&quads, find_vertex, make_part](const float3 & norm, const int3 & part, const int3 & tan, const int3 & bitan)
    {
        quads.push_back({
            find_vertex({norm, make_part(part)}),
            find_vertex({norm, make_part(part + tan)}),
            find_vertex({norm, make_part(part + tan + bitan)}),
            find_vertex({norm, make_part(part + bitan)})
        });
    };
    for(int x : {0,1}) add_face({1,0,0}, {x,0,0}, {0,1,0}, {0,0,1});
    for(int y : {0,1}) add_face({0,1,0}, {0,y,0}, {0,0,1}, {1,0,0});
    for(int z : {0,1}) add_face({0,0,1}, {0,0,z}, {1,0,0}, {0,1,0});

    // Generate quads for the twelve edges, using existing vertices
    const auto add_edge = [&quads, find_vertex, make_part, n, fn](const float3 & norm, const float3 & binorm, const int3 & part, const int3 & tan)
    {
        for(uint32_t i=0; i<n; ++i) quads.push_back({
            find_vertex({normalize(norm*(i/fn) + binorm), make_part(part)}),
            find_vertex({normalize(norm*((i+1)/fn) + binorm), make_part(part)}),
            find_vertex({normalize(norm*((i+1)/fn) + binorm), make_part(part + tan)}),
            find_vertex({normalize(norm*(i/fn) + binorm), make_part(part + tan)})
        });
        for(uint32_t i=0; i<n; ++i) quads.push_back({
            find_vertex({normalize(norm + binorm*(i/fn)), make_part(part)}),
            find_vertex({normalize(norm + binorm*(i/fn)), make_part(part + tan)}),
            find_vertex({normalize(norm + binorm*((i+1)/fn)), make_part(part + tan)}),
            find_vertex({normalize(norm + binorm*((i+1)/fn)), make_part(part)})
        });
    };
    for(int y : {0,1}) for(int z : {0,1}) add_edge({0,1,0}, {0,0,1}, {0,y,z}, {1,0,0});
    for(int z : {0,1}) for(int x : {0,1}) add_edge({0,0,1}, {1,0,0}, {x,0,z}, {0,1,0});
    for(int x : {0,1}) for(int y : {0,1}) add_edge({1,0,0}, {0,1,0}, {x,y,0}, {0,0,1});

    // Scale vertices in the negative bevels by -1, and invert quads to maintain consistent winding
    for(auto & v : vertices) v.normal *= float3((v.part&1) ? 1 : -1, (v.part&2) ? 1 : -1, (v.part&4) ? 1 : -1);
    for(auto & q : quads) if(!(vertices[q.x].part&1) ^ !(vertices[q.x].part&2) ^ !(vertices[q.x].part&4)) q = uint4(q.w,q.z,q.y,q.x);

    GLuint vao;
    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);

    GLuint buffers[2];
    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex)*vertices.size(), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const GLvoid *)offsetof(vertex, normal));
    glEnableVertexAttribArray(1); glVertexAttribIPointer(1, 1, GL_INT, sizeof(vertex), (const GLvoid *)offsetof(vertex, part));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint4)*quads.size(), quads.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    return {vao, GL_QUADS, quads.size()*4, GL_UNSIGNED_INT};
}

void draw_beveled_box(const mesh & box, GLuint program, const float4x4 & model_matrix, const float3 & neg_bevels, const float3 & half_dims, const float3 & pos_bevels)
{
    set_uniform(program, "u_model_matrix", model_matrix);
    set_uniform(program, "u_normal_matrix", inverse(transpose(model_matrix)));
    set_uniform(program, "u_part_offsets[0]", -half_dims);
    set_uniform(program, "u_part_offsets[1]", half_dims);
    set_uniform(program, "u_part_scales[0]", neg_bevels);
    set_uniform(program, "u_part_scales[1]", pos_bevels);
    glUseProgram(program);
    glBindVertexArray(box.vao);
    glDrawElements(box.mode, box.count, box.type, nullptr);
}

#include <iostream>

int main() try
{
    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Render App", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glewInit();

    auto shader = make_beveled_box_vertex_shader();
    auto phong_lighting = link_program({shader, compile_shader(GL_FRAGMENT_SHADER, {R"(#version 430
        uniform vec3 u_eye_position, u_light_direction;
        in vec3 position, normal;
        layout(location = 0) out vec4 f_color;
        void main() 
        { 
            vec3 normal_vec = normalize(normal);
            vec3 eye_vec = normalize(u_eye_position - position);

            vec3 light_vec = u_light_direction;
            float diffuse = max(dot(normal_vec, light_vec), 0);

            vec3 half_vec = normalize(eye_vec + u_light_direction);
            float specular = pow(max(dot(normal_vec, half_vec), 0), 256);                

            f_color = vec4(vec3(0.1+diffuse+specular),1);
        }
    )"})});
    auto show_normals = link_program({shader, compile_shader(GL_FRAGMENT_SHADER, {R"(#version 430
        uniform vec3 u_eye_position, u_light_direction;
        in vec3 position, normal;
        layout(location = 0) out vec4 f_color;
        void main() { f_color = vec4(normal,1); }
    )"})});

    auto box = make_beveled_box_mesh(4);

    std::cout << "Controls:" << std::endl;
    std::cout << "    WASD - Move camera" << std::endl;
    std::cout << "    Click and drag left mouse - Rotate camera" << std::endl;
    std::cout << "    Hold F - Show wireframe" << std::endl;
    std::cout << "    Hold N - Show normals" << std::endl;

    float2 last_cursor;
    float3 cam_position;
    float cam_yaw=0, cam_pitch=0;
    auto t0 = std::chrono::high_resolution_clock::now();
    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        // Determine timestep
        std::this_thread::sleep_until(t0 + std::chrono::milliseconds(5));
        const auto t1 = std::chrono::high_resolution_clock::now();
        const float timestep = std::chrono::duration<float>(t1 - t0).count();
        t0 = t1;

        // Mouselook
        double cx,cy;
        glfwGetCursorPos(win, &cx, &cy);
        const float2 cursor {(float)cx, (float)cy};
        if(glfwGetMouseButton(win,GLFW_MOUSE_BUTTON_LEFT))
        {
            cam_yaw   -= (cursor.x - last_cursor.x) * 0.01f;
            cam_pitch -= (cursor.y - last_cursor.y) * 0.01f; 
            cam_pitch = std::max(cam_pitch, -1.5f);
            cam_pitch = std::min(cam_pitch, +1.5f);
        }
        last_cursor = cursor;
        const float4 cam_orientation = qmul(rotation_quat(float3(0,1,0), cam_yaw), rotation_quat(float3(1,0,0), cam_pitch));
        
        // WASD
        float3 delta; 
        if(glfwGetKey(win,GLFW_KEY_W)) delta.z -= 1;
        if(glfwGetKey(win,GLFW_KEY_A)) delta.x -= 1;
        if(glfwGetKey(win,GLFW_KEY_S)) delta.z += 1;
        if(glfwGetKey(win,GLFW_KEY_D)) delta.x += 1;
        cam_position += qrot(cam_orientation, delta)  * (timestep * 4);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT, glfwGetKey(win, GLFW_KEY_F) ? GL_LINE : GL_FILL);
        auto prog = glfwGetKey(win, GLFW_KEY_N) ? show_normals : phong_lighting;
        set_uniform(prog, "u_view_proj_matrix", mul(linalg::perspective_matrix(1.0f, (float)1280/720, 0.1f, 100.f), rotation_matrix(qconj(cam_orientation)), translation_matrix(-cam_position)));
        set_uniform(prog, "u_light_direction", normalize(float3(0.2f,1.0f,0.5f)));
        set_uniform(prog, "u_eye_position", cam_position);
        draw_beveled_box(box, prog, translation_matrix(float3(-4, 0,-5)), float3(0.25f), float3(0,0.25f,0), float3(0.25f));
        draw_beveled_box(box, prog, translation_matrix(float3(-2, 0,-5)), float3(0.2f), float3(0.2f), float3(0.2f));
        draw_beveled_box(box, prog, translation_matrix(float3( 0, 0,-5)), float3(0.45f), float3(), float3(0.45f));
        draw_beveled_box(box, prog, translation_matrix(float3(+2, 0,-5)), float3(0.4f,0,0.4f), float3(0,0.45f,0), float3(0.4f,0,0.4f));
        draw_beveled_box(box, prog, translation_matrix(float3(+4,-0.1f,-5)), float3(0.3f,0.4f,0.3f), float3(), float3(0.3f,0.5f,0.3f));
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
