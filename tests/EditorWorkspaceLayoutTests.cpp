#include <cmath>
#include <cstdio>
#include <limits>

#include "editor/EditorWorkspaceLayout.h"

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (!value) ++failures;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
}
bool Within(EditorWorkspaceRect rectangle, glm::vec2 display) {
    constexpr float tolerance = .002f;
    return std::isfinite(rectangle.position.x) && std::isfinite(rectangle.position.y) &&
        std::isfinite(rectangle.size.x) && std::isfinite(rectangle.size.y) &&
        rectangle.position.x >= -tolerance && rectangle.position.y >= -tolerance &&
        rectangle.size.x >= 0 && rectangle.size.y >= 0 &&
        rectangle.position.x + rectangle.size.x <= display.x + tolerance &&
        rectangle.position.y + rectangle.size.y <= display.y + tolerance;
}
bool Overlap(EditorWorkspaceRect a, EditorWorkspaceRect b) {
    if (a.size.x <= 0 || a.size.y <= 0 || b.size.x <= 0 || b.size.y <= 0) return false;
    return a.position.x < b.position.x + b.size.x - .001f &&
        b.position.x < a.position.x + a.size.x - .001f &&
        a.position.y < b.position.y + b.size.y - .001f &&
        b.position.y < a.position.y + a.size.y - .001f;
}
void SizeCase(glm::vec2 display, float left, float right, float bottom) {
    const auto layout = CalculateEditorWorkspaceLayout(display, left, right, bottom, true, true);
    const EditorWorkspaceRect rectangles[] = {layout.viewport, layout.hierarchy, layout.inspector, layout.assets,
        layout.toolbar, layout.status, layout.leftSplitter, layout.rightSplitter, layout.bottomSplitter};
    bool contained = true, separate = true;
    for (const auto rectangle : rectangles) contained &= Within(rectangle, display);
    for (std::size_t i = 0; i < 9; ++i)
        for (std::size_t j = i + 1; j < 9; ++j) separate &= !Overlap(rectangles[i], rectangles[j]);
    Check(contained, "workspace panels and dividers stay inside the resized window");
    Check(separate, "scene, authoring panels, toolbar and status do not overlap");
    Check(layout.viewport.size.x >= std::min(320.0f, display.x * .35f) - .002f &&
          layout.viewport.size.y >= 180.0f - .002f, "rail and asset resizing preserves a usable central scene");
    const auto center = layout.viewport.position + layout.viewport.size * .5f;
    Check(layout.viewport.Contains(center) && !layout.viewport.Contains(layout.inspector.position + layout.inspector.size * .5f),
          "scene ownership accepts its center and excludes the inspector");
}
}
int main() {
    SizeCase({640, 480}, 250, 340, 200);
    SizeCase({1280, 800}, 250, 340, 200);
    SizeCase({1920, 1080}, 420, 520, 340);
    SizeCase({640, 480}, 1e6f, 1e6f, 1e6f);
    SizeCase({1280, 800}, -100, -100, -100);
    const auto hiddenAssets = CalculateEditorWorkspaceLayout({1280, 800}, 250, 340, 200, false, true);
    Check(!hiddenAssets.showAssets && hiddenAssets.assets.size.y == 0 && hiddenAssets.viewport.size.y == 706,
          "hiding assets gives their height back to the scene and hierarchy");
    const auto play = CalculateEditorWorkspaceLayout({1280, 800}, 250, 340, 200, true, false);
    Check(!play.showRails && !play.showAssets && play.hierarchy.size.x == 0 && play.inspector.size.x == 0,
          "running Play collapses authoring rails and assets");
    Check(play.viewport.position.x == 0 && play.viewport.size.x == 1280,
          "collapsed workspace leaves the full window width available to Play");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto invalid = CalculateEditorWorkspaceLayout({640, 480}, nan, nan, nan, true, true);
    Check(Within(invalid.viewport, {640, 480}) && invalid.viewport.size.x >= 223.998f && invalid.viewport.size.y >= 179.998f,
          "invalid saved dimensions restore bounded default layout values");
    std::printf("Editor workspace layout: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
