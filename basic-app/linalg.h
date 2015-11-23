// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef LINALG_H
#define LINALG_H

#include <cmath>
#include <cstdint>

namespace linalg
{
    template<class T, int M> struct vec;
    template<class T> struct vec<T,2>
    {
        T                           x,y;
                                    vec()                       : x(), y() {}
                                    vec(T x, T y)               : x(x), y(y) {}
        explicit                    vec(T s)                    : vec(s, s) {}
        template<class U> explicit  vec(vec<U,2> & v)           : vec(static_cast<T>(v.x), static_cast<T>(v.y)) {}
        const T &                   operator[] (int i) const    { return (&x)[i]; }
        T &                         operator[] (int i)          { return (&x)[i]; }
    };
    template<class T> struct vec<T,3>
    {
        T                           x,y,z;
                                    vec()                       : x(), y(), z() {}
                                    vec(T x, T y, T z)          : x(x), y(y), z(z) {}
                                    vec(vec<T,2> xy, T z)       : vec(xy.x, xy.y, z) {}
        explicit                    vec(T s)                    : vec(s, s, s) {}
        template<class U> explicit  vec(vec<U,3> & v)           : vec(static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z)) {}
        const vec<T,2> &            xy() const                  { return reinterpret_cast<const vec<T,2> &>(x); }
        const T &                   operator[] (int i) const    { return (&x)[i]; }
        T &                         operator[] (int i)          { return (&x)[i]; }
    };
    template<class T> struct vec<T,4>
    {
        T                           x,y,z,w;
                                    vec()                       : x(), y(), z(), w() {}
                                    vec(T x, T y, T z, T w)     : x(x), y(y), z(z), w(w) {}
                                    vec(vec<T,3> xyz, T w)      : vec(xyz.x, xyz.y, xyz.z, w) {}
        explicit                    vec(T s)                    : vec(s, s, s, s) {}
        template<class U> explicit  vec(vec<U,4> & v)           : vec(static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z), static_cast<T>(v.w)) {}
        const vec<T,3> &            xyz() const                 { return reinterpret_cast<const vec<T,3> &>(x); }
        const T &                   operator[] (int i) const    { return (&x)[i]; }
        T &                         operator[] (int i)          { return (&x)[i]; }
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

    template<class T>        vec<T,3> cross     (const vec<T,3> & a, const vec<T,3> & b)      { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
    template<class T>        T        dot       (const vec<T,2> & a, const vec<T,2> & b)      { return a.x*b.x + a.y*b.y; }
    template<class T>        T        dot       (const vec<T,3> & a, const vec<T,3> & b)      { return a.x*b.x + a.y*b.y + a.z*b.z; }
    template<class T>        T        dot       (const vec<T,4> & a, const vec<T,4> & b)      { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
    template<class T, int N> vec<T,N> lerp      (const vec<T,N> & a, const vec<T,N> & b, T t) { return a*(1-t) + b*t; }
    template<class T, int N> T        mag2      (const vec<T,N> & a)                          { return dot(a,a); }
    template<class T, int N> T        mag       (const vec<T,N> & a)                          { return std::sqrt(mag2(a)); }
    template<class T, int N> vec<T,N> normalize (const vec<T,N> & a)                          { return a / mag(a); }

    template<class T> vec<T,4> qconj (const vec<T,4> & q)                     { return {-q.x,-q.y,-q.z,q.w}; }
    template<class T> vec<T,4> qmul  (const vec<T,4> & a, const vec<T,4> & b) { return {a.x*b.w+a.w*b.x+a.y*b.z-a.z*b.y, a.y*b.w+a.w*b.y+a.z*b.x-a.x*b.z, a.z*b.w+a.w*b.z+a.x*b.y-a.y*b.x, a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}; }
    template<class T> vec<T,3> qxdir (const vec<T,4> & q)                     { return {q.w*q.w+q.x*q.x-q.y*q.y-q.z*q.z, (q.x*q.y+q.z*q.w)*2, (q.z*q.x-q.y*q.w)*2}; }
    template<class T> vec<T,3> qydir (const vec<T,4> & q)                     { return {(q.x*q.y-q.z*q.w)*2, q.w*q.w-q.x*q.x+q.y*q.y-q.z*q.z, (q.y*q.z+q.x*q.w)*2}; }
    template<class T> vec<T,3> qzdir (const vec<T,4> & q)                     { return {(q.z*q.x+q.y*q.w)*2, (q.y*q.z-q.x*q.w)*2, q.w*q.w-q.x*q.x-q.y*q.y+q.z*q.z}; }
    template<class T> vec<T,3> qrot  (const vec<T,4> & q, const vec<T,3> & v) { return qxdir(q)*v.x + qydir(q)*v.y + qzdir(q)*v.z; }

    template<class T, int N> const T * begin(const vec<T,N> & v) { return &v.x; }
    template<class T, int N> const T * end(const vec<T,N> & v) { return &v.x + N; }
    template<class T, int N> T * begin(vec<T,N> & v) { return &v.x; }
    template<class T, int N> T * end(vec<T,N> & v) { return &v.x + N; }

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

    template<class T, int M> mat<T,M,2> operator * (const mat<T,M,2> & a, T b) { return {a.x*b, a.y*b}; }
    template<class T, int M> mat<T,M,3> operator * (const mat<T,M,3> & a, T b) { return {a.x*b, a.y*b, a.z*b}; }
    template<class T, int M> mat<T,M,4> operator * (const mat<T,M,4> & a, T b) { return {a.x*b, a.y*b, a.z*b, a.w*b}; }

    template<class T, int M> vec<T,M> operator * (const mat<T,M,2> & a, const vec<T,2> & b) { return a.x*b.x + a.y*b.y; }
    template<class T, int M> vec<T,M> operator * (const mat<T,M,3> & a, const vec<T,3> & b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
    template<class T, int M> vec<T,M> operator * (const mat<T,M,4> & a, const vec<T,4> & b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

    template<class T, int M, int N> mat<T,M,2> operator * (const mat<T,M,N> & a, const mat<T,N,2> & b) { return {a*b.x, a*b.y}; }
    template<class T, int M, int N> mat<T,M,3> operator * (const mat<T,M,N> & a, const mat<T,N,3> & b) { return {a*b.x, a*b.y, a*b.z}; }
    template<class T, int M, int N> mat<T,M,4> operator * (const mat<T,M,N> & a, const mat<T,N,4> & b) { return {a*b.x, a*b.y, a*b.z, a*b.w}; }

    template<class T> mat<T,2,2> adjugate(const mat<T,2,2> & a) { return {{a.y.y, -a.x.y}, {-a.y.x, a.x.x}}; }
    template<class T> mat<T,3,3> adjugate(const mat<T,3,3> & a) { return {{a.y.y*a.z.z - a.z.y*a.y.z, a.z.y*a.x.z - a.x.y*a.z.z, a.x.y*a.y.z - a.y.y*a.x.z},
                                                                          {a.y.z*a.z.x - a.z.z*a.y.x, a.z.z*a.x.x - a.x.z*a.z.x, a.x.z*a.y.x - a.y.z*a.x.x},
                                                                          {a.y.x*a.z.y - a.z.x*a.y.y, a.z.x*a.x.y - a.x.x*a.z.y, a.x.x*a.y.y - a.y.x*a.x.y}}; }
    template<class T> mat<T,4,4> adjugate(const mat<T,4,4> & a) { return {{a.y.y*a.z.z*a.w.w + a.w.y*a.y.z*a.z.w + a.z.y*a.w.z*a.y.w - a.y.y*a.w.z*a.z.w - a.z.y*a.y.z*a.w.w - a.w.y*a.z.z*a.y.w,
                                                                           a.x.y*a.w.z*a.z.w + a.z.y*a.x.z*a.w.w + a.w.y*a.z.z*a.x.w - a.w.y*a.x.z*a.z.w - a.z.y*a.w.z*a.x.w - a.x.y*a.z.z*a.w.w,
                                                                           a.x.y*a.y.z*a.w.w + a.w.y*a.x.z*a.y.w + a.y.y*a.w.z*a.x.w - a.x.y*a.w.z*a.y.w - a.y.y*a.x.z*a.w.w - a.w.y*a.y.z*a.x.w,
                                                                           a.x.y*a.z.z*a.y.w + a.y.y*a.x.z*a.z.w + a.z.y*a.y.z*a.x.w - a.x.y*a.y.z*a.z.w - a.z.y*a.x.z*a.y.w - a.y.y*a.z.z*a.x.w},
                                                                          {a.y.z*a.w.w*a.z.x + a.z.z*a.y.w*a.w.x + a.w.z*a.z.w*a.y.x - a.y.z*a.z.w*a.w.x - a.w.z*a.y.w*a.z.x - a.z.z*a.w.w*a.y.x,
                                                                           a.x.z*a.z.w*a.w.x + a.w.z*a.x.w*a.z.x + a.z.z*a.w.w*a.x.x - a.x.z*a.w.w*a.z.x - a.z.z*a.x.w*a.w.x - a.w.z*a.z.w*a.x.x,
                                                                           a.x.z*a.w.w*a.y.x + a.y.z*a.x.w*a.w.x + a.w.z*a.y.w*a.x.x - a.x.z*a.y.w*a.w.x - a.w.z*a.x.w*a.y.x - a.y.z*a.w.w*a.x.x,
                                                                           a.x.z*a.y.w*a.z.x + a.z.z*a.x.w*a.y.x + a.y.z*a.z.w*a.x.x - a.x.z*a.z.w*a.y.x - a.y.z*a.x.w*a.z.x - a.z.z*a.y.w*a.x.x},
                                                                          {a.y.w*a.z.x*a.w.y + a.w.w*a.y.x*a.z.y + a.z.w*a.w.x*a.y.y - a.y.w*a.w.x*a.z.y - a.z.w*a.y.x*a.w.y - a.w.w*a.z.x*a.y.y,
                                                                           a.x.w*a.w.x*a.z.y + a.z.w*a.x.x*a.w.y + a.w.w*a.z.x*a.x.y - a.x.w*a.z.x*a.w.y - a.w.w*a.x.x*a.z.y - a.z.w*a.w.x*a.x.y,
                                                                           a.x.w*a.y.x*a.w.y + a.w.w*a.x.x*a.y.y + a.y.w*a.w.x*a.x.y - a.x.w*a.w.x*a.y.y - a.y.w*a.x.x*a.w.y - a.w.w*a.y.x*a.x.y,
                                                                           a.x.w*a.z.x*a.y.y + a.y.w*a.x.x*a.z.y + a.z.w*a.y.x*a.x.y - a.x.w*a.y.x*a.z.y - a.z.w*a.x.x*a.y.y - a.y.w*a.z.x*a.x.y},
                                                                          {a.y.x*a.w.y*a.z.z + a.z.x*a.y.y*a.w.z + a.w.x*a.z.y*a.y.z - a.y.x*a.z.y*a.w.z - a.w.x*a.y.y*a.z.z - a.z.x*a.w.y*a.y.z,
                                                                           a.x.x*a.z.y*a.w.z + a.w.x*a.x.y*a.z.z + a.z.x*a.w.y*a.x.z - a.x.x*a.w.y*a.z.z - a.z.x*a.x.y*a.w.z - a.w.x*a.z.y*a.x.z,
                                                                           a.x.x*a.w.y*a.y.z + a.y.x*a.x.y*a.w.z + a.w.x*a.y.y*a.x.z - a.x.x*a.y.y*a.w.z - a.w.x*a.x.y*a.y.z - a.y.x*a.w.y*a.x.z,
                                                                           a.x.x*a.y.y*a.z.z + a.z.x*a.x.y*a.y.z + a.y.x*a.z.y*a.x.z - a.x.x*a.z.y*a.y.z - a.y.x*a.x.y*a.z.z - a.z.x*a.y.y*a.x.z}}; }
    template<class T> T determinant(const mat<T,2,2> & a) { return a.x.x*a.y.y - a.x.y*a.y.x; }
    template<class T> T determinant(const mat<T,3,3> & a) { return a.x.x*(a.y.y*a.z.z - a.z.y*a.y.z) + a.x.y*(a.y.z*a.z.x - a.z.z*a.y.x) + a.x.z*(a.y.x*a.z.y - a.z.x*a.y.y); }
    template<class T> T determinant(const mat<T,4,4> & a) { return a.x.x*(a.y.y*a.z.z*a.w.w + a.w.y*a.y.z*a.z.w + a.z.y*a.w.z*a.y.w - a.y.y*a.w.z*a.z.w - a.z.y*a.y.z*a.w.w - a.w.y*a.z.z*a.y.w)
                                                                 + a.x.y*(a.y.z*a.w.w*a.z.x + a.z.z*a.y.w*a.w.x + a.w.z*a.z.w*a.y.x - a.y.z*a.z.w*a.w.x - a.w.z*a.y.w*a.z.x - a.z.z*a.w.w*a.y.x)
                                                                 + a.x.z*(a.y.w*a.z.x*a.w.y + a.w.w*a.y.x*a.z.y + a.z.w*a.w.x*a.y.y - a.y.w*a.w.x*a.z.y - a.z.w*a.y.x*a.w.y - a.w.w*a.z.x*a.y.y)
                                                                 + a.x.w*(a.y.x*a.w.y*a.z.z + a.z.x*a.y.y*a.w.z + a.w.x*a.z.y*a.y.z - a.y.x*a.z.y*a.w.z - a.w.x*a.y.y*a.z.z - a.z.x*a.w.y*a.y.z); }
    template<class T, int N> mat<T,N,N> inverse(const mat<T,N,N> & a) { return adjugate(a)*(1/determinant(a)); }


    namespace aliases
    {
        typedef vec<uint8_t,2> byte2; typedef vec<int16_t,2> short2; typedef vec<int,2> int2; typedef vec<unsigned,2> uint2;
        typedef vec<uint8_t,3> byte3; typedef vec<int16_t,3> short3; typedef vec<int,3> int3; typedef vec<unsigned,3> uint3;
        typedef vec<uint8_t,4> byte4; typedef vec<int16_t,4> short4; typedef vec<int,4> int4; typedef vec<unsigned,4> uint4;
        typedef vec<float,2> float2; typedef mat<float,2,2> float2x2; typedef mat<float,3,2> float3x2; typedef mat<float,4,2> float4x2;
        typedef vec<float,3> float3; typedef mat<float,2,3> float2x3; typedef mat<float,3,3> float3x3; typedef mat<float,4,3> float4x3;
        typedef vec<float,4> float4; typedef mat<float,2,4> float2x4; typedef mat<float,3,4> float3x4; typedef mat<float,4,4> float4x4;
        typedef vec<double,2> double2; typedef mat<double,2,2> double2x2; typedef mat<double,3,2> double3x2; typedef mat<double,4,2> double4x2;
        typedef vec<double,3> double3; typedef mat<double,2,3> double2x3; typedef mat<double,3,3> double3x3; typedef mat<double,4,3> double4x3;
        typedef vec<double,4> double4; typedef mat<double,2,4> double2x4; typedef mat<double,3,4> double3x4; typedef mat<double,4,4> double4x4;
    }
}

#endif