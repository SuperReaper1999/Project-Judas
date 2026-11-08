#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

// The scene renderer and the panels share these rectangles. This is layout
// geometry only; resizing panels never edits authored scene data.
struct EditorWorkspaceRect {
    glm::vec2 position{0};
    glm::vec2 size{0};
    bool Contains(glm::vec2 point) const {
        return point.x >= position.x && point.y >= position.y &&
            point.x < position.x + size.x && point.y < position.y + size.y;
    }
};

struct EditorWorkspaceLayout {
    EditorWorkspaceRect viewport, hierarchy, inspector, assets, toolbar, status;
    bool showRails = false;
    bool showAssets = false;
};
