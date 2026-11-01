#pragma once

#include "AssetDatabase.h"
#include <string>
#include <vector>

struct EditorPanelState;

// AI definitions remain ordinary script assets: an exact JSON default export.
// This constrained format is intentionally never evaluated by the editor.
bool DecodeUtilityAIModule(const std::string& source, std::string& definition, std::string& error);
std::string EncodeUtilityAIModule(const std::string& definition);

struct UtilityAIEditor {
    std::string asset, path, definition, savedDefinition, savedSource, status;
    std::string newName = "Citizen.ai.js";
    std::vector<std::string> undo;
    int action = 0, runtimeAgent = 0;

    bool Dirty() const;
    bool Guard(EditorPanelState&) const;
    bool Open(const AssetRecord&, std::string& error);
    bool Create(AssetDatabase&, const std::string& fileName, std::string& error);
    bool Save(AssetDatabase&, std::string& error);
    void Remember();
    void Discard();
    void Panel(EditorPanelState&);
};
