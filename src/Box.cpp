#include "Box.h"

#include <algorithm>
#include <cmath>

#include "Renderer.h"
#include "Window.h"

Box::Box(float startX, float startY) : m_x(startX), m_y(startY) {}

void Box::Update(const Window& window, float deltaTime) {
    float dx = 0.0f;
    float dy = 0.0f;
    if (window.IsActionActive(Action::MoveRight)) dx += 1.0f;
    if (window.IsActionActive(Action::MoveLeft)) dx -= 1.0f;
    if (window.IsActionActive(Action::MoveUp)) dy += 1.0f;
    if (window.IsActionActive(Action::MoveDown)) dy -= 1.0f;

    // Normalize so diagonal movement isn't faster than axis-aligned movement.
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length > 0.0f) {
        dx /= length;
        dy /= length;
    }

    m_x += dx * kSpeed * deltaTime;
    m_y += dy * kSpeed * deltaTime;

    const float minX = kHalfSize;
    const float maxX = static_cast<float>(window.Width()) - kHalfSize;
    const float minY = kHalfSize;
    const float maxY = static_cast<float>(window.Height()) - kHalfSize;
    m_x = std::clamp(m_x, minX, maxX);
    m_y = std::clamp(m_y, minY, maxY);
}

void Box::Draw(Renderer& renderer) const {
    renderer.DrawRect(m_x, m_y, kHalfSize * 2.0f, kHalfSize * 2.0f, 1.0f, 0.6f, 0.1f, 1.0f);
}
