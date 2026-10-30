#include "SpriteSheetEditor.h"
#include "EditorPanels.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "TextureLoader.h"
#include "imgui.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
namespace {
    bool Text(const char* label,std::string& s){
        std::vector<char> b(std::max(size_t(8192),s.size()+256),0);
        std::copy(s.begin(),s.end(),b.begin());
        if(!ImGui::InputText(label,b.data(),b.size()))return false;
        s=b.data();
        return true;
    }
}
void SpriteSheetEditor::Remember(){
    if(undo.size()==64)undo.erase(undo.begin());
    undo.push_back(draft);
}
bool SpriteSheetEditor::Dirty()const{
    return !path.empty()&&EncodeSpriteSheet(draft)!=saved;
}
bool SpriteSheetEditor::Open(const AssetRecord& record,std::string& error){
    if(record.type!=AssetType::SpriteSheet||record.missing){
        error="select a valid sprite sheet";
        return false;
    }
    if(Dirty()){
        error="save or discard the current sprite-sheet draft before switching";
        return false;
    }
    SpriteSheet candidate;
    if(!LoadSpriteSheet(record.path,candidate,error))return false;
    path=record.path;
    asset=record.id;
    draft=std::move(candidate);
    saved=EncodeSpriteSheet(draft);
    undo.clear();
    frame=clip=0;
    previewTime=0;
    previewPlaying=false;
    error.clear();
    return true;
}
bool SpriteSheetEditor::Save(std::string& error){
    SpriteSheet disk;
    if(!LoadSpriteSheet(path,disk,error))return false;
    if(EncodeSpriteSheet(disk)!=saved){
        error="sprite sheet changed on disk; preserve this draft and reopen before overwriting";
        return false;
    }
    if(!SaveSpriteSheet(path,draft,error))return false;
    saved=EncodeSpriteSheet(draft);
    error.clear();
    return true;
}
bool CreateSpriteSheetAsset(AssetDatabase& db,const AssetId& image,AssetRecord& out,std::string& error){
    const auto* record=db.Find(image);
    if(!record||record->missing||record->readOnly||record->type!=AssetType::Texture){
        error="choose a writable tracked image";
        return false;
    }
    auto path=std::filesystem::path(record->path);
    path.replace_extension(".judassprite");
    auto relative=std::filesystem::path(record->relativePath);
    relative.replace_extension(".judassprite");
    if(auto* existing=db.FindByRelativePath(relative.generic_string())){
        if(existing->type!=AssetType::SpriteSheet){
            error="destination is not a sprite sheet";
            return false;
        }
        out=*existing;
        return true;
    }
    if(std::filesystem::exists(path)){
        error="sprite sheet already exists untracked; import it explicitly";
        return false;
    }
    TextureData texture;
    if(!LoadTextureFromFile(record->path,texture,error))return false;
    SpriteSheet sheet;
    sheet.texture=image;
    sheet.width=texture.width;
    sheet.height=texture.height;
    sheet.frames.push_back({
        "whole","Whole image",{
            0,0,texture.width,texture.height
        },{
            .5f,.5f
        }
    });
    if(!SaveSpriteSheet(path.string(),sheet,error))return false;
    if(!db.Track(path.string(),out,error)){
        std::filesystem::remove(path);
        return false;
    }
    return true;
}
void SpriteSheetEditor::Panel(EditorPanelState& state){
    if(!state.showSpriteSheetEditor)return;
    ImGui::SetNextWindowSize({
        850,650
    },ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Sprite sheets and frame animation",&state.showSpriteSheetEditor)){
        ImGui::End();
        return;
    }
    const auto* requested=state.assets?state.assets->Find(state.spriteSheetEdit):nullptr;
    if(requested&&requested->path!=path){
        if(Dirty()){
            ImGui::TextWrapped("Unsaved draft: %s. Save or discard it before changing sheets/projects.",path.c_str());
            if(ImGui::Button("Save current draft")){
                if(Save(status)&&state.resources)state.resources->Invalidate(asset);
            }
            ImGui::SameLine();
            if(ImGui::Button("Discard draft")){
                path.clear();
                saved.clear();
                undo.clear();
            }
            ImGui::End();
            return;
        }
        Open(*requested,status);
    }
    if(path.empty()){
        ImGui::TextWrapped("Right-click an image in Assets → Create/edit sprite sheet, or open a .judassprite asset.");
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("%s%s",path.c_str(),Dirty()?"  (unsaved)":"");
    const bool editable=state.mode==EditorMode::Edit&&requested&&!requested->readOnly&&requested->path==path;
    ImGui::BeginDisabled(!editable);
    if(ImGui::Button("Save sheet and clips")){
        if(Save(status)){
            status="Saved; references retain their frame keys";
            if(state.resources)state.resources->Invalidate(asset);
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Undo draft")&&!undo.empty()){
        draft=undo.back();
        undo.pop_back();
    }
    ImGui::SameLine();
    if(ImGui::Button("Discard / reload")){
        path.clear();
        saved.clear();
        Open(*requested,status);
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s",status.c_str());
    ImGui::BeginChild("Sheet authoring",{
        -280,0
    },ImGuiChildFlags_Borders);
    if(ImGui::CollapsingHeader("Slice image / named frames",ImGuiTreeNodeFlags_DefaultOpen)){
        if(state.resources&&state.resources->GetRenderer()){
            state.resources->RequestTexture(draft.texture);
            auto tex=state.resources->TryGetTexture(draft.texture);
            if(tex.IsValid()){
                const float width=std::min(ImGui::GetContentRegionAvail().x,640.f),height=width*draft.height/draft.width;
                auto origin=ImGui::GetCursorScreenPos();
                ImGui::Image(ImTextureID(state.resources->GetRenderer()->EditorImageToken(tex)),{
                    width,height
                },{
                    0,1
                },{
                    1,0
                });
                auto* draw=ImGui::GetWindowDrawList();
                for(size_t i=0;i<draft.frames.size();++i){
                    const auto& f=draft.frames[i];
                    ImVec2 a{
                        origin.x+width*f.pixels.x/draft.width,origin.y+height*f.pixels.y/draft.height
                    },b{
                        a.x+width*f.pixels.z/draft.width,a.y+height*f.pixels.w/draft.height
                    };
                    draw->AddRect(a,b,i==size_t(frame)?IM_COL32(255,204,64,255):IM_COL32(140,160,180,180),0,0,i==size_t(frame)?2.f:1.f);
                    if(ImGui::IsItemHovered()){
                        auto mouse=ImGui::GetMousePos();
                        if(mouse.x>=a.x&&mouse.y>=a.y&&mouse.x<b.x&&mouse.y<b.y){
                            ImGui::SetTooltip("%s (%s)",f.name.c_str(),f.id.c_str());
                            if(ImGui::IsMouseClicked(0)){
                                frame=int(i);
                                previewClip=false;
                            }
                        }
                    }
                }
            }
        }
        ImGui::BeginDisabled(!editable);
        ImGui::InputInt2("Cell width / height",&cell.x);
        ImGui::InputInt2("Margin x / y",&margin.x);
        ImGui::InputInt2("Spacing x / y",&spacing.x);
        if(ImGui::Button("Slice grid (preserve coordinate keys)")){
            auto next=draft;
            if(SliceSpriteSheet(next,cell,margin,spacing,status)){
                Remember();
                draft=std::move(next);
                frame=0;
            }
        }
        auto ppu=draft.pixelsPerUnit;
        if(ImGui::DragFloat("Pixels per world unit",&ppu,.1f,.001f,1e6f)){
            Remember();
            draft.pixelsPerUnit=ppu;
        }
        frame=std::clamp(frame,0,int(draft.frames.size())-1);
        if(ImGui::BeginCombo("Frame",draft.frames[frame].name.c_str())){
            for(size_t i=0;i<draft.frames.size();++i)if(ImGui::Selectable((draft.frames[i].name+"##"+draft.frames[i].id).c_str(),frame==int(i))){
                frame=int(i);
                previewClip=false;
            }
            ImGui::EndCombo();
        }
        auto f=draft.frames[frame];
        ImGui::Text("Stable key: %s",f.id.c_str());
        bool changed=Text("Display name",f.name);
        changed=ImGui::InputInt4("Top-left x / y / width / height",&f.pixels.x)||changed;
        changed=ImGui::SliderFloat2("Pivot",&f.pivot.x,0,1)||changed;
        if(changed){
            Remember();
            draft.frames[frame]=f;
        }
        if(ImGui::Button("Add manual frame")){
            Remember();
            std::string id="frame-"+std::to_string(draft.frames.size());
            while(std::any_of(draft.frames.begin(),draft.frames.end(),[&](const auto& v){
                return v.id==id;
            }))id+="_";
            draft.frames.push_back({
                id,id,{
                    0,0,32,32
                },{
                    .5f,.5f
                }
            });
            frame=int(draft.frames.size())-1;
        }
        ImGui::SameLine();
        if(ImGui::Button("Remove selected frame")&&draft.frames.size()>1){
            Remember();
            draft.frames.erase(draft.frames.begin()+frame);
            frame=0;
        }
        ImGui::EndDisabled();
    }
    if(ImGui::CollapsingHeader("Clips and timed events",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::BeginDisabled(!editable);
        if(ImGui::Button("Add clip")){
            Remember();
            std::string name="clip-"+std::to_string(draft.clips.size());
            while(FindSpriteClip(draft,name))name+="_";
            draft.clips.push_back({
                name,{
                    {
                        draft.frames[frame].id,.1
                    }
                }, {
                }
            });
            clip=int(draft.clips.size())-1;
            previewTime=0;
        }
        if(!draft.clips.empty()){
            clip=std::clamp(clip,0,int(draft.clips.size())-1);
            if(ImGui::BeginCombo("Clip",draft.clips[clip].name.c_str())){
                for(size_t i=0;i<draft.clips.size();++i)if(ImGui::Selectable(draft.clips[i].name.c_str(),clip==int(i))){
                    clip=int(i);
                    previewTime=0;
                    previewClip=true;
                }
                ImGui::EndCombo();
            }
            auto c=draft.clips[clip];
            bool changed=Text("Clip name",c.name);
            for(size_t i=0;i<c.frames.size();++i){
                ImGui::PushID(int(i));
                auto& v=c.frames[i];
                if(ImGui::BeginCombo("Frame key",v.frame.c_str())){
                    for(const auto& f:draft.frames)if(ImGui::Selectable((f.name+"##"+f.id).c_str(),v.frame==f.id)){
                        v.frame=f.id;
                        changed=true;
                    }
                    ImGui::EndCombo();
                }
                changed=ImGui::InputDouble("Seconds",&v.seconds,.01,.1,"%.4f")||changed;
                if(ImGui::Button("Move earlier")&&i>0){
                    std::swap(c.frames[i-1],c.frames[i]);
                    changed=true;
                }
                ImGui::SameLine();
                if(ImGui::Button("Remove")&&c.frames.size()>1){
                    c.frames.erase(c.frames.begin()+i);
                    changed=true;
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if(ImGui::Button("Append selected frame")){
                c.frames.push_back({
                    draft.frames[frame].id,.1
                });
                changed=true;
            }
            for(size_t i=0;i<c.events.size();++i){
                ImGui::PushID(int(i)+10000);
                auto& e=c.events[i];
                ImGui::Separator();
                ImGui::Text("Event key: %s",e.id.c_str());
                changed=Text("Event name",e.name)||changed;
                changed=Text("Data",e.data)||changed;
                changed=ImGui::InputDouble("Event seconds",&e.time,.01,.1,"%.6f")||changed;
                if(ImGui::Button("Delete event")){
                    c.events.erase(c.events.begin()+i);
                    changed=true;
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if(ImGui::Button("Add event at preview time")){
                std::string id="event-"+std::to_string(c.events.size());
                while(std::any_of(c.events.begin(),c.events.end(),[&](const auto& e){
                    return e.id==id;
                }))id+="_";
                c.events.push_back({
                    id,"event","",double(previewTime)
                });
                changed=true;
            }
            if(changed){
                Remember();
                draft.clips[clip]=std::move(c);
            }
            if(ImGui::Button("Remove clip")){
                Remember();
                draft.clips.erase(draft.clips.begin()+clip);
                clip=0;
            }
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Sheet preview",{
        0,0
    },ImGuiChildFlags_Borders);
    // Preview is a local clock and sampler: never enters a RuntimeWorld or emits events.
    SpritePlayback preview;
    preview.sheet=asset;
    preview.playing=false;
    preview.frame=draft.frames[std::clamp(frame,0,int(draft.frames.size())-1)].id;
    ImGui::Checkbox("Preview selected clip",&previewClip);
    if(previewClip&&!draft.clips.empty()){
        clip=std::clamp(clip,0,int(draft.clips.size())-1);
        const auto& c=draft.clips[clip];
        preview.clip=c.name;
        double duration=SpriteClipDuration(c);
        ImGui::Checkbox("Preview playback (no gameplay events)",&previewPlaying);
        if(previewPlaying&&duration>0)previewTime=std::fmod(previewTime+ImGui::GetIO().DeltaTime,duration);
        const double zero=0;
        ImGui::SliderScalar("Scrub / seconds",ImGuiDataType_Double,&previewTime,&zero,&duration,"%.6f s");
    }
    preview.time=previewTime;
    preview.loop=previewPlaying;
    SpriteSettings sprite;
    std::string error;
    if(ValidSpriteSheet(draft,error)&&SampleSpriteFrame(draft,preview,sprite,error)&&state.resources){
        state.resources->RequestTexture(draft.texture);
        auto tex=state.resources->TryGetTexture(draft.texture);
        if(tex.IsValid()){
            const float scale=std::min(160.f,240.f/std::max(sprite.size.x,sprite.size.y));
            auto size=sprite.size*scale;
            // Texture handle is resolved through the renderer's UI token, never assumed a GL name.
            auto* renderer=state.resources->GetRenderer();
            if(renderer){
                auto token=renderer->EditorImageToken(tex);
                ImGui::Image((ImTextureID)(intptr_t)token,{
                    size.x,size.y
                },{
                    sprite.rect.x,1-sprite.rect.y
                },{
                    sprite.rect.x+sprite.rect.z,1-sprite.rect.y-sprite.rect.w
                });
            }
        }
    }
    else ImGui::TextWrapped("Draft error: %s",error.c_str());
    ImGui::TextWrapped("Save is explicit. Frame keys remain stable when renamed. Removed referenced keys are errors. Preview/scrub never delivers gameplay events. Play uses the last saved asset.");
    ImGui::EndChild();
    ImGui::End();
}
