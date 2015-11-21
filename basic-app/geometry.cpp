// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "geometry.h"

bool intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2, float * hit_t, float2 * hit_uv)
{
    auto e1 = v1 - v0, e2 = v2 - v0, h = cross(ray.direction, e2);
    auto a = dot(e1, h);
    if (a > -0.0001f && a < 0.0001f) return false;

    float f = 1 / a;
    auto s = ray.origin - v0;
	auto u = f * dot(s, h);
	if (u < 0 || u > 1) return false;

	auto q = cross(s, e1);
	auto v = f * dot(ray.direction, q);
	if (v < 0 || u + v > 1) return false;

    auto t = f * dot(e2, q);
    if(t < 0) return false;

    if(hit_t) *hit_t = t;
    if(hit_uv) *hit_uv = {u,v};
    return true;
}

bool intersect_ray_mesh(const ray & ray, const geometry_mesh & mesh, float * hit_t, int * hit_tri, float2 * hit_uv)
{
    float best_t = std::numeric_limits<float>::infinity(), t;
    float2 best_uv, uv;
    int best_tri = -1;
    for(auto & tri : mesh.triangles)
    {
        if(intersect_ray_triangle(ray, mesh.vertices[tri[0]].position, mesh.vertices[tri[1]].position, mesh.vertices[tri[2]].position, &t, &uv) && t < best_t)
        {
            best_t = t;
            best_uv = uv;
            best_tri = &tri - mesh.triangles.data();
        }
    }
    if(best_tri == -1) return false;
    if(hit_t) *hit_t = best_t;
    if(hit_tri) *hit_tri = best_tri;
    if(hit_uv) *hit_uv = best_uv;
    return true;
}

// Procedural geometry
geometry_mesh make_box_geometry(const float3 & min_bounds, const float3 & max_bounds)
{
    const auto a = min_bounds, b = max_bounds;
    geometry_mesh mesh;
    mesh.vertices = {
        {{a.x, a.y, a.z}, {-1,0,0}, {1,0}},
        {{a.x, a.y, b.z}, {-1,0,0}, {0,0}},
        {{a.x, b.y, b.z}, {-1,0,0}, {0,1}},
        {{a.x, b.y, a.z}, {-1,0,0}, {1,1}},
        {{b.x, a.y, a.z}, {+1,0,0}, {0,0}},
        {{b.x, b.y, a.z}, {+1,0,0}, {1,0}},
        {{b.x, b.y, b.z}, {+1,0,0}, {1,1}},
        {{b.x, a.y, b.z}, {+1,0,0}, {0,1}},
        {{a.x, a.y, a.z}, {0,-1,0}, {1,0}},
        {{b.x, a.y, a.z}, {0,-1,0}, {0,0}},
        {{b.x, a.y, b.z}, {0,-1,0}, {0,1}},
        {{a.x, a.y, b.z}, {0,-1,0}, {1,1}},
        {{a.x, b.y, a.z}, {0,+1,0}, {0,0}},
        {{a.x, b.y, b.z}, {0,+1,0}, {1,0}},
        {{b.x, b.y, b.z}, {0,+1,0}, {1,1}},
        {{b.x, b.y, a.z}, {0,+1,0}, {0,1}},
        {{a.x, a.y, a.z}, {0,0,-1}, {1,0}},
        {{a.x, b.y, a.z}, {0,0,-1}, {0,0}},
        {{b.x, b.y, a.z}, {0,0,-1}, {0,1}},
        {{b.x, a.y, a.z}, {0,0,-1}, {1,1}},
        {{a.x, a.y, b.z}, {0,0,+1}, {0,0}},
        {{b.x, a.y, b.z}, {0,0,+1}, {1,0}},
        {{b.x, b.y, b.z}, {0,0,+1}, {1,1}},
        {{a.x, b.y, b.z}, {0,0,+1}, {0,1}},
    };
    mesh.triangles = {{0,1,2}, {0,2,3}, {4,5,6}, {4,6,7}, {8,9,10}, {8,10,11}, {12,13,14}, {12,14,15}, {16,17,18}, {16,18,19}, {20,21,22}, {20,22,23}};
    return mesh;
}

geometry_mesh make_cylinder_geometry(const float3 & axis, const float3 & arm1, const float3 & arm2, int slices)
{
    // Generated curved surface
    geometry_mesh mesh;
    for(int i=0; i<=slices; ++i)
    {
        const float tex_s = static_cast<float>(i) / slices, angle = (float)(i%slices) * tau / slices;
        const float3 arm = arm1 * std::cos(angle) + arm2 * std::sin(angle);
        mesh.vertices.push_back({arm, normalize(arm), {tex_s,0}});
        mesh.vertices.push_back({arm + axis, normalize(arm), {tex_s,1}});
    }
    for(int i=0; i<slices; ++i)
    {
        mesh.triangles.push_back({i*2, i*2+2, i*2+3});
        mesh.triangles.push_back({i*2, i*2+3, i*2+1});
    }

    // Generate caps
    int base = mesh.vertices.size();
    for(int i=0; i<slices; ++i)
    {
        const float angle = static_cast<float>(i%slices) * tau / slices, c = std::cos(angle), s = std::sin(angle);
        const float3 arm = arm1 * c + arm2 * s;
        mesh.vertices.push_back({arm + axis, normalize(axis), {c*0.5f+0.5f, s*0.5f+0.5f}});
        mesh.vertices.push_back({arm, -normalize(axis), {c*0.5f+0.5f, 0.5f-s*0.5f}});
    }
    for(int i=2; i<slices; ++i)
    {
        mesh.triangles.push_back({base, base+i*2-2, base+i*2});
        mesh.triangles.push_back({base+1, base+i*2+1, base+i*2-1});
    }
    return mesh;
}