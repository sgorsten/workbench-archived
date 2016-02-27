// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "geometry.h"

bool intersect_ray_plane(const ray & ray, const float4 & plane, float * hit_t)
{
    float denom = dot(plane.xyz(), ray.direction);
    if(std::abs(denom) == 0) return false;

    if(hit_t) *hit_t = -dot(plane, float4(ray.origin,1)) / denom;
    return true;
}

bool intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2, float * hit_t, float2 * hit_uv)
{
    auto e1 = v1 - v0, e2 = v2 - v0, h = cross(ray.direction, e2);
    auto a = dot(e1, h);
    if(std::abs(a) == 0) return false;

    float f = 1 / a;
    auto s = ray.origin - v0;
	auto u = f * dot(s, h);
	if(u < 0 || u > 1) return false;

	auto q = cross(s, e1);
	auto v = f * dot(ray.direction, q);
	if(v < 0 || u + v > 1) return false;

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
void compute_normals(geometry_mesh & mesh)
{
    for(geometry_vertex & v : mesh.vertices) v.normal = float3();
    for(int3 & t : mesh.triangles)
    {
        geometry_vertex & v0 = mesh.vertices[t.x], & v1 = mesh.vertices[t.y], & v2 = mesh.vertices[t.z];
        const float3 n = cross(v1.position - v0.position, v2.position - v0.position);
        v0.normal += n; v1.normal += n; v2.normal += n;
    }
    for(geometry_vertex & v : mesh.vertices)
    {
        v.normal = normalize(v.normal);
    }
}

void compute_tangents(geometry_mesh & mesh)
{
    for(geometry_vertex & v : mesh.vertices) v.tangent = v.bitangent = float3();
    for(int3 & t : mesh.triangles)
    {
        geometry_vertex & v0 = mesh.vertices[t.x], & v1 = mesh.vertices[t.y], & v2 = mesh.vertices[t.z];
        const float3 e1 = v1.position - v0.position, e2 = v2.position - v0.position;
        const float2 d1 = v1.texcoords - v0.texcoords, d2 = v2.texcoords - v0.texcoords;
        const float3 dpds = float3(d2.y * e1.x - d1.y * e2.x, d2.y * e1.y - d1.y * e2.y, d2.y * e1.z - d1.y * e2.z) / cross(d1, d2);
        const float3 dpdt = float3(d1.x * e2.x - d2.x * e1.x, d1.x * e2.y - d2.x * e1.y, d1.x * e2.z - d2.x * e1.z) / cross(d1, d2);
        v0.tangent += dpds; v1.tangent += dpds; v2.tangent += dpds;
        v0.bitangent += dpdt; v1.bitangent += dpdt; v2.bitangent += dpdt;
    }
    for(geometry_vertex & v : mesh.vertices)
    {
        v.tangent = normalize(v.tangent);
        v.bitangent = normalize(v.bitangent);
    }
}

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
    compute_tangents(mesh);
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

    compute_tangents(mesh);
    return mesh;
}

geometry_mesh make_lathed_geometry(const float3 & axis, const float3 & arm1, const float3 & arm2, int slices, std::initializer_list<float2> points)
{
    geometry_mesh mesh;
    for(int i=0; i<=slices; ++i)
    {
        const float angle = static_cast<float>(i%slices) * tau / slices, c = std::cos(angle), s = std::sin(angle);
        const float3x2 mat = {axis, arm1 * c + arm2 * s};
        float3 n = normalize(mat.y); // TODO: Proper normals for each segment
        for(auto & p : points) mesh.vertices.push_back({mul(mat,p), n});

        if(i > 0)
        {
            for(size_t j = 1; j < points.size(); ++j)
            {
                int i0 = (i-1)*points.size() + (j-1);
                int i1 = (i-0)*points.size() + (j-1);
                int i2 = (i-0)*points.size() + (j-0);
                int i3 = (i-1)*points.size() + (j-0);
                mesh.triangles.push_back({i0,i1,i2});
                mesh.triangles.push_back({i0,i2,i3});
            }
        }
    }
    compute_normals(mesh);
    return mesh;
}

void generate_texcoords_cubic(geometry_mesh & mesh, float scale)
{
    for(auto & vert : mesh.vertices)
    {
        if(std::abs(vert.normal.x) > std::abs(vert.normal.y) && std::abs(vert.normal.x) > std::abs(vert.normal.z))
        {
            vert.texcoords = float2(vert.position.y, vert.position.z) * scale;
        }
        else if(std::abs(vert.normal.y) > std::abs(vert.normal.z))
        {
            vert.texcoords = float2(vert.position.z, vert.position.x) * scale;
        }
        else
        {
            vert.texcoords = float2(vert.position.x, vert.position.y) * scale;
        }
    }
}