// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef RECT_H
#define RECT_H

#include "../thirdparty/linalg/linalg.h"
using namespace linalg::aliases;

struct rect 
{ 
    int x0, y0, x1, y1; 
    int width() const { return x1 - x0; }
    int height() const { return y1 - y0; }
    int2 dims() const { return {width(), height()}; }
    float aspect_ratio() const { return (float)width()/height(); }
    template<class T> bool contains(const linalg::vec<T,2> & point) const { return point.x >= x0 && point.y >= y0 && point.x < x1 && point.y < y1; }
};

#endif