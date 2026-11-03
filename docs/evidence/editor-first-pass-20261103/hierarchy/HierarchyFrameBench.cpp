#include "EditorPanels.h"
#include "Project.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

void Draw(EditorDocument& document, EditorPanelState& state) {
    EditorRequests requests;
    DrawHierarchyPanel(document, state, requests);
}

ImGuiWindow* RowsWindow() {
    const auto* panel = ImGui::FindWindowByName("Hierarchy");
    return panel && panel->DC.ChildWindows.Size ? panel->DC.ChildWindows.front() : nullptr;
}

void Bench(int count, float scale, bool tagged, int repetitions) {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = {1200, 800};
    io.DeltaTime = 1.f / 60;
    io.FontGlobalScale = scale;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    EditorDocument document;
    auto& scene = document.GetScene();
    SceneObjectId parent = 0;
    for (int i = 0; i < count; ++i) {
        auto& object = scene.CreateObject("Entity " + std::to_string(i));
        object.parent = i % 9 == 0 ? 0 : parent;
        if (i % 9 == 2) object.tags = CategoryBit(4);
        parent = object.id;
    }
    Project project;
    project.Settings().classification.tags.names.emplace(4, "Eligible group");
    EditorPanelState state;
    state.workspace.showRails = true;
    state.workspace.hierarchy = {{20, 20}, {380, 620}};
    state.project = &project;
    if (tagged) state.hierarchySearch = "Eligible group";

    std::vector<double> top, bottom;
    double cold = 0.;
    for (int i = 0; i < 2 * repetitions + 3; ++i) {
        if (i == repetitions + 3)
            if (auto* rows = RowsWindow()) ImGui::SetScrollY(rows, rows->ScrollMax.y);
        ImGui::NewFrame();
        const auto start = Clock::now();
        Draw(document, state);
        const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        ImGui::Render();
        if (i == 0) cold = elapsed;
        if (i >= 3) (i < repetitions + 3 ? top : bottom).push_back(elapsed);
    }
    const auto report = [&](const char* scroll, std::vector<double>& values) {
        std::sort(values.begin(), values.end());
        std::printf("%d,%.2f,%d,%s,%d,%.6f,%.6f,%.6f,%.6f\n", count, scale, tagged, scroll,
                    repetitions, cold, values[values.size() / 2],
                    values[(95 * values.size() + 99) / 100 - 1], values.back());
    };
    report("top", top);
    report("bottom", bottom);
    ImGui::DestroyContext();
}
} // namespace

int main(int argc, char** argv) {
    const int repetitions = argc == 2 ? std::atoi(argv[1]) : 41;
    if (repetitions < 1 || repetitions > 1000) return 2;
    std::puts("objects,ui_scale,tagged,scroll,samples,cold_ms,median_ms,p95_ms,worst_ms");
    for (int count : {128, 1024, 8192})
        for (float scale : {1.f, 1.5f})
            for (bool tagged : {false, true})
                Bench(count, scale, tagged, repetitions);
}
