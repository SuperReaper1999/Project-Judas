#pragma once

#include "EditorContext.h"
#include <functional>
#include <string>
#include <vector>

struct EditorPanelDescriptor {
    std::string id;
    std::string title;
    std::string category;
    unsigned phases = kEditorAllPanelPhases;
    bool EditorPanelState::*visibility = nullptr;
};

// Registration is setup-time only. Drawing is one deterministic pass and does
// not own the editor session or run simulation. Panel code is supplied by its
// module; adding an entry never requires modifying the application's frame loop.
class EditorPanelRegistry {
  public:
    using Draw = std::function<void(EditorContext &)>;
    void Add(EditorPanelDescriptor descriptor, Draw draw);
    bool Contains(const std::string &id) const;
    void DrawAll(EditorContext &context);
    void DrawWindowMenu(EditorContext &context) const;
    std::vector<EditorPanelDescriptor> Descriptors() const;

  private:
    struct Entry {
        EditorPanelDescriptor descriptor;
        Draw draw;
    };
    std::vector<Entry> entries;
    bool drawing = false;
};
