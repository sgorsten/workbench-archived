// This is free and unencumbered software released into the public domain.
// For more information, please refer to <http://unlicense.org>

#include "draw.h"
#include "ui.h"

#include <cassert>
#include <cstdlib>
#include <set>
#include <sstream>
#include <algorithm>
#include <GLFW\glfw3.h>
#pragma comment(lib, "opengl32.lib")

#define SHADER_PREAMBLE "#version 420\nlayout(shared, binding=1) uniform PerScene { mat4 u_viewProj; vec3 u_eyePos, u_lightPos, u_lightColor; };\n"

const char * vert_shader_source = SHADER_PREAMBLE R"(
layout(binding=2) uniform PerObject
{
    mat4 u_model, u_modelIT;
    vec3 u_diffuseMtl;
};

layout(location = 0) in vec3 v_position; 
layout(location = 1) in vec3 v_normal; 
layout(location = 2) in vec3 v_tangent; 
layout(location = 3) in vec3 v_bitangent; 
layout(location = 4) in vec2 v_texCoord; 
out vec3 position, normal, tangent, bitangent;
out vec2 texCoord;
void main() 
{
    position = (u_model * vec4(v_position,1)).xyz;
    normal = (u_modelIT * vec4(v_normal,0)).xyz;
    tangent = (u_modelIT * vec4(v_tangent,0)).xyz;
    bitangent = (u_modelIT * vec4(v_bitangent,0)).xyz;
    texCoord = v_texCoord;
    gl_Position = u_viewProj * vec4(position,1);
})";

const char * frag_shader_source = SHADER_PREAMBLE R"(
layout(binding=2) uniform PerObject
{
    mat4 u_model, u_modelIT;
    vec3 u_diffuseMtl;
};

uniform sampler2D u_diffuseTex;
uniform sampler2D u_normalTex;
in vec3 position, normal, tangent, bitangent;
in vec2 texCoord;
void main() 
{ 
    vec3 tsNormal = texture2D(u_normalTex, texCoord).xyz * 2 - 1;
    vec3 normalVec = normalize(normalize(tangent) * tsNormal.x + normalize(bitangent) * -tsNormal.y + normalize(normal) * tsNormal.z);
    vec3 eyeVec = normalize(u_eyePos - position);
    vec3 lightDir = normalize(u_lightPos - position);
    vec3 halfVec = normalize(lightDir + eyeVec);

    vec3 diffuseMtl = texture2D(u_diffuseTex, texCoord).rgb * u_diffuseMtl;
    float diffuseLight = max(dot(lightDir, normalVec), 0);
    float specularLight = max(pow(dot(halfVec, normalVec), 1024), 0);
    gl_FragColor = vec4(diffuseMtl * u_lightColor * (diffuseLight + 0.1f) + vec3(0.5) * specularLight, 1);
})";

const char * diffuse_vert_shader_source = SHADER_PREAMBLE R"(
layout(binding=2) uniform PerObject
{
    mat4 u_model, u_modelIT;
    vec3 u_diffuseMtl;
};

layout(location = 0) in vec3 v_position; 
layout(location = 1) in vec3 v_normal;
out vec3 position, normal;
void main() 
{
    position = (u_model * vec4(v_position,1)).xyz;
    normal = (u_modelIT * vec4(v_normal,0)).xyz;
    gl_Position = u_viewProj * vec4(position,1);
})";

const char * diffuse_frag_shader_source = SHADER_PREAMBLE R"(
layout(binding=2) uniform PerObject
{
    mat4 u_model, u_modelIT;
    vec3 u_diffuseMtl;
};

in vec3 position, normal;
void main() 
{ 
    vec3 normalVec = normalize(normal);
    vec3 eyeVec = normalize(u_eyePos - position);
    vec3 lightDir = normalize(vec3(1,1,1));
    vec3 halfVec = normalize(lightDir + eyeVec);

    float diffuseLight = dot(lightDir, normalVec)*0.5f + 0.5f;
    float specularLight = max(pow(dot(halfVec, normalVec), 1024), 0);
    gl_FragColor = vec4(u_diffuseMtl * diffuseLight + vec3(0.5) * specularLight, 1);
})";

void render_gui(const gui & g, GLuint sprite_tex)
{
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, g.window_size.x, g.window_size.y, 0, -1, +1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, sprite_tex);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const GLenum states[] = {GL_VERTEX_ARRAY, GL_COLOR_ARRAY, GL_TEXTURE_COORD_ARRAY};
    for(auto s : states) glEnableClientState(s);
    glVertexPointer(2, GL_SHORT, sizeof(gui::vertex), &g.vertices.data()->position);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(gui::vertex), &g.vertices.data()->color);
    glTexCoordPointer(2, GL_FLOAT, sizeof(gui::vertex), &g.vertices.data()->texcoord);
    for(const auto & list : g.lists) glDrawArrays(GL_QUADS, list.first, list.last - list.first);
    for(auto s : states) glDisableClientState(s);
}

struct scene_object
{
    std::string name;
    float3 position;

    scene_object(std::string name, float3 position) : name(name), position(position) {}

    float4x4 get_model_matrix() const { return translation_matrix(position); }
    virtual bool intersect_ray(ray r, float * t) const { return false; }
    virtual void draw(draw_list & list) const {}
    virtual void on_gui(gui & g, const rect & r, int offset) = 0;
};

struct point_light : public scene_object
{
    float3 color;

    point_light(std::string name, float3 position, float3 color) : scene_object(name, position), color(color) {}

    void on_gui(gui & g, const rect & r, int offset)
    {
        int y0 = r.y0 + 4 - offset;

        const int line_height = g.default_font.line_height + 4, mid = (r.x0 + r.x1) / 2;
        draw_shadowed_text(g, {r.x0 + 4, y0 + 2}, {1,1,1,1}, "Name");
        edit(g, 1, {mid + 2, y0, r.x1 - 4, y0 + line_height}, name);
        y0 += line_height + 4;
        draw_shadowed_text(g, {r.x0 + 4, y0 + 2}, {1,1,1,1}, "Position");
        edit(g, 2, {mid + 2, y0, r.x1 - 4, y0 + line_height}, position);
        y0 += line_height + 4;
        draw_shadowed_text(g, {r.x0 + 4, y0 + 2}, {1,1,1,1}, "Color");
        edit(g, 3, {mid + 2, y0, r.x1 - 4, y0 + line_height}, color);
        y0 += line_height + 4;    
    }
};

struct static_mesh : public scene_object
{
    const geometry_mesh * mesh;
    std::shared_ptr<const gfx::program> program;
    std::shared_ptr<const gfx::texture> diffuse_tex;
    std::shared_ptr<const gfx::texture> normal_tex;
    std::shared_ptr<const gfx::mesh> gmesh;
    float3 diffuse;

    static_mesh(std::string name, float3 position, const geometry_mesh * mesh, std::shared_ptr<const gfx::program> program,
        std::shared_ptr<const gfx::texture> diffuse_tex, std::shared_ptr<const gfx::texture> normal_tex, std::shared_ptr<const gfx::mesh> gmesh, const float3 & diffuse) : 
        scene_object(name, position), mesh(mesh), program(program), diffuse_tex(diffuse_tex), normal_tex(normal_tex), gmesh(gmesh), diffuse(diffuse) {}

    bool intersect_ray(ray r, float * t) const
    {
        r.origin -= position;
        return intersect_ray_mesh(r, *mesh, t);
    }

    void draw(draw_list & list) const
    {
        const auto model = get_model_matrix();
        list.begin_object(gmesh, program);
        list.set_uniform("u_model", model);
        list.set_uniform("u_modelIT", inverse(transpose(model)));
        list.set_uniform("u_diffuseMtl", diffuse);
        list.set_sampler("u_diffuseTex", diffuse_tex);
        list.set_sampler("u_normalTex", normal_tex);
    }

    void on_gui(gui & g, const rect & r, int offset)
    {
        int y0 = r.y0 + 4 - offset;

        const int line_height = g.default_font.line_height + 4, mid = (r.x0 + r.x1) / 2;
        draw_shadowed_text(g, {r.x0 + 4, y0 + 2}, {1,1,1,1}, "Name");
        edit(g, 1, {mid + 2, y0, r.x1 - 4, y0 + line_height}, name);
        y0 += line_height + 4;
        draw_shadowed_text(g, {r.x0 + 4, y0 + 2}, {1,1,1,1}, "Position");
        edit(g, 2, {mid + 2, y0, r.x1 - 4, y0 + line_height}, position);
        y0 += line_height + 4;
        draw_shadowed_text(g, {r.x0 + 4, y0 + 2}, {1,1,1,1}, "Diffuse");
        edit(g, 3, {mid + 2, y0, r.x1 - 4, y0 + line_height}, diffuse);
        y0 += line_height + 4;
    }
};

scene_object * raycast(const ray & ray, const std::vector<scene_object *> & objects)
{
    scene_object * best = nullptr;
    float t, best_t;
    for(auto * obj : objects)
    {
        if(obj->intersect_ray(ray, &t) && (!best || t < best_t))
        {
            best = obj;
            best_t = t;
        }
    }
    return best;
}

float3 get_center_of_mass(const std::set<scene_object *> & objects)
{
    float3 sum;
    for(auto obj : objects) sum += obj->position;
    return sum / (float)objects.size();
}

void object_list_ui(gui & g, int id, rect r, const std::vector<scene_object *> & objects, std::set<scene_object *> & selection, int & offset)
{
    r = tabbed_frame(g, r, "Object List");

    auto panel = vscroll_panel(g, id, r, objects.size()*30+20, offset);
    g.begin_children(id);
    g.begin_scissor(panel);
    int y0 = panel.y0 + 4 - offset;
    for(auto * obj : objects)
    {
        const rect list_entry = {panel.x0 + 4, y0, panel.x1 - 4, y0 + g.default_font.line_height};
        if(g.check_click(&obj - objects.data(), list_entry))
        {
            if(!g.is_control_held()) selection.clear();
            auto it = selection.find(obj);
            if(it == end(selection)) selection.insert(obj);
            else selection.erase(it);
            g.gizmode = gizmo_mode::none;
        }

        bool selected = selection.find(obj) != end(selection);
        draw_shadowed_text(g, {list_entry.x0, list_entry.y0}, selected ? float4(1,1,0,1) : float4(1,1,1,1), obj->name);
        y0 += g.default_font.line_height + 4;
    }
    g.end_scissor();
    g.end_children();
}

void object_properties_ui(gui & g, int id, rect r, std::set<scene_object *> & selection, int & offset)
{
    r = tabbed_frame(g, r, "Object Properties");

    if(selection.size() != 1) return;

    auto & obj = **selection.begin();

    auto panel = vscroll_panel(g, id, r, 1000, offset); // TODO: Determine correct size
    g.begin_children(id);
    g.begin_scissor(panel);
    obj.on_gui(g, panel, offset);
    g.end_scissor();
    g.end_children();
}

void viewport_ui(gui & g, int id, rect r, std::vector<scene_object *> & objects, std::set<scene_object *> & selection)
{
    g.viewport3d = r = tabbed_frame(g, r, "Scene View");

    if(!selection.empty())
    {
        g.begin_children(id);
        float3 com = get_center_of_mass(selection), new_com = com;
        position_gizmo(g, 1, new_com);
        if(new_com != com) for(auto obj : selection) obj->position += new_com - com;
        g.end_children();
    }
    if(g.is_child_pressed(id)) return;

    if(g.check_click(id, r))
    {
        if(!selection.empty() && g.gizmode == gizmo_mode::none && !g.is_control_held()) selection.clear();

        if(selection.empty() || g.is_control_held())
        {
            if(auto picked_object = raycast(g.get_ray_from_cursor(), objects))
            {
                auto it = selection.find(picked_object);
                if(it == end(selection)) selection.insert(picked_object);
                else selection.erase(it);
                g.gizmode = gizmo_mode::none;
            }
        }
    }

    if(g.mr)
    {
        g.cam.yaw -= g.in.motion.x * 0.01f;
        g.cam.pitch -= g.in.motion.y * 0.01f;

        const float4 orientation = g.cam.get_orientation();
        float3 move;
        if(g.bf) move -= qzdir(orientation);
        if(g.bl) move -= qxdir(orientation);
        if(g.bb) move += qzdir(orientation);
        if(g.br) move += qxdir(orientation);
        if(mag2(move) > 0) g.cam.position += normalize(move) * (g.timestep * 8);
    }
}

std::shared_ptr<gfx::mesh> make_draw_mesh(std::shared_ptr<gfx::context> ctx, const geometry_mesh & mesh)
{
    const geometry_vertex * vertex = 0;
    auto m = create_mesh(ctx);
    set_vertices(*m, mesh.vertices.data(), mesh.vertices.size() * sizeof(geometry_vertex));
    set_attribute(*m, 0, &geometry_vertex::position);
    set_attribute(*m, 1, &geometry_vertex::normal);
    set_attribute(*m, 2, &geometry_vertex::tangent);
    set_attribute(*m, 3, &geometry_vertex::bitangent);
    set_attribute(*m, 4, &geometry_vertex::texcoords);
    set_indices(*m, GL_TRIANGLES, (const unsigned int *)mesh.triangles.data(), mesh.triangles.size() * 3);
    return m;
}

int main(int argc, char * argv[])
{
    sprite_sheet sprites;

    gui g(sprites);
    g.cam.yfov = 1.0f;
    g.cam.near_clip = 0.1f;
    g.cam.far_clip = 16.0f;
    g.cam.position = {0,1.5f,4};

    sprites.prepare_texture();
   
    auto ctx = gfx::create_context();
    auto vert_shader = compile_shader(ctx, GL_VERTEX_SHADER, vert_shader_source);
    auto frag_shader = compile_shader(ctx, GL_FRAGMENT_SHADER, frag_shader_source);
    auto program = gfx::link_program(ctx, {vert_shader, frag_shader});
    auto program2 = gfx::link_program(ctx, {compile_shader(ctx, GL_VERTEX_SHADER, diffuse_vert_shader_source), compile_shader(ctx, GL_FRAGMENT_SHADER, diffuse_frag_shader_source)});
    auto diffuse_tex = load_texture(ctx, "pattern_191_diffuse.png");
    auto normal_tex = load_texture(ctx, "pattern_191_normal.png");

    auto ground = make_box_geometry({-4,-0.1f,-4}, {4,0,4});
    generate_texcoords_cubic(ground, 0.5);
    const auto box = make_box_geometry({-0.4f,0.0f,-0.4f}, {0.4f,0.8f,0.4f});
    const auto cylinder = make_cylinder_geometry({0,1,0}, {0,0,0.4f}, {0.4f,0,0}, 24);
    auto g_ground = make_draw_mesh(ctx, ground);
    auto g_box = make_draw_mesh(ctx, box);
    auto g_cylinder = make_draw_mesh(ctx, cylinder);

    auto plight = new point_light("Point Light", {0,+2,0}, {1,1,1});
    std::vector<scene_object *> objects = {
        new static_mesh("Ground", {0,0,0}, &ground, program, diffuse_tex, normal_tex, g_ground, {0.8f,0.8f,0.8f}),
        new static_mesh("Box", {-1,0,0}, &box, program, diffuse_tex, normal_tex, g_box, {1,0.5f,0.5f}),
        new static_mesh("Cylinder", {0,0,0}, &cylinder, program, diffuse_tex, normal_tex, g_cylinder, {0.5f,1,0.5f}),
        new static_mesh("Box 2", {+1,0,0}, &box, program, diffuse_tex, normal_tex, g_box, {0.5f,0.5f,1}),
        plight
    };
    std::set<scene_object *> selection;
    
    g.gizmo_res.program = gfx::link_program(ctx, {compile_shader(ctx, GL_VERTEX_SHADER, diffuse_vert_shader_source), compile_shader(ctx, GL_FRAGMENT_SHADER, diffuse_frag_shader_source)});
    for(int i=0; i<6; ++i) g.gizmo_res.meshes[i] = make_draw_mesh(ctx, g.gizmo_res.geomeshes[i]);

    renderer the_renderer;
    
    auto win = gfx::create_window(*ctx, {1280, 720}, "Basic Workbench App");
    std::vector<input_event> events;
    install_input_callbacks(win, events);
    glfwMakeContextCurrent(win);

    GLuint sprite_tex = 0;
    glGenTextures(1, &sprite_tex);
    glBindTexture(GL_TEXTURE_2D, sprite_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, sprites.get_texture_dims().x, sprites.get_texture_dims().y, 0, GL_ALPHA, GL_UNSIGNED_BYTE, sprites.get_texture_data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    GLFWcursor * arrow_cursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    GLFWcursor * ibeam_cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    GLFWcursor * hresize_cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    GLFWcursor * vresize_cursor = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);

    int split1 = 1080, split2 = 358, offset0 = 0, offset1 = 0;
    double t0 = glfwGetTime();
    while(!glfwWindowShouldClose(win))
    {
        g.icon = cursor_icon::arrow;
        glfwPollEvents();

        if(events.empty()) emit_empty_event(win);
        g.in = events.front();
        events.erase(begin(events));
        switch(g.in.type)
        {
        case input::key_down: case input::key_up:
            switch(g.in.key)
            {
            case GLFW_KEY_W: g.bf = g.in.is_down(); break;
            case GLFW_KEY_A: g.bl = g.in.is_down(); break;
            case GLFW_KEY_S: g.bb = g.in.is_down(); break;
            case GLFW_KEY_D: g.br = g.in.is_down(); break;
            }
            break;
        case input::mouse_down: case input::mouse_up:               
            switch(g.in.button)
            {
            case GLFW_MOUSE_BUTTON_LEFT: g.ml = g.in.is_down(); break;
            case GLFW_MOUSE_BUTTON_RIGHT: g.mr = g.in.is_down(); break;
            }
            break;
        }

        int fw, fh, w, h;
        glfwGetFramebufferSize(win, &fw, &fh);
        glfwGetWindowSize(win, &w, &h);

        const double t1 = glfwGetTime();
        g.timestep = static_cast<float>(t1-t0);
        t0 = t1;

        g.begin_frame({w, h});

        // Experimental support for a menu bar
        begin_menu(g, 1, {0, 0, w, 20});
        {
            begin_popup(g, 1, "File");
            {
                begin_popup(g, 1, "New");
                {
                    menu_item(g, "Game");
                    menu_item(g, "Scene");
                    menu_item(g, "Script");
                }
                end_popup(g);

                menu_item(g, "Open", GLFW_MOD_CONTROL, GLFW_KEY_O);
                menu_item(g, "Save", GLFW_MOD_CONTROL, GLFW_KEY_S);
                if(menu_item(g, "Quit", GLFW_MOD_CONTROL, GLFW_KEY_Q)) glfwSetWindowShouldClose(win, 1);
            }
            end_popup(g);

            begin_popup(g, 2, "Edit");
            {
                menu_item(g, "Undo", GLFW_MOD_CONTROL, GLFW_KEY_Z);
                menu_item(g, "Redo", GLFW_MOD_CONTROL, GLFW_KEY_Y);

                menu_item(g, "Cut", GLFW_MOD_CONTROL, GLFW_KEY_X);
                menu_item(g, "Copy", GLFW_MOD_CONTROL, GLFW_KEY_C);
                menu_item(g, "Paste", GLFW_MOD_CONTROL, GLFW_KEY_V);

                if(menu_item(g, "Select All", GLFW_MOD_CONTROL, GLFW_KEY_A))
                {
                    selection.clear();
                    for(auto * obj : objects) selection.insert(obj);
                }
            }
            end_popup(g);

            begin_popup(g, 3, "Help");
            {
                menu_item(g, "View Help", GLFW_MOD_CONTROL, GLFW_KEY_F1);
            }
            end_popup(g);
        }
        end_menu(g);

        auto s = hsplitter(g, 2, {0, 21, w, h}, split1);
        viewport_ui(g, 3, s.first, objects, selection);
        s = vsplitter(g, 4, s.second, split2);
        object_list_ui(g, 5, s.first, objects, selection, offset0);
        object_properties_ui(g, 6, s.second, selection, offset1);
        g.end_frame();

        switch(g.icon)
        {
        case cursor_icon::arrow: glfwSetCursor(win, arrow_cursor); break;
        case cursor_icon::ibeam: glfwSetCursor(win, ibeam_cursor); break;
        case cursor_icon::hresize: glfwSetCursor(win, hresize_cursor); break;
        case cursor_icon::vresize: glfwSetCursor(win, vresize_cursor); break;
        }

        const auto * per_scene = get_desc(*program).get_block_desc("PerScene");
        std::vector<byte> scene_buffer(per_scene->data_size);
        per_scene->set_uniform(scene_buffer.data(), "u_viewProj", g.get_viewproj_matrix());
        per_scene->set_uniform(scene_buffer.data(), "u_eyePos", g.cam.position);
        per_scene->set_uniform(scene_buffer.data(), "u_lightPos", plight->position);
        per_scene->set_uniform(scene_buffer.data(), "u_lightColor", plight->color);

        draw_list list;
        for(auto * obj : objects) obj->draw(list);

        glViewport(0, 0, fw, fh);
        glClearColor(0.1f, 0.1f, 0.1f, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        the_renderer.draw_scene(win, g.viewport3d, per_scene, scene_buffer.data(), list);
        the_renderer.draw_scene(win, g.viewport3d, per_scene, scene_buffer.data(), g.draw);
        render_gui(g, sprite_tex);

        glfwSwapBuffers(win);
    }
    glfwTerminate();
    return EXIT_SUCCESS;
}