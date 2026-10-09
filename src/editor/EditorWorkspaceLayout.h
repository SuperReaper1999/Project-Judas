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
    EditorWorkspaceRect leftSplitter, rightSplitter, bottomSplitter;
    bool showRails = false;
    bool showAssets = false;
};

inline EditorWorkspaceLayout CalculateEditorWorkspaceLayout(glm::vec2 displaySize, float leftWidth,
                                                            float rightWidth, float bottomHeight,
                                                            bool showAssets, bool showRails) {
    const float width = std::isfinite(displaySize.x) ? std::max(displaySize.x, 0.0f) : 640.0f;
    const float height = std::isfinite(displaySize.y) ? std::max(displaySize.y, 0.0f) : 480.0f;
    if (!std::isfinite(leftWidth)) leftWidth = 250;
    if (!std::isfinite(rightWidth)) rightWidth = 340;
    if (!std::isfinite(bottomHeight)) bottomHeight = 200;
    const float menuHeight = std::min(28.0f, height);
    const float toolbarHeight = std::min(40.0f, std::max(0.0f, height - menuHeight));
    const float statusHeight = std::min(26.0f, std::max(0.0f, height - menuHeight - toolbarHeight));
    const float top = menuHeight + toolbarHeight;
    const float contentHeight = std::max(0.0f, height - top - statusHeight);
    constexpr float splitter = 5.0f;

    EditorWorkspaceLayout result;
    result.showRails = showRails;
    result.showAssets = showAssets && showRails;
    result.toolbar = {{0, menuHeight}, {width, toolbarHeight}};
    result.status = {{0, height - statusHeight}, {width, statusHeight}};
    if (!showRails) {
        result.viewport = {{0, top}, {width, contentHeight}};
        return result;
    }

    // Preserve a usable scene at 640x480 without pushing either rail outside
    // the window. Larger windows retain the user's rail/splitter dimensions.
    const float minimumSceneWidth = std::min(320.0f, width * 0.35f);
    const float railBudget = std::max(0.0f, width - minimumSceneWidth - 2.0f * splitter);
    float left = std::clamp(leftWidth, 150.0f, std::max(150.0f, width * 0.40f));
    float right = std::clamp(rightWidth, 220.0f, std::max(220.0f, width * 0.46f));
    if (left + right > railBudget) {
        const float factor = railBudget / (left + right);
        left *= factor;
        right *= factor;
    }
    const float leftGap = std::min(splitter, std::max(0.0f, width - left - right));
    const float rightGap = std::min(splitter, std::max(0.0f, width - left - right - leftGap));
    const float availableBottomWidth = std::max(0.0f, width - right - rightGap);
    float assets = 0.0f;
    float bottomGap = 0.0f;
    if (result.showAssets) {
        const float minimumSceneHeight = std::min(180.0f, contentHeight * 0.55f);
        const float maximumAssets = std::max(0.0f, contentHeight - minimumSceneHeight - splitter);
        assets = std::min(std::max(bottomHeight, 80.0f), maximumAssets);
        bottomGap = std::min(splitter, std::max(0.0f, contentHeight - assets));
    }
    const float sceneHeight = std::max(0.0f, contentHeight - assets - bottomGap);
    result.hierarchy = {{0, top}, {left, sceneHeight}};
    result.inspector = {{width - right, top}, {right, contentHeight}};
    result.viewport = {{left + leftGap, top},
                       {std::max(0.0f, width - left - leftGap - right - rightGap), sceneHeight}};
    result.assets = {{0, top + sceneHeight + bottomGap}, {availableBottomWidth, assets}};
    result.leftSplitter = {{left, top}, {leftGap, sceneHeight}};
    result.rightSplitter = {{width - right - rightGap, top}, {rightGap, contentHeight}};
    result.bottomSplitter = {{0, top + sceneHeight}, {availableBottomWidth, bottomGap}};
    return result;
}
