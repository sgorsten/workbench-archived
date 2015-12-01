// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui.h"

#include <cassert>
#include <cstdlib>
#include <set>
#include <sstream>
#include <algorithm>
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")

void gl_load_matrix(const float4x4 & m) { glLoadMatrixf(&m.x.x); }
void gl_color(const float3 & v) { glColor3fv(begin(v)); }

void render_geometry(const geometry_mesh & mesh)
{
    const GLenum states[] = {GL_VERTEX_ARRAY, GL_NORMAL_ARRAY, GL_TEXTURE_COORD_ARRAY};
    for(auto s : states) glEnableClientState(s);
    glVertexPointer(3, GL_FLOAT, sizeof(geometry_vertex), &mesh.vertices.data()->position);
    glNormalPointer(GL_FLOAT, sizeof(geometry_vertex), &mesh.vertices.data()->normal);
    glTexCoordPointer(2, GL_FLOAT, sizeof(geometry_vertex), &mesh.vertices.data()->texcoords);
    glDrawElements(GL_TRIANGLES, mesh.triangles.size()*3, GL_UNSIGNED_INT, mesh.triangles.data());
    for(auto s : states) glDisableClientState(s);
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

int main(int argc, char * argv[])
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
    std::set<scene_object *> selection;
    
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

        glMatrixMode(GL_PROJECTION);
        gl_load_matrix(cam.get_projection_matrix({0, 0, fw, fh}));

        glMatrixMode(GL_MODELVIEW);
        gl_load_matrix(cam.get_view_matrix());
        glEnable(GL_DEPTH_TEST);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glLightfv(GL_LIGHT0, GL_POSITION, begin(normalize(float4(0.1f, 0.9f, 0.3f, 0))));
        glEnable(GL_COLOR_MATERIAL);
        glEnable(GL_CULL_FACE);
        for(auto & obj : objects)
        {            
            gl_load_matrix(cam.get_view_matrix() * translation_matrix(obj.position));
            gl_color(obj.diffuse); render_geometry(*obj.mesh);
        }

        glPopAttrib();

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}