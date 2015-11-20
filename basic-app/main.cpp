#include <cstdlib>
#define GLFW_INCLUDE_GLU
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")

namespace linalg
{
    template<class T, int M> struct vec;
    template<class T> struct vec<T,2>
    {
        T           x,y;
                    vec()                       : x(), y() {}
                    vec(T x, T y)               : x(x), y(y) {}
        explicit    vec(T s)                    : x(s), y(s) {}
        const T &   operator[] (int i) const    { return (&x)[i]; }
        T &         operator[] (int i)          { return (&x)[i]; }
    };
    template<class T> struct vec<T,3>
    {
        T           x,y,z;
                    vec()                       : x(), y(), z() {}
                    vec(T x, T y, T z)          : x(x), y(y), z(z) {}
        explicit    vec(T s)                    : x(s), y(s), z(s) {}
        const T &   operator[] (int i) const    { return (&x)[i]; }
        T &         operator[] (int i)          { return (&x)[i]; }
    };
    template<class T> struct vec<T,4>
    {
        T           x,y,z,w;
                    vec()                       : x(), y(), z(), w() {}
                    vec(T x, T y, T z, T w)     : x(x), y(y), z(z), w(w) {}
        explicit    vec(T s)                    : x(s), y(s), z(s), w(s) {}
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

    namespace aliases
    {
        typedef vec<int,2> int2; typedef vec<float,2> float2;
        typedef vec<int,3> int3; typedef vec<float,3> float3;
        typedef vec<int,4> int4; typedef vec<float,4> float4;
    }
}
using namespace linalg::aliases;

bool bf,bl,bb,br;

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

    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        const double t1 = glfwGetTime();
        float timestep = static_cast<float>(t1-t0);
        t0 = t1;

        float3 move;
        if(bf) move.z -= 1;
        if(bl) move.x -= 1;
        if(bb) move.z += 1;
        if(br) move.x += 1;
        cam_pos += move * (timestep * 4);

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
        gluLookAt(cam_pos.x, cam_pos.y, cam_pos.z, cam_pos.x, cam_pos.y, cam_pos.z-1, 0, 1, 0);

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