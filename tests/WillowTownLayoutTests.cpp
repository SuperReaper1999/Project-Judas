#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "ScreenshotWriter.h"
#include "ScriptSystem.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {
unsigned failures = 0;
void Check(bool ok, const std::string& label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    failures += !ok;
}
bool Overlap(const UIRect& a, const UIRect& b) {
    return a.position.x < b.position.x+b.size.x-.5f && b.position.x < a.position.x+a.size.x-.5f &&
           a.position.y < b.position.y+b.size.y-.5f && b.position.y < a.position.y+a.size.y-.5f;
}
void Inspect(EngineHost& host, RuntimeWorld& world) {
    auto& ui = world.UI(); const auto h = ui.Find("willow_town");
    ui.Layout(host.GetWindow().Width(), host.GetWindow().Height());
    const auto* doc = static_cast<const RuntimeUI&>(ui).Document(h);
    Check(doc != nullptr, "real application publishes its authored UI");
    if (!doc) return;
    const float scale = std::min(host.GetWindow().Width()/doc->reference.x,
                                 host.GetWindow().Height()/doc->reference.y) * ui.Scale(h);
    struct Ink { std::string id, parent; UIRect bounds; };
    std::vector<Ink> ink;
    for (const auto& element : doc->elements) {
        if (element.id == "selected-label" || element.text.empty() || !element.visible) continue;
        const auto* layout = ui.LayoutOf(h, element.id);
        if (!layout || !layout->visible) continue;
        TextOptions options; options.pixels = element.fontSize * scale;
        options.width = layout->rect.size.x; options.wrap = element.wrap;
        options.direction = element.direction;
        options.alignment = element.textLogicalAlign >= 0 ? TextAlignment(element.textLogicalAlign) :
            element.textAlign.x >= .75f ? TextAlignment::Right :
            element.textAlign.x >= .25f ? TextAlignment::Centre : TextAlignment::Left;
        auto text = host.GetRenderer().LayoutText(element.text, options);
        Check(text->height > 0 && !text->lines.empty(), "text shaping actually produced rows: " + element.id);
        Check(text->height <= layout->rect.size.y + 1.f, "actual shaped text fits row: " + element.id);
        Check(text->width <= layout->rect.size.x + 1.f, "actual shaped text fits width: " + element.id);
        UIRect bounds = layout->rect; bounds.size = {text->width, text->height};
        if (options.alignment == TextAlignment::Centre)
            bounds.position.x += std::max(0.f, layout->rect.size.x - text->width) * .5f;
        else if (options.alignment == TextAlignment::Right)
            bounds.position.x += std::max(0.f, layout->rect.size.x - text->width);
        bounds.position.y += element.textAlign.y * std::max(0.f, layout->rect.size.y - text->height);
        // Compare content coordinates even when a row is scrolled out of sight.
        for (const auto& other : ink)
            if (element.parent == other.parent)
                Check(!Overlap(bounds, other.bounds), "text rows do not overlap: " + other.id + "/" + element.id);
        ink.push_back({element.id, element.parent, bounds});
    }
    Check(ui.Focus(h, "restock"), "keyboard focus can reach lower shop controls");
    ui.Layout(host.GetWindow().Width(), host.GetWindow().Height());
    const auto* bottom = ui.LayoutOf(h, "restock");
    Check(bottom && bottom->clip.Contains(bottom->rect.position + bottom->rect.size*.5f),
          "lower control is genuinely inside the scroll clip after focus");
    ui.SetScroll(h, "sidebar", {0, 0});
    ui.Layout(host.GetWindow().Width(), host.GetWindow().Height());
}
}
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::filesystem::path out = std::filesystem::absolute(argv[2]);
    std::filesystem::create_directories(out);
    SDL_setenv("XDG_DATA_HOME", (out/"data").string().c_str(), 1);
    SDL_setenv("JUDAS_WINDOW_SIZE", "681x386", 1);
    const std::array<glm::ivec2, 4> sizes{{{681,386},{1280,720},{640,480},{681,386}}};
    unsigned frame = 0, index = 0;
    ApplicationControl control; control.hidden = true;
    control.frameSeconds = [](float) { return 1.f/60; };
    control.beforeFrame = [&](EngineHost& host, RuntimeWorld&, InteractivePlay&) {
        if (frame && frame % 8 == 0 && index < sizes.size()) {
            std::string error;
            Check(host.GetWindow().Resize(sizes[index].x, sizes[index].y, error), "native window resize: " + error);
        }
    };
    control.afterFrame = [&](EngineHost& host, RuntimeWorld& world, InteractivePlay&) {
        ++frame;
        if (frame % 8) return;
        Check(host.GetWindow().Width()==sizes[index].x && host.GetWindow().Height()==sizes[index].y,
              "actual viewport matches requested authoring size");
        Inspect(host, world);
        Check(!world.Scripts() || world.Scripts()->Diagnostics().empty(), "project remains script-fault free");
        std::vector<unsigned char> pixels;
        host.GetRenderer().CaptureFrame(host.GetWindow().Width(), host.GetWindow().Height(), pixels);
        Check(WriteRgbPng((out/("viewport-"+std::to_string(index)+".png")).string(),
                         host.GetWindow().Width(), host.GetWindow().Height(), pixels), "native rendered reference");
        if (++index == sizes.size()) { SDL_Event quit{}; quit.type=SDL_QUIT; SDL_PushEvent(&quit); }
    };
    const int result = Application{}.Run(2, argv, &control);
    return result || failures || index != sizes.size() ? 1 : 0;
}
