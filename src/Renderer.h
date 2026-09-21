#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "gl_core33.h"

// Owns the GL objects and draw calls. Low-level calls (glClear, glDrawArrays,
// ...) come from an external API, but the *concept* of "begin a frame / set
// the active camera / draw a box / end a frame" is this engine's own
// boundary, and is what has to survive a future move to a different graphics
// API. Callers (Application, Camera, demo scene code) never touch OpenGL
// directly.
class Renderer {
public:
    bool Init();
    void Shutdown();

    // Clears the color and depth buffers and sets the viewport to the given
    // window size.
    void BeginFrame(int windowWidth, int windowHeight);

    // Sets the view/projection matrices used by all DrawBox calls until
    // the next SetCamera call. Recomputing this every frame (rather than
    // reacting to a resize event) is what keeps the projection's aspect
    // ratio correct across window resizes.
    void SetCamera(const glm::mat4& view, const glm::mat4& projection);

    // Draws a box mesh: `halfExtents` sets its size along each axis (the
    // local unit cube is scaled by 2*halfExtents), `rotation` its
    // orientation. Callers pass whatever position/rotation they have —
    // Milestone 3's physics-driven cube passes values read straight from
    // PhysicsWorld::GetTransform, with no separate rendering-side motion
    // logic.
    void DrawBox(const glm::vec3& position, const glm::quat& rotation,
                 const glm::vec3& halfExtents, const glm::vec3& colorRgb);

    // Draws a sphere mesh of the given world-space radius. Added in
    // Milestone 5 for the spherical test world; a sphere looks identical
    // under any rotation, so unlike DrawBox there is no rotation parameter.
    void DrawSphere(const glm::vec3& position, float radius, const glm::vec3& colorRgb);

    void EndFrame();

private:
    GLuint m_shaderProgram = 0;
    GLuint m_cubeVao = 0;
    GLuint m_cubeVbo = 0;
    GLuint m_sphereVao = 0;
    GLuint m_sphereVbo = 0;
    GLsizei m_sphereVertexCount = 0;

    GLint m_uModel = -1;
    GLint m_uView = -1;
    GLint m_uProjection = -1;
    GLint m_uColor = -1;

    glm::mat4 m_view{1.0f};
    glm::mat4 m_projection{1.0f};
};
