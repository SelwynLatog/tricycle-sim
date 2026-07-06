#pragma once
#include "editor_state.hpp"

// scans assets/props (+ assets/entity/) for .obj files, fills editor.prop_list and editor.entity_list
void editor_scan_props(EditorState& editor, const char* assets_dir);

// scans assets/audio recursively for .wav/.ogg files, fills editor.audio_file_list
void editor_scan_audio(EditorState& editor, const char* assets_dir);