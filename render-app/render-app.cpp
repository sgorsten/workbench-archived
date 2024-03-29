#include <linalg.h>
using namespace linalg::aliases;

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#pragma comment(lib, "opengl32.lib")

#include <string>
#include <chrono>
#include <thread>

GLuint compile_shader(GLenum type, std::initializer_list<const char *> sources)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, (GLsizei)sources.size(), const_cast<const char **>(sources.begin()), nullptr);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE)
    {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(length,' ');
        glGetShaderInfoLog(shader, length, nullptr, &log[0]);
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
        glGetProgramInfoLog(program, length, nullptr, &log[0]);
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

mesh make_beveled_box_mesh(int n) 
{
    GLuint buffers[2];
    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    struct vertex { float3 normal; int part; vertex(float3 normal, float3 corner) : normal(normal), part((corner.x>0)<<0 | (corner.y>0)<<1 | (corner.z>0)<<2) {} };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex)*(n+1)*(n+1)*24, nullptr, GL_STATIC_DRAW);
    auto vertices = reinterpret_cast<vertex *>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
    for(auto & s : std::initializer_list<float3x2>{{{1,0,0},{0,1,0}}, {{-1,0,0},{0,1,0}}, {{0,1,0},{0,0,1}}, {{0,-1,0},{0,0,1}}, {{0,0,1},{1,0,0}}, {{0,0,-1},{1,0,0}}})
    {
        for(int i=0; i<=n; ++i)
        {
            for(int j=0; j<=n; ++j) *vertices++ = {normalize(cross(s.x,s.y) - s.x*((n-j)/float(n)) - s.y*((n-i)/float(n))), cross(s.x,s.y)-s.x-s.y};
            for(int j=0; j<=n; ++j) *vertices++ = {normalize(cross(s.x,s.y) + s.x*((0+j)/float(n)) - s.y*((n-i)/float(n))), cross(s.x,s.y)+s.x-s.y};
        }
        for(int i=0; i<=n; ++i)
        {
            for(int j=0; j<=n; ++j) *vertices++ = {normalize(cross(s.x,s.y) - s.x*((n-j)/float(n)) + s.y*((0+i)/float(n))), cross(s.x,s.y)-s.x+s.y};
            for(int j=0; j<=n; ++j) *vertices++ = {normalize(cross(s.x,s.y) + s.x*((0+j)/float(n)) + s.y*((0+i)/float(n))), cross(s.x,s.y)+s.x+s.y};
        }
    };
    glUnmapBuffer(GL_ARRAY_BUFFER);

    GLuint vao;
    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (const GLvoid *)offsetof(vertex, normal));
    glEnableVertexAttribArray(1); glVertexAttribIPointer(1, 1, GL_INT, sizeof(vertex), (const GLvoid *)offsetof(vertex, part));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint4)*(n*2+1)*(n*2+1)*6, nullptr, GL_STATIC_DRAW);
    auto quads = reinterpret_cast<uint4 *>(glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY));
    for(uint32_t k=0, m=2*(n+1); k<6; ++k) for(uint32_t i=1; i<m; ++i) for(uint32_t j=1; j<m; ++j) *quads++ = uint4((i-1)*m+j-1, (i-1)*m+j, i*m+j, i*m+j-1) + k*m*m;
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return {vao, GL_QUADS, (GLsizei)(n*2+1)*(n*2+1)*24, GL_UNSIGNED_INT};
}

void draw_beveled_box(const mesh & box, GLuint program, const float4x4 & model_matrix, const float3 & neg_bevels, const float3 & half_inner, const float3 & pos_bevels)
{
    set_uniform(program, "u_model_matrix", model_matrix);
    set_uniform(program, "u_normal_matrix", inverse(transpose(model_matrix)));
    set_uniform(program, "u_part_offsets[0]", -half_inner);
    set_uniform(program, "u_part_offsets[1]", half_inner);
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
        draw_beveled_box(box, prog, translation_matrix(float3(-3.3f, 0,-5)),    float3(0.4f,0.4f,0), float3(0,0,0.05f), float3(0.4f,0.4f,0));
        draw_beveled_box(box, prog, translation_matrix(float3(-2.2f, 0,-5)),    float3(0.25f), float3(0,0.25f,0), float3(0.25f));
        draw_beveled_box(box, prog, translation_matrix(float3(-1.1f, 0,-5)),    float3(0.2f), float3(0.2f), float3(0.2f));
        draw_beveled_box(box, prog, translation_matrix(float3( 0.0f, 0,-5)),    float3(0.45f), float3(), float3(0.45f));
        draw_beveled_box(box, prog, translation_matrix(float3(+1.1f, 0,-5)),    float3(0.4f,0,0.4f), float3(0,0.45f,0), float3(0.4f,0,0.4f));
        draw_beveled_box(box, prog, translation_matrix(float3(+2.2f,-0.1f,-5)), float3(0.3f,0.4f,0.3f), float3(), float3(0.3f,0.5f,0.3f));
        draw_beveled_box(box, prog, translation_matrix(float3(+3.3f, 0,-5)),    float3(), float3(0.35f), float3());
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
