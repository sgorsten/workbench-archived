// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "ui3D.h"

ray camera::get_ray_from_pixel(const float2 & pixel, const rect & viewport) const
{
    const float x = 2 * (pixel.x - viewport.x0) / viewport.width() - 1, y = 1 - 2 * (pixel.y - viewport.y0) / viewport.height();
    const float4x4 inv_view_proj = inverse(get_viewproj_matrix(viewport));
    const float4 p0 = inv_view_proj * float4(x, y, -1, 1), p1 = inv_view_proj * float4(x, y, +1, 1);
    return {position, p1.xyz()*p0.w - p0.xyz()*p1.w};
}

gui3D::gui3D(gui & g) : g(g), bf(), bl(), bb(), br(), ml(), mr(), cam({}), gizmode()
{
    std::initializer_list<float2> arrow_points = {{0, 0.05f}, {1, 0.05f}, {1, 0.10f}, {1.2f, 0}};
    std::initializer_list<float2> ring_points = {{+0.05f, 1}, {-0.05f, 1}, {-0.05f, 1}, {-0.05f, 1.2f}, {-0.05f, 1.2f}, {+0.05f, 1.2f}, {+0.05f, 1.2f}, {+0.05f, 1}};
    gizmo_res.geomeshes[0] = make_lathed_geometry({1,0,0}, {0,1,0}, {0,0,1}, 12, arrow_points);
    gizmo_res.geomeshes[1] = make_lathed_geometry({0,1,0}, {0,0,1}, {1,0,0}, 12, arrow_points);
    gizmo_res.geomeshes[2] = make_lathed_geometry({0,0,1}, {1,0,0}, {0,1,0}, 12, arrow_points);
    gizmo_res.geomeshes[3] = make_box_geometry({-0.01f,0,0}, {0.01f,0.4f,0.4f});
    gizmo_res.geomeshes[4] = make_box_geometry({0,-0.01f,0}, {0.4f,0.01f,0.4f});
    gizmo_res.geomeshes[5] = make_box_geometry({0,0,-0.01f}, {0.4f,0.4f,0.01f});
    gizmo_res.geomeshes[6] = make_lathed_geometry({1,0,0}, {0,1,0}, {0,0,1}, 24, ring_points);
    gizmo_res.geomeshes[7] = make_lathed_geometry({0,1,0}, {0,0,1}, {1,0,0}, 24, ring_points);
    gizmo_res.geomeshes[8] = make_lathed_geometry({0,0,1}, {1,0,0}, {0,1,0}, 24, ring_points);
}

void gui3D::begin_frame() 
{ 
    draw = {}; 
    
    switch(g.in.type)
    {
    case input::key_down: case input::key_up:
        switch(g.in.key)
        {
        case GLFW_KEY_W: bf = g.in.is_down(); break;
        case GLFW_KEY_A: bl = g.in.is_down(); break;
        case GLFW_KEY_S: bb = g.in.is_down(); break;
        case GLFW_KEY_D: br = g.in.is_down(); break;
        }
        break;
    case input::mouse_down: case input::mouse_up:               
        switch(g.in.button)
        {
        case GLFW_MOUSE_BUTTON_LEFT: ml = g.in.is_down(); break;
        case GLFW_MOUSE_BUTTON_RIGHT: mr = g.in.is_down(); break;
        }
        break;
    }    
}

/////////////////
// 3D controls //
/////////////////

void plane_translation_dragger(gui3D & g, const float3 & plane_normal, float3 & point)
{
    if(g.g.in.type == input::mouse_down && g.g.in.button == GLFW_MOUSE_BUTTON_LEFT) { g.g.original_position = point; }

    if(g.ml)
    {
        // Define the plane to contain the original position of the object
        const float3 plane_point = g.g.original_position;

        // Define a ray emitting from the camera underneath the cursor
        const ray ray = g.get_ray_from_cursor();

        // If an intersection exists between the ray and the plane, place the object at that point
        const float denom = dot(ray.direction, plane_normal);
        if(std::abs(denom) == 0) return;
        const float t = dot(plane_point - ray.origin, plane_normal) / denom;
        if(t < 0) return;
        point = ray.origin + ray.direction * t;
    }
}

void axis_translation_dragger(gui3D & g, const float3 & axis, float3 & point)
{
    if(g.ml)
    {
        // First apply a plane translation dragger with a plane that contains the desired axis and is oriented to face the camera
        const float3 plane_tangent = cross(axis, point - g.cam.position);
        const float3 plane_normal = cross(axis, plane_tangent);
        plane_translation_dragger(g, plane_normal, point);

        // Constrain object motion to be along the desired axis
        point = g.g.original_position + axis * dot(point - g.g.original_position, axis);
    }
}

void position_gizmo(gui3D & g, int id, float3 & position)
{
    // On click, set the gizmo mode based on which component the user clicked on
    if(g.g.in.type == input::mouse_down && g.g.in.button == GLFW_MOUSE_BUTTON_LEFT)
    {
        g.gizmode = gizmo_mode::none;
        auto ray = g.get_ray_from_cursor();
        ray.origin -= position;
        float best_t = std::numeric_limits<float>::infinity(), t;           
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[0], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_x; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[1], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_y; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[2], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_z; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[3], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_yz; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[4], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_zx; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[5], &t) && t < best_t) { g.gizmode = gizmo_mode::translate_xy; best_t = t; }
        if(g.gizmode != gizmo_mode::none)
        {
            g.g.click_offset = ray.origin + ray.direction*t;
            g.g.set_pressed(id);
        }
    }

    // If the user has previously clicked on a gizmo component, allow the user to interact with that gizmo
    if(g.g.is_pressed(id))
    {
        position += g.g.click_offset;
        switch(g.gizmode)
        {
        case gizmo_mode::translate_x: axis_translation_dragger(g, {1,0,0}, position); break;
        case gizmo_mode::translate_y: axis_translation_dragger(g, {0,1,0}, position); break;
        case gizmo_mode::translate_z: axis_translation_dragger(g, {0,0,1}, position); break;
        case gizmo_mode::translate_yz: plane_translation_dragger(g, {1,0,0}, position); break;
        case gizmo_mode::translate_zx: plane_translation_dragger(g, {0,1,0}, position); break;
        case gizmo_mode::translate_xy: plane_translation_dragger(g, {0,0,1}, position); break;
        }        
        position -= g.g.click_offset;
    }

    // On release, deactivate the current gizmo mode
    if(g.g.check_release(id)) g.gizmode = gizmo_mode::none;

    // Add the gizmo to our 3D draw list
    const float3 colors[] = {
        g.gizmode == gizmo_mode::translate_x ? float3(1,0.5f,0.5f) : float3(1,0,0),
        g.gizmode == gizmo_mode::translate_y ? float3(0.5f,1,0.5f) : float3(0,1,0),
        g.gizmode == gizmo_mode::translate_z ? float3(0.5f,0.5f,1) : float3(0,0,1),
        g.gizmode == gizmo_mode::translate_yz ? float3(0.5f,1,1) : float3(0,1,1),
        g.gizmode == gizmo_mode::translate_zx ? float3(1,0.5f,1) : float3(1,0,1),
        g.gizmode == gizmo_mode::translate_xy ? float3(1,1,0.5f) : float3(1,1,0),   
    };

    auto model = translation_matrix(position), modelIT = inverse(transpose(model));
    for(int i=0; i<6; ++i)
    {
        g.draw.begin_object(g.gizmo_res.meshes[i], g.gizmo_res.program);
        g.draw.set_uniform("u_model", model);
        g.draw.set_uniform("u_modelIT", modelIT);
        g.draw.set_uniform("u_diffuseMtl", colors[i]);
    }
}

void axis_rotation_dragger(gui3D & g, const float3 & axis, const float3 & center, float4 & orientation)
{
    if(g.ml)
    {
        pose original_pose = {g.g.original_orientation, g.g.original_position};
        float3 the_axis = original_pose.transform_vector(axis);
        float4 the_plane = {the_axis, -dot(the_axis, g.g.click_offset)};
        const ray the_ray = g.get_ray_from_cursor();

        float t;
        if(intersect_ray_plane(the_ray, the_plane, &t))
        {
            float3 center_of_rotation = g.g.original_position + the_axis * dot(the_axis, g.g.click_offset - g.g.original_position);
            float3 arm1 = normalize(g.g.click_offset - center_of_rotation);
            float3 arm2 = normalize(the_ray.origin + the_ray.direction * t - center_of_rotation); 

            float d = dot(arm1, arm2);
            if(d > 0.999f) { orientation = g.g.original_orientation; return; }
            float angle = std::acos(d);
            if(angle < 0.001f) { orientation = g.g.original_orientation; return; }
            auto a = normalize(cross(arm1, arm2));
            orientation = qmul(rotation_quat(a, angle), g.g.original_orientation);
        }
    }
}

void orientation_gizmo(gui3D & g, int id, const float3 & center, float4 & orientation)
{
    auto p = pose(orientation, center);
    // On click, set the gizmo mode based on which component the user clicked on
    if(g.g.in.type == input::mouse_down && g.g.in.button == GLFW_MOUSE_BUTTON_LEFT)
    {
        g.gizmode = gizmo_mode::none;
        auto ray = detransform(p, g.get_ray_from_cursor());
        float best_t = std::numeric_limits<float>::infinity(), t;           
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[6], &t) && t < best_t) { g.gizmode = gizmo_mode::rotate_yz; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[7], &t) && t < best_t) { g.gizmode = gizmo_mode::rotate_zx; best_t = t; }
        if(intersect_ray_mesh(ray, g.gizmo_res.geomeshes[8], &t) && t < best_t) { g.gizmode = gizmo_mode::rotate_xy; best_t = t; }
        if(g.gizmode != gizmo_mode::none)
        {
            g.g.original_position = center;
            g.g.original_orientation = orientation;
            g.g.click_offset = p.transform_point(ray.origin + ray.direction*t);
            g.g.set_pressed(id);
        }
    }

    // If the user has previously clicked on a gizmo component, allow the user to interact with that gizmo
    if(g.g.is_pressed(id))
    {
        switch(g.gizmode)
        {
        case gizmo_mode::rotate_yz: axis_rotation_dragger(g, {1,0,0}, center, orientation); break;
        case gizmo_mode::rotate_zx: axis_rotation_dragger(g, {0,1,0}, center, orientation); break;
        case gizmo_mode::rotate_xy: axis_rotation_dragger(g, {0,0,1}, center, orientation); break;
        }
    }

    // On release, deactivate the current gizmo mode
    if(g.g.check_release(id)) g.gizmode = gizmo_mode::none;

    // Add the gizmo to our 3D draw list
    const float3 colors[] = {
        g.gizmode == gizmo_mode::rotate_yz ? float3(0.5f,1,1) : float3(0,1,1),
        g.gizmode == gizmo_mode::rotate_zx ? float3(1,0.5f,1) : float3(1,0,1),
        g.gizmode == gizmo_mode::rotate_xy ? float3(1,1,0.5f) : float3(1,1,0),   
    };

    const auto model = p.matrix();
    for(int i=6; i<9; ++i)
    {
        g.draw.begin_object(g.gizmo_res.meshes[i], g.gizmo_res.program);
        g.draw.set_uniform("u_model", model);
        g.draw.set_uniform("u_modelIT", model); // No scaling, no need to inverse-transpose
        g.draw.set_uniform("u_diffuseMtl", colors[i-6]);
    }
}