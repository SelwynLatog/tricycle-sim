#pragma once
#include <glad/glad.h>
#include <string>

// bitmap font renderer 8x8 ASCII glyphs baked into a texture
// renders screen-space quads
// only printable ASCII (32–126)
struct Font {
    GLuint texture  = 0;  // 128x48 atlas: 16 cols x 6 rows of 8x8 glyphs
    GLuint vao      = 0;
    GLuint vbo      = 0;
    GLuint shader   = 0;
    int    win_w    = 0;
    int    win_h    = 0;
};

// call once after OpenGL context is ready
void font_init(Font& f, int window_width, int window_height);

// draw a string at screen pixel coords (origin = top-left)
// scale=1 -> 8x8px, scale=2 -> 16x16px, etc
void font_draw(const Font& f, const std::string& text, int x, int y,
               int scale, float r, float g, float b);

void font_destroy(Font& f);