#pragma once
#include "ScriptSystem.h"
#include <string>
#include <vector>
class AssetDatabase;
class EngineHost;
class RuntimeWorld;
// Editor-owned tools. No ImGui/SDL dependency in ScriptSystem or exported games.
struct ScriptTools {
    bool pendingBreak=false;
    bool visible=false,enabled=false,preserve=true,reloadRequested=false,stopRequested=false,quitRequested=false,clockReset=false;
    std::string file,sourcePath,status,loadedBytes;
    std::vector<char> edit;
    std::vector<ScriptDiagnostic> projectDiagnostics;
    std::vector<ScriptBreakpoint> points;
    int requestedLine=1,selectedFrame=0;
    bool editing=false,scrollToLine=false;
    void Attach(RuntimeWorld&,EngineHost&);
    void Panel(RuntimeWorld*,const AssetDatabase&);
    void ApplyReload(RuntimeWorld&);
    bool SaveSourceEdits(const AssetDatabase&);
    bool OpenForEditing(const std::string& relativePath, const AssetDatabase&);
    ScriptDebugAction Pause(const ScriptDebugStop&,EngineHost&,bool online=false);
private:
    void Open(const std::string&,const AssetDatabase&);
    void OpenDiagnostic(const ScriptDiagnostic&,const AssetDatabase&);
    void Source(const AssetDatabase&,int activeLine=0,const std::string* compiled=nullptr);
    void Variables(const ScriptDebugVariable&);
};
