#include "render_hud.hpp"
#include "render_helpers.hpp"
#include "../core/const.hpp"
#include "../core/settings.hpp"
#include "../core/map_manager.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

/**********************************************************************
RENDER HUD
Responsibilities
- Mode-specific control hints + status text (terrain/road/ocean/light/
  audio/ambience/pose/object palette + status panel)
- Settings menu pages (main/graphics/controls/maps)
**********************************************************************/

void editor_renderer_draw_hud(EditorRenderer& er, const EditorState& editor, const WorldMap& map){
    if (editor.mode == MODE_TERRAIN){
        if (!editor.paint_mode){
            font_draw(er.font, "[ TERRAIN MODE ]  P=paint mode", 220, 16, 3, 0.30f, 0.90f, 0.25f);
            font_draw(er.font, "LMB=raise  RMB=lower  SHIFT=smooth  [/]=brush size  H=exit",
                220, 40, 2, 0.30f, 0.90f, 0.25f);
            char brush_buf[64];
            snprintf(brush_buf, sizeof(brush_buf), "BRUSH RADIUS: %.1fm", editor.brush_radius);
            font_draw(er.font, brush_buf, 220, 60, 2, 0.30f, 0.90f, 0.25f);
        }
        else {
            font_draw(er.font, "[ PAINT MODE ]  P=sculpt mode", 220, 16, 3, 0.95f, 0.70f, 0.10f);
            font_draw(er.font, "LMB=paint  S=spline  [/]=brush  0=erase 1=asphalt 2=gravel 3=dirt 4=sand 5=grass 6=cement 7=rock",
                220, 40, 2, 0.95f, 0.70f, 0.10f);
            font_draw(er.font, "Ctrl+Shift+W=wipe canvas  H=exit terrain",
                220, 58, 2, 0.95f, 0.70f, 0.10f);
            char surf_buf[64];
            snprintf(surf_buf, sizeof(surf_buf), "SURFACE: %s   BRUSH: %.1fm",
                Const::SURFACE_NAMES[(int)editor.paint_surface], editor.brush_radius);
            font_draw(er.font, surf_buf, 220, 76, 2, 0.95f, 0.70f, 0.10f);
        }
    }

    if (editor.mode == MODE_ROAD){
        static const char* ROAD_TYPE_NAMES[ROAD_COUNT] = {
            "ASPHALT", "GRAVEL", "DIRT", "SAND", "GRASS", "CEMENT", "ROAD_LINES"
        };
        font_draw(er.font, "[ ROAD MODE ]", 180, 16, 3, 0.25f, 0.75f, 1.00f);
        font_draw(er.font, "LMB=add point  RMB=undo  ENTER=finish  DEL=delete  [/]=road type  M=exit",
            220, 40, 2, 0.25f, 0.75f, 1.00f);
        bool found = false;
        for (const auto& r : map.roads){
            if (r.id == editor.active_road_id){
                char buf[64];
                const char* type_name = ROAD_TYPE_NAMES[glm::clamp((int)r.type, 0, (int)ROAD_COUNT - 1)];
                snprintf(buf, sizeof(buf), "TYPE: %s   POINTS: %d", type_name, (int)r.points.size());
                font_draw(er.font, buf, 220, 60, 2, 0.25f, 0.75f, 1.00f);
                found = true;
                break;
            }
        }
        if (!found){
            char buf[64];
            snprintf(buf, sizeof(buf), "TYPE: %s   (no active spline)",
                ROAD_TYPE_NAMES[glm::clamp((int)editor.active_road_id, 0, (int)ROAD_COUNT - 1)]);
            font_draw(er.font, buf, 220, 60, 2, 0.50f, 0.50f, 0.50f);
        }
    }

    if (editor.mode == MODE_OCEAN){
        font_draw(er.font, "[ OCEAN MODE ]", 180, 16, 3, 0.10f, 0.55f, 0.90f);
        font_draw(er.font, "PgUp/Dn=y level  E=toggle on/off  [/]=rebuild  O=exit",
            220, 40, 2, 0.10f, 0.55f, 0.90f);
        char buf[64];
        snprintf(buf, sizeof(buf), "OCEAN Y: %.2f  [PgUp/Dn] nudge  [E] toggle %s",
            map.ocean.y_level, map.ocean.enabled ? "ON" : "OFF");
        font_draw(er.font, buf, 220, 60, 2, 1.0f, 0.80f, 0.10f);
    }

    if (editor.mode == MODE_LIGHT){
        font_draw(er.font, "[ LIGHT MODE ]", 180, 16, 3, 1.0f, 0.90f, 0.30f);
        font_draw(er.font, "LMB=place/select  DEL=delete  Arrows=move XZ  PgUp/Dn=move Y  [/]=radius  +/-=intensity",
            220, 40, 2, 1.0f, 0.90f, 0.30f);
        font_draw(er.font, "Q/E=red  Z/X=green  C/V=blue  L=exit",
            220, 58, 2, 1.0f, 0.90f, 0.30f);
        if (editor.selected_light_id != -1){
            for (const auto& l : map.lights){
                if (l.id != editor.selected_light_id) continue;
                char buf[128];
                snprintf(buf, sizeof(buf), "POS (%.1f, %.1f, %.1f)  R:%.2f G:%.2f B:%.2f",
                    l.position.x, l.position.y, l.position.z,
                    l.color.r, l.color.g, l.color.b);
                font_draw(er.font, buf, 220, 76, 2, 1.0f, 1.0f, 1.0f);
                snprintf(buf, sizeof(buf), "RADIUS: %.1fm   INTENSITY: %.2f",
                    l.radius, l.intensity);
                font_draw(er.font, buf, 220, 94, 2, 1.0f, 1.0f, 1.0f);
                break;
            }
        }
    }

    if (editor.mode == MODE_AUDIO){
        static const char* SLOT_NAMES[] = {
            "impact", "proximity",
            "hail", "pickup", "yap",
            "dropoff_good", "dropoff_bad",
            "crash_mild", "crash_heavy", "crash_rollover"
        };
        static constexpr int AUDIO_SLOT_COUNT = 10;
        static constexpr int AUDIO_PAGE_SIZE  = 8;

        font_draw(er.font, "[ AUDIO MODE ]", 180, 16, 3, 0.80f, 0.40f, 1.00f);
        font_draw(er.font, "TAB=cycle slot  1-8=assign file  UP/DN=scroll  DEL=clear  Z=exit",
            220, 40, 2, 0.80f, 0.40f, 1.00f);

        const WorldObject* target = nullptr;
        for (const auto& o : map.objects)
            if (o.id == editor.selected_id){ target = &o; break; }

        if (target){
            int x = 16, y = 60;
            font_draw(er.font, "SLOTS", x, y, 2, 0.9f, 0.9f, 0.9f);
            y += 28;

            auto get_slot_val = [&](const WorldObject& o, int slot) -> const std::string& {
                switch(slot){
                    case 0: return o.audio_impact;
                    case 1: return o.audio_proximity;
                    case 2: return o.audio_hail;
                    case 3: return o.audio_pickup;
                    case 4: return o.audio_yap;
                    case 5: return o.audio_dropoff_good;
                    case 6: return o.audio_dropoff_bad;
                    case 7: return o.audio_crash_mild;
                    case 8: return o.audio_crash_heavy;
                    case 9: return o.audio_crash_rollover;
                    default: return o.audio_impact;
                }
            };

            for (int i = 0; i < AUDIO_SLOT_COUNT; i++){
                bool active = (i == editor.audio_slot);
                const std::string& val = get_slot_val(*target, i);
                std::string label = std::string(active ? "> " : "  ")
                    + SLOT_NAMES[i] + ": "
                    + (val.empty() ? "(none)" : val.substr(val.find_last_of('/') + 1));
                if (label.size() > 36) label = label.substr(0, 34) + "..";
                float r = active ? 0.0f : 0.55f;
                float g = active ? 1.0f : 0.55f;
                float b = active ? 0.8f : 0.55f;
                font_draw(er.font, label, x, y, active ? 2 : 1, r, g, b);
                y += active ? 22 : 18;
            }

            int x2 = 340, y2 = 60;
            font_draw(er.font, "FILES", x2, y2, 2, 0.9f, 0.9f, 0.9f);
            y2 += 28;
            int total = (int)editor.audio_file_list.size();
            if (total == 0){
                font_draw(er.font, "no .wav/.ogg in assets/audio/", x2, y2, 1, 0.5f, 0.5f, 0.5f);
            }
            else {
                int end = std::min(editor.audio_file_page + AUDIO_PAGE_SIZE, total);
                for (int i = editor.audio_file_page; i < end; i++){
                    int slot_key = i - editor.audio_file_page + 1;
                    std::string name = editor.audio_file_list[i];
                    name = name.substr(name.find_last_of('/') + 1);
                    if (name.size() > 28) name = name.substr(0, 26) + "..";
                    std::string line = std::to_string(slot_key) + " " + name;
                    font_draw(er.font, line, x2, y2, 1, 0.75f, 0.75f, 0.75f);
                    y2 += 18;
                }
                char pg[64];
                snprintf(pg, sizeof(pg), "[%d-%d / %d]  UP/DN scroll",
                    editor.audio_file_page + 1,
                    std::min(editor.audio_file_page + AUDIO_PAGE_SIZE, total), total);
                font_draw(er.font, pg, x2, y2 + 4, 1, 0.4f, 0.4f, 0.4f);
            }
            if (editor.audio_slot == 1){
                char buf[64];
                snprintf(buf, sizeof(buf), "RADIUS: %.1fm  ([/] to adjust)", target->audio_radius);
                font_draw(er.font, buf, 16, Const::WINDOW_HEIGHT - 100, 2, 0.80f, 0.40f, 1.00f);
            }
        }
        else {
            font_draw(er.font, "no object selected", 220, 60, 2, 0.5f, 0.5f, 0.5f);
        }
    }

    if (editor.mode == MODE_AMBIENCE){
        font_draw(er.font, "[ AMBIENCE MODE ]", 180, 16, 3, 0.30f, 0.95f, 0.60f);
        font_draw(er.font, "LMB=place/select  DEL=delete  F=type  [/]=radius  1-8=assign audio  UP/DN=scroll  I=exit",
            220, 40, 2, 0.30f, 0.95f, 0.60f);

        for (int z = 0; z < map.ambience_count; z++){
            const AmbienceZone& zone = map.ambience_zones[z];
            if (zone.id != editor.selected_zone_id) continue;

            static constexpr int AMB_PAGE_SIZE = 8;
            int x = 16, y = 60;
            font_draw(er.font, "FILES", x, y, 2, 0.9f, 0.9f, 0.9f);
            y += 28;
            int total = (int)editor.audio_file_list.size();
            if (total == 0){
                font_draw(er.font, "no .wav/.ogg in assets/audio/", x, y, 1, 0.5f, 0.5f, 0.5f);
            }
            else {
                int end = std::min(editor.ambience_file_page + AMB_PAGE_SIZE, total);
                for (int i = editor.ambience_file_page; i < end; i++){
                    int slot_key = i - editor.ambience_file_page + 1;
                    std::string name = editor.audio_file_list[i];
                    name = name.substr(name.find_last_of('/') + 1);
                    if (name.size() > 28) name = name.substr(0, 26) + "..";
                    font_draw(er.font, std::to_string(slot_key) + " " + name,
                        x, y, 1, 0.75f, 0.75f, 0.75f);
                    y += 18;
                }
                char pg[32];
                snprintf(pg, sizeof(pg), "[%d-%d / %d]",
                    editor.ambience_file_page + 1,
                    std::min(editor.ambience_file_page + AMB_PAGE_SIZE, total), total);
                font_draw(er.font, pg, x, y + 4, 1, 0.4f, 0.4f, 0.4f);
            }
            char buf[128];
            snprintf(buf, sizeof(buf), "ZONE id=%d  TYPE: %s  RADIUS: %.1fm",
                zone.id,
                zone.type == AMBIENCE_NIGHT ? "NIGHT" : "PROXIMITY",
                zone.radius);
            font_draw(er.font, buf, 16, Const::WINDOW_HEIGHT - 120, 2, 0.30f, 0.95f, 0.60f);
            std::string apath = zone.audio_path[0] ? zone.audio_path : "(none)";
            std::string aname = apath.substr(apath.find_last_of('/') + 1);
            font_draw(er.font, "AUDIO: " + aname, 16, Const::WINDOW_HEIGHT - 98, 2, 0.30f, 0.95f, 0.60f);
            break;
        }
    }

    if (editor.mode == MODE_POSE){
        static const char* bone_names[6] = {
            "TORSO", "HEAD", "LEG_L", "LEG_R", "ARM_L", "ARM_R"
        };
        font_draw(er.font, "[ POSE MODE ]", 180, 16, 3, 1.0f, 0.60f, 0.10f);
        font_draw(er.font, "F=next bone  Arrows=rot XY  PgUp/Dn=rot Z  NP8/2=seat Z  NP4/6=seat X  NP+/-=seat Y",
            220, 40, 2, 1.0f, 0.60f, 0.10f);
        font_draw(er.font, "SHIFT=fine  ENTER=dump values  V=hail/mount toggle  K=exit", 220, 58, 2, 1.0f, 0.60f, 0.10f);
        if (editor.pose_npc_id != -1){
            const char* pose_label = editor.pose_editing_hail ? "EDITING: HAIL  [Ctrl+H to save]" : "EDITING: MOUNT  [Ctrl+M to save]";
            font_draw(er.font, pose_label, 220, 112, 2, editor.pose_editing_hail ? 0.4f : 0.2f, 1.0f, 0.4f);
        }
        const glm::quat& bq = editor.pose_quat[editor.pose_bone];
        float bangle = glm::degrees(2.0f * std::acos(glm::clamp(bq.w, -1.0f, 1.0f)));
        float bs = std::sqrt(std::max(0.0f, 1.0f - bq.w * bq.w));
        glm::vec3 baxis = (bs > 0.001f)
            ? glm::vec3(bq.x/bs, bq.y/bs, bq.z/bs)
            : glm::vec3(1,0,0);
        char buf[128];
        snprintf(buf, sizeof(buf), "BONE: %s  [%d]  axis(%.2f,%.2f,%.2f)  angle:%.1fdeg",
            bone_names[editor.pose_bone], editor.pose_bone,
            baxis.x, baxis.y, baxis.z, bangle);
        font_draw(er.font, buf, 220, 76, 2, 1.0f, 1.0f, 1.0f);
        if (editor.pose_numpad_translate){
            const glm::vec3& off = editor.pose_offset[editor.pose_bone];
            snprintf(buf, sizeof(buf), "NP=BONE TRANSLATE  X:%.3f  Y:%.3f  Z:%.3f  [NP0 for seat]",
                off.x, off.y, off.z);
            font_draw(er.font, buf, 220, 94, 2, 1.0f, 0.7f, 0.3f);
        }
        else {
            snprintf(buf, sizeof(buf), "NP=SEAT  X:%.3f  Y:%.3f  Z:%.3f  [NP0 for bone translate]",
                editor.pose_seat.x, editor.pose_seat.y, editor.pose_seat.z);
            font_draw(er.font, buf, 220, 94, 2, 0.7f, 1.0f, 0.7f);
        }
    }

    // prop palette
    if (editor.mode != MODE_AUDIO && editor.mode != MODE_AMBIENCE){
        const int PAGE_SIZE = Const::EDITOR_PAGE_SIZE;
        int total = (int)editor.prop_list.size();
        int x = 16, y = 60;
        font_draw(er.font, "PROPS", x, y, 2, 0.9f, 0.9f, 0.9f);
        y += 30;
        if (total == 0){
            font_draw(er.font, "no .obj in assets/props/", x, y, 1, 0.5f, 0.5f, 0.5f);
        }
        else {
            int page_start = editor.prop_page * PAGE_SIZE;
            int page_end   = std::min(page_start + PAGE_SIZE, total);
            for (int i = page_start; i < page_end; i++){
                int slot = i - page_start + 1;
                bool active = (editor.prop_list[i] == editor.selected_model);
                std::string name = editor.prop_list[i];
                if (name.size() > 20) name = name.substr(0, 18) + "..";
                std::string line = std::to_string(slot) + " " + name;
                if (active)
                    font_draw(er.font, line, x, y, 1, 0.0f, 1.0f, 1.0f);
                else
                    font_draw(er.font, line, x, y, 1, 0.7f, 0.7f, 0.7f);
                y += 20;
            }
            int max_page = (total - 1) / PAGE_SIZE;
            std::string page_str = "[ pg " + std::to_string(editor.prop_page + 1)
                + "/" + std::to_string(max_page + 1) + " ]";
            font_draw(er.font, page_str, x, y + 4, 3, 0.5f, 0.5f, 0.5f);
        }
    }

    // status HUD
    {
        int x = 16;
        int bottom = Const::WINDOW_HEIGHT - 220;
        int y = bottom;
        const char* tool_str = editor.tool == TOOL_TRANSLATE ? "TRANSLATE" :
            editor.tool == TOOL_ROTATE ? "ROTATE" : "SCALE";
        font_draw(er.font, std::string("TOOL: ") + tool_str, x, y, 2, 1.0f, 1.0f, 0.2f);
        y += 20;
        std::string model_label = "MODEL: " + (editor.selected_model.empty() ? "(none)" : editor.selected_model);
        font_draw(er.font, model_label, x, y, 2, 1.0f, 1.0f, 1.0f);
        y += 20;
        font_draw(er.font, "OBJECTS: " + std::to_string(map.objects.size()), x, y, 2, 0.7f, 0.7f, 0.7f);
        y += 20;
        if (editor.selected_id != -1){
            for (const auto& o : map.objects){
                if (o.id != editor.selected_id) continue;
                char buf[128];
                snprintf(buf, sizeof(buf), "POS X:%.1f  Y:%.1f  Z:%.1f",
                    o.position.x, o.position.y, o.position.z);
                font_draw(er.font, buf, x, y, 2, 0.6f, 1.0f, 0.6f);
                y += 20;
                snprintf(buf, sizeof(buf), "ROT Y:%.1f deg", glm::degrees(o.rotation.y));
                font_draw(er.font, buf, x, y, 2, 0.6f, 1.0f, 0.6f);
                y += 20;
                snprintf(buf, sizeof(buf), "SCALE X:%.2f  Y:%.2f  Z:%.2f",
                    o.scale.x, o.scale.y, o.scale.z);
                font_draw(er.font, buf, x, y, 2, 0.6f, 1.0f, 0.6f);
                y += 20;
                const char* bname = "STATIC";
                float br = 0.55f, bg = 0.55f, bb = 0.55f;
                switch(o.behavior){
                    case STATIC:     bname="STATIC";     br=0.55f; bg=0.55f; bb=0.55f; break;
                    case DYNAMIC:    bname="DYNAMIC";    br=0.20f; bg=0.50f; bb=1.00f; break;
                    case DECORATION: bname="DECORATION"; br=0.95f; bg=0.80f; bb=0.10f; break;
                    case PEDESTRIAN: bname="PEDESTRIAN"; br=0.20f; bg=0.85f; bb=0.30f; break;
                }
                snprintf(buf, sizeof(buf), "BEHAVIOR: %s  [B] cycle", bname);
                font_draw(er.font, buf, x, y, 2, br, bg, bb);
                y += 20;
                if (o.behavior == DYNAMIC){
                    snprintf(buf, sizeof(buf), "MASS:%.1f  REST:%.2f  FRIC:%.2f  [N] preset",
                        o.mass, o.restitution, o.friction);
                    font_draw(er.font, buf, x, y, 2, 0.4f, 0.8f, 1.0f);
                }
                break;
            }
        }
    }
}

void editor_renderer_draw_settings_menu(EditorRenderer& er, const EditorState& editor){
    draw_settings_overlay(er);
    // full-screen dark overlay here so world is still visible but dimmed
    // drawn as a screen-space font overlay no GL geometry needed
    // font coords are pixel space: 0,0 top left
    
    static const int SW = Const::WINDOW_WIDTH;
    static const int SH = Const::WINDOW_HEIGHT;
    static const int CX = SW / 2;

    // CONTROLS PAGE

     /*
        HOW TO ADD A NEW KEYBIND TO THIS SCREEN

        1. find the ControlPage in PAGES[] matching the mode your key belongs to
           (DRIVE MODE, OBJECT MODE, TERRAIN & ROAD, LIGHT/OCEAN/AMBIENCE, POSE & AUDIO)
        2. add a row to that page's keys[][] array:
               { "KEY NAME",  "What it does" },
           - left string  = the key/combo exactly as pressed, eg "Ctrl+S", "[ / ]", "PgUp / PgDn"
           - right string = short present-tense description, eg "Save map"
        3. increment that page's count by 1 IMPORTANT: it must match the number of rows you filled
        4. keys[20][2] is a hard cap of 20 rows per page if a page is full,
           either trim an existing row or start a new ControlPage and bump PAGE_COUNT below

        that's it rows auto-split into two columns and the page header/pagination
        update themselves from title/desc/count, no other code needs to change
        */

    if (editor.settings_page == SETTINGS_PAGE_CONTROLS){

        struct ControlPage {
            const char* title;
            const char* desc;
            const char* keys[20][2]; // [key, action]
            int count;
        };

        static const ControlPage PAGES[] = {
            {
                "DRIVE MODE",
                "Controls while driving or on foot in the barangay.",
                {
                    { "W",            "Accelerate"                    },
                    { "S",            "Brake  /  Reverse"             },
                    { "A / D",        "Steer left / right"            },
                    { "E",            "Mount or dismount trike"       },
                    { "Q",            "Accept hailing passenger"      },
                    { "L",            "Toggle headlights"             },
                    { "P",            "Radio on / off"                },
                    { "/",            "Next radio track"              },
                    { "R",            "Reset trike and position"      },
                    { "F",            "Free camera toggle"            },
                    { "H",            "Show collision hitboxes"       },
                    { "Arrows",       "Orbit camera around trike"     },
                    { "TAB",          "Enter editor mode"             },
                    { "ESC",          "Open settings menu"            },
                },
                14
            },
            {
                "OBJECT MODE",
                "Place, select, and transform props in the world.",
                {
                    { "L Click",      "Place prop / select object"    },
                    { "Ctrl+Click",   "Select smallest object hit"    },
                    { "Shift+Click",  "Place on top of selected"      },
                    { "DEL",          "Delete selected object"        },
                    { "T",            "Translate tool"                },
                    { "R",            "Rotate tool"                   },
                    { "Y",            "Scale tool"                    },
                    { "Arrows",       "Move / rotate / scale"         },
                    { "Shift+Arrows", "Fine step (5cm)"               },
                    { "PgUp / PgDn",  "Nudge Y up / down"            },
                    { "B",            "Cycle behavior (Static etc)"   },
                    { "N",            "Cycle physics preset (Dynamic)"},
                    { "1-9",          "Select prop from palette"      },
                    { "[ / ]",        "Prev / next prop page"         },
                    { "Ctrl+C / V",   "Copy / paste object"           },
                    { "Ctrl+S",       "Save map"                      },
                    { "F5",           "Rescan assets folder"          },
                },
                17
            },
            {
                "TERRAIN  &  ROAD",
                "Sculpt the heightfield and lay road splines.",
                {
                    { "H",            "Toggle terrain sculpt mode"    },
                    { "L Click hold", "Raise terrain"                 },
                    { "R Click hold", "Lower terrain"                 },
                    { "Shift+Click",  "Smooth brush"                  },
                    { "[ / ]",        "Shrink / grow brush radius"    },
                    { "Ctrl+Z",       "Undo last sculpt stroke"       },
                    { "P",            "Toggle surface paint mode"     },
                    { "0-7",          "Select surface type to paint"  },
                    { "Ctrl+Shift+W", "Wipe entire surface canvas"    },
                    { "M",            "Toggle road spline mode"       },
                    { "L Click",      "Add spline control point"      },
                    { "R Click",      "Undo last control point"       },
                    { "[ / ]",        "Cycle road type"               },
                    { "ENTER",        "Finish spline"                 },
                    { "DEL",          "Delete active spline"          },
                    { "Ctrl+S",       "Save"                          },
                },
                16
            },
            {
                "LIGHT, OCEAN, AMBIENCE",
                "Place point lights, water zones, and ambient audio.",
                {
                    { "L",            "Toggle light placement mode"   },
                    { "L Click",      "Place or select a light"       },
                    { "Arrows",       "Move selected light XZ"        },
                    { "PgUp / PgDn",  "Move selected light Y"         },
                    { "[ / ]",        "Adjust light radius"           },
                    { "+ / -",        "Adjust intensity"              },
                    { "Q/E  Z/X  C/V","Tune R / G / B tint"           },
                    { "DEL",          "Delete selected light"         },
                    { "O",            "Toggle ocean mode"             },
                    { "PgUp / PgDn",  "Nudge ocean Y level"           },
                    { "E",            "Toggle ocean on / off"         },
                    { "I",            "Toggle ambience zone mode"     },
                    { "L Click",      "Place or select zone"          },
                    { "[ / ]",        "Adjust zone radius"            },
                    { "F",            "Toggle zone type (Night/Prox)" },
                    { "1-8",          "Assign audio file to zone"     },
                },
                16
            },
            {
                "POSE  &  AUDIO",
                "Edit driver and NPC bone poses. Assign object audio.",
                {
                    { "K",            "Toggle pose editor mode"       },
                    { "F",            "Cycle active bone"             },
                    { "Arrows",       "Rotate bone X / Y axis"        },
                    { "PgUp / PgDn",  "Rotate bone Z axis"            },
                    { "Shift+any",    "Fine rotation mode"            },
                    { "NP0",          "Toggle seat / bone translate"  },
                    { "NP8/2/4/6",    "Move seat or bone offset"      },
                    { "NP+ / NP-",    "Seat / bone Y up / down"       },
                    { "V",            "Toggle hail / mount pose"      },
                    { "Ctrl+H",       "Save hail pose to NPC"         },
                    { "Ctrl+M",       "Save mount pose to NPC"        },
                    { "Ctrl+S",       "Save driver pose to file"      },
                    { "ENTER",        "Dump pose as code to console"  },
                    { "Z",            "Toggle audio editor mode"      },
                    { "TAB",          "Cycle audio slot"              },
                    { "1-8",          "Assign audio file to slot"     },
                    { "DEL",          "Clear audio slot"              },
                },
                17
            },
        };
        static const int PAGE_COUNT = 5;

        // settings_cursor doubles as the sub-page index on the controls page
        // clamped in input handling
        int sub = glm::clamp(editor.settings_cursor, 0, PAGE_COUNT - 1);
        const ControlPage& cp = PAGES[sub];

        // header
        char pg_buf[32];
        snprintf(pg_buf, sizeof(pg_buf), "PAGE %d / %d", sub + 1, PAGE_COUNT);
        font_draw(er.font, "CONTROLS", 60, 140, 5, 1.0f, 1.0f, 1.0f);
        font_draw(er.font, pg_buf, 380, 152, 3, 0.5f, 0.5f, 0.5f);
        font_draw(er.font, cp.title, 60, 220, 4, 0.20f, 1.00f, 0.55f);
        font_draw(er.font, cp.desc, 60, 265, 2, 0.60f, 0.60f, 0.60f);

        // key list 
        // two columns
        int col_x[2] = { 60, 780 };
        int y = 295;
        int half = (cp.count + 1) / 2;
        for (int i = 0; i < cp.count; i++){
            int col = i / half;
            int row = i % half;
            int rx = col_x[col];
            int ry = y + row * 28;
            font_draw(er.font, cp.keys[i][0], rx, ry, 2, 0.90f, 0.85f, 0.40f);
            font_draw(er.font, cp.keys[i][1], rx + 200, ry, 2, 0.80f, 0.80f, 0.80f);
        }
        
        // nav hints
        font_draw(er.font, "LEFT / RIGHT = change page",
            60, SH - 58, 3, 0.6f, 0.6f, 0.6f);
        font_draw(er.font, "ESC = close ENTER = back to settings",
            60, SH - 28, 3, 0.6f, 0.6f, 0.6f);
        return;
    }



    // GRAPHICS PAGE
    if (editor.settings_page == SETTINGS_PAGE_GRAPHICS){
        font_draw(er.font, "GRAPHICS", 60, 80, 5, 1.0f, 1.0f, 1.0f);

        // preset row
        {
            bool sel = (editor.settings_cursor == 0);
            font_draw(er.font, "PRESET", 60, 170, 3, 0.9f, 0.9f, 0.9f);
            const char* presets[] = { "LOW", "MODERATE", "HIGH", "CUSTOM" };
            int px = 280;
            for (int i = 0; i < 4; i++){
                bool active = ((int)my_settings.preset == i);
                float r = active ? 0.20f : 0.45f;
                float g = active ? 1.00f : 0.45f;
                float b = active ? 0.55f : 0.45f;
                if (sel && active){ r = 1.0f; g = 1.0f; b = 0.3f; }
                font_draw(er.font, presets[i], px, 170, 3, r, g, b);
                px += (int)(strlen(presets[i]) * 20 + 50);
            }
        }

        // individual settings rows
        // layout: label | [-] value [+]
        struct Row {
            const char* label;
            const char* unit;
            float val;
            bool is_bool;
            bool bool_val;
        };

        Row rows[] = {
            { "SHADOW MAP SIZE", "px",  (float)my_settings.shadow_map_size,       false, false },
            { "SHADOW THROTTLE", "frm", (float)my_settings.shadow_throttle_frame, false, false },
            { "PROP CULL DIST",  "m",   my_settings.prop_cull_dist,               false, false },
            { "NPC CULL DIST",   "m",   my_settings.npc_cull_dist,                false, false },
            { "LIGHT CULL DIST", "m",   my_settings.light_cull_dist,              false, false },
            { "RAIN PARTICLES",  "",    (float)my_settings.rain_particle_count,   false, false },
            { "RAIN SPLASHES",   "",    (float)my_settings.rain_splash_max,       false, false },
            { "RENDER SHADOWS",  "",    0.0f, true,  my_settings.render_shadows },
            { "SHOW HUD",        "",    0.0f, true,  my_settings.show_hud       },
            { "RENDER FOG",      "",    0.0f, true,  my_settings.render_fog    },
        };
        static const int ROW_COUNT = 10;

        int y = 240;
        for (int i = 0; i < ROW_COUNT; i++){
            bool sel = (editor.settings_cursor == i + 1); // +1 because row 0 = preset
            float lr = sel ? 1.0f : 0.70f;
            float lg = sel ? 1.0f : 0.70f;
            float lb = sel ? 0.3f : 0.70f;

            font_draw(er.font, rows[i].label, 60, y, 3, lr, lg, lb);

            if (rows[i].is_bool){
                const char* bval = rows[i].bool_val ? "ON" : "OFF";
                float vr = rows[i].bool_val ? 0.20f : 0.70f;
                float vg = rows[i].bool_val ? 1.00f : 0.30f;
                float vb = rows[i].bool_val ? 0.55f : 0.30f;
                if (sel){ font_draw(er.font, "<", 540, y, 3, 1.0f, 1.0f, 1.0f); }
                font_draw(er.font, bval, 580, y, 3, vr, vg, vb);
                if (sel){ font_draw(er.font, ">", 660, y, 3, 1.0f, 1.0f, 1.0f); }
            }
            else {
                char vbuf[32];
                snprintf(vbuf, sizeof(vbuf), "%.0f %s", rows[i].val, rows[i].unit);
                if (sel){ font_draw(er.font, "<", 540, y, 3, 1.0f, 1.0f, 1.0f); }
                font_draw(er.font, vbuf, 580, y, 3, 0.85f, 0.85f, 0.85f);
                if (sel){ font_draw(er.font, ">", 760, y, 3, 1.0f, 1.0f, 1.0f); }
            }
            y += 42;
        }

        // back row
        {
            bool sel = (editor.settings_cursor == ROW_COUNT + 1);
            font_draw(er.font, "BACK", 60, y + 10, sel ? 4 : 3,
                sel ? 0.20f : 0.60f,
                sel ? 1.00f : 0.60f,
                sel ? 0.55f : 0.60f);
        }

        font_draw(er.font, "UP/DN=navigate  LEFT/RIGHT=adjust  ENTER=preset/back  ESC=close",
            60, SH - 50, 3, 0.6f, 0.6f, 0.6f);
        return;
    }

    // MAPS PAGE
    if (editor.settings_page == SETTINGS_PAGE_MAPS){
        font_draw(er.font, "MAPS", 60, 80, 5, 1.0f, 1.0f, 1.0f);

        int total = (int)g_maps.maps.size();
        int y = 180;

        if (g_maps.rename_mode){
            font_draw(er.font, "RENAME:", 60, y, 3, 0.90f, 0.90f, 0.30f);
            std::string display = g_maps.rename_buf + "_";
            font_draw(er.font, display, 260, y, 3, 1.0f, 1.0f, 1.0f);
            font_draw(er.font, "ENTER=confirm  BACKSPACE=delete",
                60, SH - 50, 3, 0.6f, 0.6f, 0.6f);
            return;
        }

        for (int i = 0; i < total; i++){
            bool sel  = (editor.settings_cursor == i);
            bool active = (i == g_maps.active_index);
            float r = sel ? 0.20f : (active ? 0.90f : 0.65f);
            float g2 = sel ? 1.00f : (active ? 0.90f : 0.65f);
            float b  = sel ? 0.55f : (active ? 0.30f : 0.65f);
            std::string label = (active ? "* " : "  ") + g_maps.maps[i].name;
            font_draw(er.font, label, 60, y, sel ? 4 : 3, r, g2, b);
            y += sel ? 52 : 40;
        }

        // [NEW MAP] row
        {
            bool sel = (editor.settings_cursor == total);
            font_draw(er.font, "+ NEW MAP", 60, y + 8, sel ? 4 : 3,
                sel ? 0.20f : 0.50f,
                sel ? 1.00f : 0.50f,
                sel ? 0.55f : 0.50f);
        }

        font_draw(er.font, "UP/DN=select  ENTER=switch  F2=rename  LEFT=back",
            60, SH - 50, 3, 0.6f, 0.6f, 0.6f);
        return;
    }

    // MAIN PAGE
    font_draw(er.font, "SETTINGS", 100, SH/2 - 100, 5, 1.0f, 1.0f, 1.0f);
    const char* items[] = { "GRAPHICS", "CONTROLS", "CHANGE MAPS", "QUIT" };
    for (int i = 0; i < 4; i++){
        bool sel = (editor.settings_cursor == i);
        float r = sel ? 0.20f : 0.60f;
        float g = sel ? 1.00f : 0.60f;
        float b = sel ? 0.55f : 0.60f;
        font_draw(er.font, items[i], 100, SH/2 - 10 + i * 50, 4, r, g, b);
    }

    font_draw(er.font, "UP/DN=navigate ENTER=select ESC=close",
        100, SH/2 + 230, 2, 0.4f, 0.4f, 0.4f);
}