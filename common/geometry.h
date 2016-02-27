// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "../thirdparty/linalg/linalg.h" 
using namespace linalg::aliases; // NOTE: Unfriendly in a *.h file, but this file will later be consolidated into a namespace

#include <vector>

const float tau = 6.2831853f; // The circle constant, sometimes referred to as 2*pi

// Functions for producing various transformations
inline float4 rotation_quat(const float3 & axis, float angle)                          { return {axis*std::sin(angle/2), std::cos(angle/2)}; }
inline float4x4 translation_matrix(const float3 & translation)                         { return {{1,0,0,0},{0,1,0,0},{0,0,1,0},{translation,1}}; }
inline float4x4 rotation_matrix(const float4 & rotation)                               { return {{qxdir(rotation),0}, {qydir(rotation),0}, {qzdir(rotation),0}, {0,0,0,1}}; }
inline float4x4 pose_matrix(const float4 & rotation, const float3 & translation)       { return {{qxdir(rotation),0}, {qydir(rotation),0}, {qzdir(rotation),0}, {translation,1}}; }
inline float4x4 frustum_matrix(float l, float r, float b, float t, float n, float f)   { return {{2*n/(r-l),0,0,0}, {0,2*n/(t-b),0,0}, {(r+l)/(r-l),(t+b)/(t-b),-(f+n)/(f-n),-1}, {0,0,-2*f*n/(f-n),0}}; }
inline float4x4 perspective_matrix(float fovy, float aspect, float n, float f)         { float y = n*std::tan(fovy/2), x=y*aspect; return frustum_matrix(-x,x,-y,y,n,f); }

// Useful structure for representing the pose of a rigid object
struct pose
{
    float4 orientation; float3 position; 
    pose() : pose({0,0,0,1}, {0,0,0}) {}
    pose(const float4 & orientation) : pose(orientation, {0,0,0}) {}
    pose(const float3 & position) : pose({0,0,0,1}, position) {}
    pose(const float4 & orientation, const float3 & position) : orientation(orientation), position(position) {}

    float4x4 matrix() const { return pose_matrix(orientation, position); }
    float3 transform_vector(const float3 & v) const { return qrot(orientation, v); }
    float3 transform_point(const float3 & p) const { return position + transform_vector(p); }
    float3 detransform_point(const float3 & p) const { return detransform_vector(p - position); }
    float3 detransform_vector(const float3 & v) const { return qrot(qconj(orientation), v); }
};

// Shape data types
struct ray { float3 origin, direction; };
struct geometry_vertex { float3 position, normal; float2 texcoords; float3 tangent, bitangent; };
struct geometry_mesh { std::vector<geometry_vertex> vertices; std::vector<int3> triangles; };

// Shape transformations
inline ray transform(const pose & p, const ray & r) { return {p.transform_point(r.origin), p.transform_vector(r.direction)}; }
inline ray detransform(const pose & p, const ray & r) { return {p.detransform_point(r.origin), p.detransform_vector(r.direction)}; }

// Shape intersection routines
bool intersect_ray_plane(const ray & ray, const float4 & plane, float * hit_t = 0);
bool intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2, float * hit_t = 0, float2 * hit_uv = 0);
bool intersect_ray_mesh(const ray & ray, const geometry_mesh & mesh, float * hit_t = 0, int * hit_tri = 0, float2 * hit_uv = 0);

// Procedural geometry
geometry_mesh make_box_geometry(const float3 & min_bounds, const float3 & max_bounds);
geometry_mesh make_cylinder_geometry(const float3 & axis, const float3 & arm1, const float3 & arm2, int slices);
geometry_mesh make_lathed_geometry(const float3 & axis, const float3 & arm1, const float3 & arm2, int slices, std::initializer_list<float2> points);

void compute_normals(geometry_mesh & mesh);
void compute_tangents(geometry_mesh & mesh);
void generate_texcoords_cubic(geometry_mesh & mesh, float scale);

#endif