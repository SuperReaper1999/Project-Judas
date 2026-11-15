#include "ProfilerView.h"
#include "PerformanceProfiler.h"
#include "imgui.h"
#include <map>
#include <functional>
#include <cstdio>

void DrawIntegratedProfilerView(){
    auto& p=PerformanceProfiler::Get();bool enabled=p.Enabled(),frozen=p.Frozen();
    if(ImGui::Checkbox("Collect",&enabled))p.Enable(enabled);
    ImGui::SameLine();
    if(ImGui::Checkbox("Freeze capture",&frozen))p.Freeze(frozen);
    ImGui::SameLine();
    static ProfileFrameSnapshot selected;static std::uint64_t chosen=0;static bool follow=true;
    if(ImGui::Button("Clear")){p.Clear();selected={};chosen=0;}ImGui::SameLine();ImGui::Checkbox("Follow latest",&follow);
    static char path[512]="/tmp/judas-profile.json";static std::string message;
    ImGui::InputText("Report path",path,sizeof path);ImGui::SameLine();
    if(ImGui::Button("Export JSON")){std::string error;message=p.Export(path,error)?std::string("Exported ")+path:error;}
    if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
    static double refresh=0;static ProfileSummary summary;static std::vector<ProfileFrameIndex> timeline;
    static std::vector<ProfileFrameSnapshot> spikes;static float graph[ProfileLimits::Frames]{};
    const bool update=ImGui::GetTime()>=refresh;
    if(update){refresh=ImGui::GetTime()+.25;timeline=p.Timeline();summary=p.Summary();spikes=p.Spikes();for(size_t i=0;i<timeline.size();++i)graph[i]=float(timeline[i].milliseconds);if(follow&&!timeline.empty())chosen=timeline.back().id;if(chosen){auto refreshed=p.Snapshot(chosen);if(refreshed.id)selected=std::move(refreshed);}}
    ImGui::Text("Outer elapsed ms, last %zu completed frames (max %u)",summary.frames,ProfileLimits::Frames);
    ImGui::Text("Latest %.3f  Mean %.3f  Median %.3f  P95 %.3f  Max %.3f",summary.latest,summary.mean,summary.median,summary.p95,summary.maximum);
    ImGui::PlotLines("##frame-time",graph,int(timeline.size()),0,nullptr,0,FLT_MAX,ImVec2(0,80));
    if(!timeline.empty()){int index=int(timeline.size()-1);for(size_t i=0;i<timeline.size();++i)if(timeline[i].id==chosen)index=int(i);if(ImGui::SliderInt("History frame",&index,0,int(timeline.size()-1))){follow=false;chosen=timeline[index].id;selected=p.Snapshot(chosen);}}
    if(ImGui::Button("Startup")){follow=false;selected=p.Startup();chosen=selected.id;}
    if(ImGui::CollapsingHeader("Recent spikes (owned snapshots)")){for(const auto& f:spikes){char label[120];std::snprintf(label,sizeof label,"Frame %llu: %.3f ms / %zu steps",(unsigned long long)f.id,double(f.end-f.start)/1e6,f.fixed.size());if(ImGui::Selectable(label,chosen==f.id)){follow=false;selected=f;chosen=f.id;}}}
    if(!selected.id){ImGui::TextWrapped("Enable collection to record frames. Opening this window does not enable collection.");return;}
    auto& f=selected;double outer=double(f.end-f.start)/1e6;
    ImGui::Text("Frame %llu / world boundary %llu / %s%s",(unsigned long long)f.id,(unsigned long long)f.world,f.mode.c_str(),f.incomplete?" / CAPTURE BOUNDARY":"");
    ImGui::Text("Outer work %.3f ms / start interval %.3f ms / wait %.3f ms / unattributed %.3f ms",outer,double(f.interval)/1e6,double(f.waiting)/1e6,double(f.unattributed)/1e6);
    double fixed=0;for(const auto& step:f.fixed)fixed+=double(step.end-step.start-step.debuggerWaitNanoseconds)/1e6;
    ImGui::Text("Collection/snapshot cost after outer boundary: %.4f ms (included in next interval)",double(f.collectionNanoseconds)/1e6);
    ImGui::Text("Fixed: %zu steps / %.3f ms / accumulator %.6f s / cap %s / discarded %.6f s / paused %s",f.fixed.size(),fixed,f.accumulatorSeconds,f.capReached?"YES":"no",f.discardedSeconds,f.paused?"yes":"no");
    for(const auto& step:f.fixed)if(step.debuggerWaitNanoseconds)ImGui::Text("Debugger inspection excluded: %.3f ms",double(step.debuggerWaitNanoseconds)/1e6);
    if(ImGui::CollapsingHeader("Fixed step identities"))for(auto step:f.fixed)ImGui::Text("Step %llu: %.3f active ms; simulation %.6f s",(unsigned long long)step.id,double(step.end-step.start-step.debuggerWaitNanoseconds)/1e6,step.simulationSeconds);
    for(const auto& boundary:f.boundaries)ImGui::Text("Boundary: %s",boundary.c_str());
    ImGui::TextWrapped("CPU elapsed is inclusive/exclusive on its named thread. Nested scopes and worker lanes overlap: do not add them to outer time. Worker records appear when completed and may start before this frame.");
    if(ImGui::CollapsingHeader("CPU hierarchy / worker lanes",ImGuiTreeNodeFlags_DefaultOpen))for(const auto& thread:f.threads){
        ImGui::PushID(int(thread.id));ImGui::Text("Thread %llu: %s%s",(unsigned long long)thread.id,thread.name.c_str(),thread.id==f.mainThread?" (main)":" (independent lane)");
        struct Row{ProfileScopeRecord scope;unsigned calls=0;double inclusive=0,exclusive=0;};std::map<unsigned,Row> rows;
        for(const auto& s:f.scopes)if(s.thread==thread.id){auto& row=rows[s.node];row.scope=s;++row.calls;row.inclusive+=double(s.end-s.start)/1e6;row.exclusive+=double(s.exclusive)/1e6;}
        if(ImGui::BeginTable("scopes",6,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable)){
            ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch, 3.0f);
            for(auto name:{"Calls","Inclusive ms","Exclusive ms","Per call ms","% outer (overlap)"})ImGui::TableSetupColumn(name, ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();
            std::function<void(unsigned,unsigned)> draw=[&](unsigned parent,unsigned depth){for(const auto& pair:rows){auto& row=pair.second;if(row.scope.parent!=parent)continue;ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Indent(float(depth)*12);ImGui::Text("%s%s",row.scope.name.c_str(),row.scope.wait?" [wait]":"");ImGui::Unindent(float(depth)*12);
                if(ImGui::IsItemHovered()){auto stats=p.NodeSummary(pair.first,thread.id);ImGui::SetTooltip("%s\nWindow %zu frames / %zu calls: %.4f ms per frame (absent=0), %.4f ms per call",row.scope.name.c_str(),stats.frames,stats.calls,stats.perFrameMilliseconds,stats.perCallMilliseconds);}
                ImGui::TableNextColumn();ImGui::Text("%u",row.calls);ImGui::TableNextColumn();ImGui::Text("%.4f",row.inclusive);ImGui::TableNextColumn();ImGui::Text("%.4f",row.exclusive);ImGui::TableNextColumn();ImGui::Text("%.4f",row.inclusive/row.calls);ImGui::TableNextColumn();ImGui::Text("%.2f",outer>0?row.inclusive*100/outer:0);draw(pair.first,depth+1);
            }};draw(0,0); // A cross-frame child may outlive its recorded parent.
            for(const auto& pair:rows)if(pair.second.scope.parent&&!rows.count(pair.second.scope.parent)){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Text("%s [parent pending/omitted]",pair.second.scope.name.c_str());ImGui::TableNextColumn();ImGui::Text("%u",pair.second.calls);ImGui::TableNextColumn();ImGui::Text("%.4f",pair.second.inclusive);}
            ImGui::EndTable();
        }
        if(thread.id!=f.mainThread&&ImGui::TreeNode("Absolute worker intervals (ns)")){for(const auto& s:f.scopes)if(s.thread==thread.id)ImGui::Text("%s: %llu -> %llu; began in frame %llu",s.name.c_str(),(unsigned long long)s.start,(unsigned long long)s.end,(unsigned long long)s.originFrame);ImGui::TreePop();}ImGui::PopID();
    }
    if(ImGui::CollapsingHeader("Counters"))for(const auto& c:f.counters)ImGui::Text("%s: %.6g (%s)",c.name.c_str(),c.value,c.mode==ProfileCounterMode::Sum?"frame sum":c.mode==ProfileCounterMode::Maximum?"frame max":"latest observation");
    if(ImGui::CollapsingHeader("GPU (delayed; never CPU fallback)")){ImGui::TextWrapped("%s. Inclusive intervals may overlap. GPU + CPU submission + waits are not additive.",f.gpuStatus.c_str());for(const auto& g:f.gpu){if(g.pending)ImGui::Text("Frame %llu / camera %llu / %s: pending",(unsigned long long)g.frame,(unsigned long long)g.camera,g.pass.c_str());else ImGui::Text("Frame %llu / camera %llu / %s: %.4f ms",(unsigned long long)g.frame,(unsigned long long)g.camera,g.pass.c_str(),g.milliseconds);}}
    ImGui::Text("Memory: process RSS %.1f MiB / virtual %.1f MiB (1 Hz); profiler reserved estimate %.1f MiB",f.processResidentBytes/1048576.,f.processVirtualBytes/1048576.,f.profilerReservedBytes/1048576.);
    auto d=p.Diagnostics();ImGui::Text("Drops %llu / truncations %llu / incomplete %llu / GPU drops %llu / labels %llu / nodes %llu / lanes %llu",(unsigned long long)d.droppedEvents,(unsigned long long)d.truncatedFrames,(unsigned long long)d.incompleteScopes,(unsigned long long)d.gpuDropped,(unsigned long long)d.exhaustedLabels,(unsigned long long)d.exhaustedNodes,(unsigned long long)d.exhaustedThreads);
}
