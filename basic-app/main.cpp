#include <cstdlib>
#define GLFW_INCLUDE_GLU
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

#include <cmath>

namespace linalg
{
    template<class T, int M> struct vec;
    template<class T> struct vec<T,2>
    {
        T           x,y;
                    vec()                       : x(), y() {}
                    vec(T x, T y)               : x(x), y(y) {}
        explicit    vec(T s)                    : vec(s, s) {}
        const T &   operator[] (int i) const    { return (&x)[i]; }
        T &         operator[] (int i)          { return (&x)[i]; }
    };
    template<class T> struct vec<T,3>
    {
        T           x,y,z;
                    vec()                       : x(), y(), z() {}
                    vec(T x, T y, T z)          : x(x), y(y), z(z) {}
                    vec(vec<T,2> xy, T z)       : vec(xy.x, xy.y, z) {}
        explicit    vec(T s)                    : vec(s, s, s) {}
        const T &   operator[] (int i) const    { return (&x)[i]; }
        T &         operator[] (int i)          { return (&x)[i]; }
    };
    template<class T> struct vec<T,4>
    {
        T           x,y,z,w;
                    vec()                       : x(), y(), z(), w() {}
                    vec(T x, T y, T z, T w)     : x(x), y(y), z(z), w(w) {}
                    vec(vec<T,3> xyz, T w)      : vec(xyz.x, xyz.y, xyz.z, w) {}
        explicit    vec(T s)                    : vec(s, s, s, s) {}
        const T &   operator[] (int i) const    { return (&x)[i]; }
        T &         operator[] (int i)          { return (&x)[i]; }
    };

    template<class T> bool operator == (const vec<T,2> & a, const vec<T,2> & b) { return a.x==b.x && a.y==b.y; }
    template<class T> bool operator == (const vec<T,3> & a, const vec<T,3> & b) { return a.x==b.x && a.y==b.y && a.z==b.z; }
    template<class T> bool operator == (const vec<T,4> & a, const vec<T,4> & b) { return a.x==b.x && a.y==b.y && a.z==b.z && a.w==b.w; }
    template<class T, int N> bool operator != (const vec<T,N> & a, const vec<T,N> & b) { return !(a == b); }

    template<class T, class F> vec<T,2> componentwise_apply(const vec<T,2> & a, const vec<T,2> & b, F f) { return {f(a.x,b.x), f(a.y,b.y)}; }
    template<class T, class F> vec<T,3> componentwise_apply(const vec<T,3> & a, const vec<T,3> & b, F f) { return {f(a.x,b.x), f(a.y,b.y), f(a.z,b.z)}; }
    template<class T, class F> vec<T,4> componentwise_apply(const vec<T,4> & a, const vec<T,4> & b, F f) { return {f(a.x,b.x), f(a.y,b.y), f(a.z,b.z), f(a.w,b.w)}; }

    template<class T, int N> vec<T,N> operator + (const vec<T,N> & a) { return componentwise_apply(a, a, [](float l, float) { return +l; }); }
    template<class T, int N> vec<T,N> operator - (const vec<T,N> & a) { return componentwise_apply(a, a, [](float l, float) { return -l; }); }

    template<class T, int N> vec<T,N> operator + (const vec<T,N> & a, const vec<T,N> & b) { return componentwise_apply(a, b, [](float l, float r) { return l + r; }); }
    template<class T, int N> vec<T,N> operator - (const vec<T,N> & a, const vec<T,N> & b) { return componentwise_apply(a, b, [](float l, float r) { return l - r; }); }
    template<class T, int N> vec<T,N> operator * (const vec<T,N> & a, const vec<T,N> & b) { return componentwise_apply(a, b, [](float l, float r) { return l * r; }); }
    template<class T, int N> vec<T,N> operator / (const vec<T,N> & a, const vec<T,N> & b) { return componentwise_apply(a, b, [](float l, float r) { return l / r; }); }

    template<class T, int N> vec<T,N> operator + (const vec<T,N> & a, T b) { return a + vec<T,N>(b); }
    template<class T, int N> vec<T,N> operator - (const vec<T,N> & a, T b) { return a - vec<T,N>(b); }
    template<class T, int N> vec<T,N> operator * (const vec<T,N> & a, T b) { return a * vec<T,N>(b); }
    template<class T, int N> vec<T,N> operator / (const vec<T,N> & a, T b) { return a / vec<T,N>(b); }

    template<class T, int N> vec<T,N> operator + (T a, const vec<T,N> & b) { return vec<T,N>(a) + b; }
    template<class T, int N> vec<T,N> operator - (T a, const vec<T,N> & b) { return vec<T,N>(a) - b; }
    template<class T, int N> vec<T,N> operator * (T a, const vec<T,N> & b) { return vec<T,N>(a) * b; }
    template<class T, int N> vec<T,N> operator / (T a, const vec<T,N> & b) { return vec<T,N>(a) / b; }

    template<class T, int N> vec<T,N> & operator += (vec<T,N> & a, const vec<T,N> & b) { return a = a + b; }
    template<class T, int N> vec<T,N> & operator -= (vec<T,N> & a, const vec<T,N> & b) { return a = a - b; }
    template<class T, int N> vec<T,N> & operator *= (vec<T,N> & a, const vec<T,N> & b) { return a = a * b; }
    template<class T, int N> vec<T,N> & operator /= (vec<T,N> & a, const vec<T,N> & b) { return a = a / b; }

    template<class T, int N> vec<T,N> & operator += (vec<T,N> & a, T b) { return a += b; }
    template<class T, int N> vec<T,N> & operator -= (vec<T,N> & a, T b) { return a -= b; }
    template<class T, int N> vec<T,N> & operator *= (vec<T,N> & a, T b) { return a *= b; }
    template<class T, int N> vec<T,N> & operator /= (vec<T,N> & a, T b) { return a /= b; }

    template<class T> vec<T,4> qconj (const vec<T,4> & q)                     { return {-q.x,-q.y,-q.z,q.w}; }
    template<class T> vec<T,4> qmul  (const vec<T,4> & a, const vec<T,4> & b) { return {a.x*b.w+a.w*b.x+a.y*b.z-a.z*b.y, a.y*b.w+a.w*b.y+a.z*b.x-a.x*b.z, a.z*b.w+a.w*b.z+a.x*b.y-a.y*b.x, a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}; }
    template<class T> vec<T,3> qxdir (const vec<T,4> & q)                     { return {q.w*q.w+q.x*q.x-q.y*q.y-q.z*q.z, (q.x*q.y+q.z*q.w)*2, (q.z*q.x-q.y*q.w)*2}; }
    template<class T> vec<T,3> qydir (const vec<T,4> & q)                     { return {(q.x*q.y-q.z*q.w)*2, q.w*q.w-q.x*q.x+q.y*q.y-q.z*q.z, (q.y*q.z+q.x*q.w)*2}; }
    template<class T> vec<T,3> qzdir (const vec<T,4> & q)                     { return {(q.z*q.x+q.y*q.w)*2, (q.y*q.z-q.x*q.w)*2, q.w*q.w-q.x*q.x-q.y*q.y+q.z*q.z}; }
    template<class T> vec<T,3> qrot  (const vec<T,4> & q, const vec<T,3> & v) { return qxdir(q)*v.x + qydir(q)*v.y + qzdir(q)*v.z; }

    namespace aliases
    {
        typedef vec<int,2> int2; typedef vec<float,2> float2;
        typedef vec<int,3> int3; typedef vec<float,3> float3;
        typedef vec<int,4> int4; typedef vec<float,4> float4;
    }
}
using namespace linalg::aliases;

float4 rotation_axis_angle(const float3 & axis, float angle) { return {axis*std::sin(angle/2), std::cos(angle/2)}; }

float2 last_cursor;
bool bf, bl, bb, br, ml;
float pitch, yaw;

int main(int argc, char * argv[])
{
    float3 cam_pos = {0,0,4};

    glfwInit();
    auto win = glfwCreateWindow(1280, 720, "Basic Workbench App", nullptr, nullptr);
    glfwSetKeyCallback(win, [](GLFWwindow *, int key, int scancode, int action, int mods)
    {
        switch(key)
        {
        case GLFW_KEY_W: bf = action != GLFW_RELEASE; break;
        case GLFW_KEY_A: bl = action != GLFW_RELEASE; break;
        case GLFW_KEY_S: bb = action != GLFW_RELEASE; break;
        case GLFW_KEY_D: br = action != GLFW_RELEASE; break;
        }
    });
    glfwSetMouseButtonCallback(win, [](GLFWwindow *, int button, int action, int mods)
    {
        ml = action != GLFW_RELEASE;
    });
    glfwSetCursorPosCallback(win, [](GLFWwindow *, double x, double y)
    {
        const float2 cursor = {static_cast<float>(x), static_cast<float>(y)};
        if(ml)
        {
            yaw -= (cursor.x - last_cursor.x) * 0.01f;
            pitch -= (cursor.y - last_cursor.y) * 0.01f;
        }
        last_cursor = cursor;
    });

    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        const double t1 = glfwGetTime();
        float timestep = static_cast<float>(t1-t0);
        t0 = t1;

        float4 cam_orientation = linalg::qmul(rotation_axis_angle({0,1,0}, yaw), rotation_axis_angle({1,0,0}, pitch));

        float3 move;
        if(bf) move.z -= 1;
        if(bl) move.x -= 1;
        if(bb) move.z += 1;
        if(br) move.x += 1;
        cam_pos += qrot(cam_orientation, move) * (timestep * 4);

        int w,h;
        glfwGetFramebufferSize(win, &w, &h);
        glfwMakeContextCurrent(win);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        gluPerspective(60, (double)w/h, 0.1f, 16.0f);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        float3 c = cam_pos - qzdir(cam_orientation);
        gluLookAt(cam_pos.x, cam_pos.y, cam_pos.z, c.x, c.y, c.z, 0, 1, 0);

        glBegin(GL_TRIANGLES);
        glColor3f(1,0,0); glVertex3f(-0.5f,-0.5f,0);
        glColor3f(0,1,0); glVertex3f(+0.5f,-0.5f,0);
        glColor3f(0,0,1); glVertex3f( 0.0f,+0.5f,0);
        glEnd();

        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}