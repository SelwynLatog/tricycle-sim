#include "asset_scan.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>

/**********************************************************************
ASSET SCAN
Responsibilities
- Prop + entity discovery (assets/props, assets/entity)
- Audio file discovery (assets/audio, recursive)
**********************************************************************/

void editor_scan_props(EditorState& editor, const char* assets_dir){
    editor.prop_list.clear();

    // scan through assets/ and collect every .obj filename
    // store filename but reconstruct fullpath at placement time
    for (const auto& entry : std::filesystem::directory_iterator(assets_dir)){
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".obj") continue;
        editor.prop_list.push_back(entry.path().filename().string());
    }

    // sort alphabetically so the palette is stable across runs
    std::sort(editor.prop_list.begin(), editor.prop_list.end());

    std::cout << "[editor] found " << editor.prop_list.size() << " props in " << assets_dir << "\n";

    // scan entity subfolder separately for NPC models
    editor.entity_list.clear();
    std::string entity_dir = std::string("../assets/entity");
    if (std::filesystem::exists(entity_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(entity_dir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() != ".obj") continue;
            editor.entity_list.push_back(entry.path().filename().string());
        }
        std::sort(editor.entity_list.begin(), editor.entity_list.end());
    }   
    // also merge entity list into prop list with entity/ prefix
    // so they appear in the normal object mode palette
    for (const auto& e : editor.entity_list)
        editor.prop_list.push_back("entity/" + e);

    std::cout << "[editor] found " << editor.entity_list.size() << " entities in " << entity_dir << "\n";
}

void editor_scan_audio(EditorState& editor, const char* assets_dir){
    editor.audio_file_list.clear();
    std::string audio_root = std::string(assets_dir) + "/audio";
    if (!std::filesystem::exists(audio_root)) return;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(audio_root)){
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension();
        if (ext != ".wav" && ext != ".ogg") continue;
        // store relative to assets/audio/ so paths are short
        auto rel = std::filesystem::relative(entry.path(),
            std::string(assets_dir)).string();
        // normalize separators
        std::replace(rel.begin(), rel.end(), '\\', '/');
        editor.audio_file_list.push_back(rel);
    }
    std::sort(editor.audio_file_list.begin(), editor.audio_file_list.end());
    std::cout << "[editor] found " << editor.audio_file_list.size() << " audio files\n";
}