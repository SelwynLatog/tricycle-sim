#include "../core/const.hpp"
#include "ocean.hpp"
#include "height_field.hpp"
#include <glad/glad.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <unordered_map>

void ocean_build_mesh(Ocean& ocean, const HeightField& hf, float x_min, float x_max, float z_min, float z_max){
    if (ocean.mesh.vao) mesh_destroy(ocean.mesh);

    float spacing = Const::OCEAN_GRID_SPACING;
    float y = ocean.y_level;

    int cols = (int)std::ceil((x_max - x_min) / spacing) + 1;
    int rows = (int)std::ceil((z_max - z_min) / spacing) + 1;
    if (cols < 2 || rows < 2) return;

    float w = x_max - x_min;
    float d = z_max - z_min;

    // margin below y_level a cell's terrain must clear to count as "real water".
    // isolated shallow dips (potholes, road seams) that dip just under y_level
    // but aren't meaningfully below it get treated as dry land instead.
    float submerge_margin = Const::OCEAN_SUBMERGE_MARGIN; // e.g. 0.15f

    std::vector<float> verts;
    verts.reserve(rows * cols * 7);
    // per-vertex water/land flag so index generation can skip dry quads
    std::vector<bool> is_wet;
    is_wet.reserve(rows * cols);

    ocean.max_depth = 0.01f; // avoid div by zero in shader if terrain is flat

    for (int r = 0; r < rows; r++){
        for (int c = 0; c < cols; c++){
            float x = x_min + c * spacing;
            float z = z_min + r * spacing;
            if (x > x_max) x = x_max;
            if (z > z_max) z = z_max;

            float edge_x = std::min(x - x_min, x_max - x) / (w * 0.5f);
            float edge_z = std::min(z - z_min, z_max - z) / (d * 0.5f);
            float depth = std::min(glm::clamp(edge_x, 0.0f, 1.0f),
                                   glm::clamp(edge_z, 0.0f, 1.0f));

            float cr = 0.04f + depth * 0.01f;
            float cg = 0.52f - depth * (0.52f - 0.18f);
            float cb = 0.58f + depth * (0.42f - 0.58f);

            float terrain_h = heightfield_sample(hf, x, z);
            // vertical distance from water surface down to the seafloor
            // baked once here so the frag shader can do real depth-based
            // color falloff without sampling the heightfield at runtime
            float water_depth = std::max(y - terrain_h, 0.0f);
            ocean.max_depth = std::max(ocean.max_depth, water_depth);

            verts.insert(verts.end(), { x, y, z, cr, cg, cb, water_depth });

            is_wet.push_back(terrain_h < y - submerge_margin);
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve((rows-1)*(cols-1)*6);

    // skirt depth: how far below the water surface the downward curtain hangs.
    // needs to clear the deepest local terrain dip near shore so the skybox
    // never shows through the wave-crest gap regardless of wave phase
    float skirt_depth = Const::OCEAN_SKIRT_DEPTH; // e.g. 2.0f

    for (int r = 0; r < rows-1; r++){
        for (int c = 0; c < cols-1; c++){
            unsigned int tl = r*cols + c;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + cols;
            unsigned int br = bl + 1;

            // only emit this quad if ALL four corners are actually submerged
            // this is what stops isolated dry-land dips (roads, potholes) from
            // getting covered by ocean just because they briefly dip below y_level
            bool all_wet = is_wet[tl] && is_wet[tr] && is_wet[bl] && is_wet[br];
            if (!all_wet) continue;

            indices.insert(indices.end(), { tl, bl, tr, bl, br, tr });
        }
    }

    // SKIRT PASS
    // for every wet quad edge that borders a dry/missing quad, hang a vertical
    // curtain down from that edge to skirt_depth below. this is what stops the
    // skybox showing through the gap between wave crest and shoreline terrain -
    // the curtain sits behind the surface at all wave phases since it always
    // hangs straight down regardless of where the wave pushed the surface verts
    auto quad_is_wet = [&](int r, int c) -> bool {
        if (r < 0 || c < 0 || r >= rows-1 || c >= cols-1) return false;
        unsigned int tl = r*cols + c, tr = tl+1, bl = tl+cols, br = bl+1;
        return is_wet[tl] && is_wet[tr] && is_wet[bl] && is_wet[br];
    };

    unsigned int skirt_base = (unsigned int)(verts.size() / 7);
    for (int r = 0; r < rows-1; r++){
        for (int c = 0; c < cols-1; c++){
            if (!quad_is_wet(r, c)) continue;

            unsigned int tl = r*cols + c;
            unsigned int tr = tl + 1;
            unsigned int bl = tl + cols;
            unsigned int br = bl + 1;

            // check each of the 4 neighbor quads; if dry/off-grid, this edge
            // is a boundary and needs a curtain hanging down from it
            struct Edge { unsigned int a, b; int nr, nc; };
            Edge edges[4] = {
                { tl, tr, r-1, c   }, // top edge, neighbor above
                { bl, br, r+1, c   }, // bottom edge, neighbor below
                { tl, bl, r,   c-1 }, // left edge, neighbor left
                { tr, br, r,   c+1 }, // right edge, neighbor right
            };

            for (auto& e : edges){
                if (quad_is_wet(e.nr, e.nc)) continue; // interior edge, no curtain needed

                // duplicate the two edge verts at skirt_depth below, tinted as deep water
                for (unsigned int idx : { e.a, e.b }){
                    float vx = verts[idx*7 + 0];
                    float vy = verts[idx*7 + 1];
                    float vz = verts[idx*7 + 2];
                    verts.insert(verts.end(), { vx, vy - skirt_depth, vz, 0.02f, 0.15f, 0.35f, skirt_depth });
                }
                unsigned int sa = (unsigned int)(verts.size()/7) - 2; // skirt copy of e.a
                unsigned int sb = sa + 1;                              // skirt copy of e.b

                // two tris forming the vertical curtain quad (a, b at surface; sa, sb below)
                indices.insert(indices.end(), { e.a, sa, e.b, sa, sb, e.b });
            }
        }
    }


    glGenVertexArrays(1, &ocean.mesh.vao);
    GLuint vbo, ebo;
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(ocean.mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    ocean.mesh.vbo = vbo;
    ocean.mesh.count = (int)indices.size();
    ocean.mesh_dirty = false;

    std::cout << "[ocean] built mesh " << rows*cols << " verts + skirt (" << (verts.size()/7 - rows*cols) << " skirt verts)\n";
}

void ocean_destroy(Ocean& ocean){
    if (ocean.mesh.vao) mesh_destroy(ocean.mesh);
}

void ocean_save(const Ocean& ocean, const std::string& path){
    std::ofstream f(path);
    if (!f){ std::cerr << "[ocean] failed to save " << path << "\n"; return; }
    f << ocean.enabled << " " << ocean.y_level << "\n";
    std::cout << "[ocean] saved to " << path << "\n";
}

bool ocean_load(Ocean& ocean, const std::string& path){
    std::ifstream f(path);
    if (!f) return false;
    f >> ocean.enabled >> ocean.y_level;
    std::cout << "[ocean] loaded y=" << ocean.y_level << " enabled=" << ocean.enabled << "\n";
    return true;
}