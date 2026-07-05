#include "render_textures.hpp"
#include "obj_loader.hpp"
#include "../../vendor/stb/stb_image.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <vector>

/**********************************************************************
RENDER TEXTURES
Responsibilities
- Texture loading/cache (disk .texcache + GL upload)
- Water detail texture loading
- Prop mesh loading + bounds/floor-offset caching
- Texture preload pass
**********************************************************************/


// TEXTURE CACHE
// loads a texture from disk into GL, caches by path
// returns 0 on failure
// cache format: [uint64 mtime][int32 w][int32 h][w*h*4 bytes RGBA]
static bool tex_cache_load(const std::string& path, int& w, int& h, std::vector<unsigned char>& px){
    namespace fs = std::filesystem;
    std::string cp = path + ".texcache";
    if (!fs::exists(cp) || !fs::exists(path)) return false;
    uint64_t src_mtime = (uint64_t)fs::last_write_time(path).time_since_epoch().count();
    std::ifstream f(cp, std::ios::binary);
    if (!f.is_open()) return false;
    uint64_t cached_mtime = 0;
    f.read((char*)&cached_mtime, 8);
    if (cached_mtime != src_mtime) return false;
    f.read((char*)&w, 4);
    f.read((char*)&h, 4);
    px.resize(w * h * 4);
    f.read((char*)px.data(), px.size());
    return (bool)f;
}

static void tex_cache_save(const std::string& path, int w, int h, const unsigned char* px){
    namespace fs = std::filesystem;
    std::string cp = path + ".texcache";
    std::ofstream f(cp, std::ios::binary);
    if (!f.is_open()) return;
    uint64_t mtime = (uint64_t)fs::last_write_time(path).time_since_epoch().count();
    f.write((char*)&mtime, 8);
    f.write((char*)&w, 4);
    f.write((char*)&h, 4);
    f.write((char*)px, w * h * 4);
}

GLuint load_texture(EditorRenderer& er, const std::string& path){
    if (path.empty()) return 0;
    auto it = er.tex_cache.find(path);
    if (it != er.tex_cache.end()) return it->second;

    int w = 0, h = 0;
    std::vector<unsigned char> px_buf;
    unsigned char* px = nullptr;
    bool from_cache = tex_cache_load(path, w, h, px_buf);

    if (from_cache){
        px = px_buf.data();
    }
    else {
        stbi_set_flip_vertically_on_load(1);
        int ch;
        px = stbi_load(path.c_str(), &w, &h, &ch, 4);
        if (!px){
            std::cerr << "[tex] failed to load: " << path << "\n";
            er.tex_cache[path] = 0;
            return 0;
        }
        tex_cache_save(path, w, h, px);
    }

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

   if (!from_cache) stbi_image_free(px);
    er.tex_cache[path] = id;
    std::cout << "[tex] " << (from_cache ? "cache hit" : "loaded") << " " << w << "x" << h << " " << path << "\n";
    return id;
}

// loads a water-detail texture (normal map / foam) with linear filtering,
// no disk cache needed since these load once at startup
GLuint load_water_texture(const std::string& path){
    stbi_set_flip_vertically_on_load(1);
    int w, h, ch;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!px){
        std::cerr << "[water_tex] failed to load: " << path << "\n";
        return 0;
    }
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(px);
    std::cout << "[water_tex] loaded " << w << "x" << h << " " << path << "\n";
    return id;
}

// =====================================================
// PROP CACHE + BOUNDS GENERATION
//
// Loads OBJ assets lazily.
//
// Also computes:
//
// - local bounds
// - floor offset
// - cached AABBs
//
// Used by:
//
// - selection
// - hitboxes
// - collision setup
// - placement
//
// =====================================================
ObjMesh& get_prop_mesh(EditorRenderer& er, const std::string& filename){
    auto it = er.prop_cache.find(filename);
    if (it != er.prop_cache.end()) return it->second;

    // check filename with entity/ sub dir, then scan props/
    std::string full_path = (filename.rfind("entity/", 0) == 0)
        ? std::string("../assets/") + filename
        : std::string("../assets/props/") + filename;
    ObjData data;
    if (!obj_load(full_path, data)){
        er.prop_cache[filename] = ObjMesh{};
        return er.prop_cache[filename];
    }

    // compute y min from vertex buffer before uploading
    // layout is px py pz nx ny nz so Y is at every 6 floats
    float x_min= 1e9f, x_max=-1e9f;
    float y_min= 1e9f, y_max=-1e9f;
    float z_min= 1e9f, z_max=-1e9f;
    for (int i = 0; i < (int)data.vertices.size(); i += 8){
        float x = data.vertices[i];
        float y = data.vertices[i+1];
        float z = data.vertices[i+2];
        x_min=std::min(x_min,x); x_max=std::max(x_max,x);
        y_min=std::min(y_min,y); y_max=std::max(y_max,y);
        z_min=std::min(z_min,z); z_max=std::max(z_max,z);
    }

    // store offset so model matrix can push mesh up to y = 0
    er.prop_y_offset[filename] = (y_min < 1e9f) ? -y_min : 0.0f;

    EditorRenderer::PropBounds bounds;
    bounds.local_min = glm::vec3(x_min, y_min, z_min);
    bounds.local_max = glm::vec3(x_max, y_max, z_max);
    er.prop_bounds[filename] = bounds;

    ObjMesh mesh{};
    obj_mesh_init(mesh, std::move(data));
    er.prop_cache[filename] = std::move(mesh);
    return er.prop_cache[filename];
}

// returns the y floor offset for a given prop filename
float editor_get_y_floor_offset(EditorRenderer& er, const std::string& filename){
    get_prop_mesh(er, filename); // ensure cached
    auto it = er.prop_y_offset.find(filename);
    return (it != er.prop_y_offset.end()) ? it->second : 0.0f;
}

void editor_renderer_preload_textures(EditorRenderer& er){
    for (auto& [name, mesh] : er.prop_cache){
        for (int i = 0; i < (int)mesh.data.groups.size(); i++){
            const ObjGroup& grp = mesh.data.groups[i];
            const ObjMaterial* mat = obj_find_material(mesh.data, grp.mat_name);
            if (mat && !mat->tex_path.empty())
                load_texture(er, mat->tex_path);
        }
    }
}