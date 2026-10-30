#include "ScriptFaultNotice.h"
#include "AssetDatabase.h"
#include "BuildIdentity.h"
#include "Renderer.h"
#include "ScriptSystem.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <vector>

ScriptFaultNotice LatestScriptFaultNotice(const ScriptSystem* scripts, const AssetDatabase* assets) {
    if (!scripts) return {};
    const auto& records = scripts->Diagnostics();
    for (auto it = records.rbegin(); it != records.rend(); ++it) {
        if (scripts->InspectPhase(it->entity, it->slot) != ScriptInstancePhase::Faulted) continue;
        ScriptFaultNotice out;
        const auto* record = assets ? assets->Find(it->asset) : nullptr;
        out.source = record ? record->relativePath : it->asset;
        out.callback = it->callback;
        // Detailed diagnostics retain the original message; presentation is bounded.
        std::istringstream lines(it->message.substr(0, 8192));
        std::getline(lines, out.message);
        while (std::getline(lines, out.stack) && out.stack.empty()) {}
        return out;
    }
    return {};
}

bool DefaultScriptFaultNoticeEnabled() {
    if (const char* value = std::getenv("JUDAS_SCRIPT_FAULT_OVERLAY"))
        return std::string(value) == "1";
    const auto& build = GetBuildIdentity();
    return std::string(build.channel) == "Experimental" || std::string(build.configuration) == "Debug";
}

void DrawScriptFaultNotice(Renderer& renderer, int width, int height,
                          const ScriptSystem* scripts, const AssetDatabase* assets) {
    if (!scripts || scripts->Diagnostics().empty()) return;
    const auto notice = LatestScriptFaultNotice(scripts, assets);
    if (notice.source.empty()) return;
    const float scale = std::clamp(width / 1000.f, .55f, 1.f);
    const float line = renderer.GetUITextLineHeight(scale);
    renderer.DrawUIRect({8, 8},
        {float(std::max(1, width - 16)), std::max(1.f, std::min(float(height - 16), line * 4 + 16))},
        {.28f, .025f, .035f, .97f});
    const std::string lines[] = {
        "SCRIPT STOPPED: " + notice.source + " [" + notice.callback + "]",
        notice.message, notice.stack,
        "Fix the script and reload / restart. See script diagnostics for details."
    };
    for (int i = 0; i < 4; ++i) {
        auto text = lines[i];
        if (text.size() > 2048) {
            size_t end = 2048;
            while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
            text.resize(end);
        }
        // Fit at UTF-8 boundaries with logarithmically many font measurements.
        std::vector<size_t> ends{0};
        for (size_t j = 1; j <= text.size(); ++j)
            if (j == text.size() || (static_cast<unsigned char>(text[j]) & 0xc0) != 0x80)
                ends.push_back(j);
        size_t lo = 0, hi = ends.size();
        while (lo + 1 < hi) {
            const size_t mid = (lo + hi) / 2;
            if (renderer.MeasureUIText(text.substr(0, ends[mid]), scale).x <= std::max(0, width - 40))
                lo = mid;
            else hi = mid;
        }
        text.resize(ends[lo]);
        renderer.DrawUIText(text, {16, 16 + line * i}, scale, {1, .94f, .9f, 1});
    }
}
