// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include <cstdlib>
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")

#include <cmath>

namespace linalg
{
    template<class T, int M> struct vec;
    template<class T> struct vec<T,2>
    {
        T                   x,y;
                            vec()                           : x(), y() {}
                            vec(T x, T y)                   : x(x), y(y) {}
        explicit            vec(T s)                        : vec(s, s) {}
        const T &           operator[] (int i) const        { return (&x)[i]; }
        T &                 operator[] (int i)              { return (&x)[i]; }
    };
    template<class T> struct vec<T,3>
    {
        T                   x,y,z;
                            vec()                           : x(), y(), z() {}
                            vec(T x, T y, T z)              : x(x), y(y), z(z) {}
                            vec(vec<T,2> xy, T z)           : vec(xy.x, xy.y, z) {}
        explicit            vec(T s)                        : vec(s, s, s) {}
        const T &           operator[] (int i) const        { return (&x)[i]; }
        T &                 operator[] (int i)              { return (&x)[i]; }
    };
    template<class T> struct vec<T,4>
    {
        T                   x,y,z,w;
                            vec()                           : x(), y(), z(), w() {}
                            vec(T x, T y, T z, T w)         : x(x), y(y), z(z), w(w) {}
                            vec(vec<T,3> xyz, T w)          : vec(xyz.x, xyz.y, xyz.z, w) {}
        explicit            vec(T s)                        : vec(s, s, s, s) {}
        const T &           operator[] (int i) const        { return (&x)[i]; }
        T &                 operator[] (int i)              { return (&x)[i]; }
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

    template<class T, int M, int N> struct mat;
    template<class T, int M> struct mat<T,M,2>
    {
        typedef vec<T,M>    C;
        C                   x,y;
                            mat()                           : x(), y() {}
                            mat(C x, C y)                   : x(x), y(y) {}
        explicit            mat(T s)                        : x(s), y(s) {}
        const T &           operator() (int i, int j) const { return (&x)[j][i]; }
        T &                 operator() (int i, int j)       { return (&x)[j][i]; }
    };
    template<class T, int M> struct mat<T,M,3>
    {
        typedef vec<T,M>    C;
        C                   x,y,z;
                            mat()                           : x(), y(), z() {}
                            mat(C x, C y, C z)              : x(x), y(y), z(z) {}
        explicit            mat(T s)                        : x(s), y(s), z(s) {}
        const T &           operator() (int i, int j) const { return (&x)[j][i]; }
        T &                 operator() (int i, int j)       { return (&x)[j][i]; }
    };
    template<class T, int M> struct mat<T,M,4>
    {
        typedef vec<T,M>    C;
        C                   x,y,z,w;
                            mat()                           : x(), y(), z(), w() {}
                            mat(C x, C y, C z, C w)         : x(x), y(y), z(z), w(w) {}
        explicit            mat(T s)                        : x(s), y(s), z(s), w(s) {}
        const T &           operator() (int i, int j) const { return (&x)[j][i]; }
        T &                 operator() (int i, int j)       { return (&x)[j][i]; }
    };

    template<class T, int M> vec<T,M> operator * (const mat<T,M,2> & a, const vec<T,2> & b) { return a.x*b.x + a.y*b.y; }
    template<class T, int M> vec<T,M> operator * (const mat<T,M,3> & a, const vec<T,3> & b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    template<class T, int M> vec<T,M> operator * (const mat<T,M,4> & a, const vec<T,4> & b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

    template<class T, int M, int N> mat<T,M,2> operator * (const mat<T,M,N> & a, const mat<T,N,2> & b) { return {a*b.x, a*b.y}; }
    template<class T, int M, int N> mat<T,M,3> operator * (const mat<T,M,N> & a, const mat<T,N,3> & b) { return {a*b.x, a*b.y, a*b.z}; }
    template<class T, int M, int N> mat<T,M,4> operator * (const mat<T,M,N> & a, const mat<T,N,4> & b) { return {a*b.x, a*b.y, a*b.z, a*b.w}; }

    namespace aliases
    {
        typedef vec<int,2> int2; typedef vec<unsigned,2> uint2; typedef vec<float,2> float2; typedef vec<double,2> double2;
        typedef vec<int,3> int3; typedef vec<unsigned,3> uint3; typedef vec<float,3> float3; typedef vec<double,3> double3;
        typedef vec<int,4> int4; typedef vec<unsigned,4> uint4; typedef vec<float,4> float4; typedef vec<double,4> double4;
        typedef mat<float,2,2> float2x2; typedef mat<float,3,2> float3x2; typedef mat<float,4,2> float4x2;
        typedef mat<float,2,3> float2x3; typedef mat<float,3,3> float3x3; typedef mat<float,4,3> float4x3;
        typedef mat<float,2,4> float2x4; typedef mat<float,3,4> float3x4; typedef mat<float,4,4> float4x4;
    }
}
using namespace linalg::aliases;

float4 rotation_quat(const float3 & axis, float angle)                          { return {axis*std::sin(angle/2), std::cos(angle/2)}; }
float4x4 translation_matrix(const float3 & translation)                         { return {{1,0,0,0},{0,1,0,0},{0,0,1,0},{translation,1}}; }
float4x4 rotation_matrix(const float4 & rotation)                               { return {{qxdir(rotation),0}, {qydir(rotation),0}, {qzdir(rotation),0}, {0,0,0,1}}; }
float4x4 frustum_matrix(float l, float r, float b, float t, float n, float f)   { return {{2*n/(r-l),0,0,0}, {0,2*n/(t-b),0,0}, {(r+l)/(r-l),(t+b)/(t-b),-(f+n)/(f-n),-1}, {0,0,-2*f*n/(f-n),0}}; }
float4x4 perspective_matrix(float fovy, float aspect, float n, float f)         { float y = n*std::tan(fovy/2), x=y*aspect; return frustum_matrix(-x,x,-y,y,n,f); }

float2 last_cursor;
bool bf, bl, bb, br, ml;
float pitch, yaw;

void gl_load_matrix(const float4x4 & m) { glLoadMatrixf(&m.x.x); }

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

        float4 cam_orientation = linalg::qmul(rotation_quat({0,1,0}, yaw), rotation_quat({1,0,0}, pitch));

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
        gl_load_matrix(perspective_matrix(1.0f, (float)w/h, 0.1f, 16.0f));
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        gl_load_matrix(rotation_matrix(qconj(cam_orientation)) * translation_matrix(-cam_pos));

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