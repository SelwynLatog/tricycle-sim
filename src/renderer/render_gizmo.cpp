#include "render_gizmo.hpp"
#include "render_helpers.hpp"
#include "render_props.hpp"
#include "../core/const.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

/**********************************************************************
RENDER GIZMO
Responsibilities
- Snap grid draw (object mode)
- Light mode gizmo wires (stems + radius circles)
- Ambience mode gizmo wires (radius circles + center crosses)
- Placed object hitbox wireframes (color by behavior)
- Ghost placement box + selection highlight
- Road mode cursor diamond + preview line
**********************************************************************/

void editor_renderer_draw(EditorRenderer& er, const EditorState& editor, const WorldMap& map,
    const glm::mat4& view, const glm::mat4& proj, bool show_hitboxes,
    const std::vector<LightSource>& lights){

    shader_bind(er.shader);
    set_mat4(er.shader, "u_model", glm::mat4(1.0f));
    set_mat4(er.shader, "u_view", view);
    set_mat4(er.shader, "u_proj", proj);

    // SNAP GRID
    // object mode only
    if (editor.mode == MODE_OBJECT){
        glBindVertexArray(er.grid.vao);
        glDrawArrays(GL_LINES, 0, er.grid.count);
        glBindVertexArray(0);
    }

    // LIGHT MODE
    // wire stems + radius circles
    if (editor.mode == MODE_LIGHT){
        er.line_verts.clear();
        for (const auto& l : map.lights){
            bool selected = (l.id == editor.selected_light_id);
            glm::vec3 col = selected ? glm::vec3(1.0f, 1.0f, 0.0f) : glm::vec3(l.color);
            float ground_y = heightfield_sample(map.terrain, l.position.x, l.position.z);
            er.line_verts.insert(er.line_verts.end(),
                {l.position.x, ground_y, l.position.z, col.r, col.g, col.b});
            er.line_verts.insert(er.line_verts.end(),
                {l.position.x, l.position.y, l.position.z, col.r, col.g, col.b});
            static const int SEGS = 32;
            for (int i = 0; i < SEGS; i++){
                float a0 = (float)i / SEGS * 2.0f * 3.14159265f;
                float a1 = (float)(i + 1) / SEGS * 2.0f * 3.14159265f;
                float x0 = l.position.x + std::cos(a0) * l.radius;
                float z0 = l.position.z + std::sin(a0) * l.radius;
                float x1 = l.position.x + std::cos(a1) * l.radius;
                float z1 = l.position.z + std::sin(a1) * l.radius;
                er.line_verts.insert(er.line_verts.end(), {x0, ground_y + 0.05f, z0, col.r, col.g, col.b});
                er.line_verts.insert(er.line_verts.end(), {x1, ground_y + 0.05f, z1, col.r, col.g, col.b});
            }
        }
        flush_line_batch(er, er.shader, view, proj);
    }

    // AMBIENCE SUB AUDIO MODE
    // radius circles + center crosses
    if (editor.mode == MODE_AMBIENCE){
        er.line_verts.clear();
        static const int SEGS = 40;
        for (int z = 0; z < map.ambience_count; z++){
            const AmbienceZone& zone = map.ambience_zones[z];
            bool selected = (zone.id == editor.selected_zone_id);
            glm::vec3 col = selected
                ? glm::vec3(1.0f, 1.0f, 1.0f)
                : (zone.type == AMBIENCE_NIGHT
                    ? glm::vec3(0.55f, 0.20f, 0.90f)
                    : glm::vec3(0.10f, 0.85f, 0.55f));
            float ground_y = heightfield_sample(map.terrain, zone.pos.x, zone.pos.z) + 0.1f;
            float cs = 0.5f;
            er.line_verts.insert(er.line_verts.end(),
                {zone.pos.x - cs, ground_y, zone.pos.z, col.r, col.g, col.b});
            er.line_verts.insert(er.line_verts.end(),
                {zone.pos.x + cs, ground_y, zone.pos.z, col.r, col.g, col.b});
            er.line_verts.insert(er.line_verts.end(),
                {zone.pos.x, ground_y, zone.pos.z - cs, col.r, col.g, col.b});
            er.line_verts.insert(er.line_verts.end(),
                {zone.pos.x, ground_y, zone.pos.z + cs, col.r, col.g, col.b});
            for (int i = 0; i < SEGS; i++){
                float a0 = (float)i / SEGS * 2.0f * 3.14159265f;
                float a1 = (float)(i + 1) / SEGS * 2.0f * 3.14159265f;
                float x0 = zone.pos.x + std::cos(a0) * zone.radius;
                float z0 = zone.pos.z + std::sin(a0) * zone.radius;
                float x1 = zone.pos.x + std::cos(a1) * zone.radius;
                float z1 = zone.pos.z + std::sin(a1) * zone.radius;
                er.line_verts.insert(er.line_verts.end(), {x0, ground_y, z0, col.r, col.g, col.b});
                er.line_verts.insert(er.line_verts.end(), {x1, ground_y, z1, col.r, col.g, col.b});
            }
        }
        flush_line_batch(er, er.shader, view, proj);
    }

    // wireframe boxes by behavior
    if (show_hitboxes){
        er.line_verts.clear();
        for (const auto& o : map.objects){
            if (o.id == editor.selected_id) continue;
            auto bit = er.prop_bounds.find(o.model_path);
            if (bit != er.prop_bounds.end()){
                float yoff = er.prop_y_offset.count(o.model_path)
                    ? er.prop_y_offset[o.model_path] : 0.0f;
                glm::vec3 wmin, wmax;
                rotated_world_bounds(bit->second.local_min, bit->second.local_max,
                    o.position, o.rotation.y, o.scale, yoff, wmin, wmax);
                push_wire_box(er.line_verts, wmin, wmax, behavior_color(o.behavior));
            }
            else {
                glm::vec3 half = o.scale * 0.5f;
                push_wire_box(er.line_verts,
                    o.position + glm::vec3(-half.x, 0.0f, -half.z),
                    o.position + glm::vec3( half.x, o.scale.y, half.z),
                    behavior_color(o.behavior));
            }
        }
        flush_line_batch(er, er.shader, view, proj);
    }

    // placed prop meshes
    editor_renderer_draw_props(er, map, view, proj, {}, {}, lights);

    // ghost box at cursor
    if (editor.placement_valid && !editor.selected_model.empty()){
        glm::vec3 gp = editor.ghost_pos;
        er.line_verts.clear();
        push_wire_box(er.line_verts,
            gp + glm::vec3(-0.5f, 0.0f, -0.5f),
            gp + glm::vec3( 0.5f, 1.0f,  0.5f),
            {0.0f, 1.0f, 1.0f});
        flush_line_batch(er, er.shader, view, proj);
    }

    // selection highlight
    if (editor.selected_id != -1){
        er.line_verts.clear();
        for (const auto& o : map.objects){
            if (o.id != editor.selected_id) continue;
            auto bit = er.prop_bounds.find(o.model_path);
            if (bit != er.prop_bounds.end()){
                float yoff = er.prop_y_offset.count(o.model_path)
                    ? er.prop_y_offset[o.model_path] : 0.0f;
                glm::vec3 wmin, wmax;
                rotated_world_bounds(bit->second.local_min, bit->second.local_max,
                    o.position, o.rotation.y, o.scale, yoff, wmin, wmax);
                push_wire_box(er.line_verts, wmin, wmax, {1.0f, 0.55f, 0.0f});
            }
            else {
                glm::vec3 half = o.scale * 0.5f;
                push_wire_box(er.line_verts,
                    o.position + glm::vec3(-half.x, 0.0f, -half.z),
                    o.position + glm::vec3( half.x, o.scale.y, half.z),
                    {1.0f, 0.55f, 0.0f});
            }
            break;
        }
        flush_line_batch(er, er.shader, view, proj);
    }

    // ROAD MODE
    // cursor diamond + preview line
    if (editor.mode == MODE_ROAD && editor.placement_valid){
        glm::vec3 p = editor.ghost_pos;
        float s = 0.4f;
        glm::vec3 top = p + glm::vec3( 0,  s,  0);
        glm::vec3 bot = p + glm::vec3( 0, -s,  0);
        glm::vec3 lft = p + glm::vec3(-s,  0,  0);
        glm::vec3 rgt = p + glm::vec3( s,  0,  0);
        glm::vec3 fwd = p + glm::vec3( 0,  0, -s);
        glm::vec3 bck = p + glm::vec3( 0,  0,  s);
        std::vector<float> diamond;
        auto push_edge = [&](glm::vec3 a, glm::vec3 b){
            diamond.insert(diamond.end(), {a.x,a.y,a.z, 0.25f,0.75f,1.00f});
            diamond.insert(diamond.end(), {b.x,b.y,b.z, 0.25f,0.75f,1.00f});
        };
        push_edge(top, lft); push_edge(top, rgt);
        push_edge(top, fwd); push_edge(top, bck);
        push_edge(bot, lft); push_edge(bot, rgt);
        push_edge(bot, fwd); push_edge(bot, bck);
        push_edge(lft, fwd); push_edge(fwd, rgt);
        push_edge(rgt, bck); push_edge(bck, lft);
        Mesh dm;
        mesh_init(dm, diamond);
        shader_bind(er.shader);
        set_mat4(er.shader, "u_model", glm::mat4(1.0f));
        set_mat4(er.shader, "u_view",  view);
        set_mat4(er.shader, "u_proj",  proj);
        glBindVertexArray(dm.vao);
        glDrawArrays(GL_LINES, 0, dm.count);
        glBindVertexArray(0);
        mesh_destroy(dm);
        for (const auto& r : map.roads){
            if (r.id != editor.active_road_id) continue;
            if (r.points.empty()) break;
            glm::vec3 last = r.points.back();
            std::vector<float> preview = {
                last.x, last.y, last.z, 0.25f, 0.75f, 1.00f,
                p.x, p.y, p.z, 0.25f, 0.75f, 1.00f,
            };
            Mesh pm;
            mesh_init(pm, preview);
            glBindVertexArray(pm.vao);
            glDrawArrays(GL_LINES, 0, pm.count);
            glBindVertexArray(0);
            mesh_destroy(pm);
            break;
        }
    }
}