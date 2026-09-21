#pragma once

#include "gl_core33.h"

// Owns the GL objects and draw calls. Low-level calls (glClear, glDrawArrays,
// ...) come from an external API, but the *concept* of "begin a frame / draw
// a rectangle / end a frame" is this engine's own boundary, and is what has
// to survive a future move to a different graphics API.
class Renderer {
public:
    bool Init();
    void Shutdown();

    // Clears the frame and sets up an orthographic projection matching the
    // given window size (world origin bottom-left, +x right, +y up, in
    // pixel units).
    void BeginFrame(int windowWidth, int windowHeight);

    void DrawRect(float centerX, float centerY, float width, float height, float r, float g,
                  float b, float a);

    void EndFrame();

private:
    GLuint m_shaderProgram = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    GLint m_uProjection = -1;
    GLint m_uPosition = -1;
    GLint m_uSize = -1;
    GLint m_uColor = -1;

    float m_projection[16] = {};
};
