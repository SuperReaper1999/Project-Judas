#include "ScriptTools.h"
#include "AssetDatabase.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "NetworkSession.h"
#include "PlatformServices.h"
#include <filesystem>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "glad/gl.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {
std::string Read(const std::string& path){
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot read script source.");
    std::string result;char block[4096];
    while(f.read(block,sizeof(block))||f.gcount()){
        result.append(block,size_t(f.gcount()));if(result.size()>1024*1024)throw std::runtime_error("Source exceeds the 1 MiB script limit.");
    }
    if(f.bad())throw std::runtime_error("Script source read failed.");
    return result;
}
}
void ScriptTools::Open(const std::string& path,const AssetDatabase& assets){
    const auto* record=assets.FindByRelativePath(path);if(!record||record->type!=AssetType::Script)return;
    if(path==file&&record->path==sourcePath)return;
    if(editing){status="Finish or discard the current source edit before opening another file.";return;}
    std::string bytes;try{bytes=Read(record->path);}catch(const std::exception& e){status=e.what();return;}
    file=path;sourcePath=record->path;loadedBytes=std::move(bytes);scrollToLine=true;
    if(loadedBytes.size()>1024*1024){status="Source exceeds the 1 MiB script limit";loadedBytes.clear();return;}
    edit.assign(1024*1024+1,0);std::copy(loadedBytes.begin(),loadedBytes.end(),edit.begin());
}
void ScriptTools::OpenDiagnostic(const ScriptDiagnostic& diagnostic,const AssetDatabase& assets){
    const AssetRecord* selected=assets.Find(diagnostic.asset);size_t earliest=std::string::npos;int line=1;
    for(const auto& [id,record]:assets.Records())if(record.type==AssetType::Script){
        (void)id;const auto marker=record.relativePath+":";size_t offset=diagnostic.message.find(marker);
        while(offset!=std::string::npos){
            const auto at=offset+marker.size();
            if(at<diagnostic.message.size()&&std::isdigit(static_cast<unsigned char>(diagnostic.message[at]))&&offset<earliest){
                try{const auto value=std::stoi(diagnostic.message.substr(at));if(value>0){selected=&record;earliest=offset;line=value;}}catch(...){}
            }
            offset=diagnostic.message.find(marker,offset+marker.size());
        }
    }
    if(selected){Open(selected->relativePath,assets);requestedLine=line;scrollToLine=true;}
}
bool ScriptTools::OpenForEditing(const std::string& path,const AssetDatabase& assets){
    visible=true;
    Open(path,assets);
    if(file!=path||sourcePath.empty())return false;
    const auto* record=assets.FindByRelativePath(path);
    if(!record||record->missing||record->readOnly){status="Source is missing or read-only.";return false;}
    editing=true;return true;
}
void ScriptTools::Attach(RuntimeWorld& world,EngineHost& host){
    if(!enabled){if(world.Scripts())world.Scripts()->SetDebugger({});return;}
    auto& scripts=world.PrepareScriptTools();
    scripts.SetDebugger([this, &host, &scripts, &world](const ScriptDebugStop& stop) {
        const auto* network = world.NetworkIfLoaded();
        const auto action = Pause(stop, host, network && network->Active());
        scripts.SetBreakpoints(points);
        return action;
    });
    scripts.SetBreakpoints(points);
    if(pendingBreak){scripts.BreakOnNextInstruction();pendingBreak=false;}
}
void ScriptTools::ApplyReload(RuntimeWorld& world){
    if(!reloadRequested)return;
    reloadRequested=false;
    if(editing){status="Save or discard source edits before reload.";return;}
    auto& scripts=world.PrepareScriptTools();std::string error;
    status=scripts.Reload(preserve,error)?"Reloaded at callback boundary. Constructors/start rerun; JSON state optionally retained. Stop/Play before saving.":"Reload rejected: "+error;
}
bool ScriptTools::SaveSourceEdits(const AssetDatabase& assets){
    const auto* record=assets.FindByRelativePath(file);
    if(!record||record->readOnly||record->type!=AssetType::Script){status="Source is missing or read-only.";return false;}
    if(record->path!=sourcePath){status="Project/source changed; read the intended file before saving.";return false;}
    if(edit.empty()||edit.back()!=0){status="Source buffer is not terminated.";return false;}
    std::string staged;
    try{
        if(Read(record->path)!=loadedBytes)throw std::runtime_error("Disk changed externally. Copy your edit, then read from disk; refusing to overwrite.");
        const std::string bytes=edit.data();if(bytes.size()>1024*1024)throw std::runtime_error("Source exceeds 1 MiB.");
        staged=CreateImportStagingFile(record->path);
        {std::ofstream out(staged,std::ios::binary|std::ios::trunc);out.write(bytes.data(),std::streamsize(bytes.size()));out.close();if(!out)throw std::runtime_error("Source staging write failed.");}
        if(Read(record->path)!=loadedBytes)throw std::runtime_error("Source changed during save; refusing to overwrite.");
        std::filesystem::permissions(staged,std::filesystem::status(record->path).permissions());
        ReplaceStagedFile(staged,record->path);loadedBytes=bytes;editing=false;status="Source saved; check sources then deliberately reload.";return true;
    }catch(const std::exception& e){if(!staged.empty()){std::error_code ec;std::filesystem::remove(staged,ec);}status=e.what();return false;}
}
void ScriptTools::Source(const AssetDatabase& assets,int activeLine,const std::string* compiled){
    if(file.empty())return;
    if(!activeLine)activeLine=requestedLine;
    auto* record=assets.FindByRelativePath(file);if(!record)return;
    ImGui::TextWrapped("%s",file.c_str());
    if(ImGui::Button(editing?"Discard and read disk":"Read from disk")){editing=false;file.clear();Open(record->relativePath,assets);}
    ImGui::SameLine();
    if(!record->readOnly&&ImGui::Button(editing?"Close editor":"Edit source")){
        if(editing&&!edit.empty()&&std::strcmp(edit.data(),loadedBytes.c_str())!=0)status="Save or discard source edits before closing the source editor.";
        else editing=!editing;
    }
    if(enabled){
    ImGui::SameLine();ImGui::SetNextItemWidth(80);ImGui::InputInt("Line",&requestedLine);
    requestedLine=std::max(1,requestedLine);
    ImGui::SameLine();if(ImGui::Button("Toggle breakpoint")){
        auto it=std::find_if(points.begin(),points.end(),[&](const auto& p){return p.file==file&&p.line==requestedLine;});
        if(it==points.end()){if(points.size()<256)points.push_back({file,requestedLine,true});}else points.erase(it);
    }
    }
    if(editing){
        ImGui::TextWrapped("Explicit disk save. Reload scripts separately. Unsaved text never executes.");
        if(ImGui::Button("Save source"))SaveSourceEdits(assets);
        ImGui::SameLine();if(ImGui::Button("Discard edit")){editing=false;file.clear();Open(record->relativePath,assets);}
        ImGui::InputTextMultiline("##script",edit.data(),edit.size(),ImVec2(-1,250),ImGuiInputTextFlags_AllowTabInput);
    }else{
        ImGui::BeginChild("Source lines",ImVec2(0,260),true);
        std::istringstream lines(compiled?*compiled:loadedBytes);std::string text;int n=0;
        while(std::getline(lines,text)){
            ++n;bool bp=std::any_of(points.begin(),points.end(),[&](const auto& p){return p.enabled&&p.file==file&&p.line==n;});
            ImGui::PushID(n);if(ImGui::Selectable((std::string(bp?"* ":"  ")+std::to_string(n)+"  "+text).c_str(),n==activeLine))requestedLine=n;ImGui::PopID();
            if(n==activeLine&&(scrollToLine||ImGui::IsWindowAppearing())){ImGui::SetScrollHereY(.4f);scrollToLine=false;}
        }
        ImGui::EndChild();
    }
}
void ScriptTools::Variables(const ScriptDebugVariable& v){
    if(v.children.empty())ImGui::TextWrapped("%s = %s",v.name.c_str(),v.value.c_str());
    else if(ImGui::TreeNode(&v,"%s = %s",v.name.c_str(),v.value.c_str())){for(const auto& c:v.children)Variables(c);ImGui::TreePop();}
}
void ScriptTools::Panel(RuntimeWorld* world,const AssetDatabase& assets){
    if(!visible)return;
    ImGui::SetNextWindowSize(ImVec2(820,700),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("JudasJS developer tools",&visible)){ImGui::End();return;}
    ImGui::TextWrapped("Edit source, Save source, then Play. During Play, save and Reload scripts deliberately.");
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("Project script",file.empty()?"Choose source":file.c_str())){
        for(const auto& [id,a]:assets.Records())if(a.type==AssetType::Script){(void)id;if(ImGui::Selectable(a.relativePath.c_str(),file==a.relativePath))Open(a.relativePath,assets);}
        ImGui::EndCombo();
    }
    Source(assets);
    if(ImGui::Button("Check project scripts")){
        projectDiagnostics=ScriptSystem::CheckProjectSources(assets);
        status=projectDiagnostics.empty()?"Scripts compile and link. Press Play to test gameplay.":"Source errors below; open the affected file.";
    }
    ImGui::SameLine();ImGui::BeginDisabled(!world);
    if(ImGui::Button("Reload scripts"))reloadRequested=true;
    ImGui::EndDisabled();
    for(const auto& d:projectDiagnostics){ImGui::PushID(&d);if(ImGui::SmallButton("Open source"))OpenDiagnostic(d,assets);ImGui::SameLine();ImGui::TextWrapped("%s",d.message.c_str());ImGui::PopID();}
    if(world){
        auto& scripts=world->PrepareScriptTools();
        if(!scripts.Diagnostics().empty()&&ImGui::CollapsingHeader("Runtime errors",ImGuiTreeNodeFlags_DefaultOpen)){
            for(const auto& d:scripts.Diagnostics()){
                ImGui::PushID(&d);if(ImGui::SmallButton("Open"))OpenDiagnostic(d,assets);ImGui::SameLine();
                ImGui::TextWrapped("Entity %llu / slot %llu / %s\n%s",(unsigned long long)d.entity,(unsigned long long)d.slot,d.callback.c_str(),d.message.c_str());ImGui::PopID();
            }
        }
        if(ImGui::CollapsingHeader("Console (latest 256)"))for(const auto& text:scripts.ConsoleMessages())ImGui::TextWrapped("%s",text.c_str());
    }
    if(ImGui::CollapsingHeader("Debugger and reload options",enabled?ImGuiTreeNodeFlags_DefaultOpen:ImGuiTreeNodeFlags_None)){
        ImGui::Checkbox("Attach debugger (Play)",&enabled);
        ImGui::Checkbox("Keep JSON state",&preserve);
        if(world){
            auto& scripts=world->PrepareScriptTools();
            if(ImGui::Button("Check sources")){std::string error;status=scripts.CheckSources(error)?"Compile/link passed; constructors execute during Play.":error;}
            ImGui::SameLine();if(ImGui::Button("Break next")){enabled=true;pendingBreak=true;}
        }
        for(size_t i=0;i<points.size();++i){ImGui::PushID(int(i));ImGui::Checkbox("##enabled",&points[i].enabled);ImGui::SameLine();ImGui::Text("%s:%d",points[i].file.c_str(),points[i].line);ImGui::PopID();}
    }
    ImGui::TextWrapped("%s",status.c_str());
    ImGui::End();
}
ScriptDebugAction ScriptTools::Pause(const ScriptDebugStop& stop,EngineHost& host,bool online){
    auto& window=host.GetWindow();const bool captured=window.IsMouseCaptured();window.SetMouseCaptured(false);window.ClearPendingRequests();
    // No world update, save/load, streaming publication or recursive JS execution
    // while its C/JS call stacks are suspended. Only the tool's UI is pumped.
    GLint framebuffer=0,viewport[4];glGetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);glGetIntegerv(GL_VIEWPORT,viewport);
    ImGui::EndFrame();ScriptDebugAction action=ScriptDebugAction::Continue;bool done=false;selectedFrame=0;
    if(!stop.frames.empty()){Open(stop.frames.front().file,host.Assets());requestedLine=stop.frames.front().line;scrollToLine=true;}
    while(!done&&!window.ShouldClose()){
        window.SetInputClaimed(true,true);window.SetHostTextInput(ImGui::GetIO().WantTextInput);window.PollEvents();
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("JudasJS stopped",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
        ImGui::Text("World suspended | entity %llu / slot %llu",(unsigned long long)stop.entity,(unsigned long long)stop.slot);
        if(online)ImGui::TextWrapped("Online session paused at a script instruction. Transport is suspended with this world; remote peers may time out. Continue, then reconnect if needed. Ordinary gameplay pause keeps networking active.");
        auto button=[&](const char* label,ScriptDebugAction a){if(ImGui::Button(label)){action=a;done=true;}ImGui::SameLine();};
        button("Continue (F5)",ScriptDebugAction::Continue);button("Step into (F11)",ScriptDebugAction::Into);button("Step over (F10)",ScriptDebugAction::Over);button("Step out",ScriptDebugAction::Out);button("Stop Play",ScriptDebugAction::Cancel);ImGui::NewLine();
        if(!ImGui::GetIO().WantTextInput){if(ImGui::IsKeyPressed(ImGuiKey_F5)){action=ScriptDebugAction::Continue;done=true;}if(ImGui::IsKeyPressed(ImGuiKey_F10)){action=ScriptDebugAction::Over;done=true;}if(ImGui::IsKeyPressed(ImGuiKey_F11)){action=ScriptDebugAction::Into;done=true;}}
        ImGui::TextWrapped("Call stack (select a frame). Values are bounded own-data snapshots; getters and proxies never run.");
        for(size_t i=0;i<stop.frames.size();++i){auto& f=stop.frames[i];ImGui::PushID(int(i));if(ImGui::Selectable((f.file+":"+std::to_string(f.line)+":"+std::to_string(f.column)+"  "+f.function).c_str(),selectedFrame==int(i))){selectedFrame=int(i);Open(f.file,host.Assets());requestedLine=f.line;scrollToLine=true;}ImGui::PopID();}
        if(selectedFrame<int(stop.frames.size())){
            auto& f=stop.frames[selectedFrame];
            if(!f.sourceKnown)ImGui::TextWrapped("Compiled source snapshot unavailable (late attachment or 8 MiB snapshot budget). Reload with debugger attached for exact source matching.");
            else if(f.source!=loadedBytes)ImGui::TextWrapped("VM snapshot differs from disk/editor text. Execution and line marker below use the compiled snapshot; save/reload to run edits.");
            if(host.Assets().FindByRelativePath(f.file))Source(host.Assets(),f.line,f.sourceKnown?&f.source:nullptr);
            else ImGui::TextWrapped("No registered project source for this frame (%s). Its binding snapshot is still available below.",f.file.c_str());
            ImGui::BeginChild("Variables",ImVec2(0,0),true);for(const auto& v:f.variables)Variables(v);ImGui::EndChild();
        }
        ImGui::End();ImGui::Render();glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,window.Width(),window.Height());glClearColor(.025f,.03f,.045f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());window.SwapBuffers();SDL_Delay(8);
    }
    if(window.ShouldClose()){quitRequested=true;window.CancelCloseRequest();action=ScriptDebugAction::Cancel;}
    stopRequested|=action==ScriptDebugAction::Cancel;clockReset=true;
    window.ClearPendingRequests();window.Input().DiscardStickHistory();if(captured&&!stopRequested)window.SetMouseCaptured(true);
    glBindFramebuffer(GL_FRAMEBUFFER,GLuint(framebuffer));glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();
    return action;
}
