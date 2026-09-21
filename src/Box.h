#pragma once

class Window;
class Renderer;

// Milestone 1's demo content: a single player-controlled rectangle.
class Box {
public:
    Box(float startX, float startY);

    void Update(const Window& window, float deltaTime);
    void Draw(Renderer& renderer) const;

private:
    float m_x;
    float m_y;

    static constexpr float kHalfSize = 30.0f;
    static constexpr float kSpeed = 400.0f;  // pixels per second
};
