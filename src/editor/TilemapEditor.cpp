#include "TilemapEditor.h"
#include "EditorDocument.h"
#include "EditorCamera.h"
#include "Window.h"
#include <SDL2/SDL.h>
#include "SceneSerialization.h"
#include "EditorPanels.h"
#include "SpriteAnimation.h"
#include "Prefab.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>

void TilemapEditor::Sync(const EditorDocument& doc){
    if(path==doc.Path()&&generation==doc.Generation())return;
    if(path!=doc.Path()&&generation!=doc.Generation()){recipe={};painting=false;}
    path=doc.Path();generation=doc.Generation();active=false;overlay.Clear();
    auto it=doc.GetScene().Settings().authoringRecipes.find(TilemapKey(recipe.root));
    if(it!=doc.GetScene().Settings().authoringRecipes.end()){
        TilemapRecipe loaded;if(ParseTilemap(it->second,loaded,status))recipe=std::move(loaded);
    }else recipe={};
    if(!doc.GetScene().Find(recipe.root)){recipe={};painting=false;}
    layer=std::clamp(layer,0,int(recipe.layers.size())-1);
    cellSize=recipe.cellSize;if(recipe.root)palette=recipe.sheet;
    name.fill(0);if(!recipe.layers.empty())std::copy_n(recipe.layers[layer].name.data(),std::min(size_t(128),recipe.layers[layer].name.size()),name.data());
}
bool TilemapEditor::Apply(EditorDocument& doc,const AssetDatabase& assets,const TilemapRecipe& candidate,std::string& error){
    if(doc.EditInProgress()){error="Finish the current scene edit before rebuilding tiles";return false;}
    Scene result;TilemapRecipe baked;if(!BakeTilemap(doc.GetScene(),candidate,assets,result,baked,error))return false;
    doc.BeginEdit();doc.GetScene()=std::move(result);doc.CommitEdit();
    if(!doc.ValidationError().empty()){error=doc.ValidationError();return false;}
    recipe=std::move(baked);generation=~uint64_t(0);Sync(doc);return true;
}
bool TilemapEditor::Create(EditorDocument& doc,const AssetDatabase& assets,const AssetId& sheet,glm::vec3 position,std::string& error){
    if(doc.EditInProgress()){error="Finish the current scene edit first";return false;}
    Scene candidate=doc.GetScene();auto root=candidate.CreateObject("Tilemap").id;candidate.Find(root)->transform.position=position;
    TilemapRecipe r;r.root=root;r.sheet=sheet;Scene result;TilemapRecipe baked;
    if(!BakeTilemap(candidate,r,assets,result,baked,error))return false;
    doc.BeginEdit();doc.GetScene()=std::move(result);doc.CommitEdit();if(!doc.ValidationError().empty()){error=doc.ValidationError();return false;}
    recipe=std::move(baked);doc.Select(recipe.root);generation=~uint64_t(0);path=doc.Path();Sync(doc);painting=true;return true;
}
bool TilemapEditor::CellAt(const Scene& scene,glm::vec3 origin,glm::vec3 direction,glm::ivec2& out,std::string& error)const{
    Scene flat;if(!FlattenHierarchy(scene,flat,error))return false;const auto* root=flat.Find(recipe.root);if(!root){error="Select a live tilemap";return false;}
    const auto& t=root->transform;const auto inverse=glm::inverse(t.rotation);
    auto ro=(inverse*(origin-t.position))/t.scale,rd=(inverse*direction)/t.scale;
    if(!std::isfinite(glm::dot(rd,rd))||std::abs(rd.z)<1e-6f){error="View is parallel to the tilemap plane";return false;}
    float hit=-ro.z/rd.z;if(hit<0){error="Tilemap is behind the view";return false;}
    auto point=(ro+rd*hit)/glm::vec3(recipe.cellSize,1);if(!std::isfinite(point.x)||!std::isfinite(point.y)||point.x < -512||point.y < -512||point.x>=513||point.y>=513){error="Brush outside -512..512 cells";return false;}
    out={int(std::floor(point.x)),int(std::floor(point.y))};return true;
}
bool TilemapEditor::Stroke(EditorDocument& doc,const AssetDatabase& assets,glm::vec3 origin,glm::vec3 direction,
                          bool canPaint,bool leftPressed,bool rightPressed,bool leftHeld,bool rightHeld,bool cancel,std::string& error){
    overlay.Clear();
    if(active&&(cancel||doc.Generation()!=strokeGeneration||doc.Path()!=path)){active=false;error="Tile stroke cancelled; scene unchanged";return true;}
    Sync(doc);
    if(!painting||!recipe.root){active=false;return false;}
    if(!canPaint&&!active)return false;
    glm::ivec2 hit;const bool hitPlane=canPaint&&CellAt(doc.GetScene(),origin,direction,hit,error);
    if(hitPlane){
        Scene flat;std::string ignored;if(FlattenHierarchy(doc.GetScene(),flat,ignored)){
            const auto& t=flat.Find(recipe.root)->transform;
            auto drawCell=[&](glm::ivec2 a,glm::ivec2 b){glm::vec3 p[4]={{a.x*recipe.cellSize.x,a.y*recipe.cellSize.y,.01f},{(b.x+1)*recipe.cellSize.x,a.y*recipe.cellSize.y,.01f},{(b.x+1)*recipe.cellSize.x,(b.y+1)*recipe.cellSize.y,.01f},{a.x*recipe.cellSize.x,(b.y+1)*recipe.cellSize.y,.01f}};
                for(auto& v:p)v=t.position+t.rotation*(t.scale*v);
                for(int i=0;i<4;++i)overlay.Line(p[i],p[(i+1)%4],{1,.7f,.18f});};
            auto world=[&](float x,float y){return t.position+t.rotation*(t.scale*glm::vec3(x*recipe.cellSize.x,y*recipe.cellSize.y,.008f));};
            for(int offset=-8;offset<=8;++offset){
                overlay.Line(world(hit.x+offset,hit.y-8),world(hit.x+offset,hit.y+8),{.22f,.30f,.36f});
                overlay.Line(world(hit.x-8,hit.y+offset),world(hit.x+8,hit.y+offset),{.22f,.30f,.36f});
            }
            drawCell(active&&tool>=2?glm::min(first,hit):hit,active&&tool>=2?glm::max(first,hit):hit);
        }
    }
    bool began=false;
    if(!active&&(leftPressed||rightPressed)&&hitPlane){
        SpriteSheet sheet;const auto* asset=assets.Find(recipe.sheet);
        if(!asset||!LoadSpriteSheet(asset->path,sheet,error)||sheet.frames.empty()){error="Choose a valid palette before painting";return true;}
        const bool erasing=rightPressed||tool==1||tool==3;
        strokeFrame=brushSheet==recipe.sheet?brushFrame:std::string{};
        if(strokeFrame.empty()||erasing)strokeFrame=sheet.frames[std::clamp(frame,0,int(sheet.frames.size())-1)].id;
        if(std::none_of(sheet.frames.begin(),sheet.frames.end(),[&](const auto& f){return f.id==strokeFrame;})){error="Brush frame was removed; select another palette tile";return true;}
        began=true;active=true;right=rightPressed;stroke=recipe;strokeGeneration=doc.Generation();first=last=hit;
    }
    if(!active)return canPaint;
    const bool erase=right||tool==1||tool==3;
    if(hitPlane&&tool<2&&(began||hit!=last)){
        // Bresenham stamps every crossed cell, including fast drags and negatives.
        auto point=last;int dx=std::abs(hit.x-point.x),dy=-std::abs(hit.y-point.y),sx=point.x<hit.x?1:-1,sy=point.y<hit.y?1:-1,err=dx+dy;
        for(;;){if(!PaintTilemap(stroke,recipe.layers[layer].id,point,point,strokeFrame,erase,flipX,flipY,error)){active=false;return true;}if(point==hit)break;int twice=err*2;if(twice>=dy){err+=dy;point.x+=sx;}if(twice<=dx){err+=dx;point.y+=sy;}}
        last=hit;
    }else if(hitPlane)last=hit;
    if(right?rightHeld:leftHeld)return true;
    if(tool>=2&&!PaintTilemap(stroke,recipe.layers[layer].id,first,last,strokeFrame,erase,flipX,flipY,error)){active=false;return true;}
    active=false;if(EncodeTilemap(stroke)!=EncodeTilemap(recipe))Apply(doc,assets,stroke,error);return true;
}
void TilemapEditor::Panel(EditorDocument& doc,EditorPanelState& state){
    if(!state.showTilemapEditor||state.mode!=EditorMode::Edit){active=false;painting=false;return;}
    Sync(doc);
    if(!active)for(auto id=doc.Selected();id;){
        auto it=doc.GetScene().Settings().authoringRecipes.find(TilemapKey(id));
        if(it!=doc.GetScene().Settings().authoringRecipes.end()&&id!=recipe.root){TilemapRecipe selected;std::string error;if(ParseTilemap(it->second,selected,error)){recipe=std::move(selected);generation=~uint64_t(0);Sync(doc);}break;}
        const auto* object=doc.GetScene().Find(id);if(!object)break;id=object->parent;
    }
    ImGui::SetNextWindowSize({420,600},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Tilemap painter",&state.showTilemapEditor)){ImGui::End();return;}
    if(!state.assets){ImGui::TextUnformatted("Open a project to paint tiles.");ImGui::End();return;}
    if(!state.tilemapPalette.empty()){palette=state.tilemapPalette;state.tilemapPalette.clear();}
    if(ImGui::BeginCombo("Map",recipe.root&&doc.GetScene().Find(recipe.root)?doc.GetScene().Find(recipe.root)->name.c_str():"Choose a map")){
        for(const auto& [key,text]:doc.GetScene().Settings().authoringRecipes)if(key.rfind("tilemap.",0)==0){TilemapRecipe r;std::string e;if(ParseTilemap(text,r,e)&&doc.GetScene().Find(r.root)){
            ImGui::PushID(key.c_str());if(ImGui::Selectable(doc.GetScene().Find(r.root)->name.c_str(),r.root==recipe.root)){recipe=r;doc.Select(r.root);generation=~uint64_t(0);Sync(doc);}ImGui::PopID();}}
        ImGui::EndCombo();
    }
    const auto* selected=state.assets->Find(palette);
    if(ImGui::BeginCombo("Sprite-sheet palette",selected?selected->relativePath.c_str():"Choose a sprite sheet")){
        for(const auto& [id,a]:state.assets->Records())if(a.type==AssetType::SpriteSheet&&!a.missing)if(ImGui::Selectable(a.relativePath.c_str(),id==palette))palette=id;
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(palette.empty()||active);
    if(ImGui::Button("Create tilemap")){if(Create(doc,*state.assets,palette,state.cameraFocus,status))state.view2D=true;state.status=status;}
    ImGui::EndDisabled();
    if(!recipe.root){ImGui::TextWrapped("Import an image, right-click it in Assets to create/slice a sprite sheet, then choose it here.");ImGui::End();return;}
    if(!state.view2D&&ImGui::Button("Switch to 2D scene view"))state.view2D=true;
    ImGui::Checkbox("Paint in Scene view",&painting);
    ImGui::TextWrapped("Left drag: draw. Right drag: erase. Release to apply; Esc cancels. Middle mouse pans; wheel zooms. Disable painting to select objects.");
    const char* modes[]={"Brush","Eraser","Rectangle fill","Rectangle erase"};ImGui::Combo("Tool",&tool,modes,4);
    ImGui::Checkbox("Flip X",&flipX);ImGui::SameLine();ImGui::Checkbox("Flip Y",&flipY);
    if(ImGui::BeginCombo("Layer",recipe.layers[layer].name.c_str())){for(size_t i=0;i<recipe.layers.size();++i)if(ImGui::Selectable(recipe.layers[i].name.c_str(),layer==int(i))){layer=int(i);generation=~uint64_t(0);Sync(doc);}ImGui::EndCombo();}
    ImGui::BeginDisabled(active);
    if(recipe.layers.size()<8&&ImGui::Button("Add decorative layer")){auto next=recipe;std::string id;for(unsigned i=0;;++i){id="layer"+std::to_string(i);if(std::none_of(next.layers.begin(),next.layers.end(),[&](const auto& l){return l.id==id;}))break;}next.layers.push_back({id,"New layer",0,next.layers.back().order+1,false});if(Apply(doc,*state.assets,next,status)){layer=int(recipe.layers.size())-1;generation=~uint64_t(0);Sync(doc);}}
    if(ImGui::CollapsingHeader("Layer and grid settings")){
        ImGui::InputText("Layer name",name.data(),name.size());if(ImGui::Button("Rename layer")){auto next=recipe;next.layers[layer].name=name.data();Apply(doc,*state.assets,next,status);}
        auto next=recipe;bool collision=next.layers[layer].collision;if(ImGui::Checkbox("Solid collision cells",&collision)){next.layers[layer].collision=collision;Apply(doc,*state.assets,next,status);}
        int order=recipe.layers[layer].order;if(ImGui::InputInt("Sort order (Enter)",&order,1,10,ImGuiInputTextFlags_EnterReturnsTrue)){next=recipe;next.layers[layer].order=order;Apply(doc,*state.assets,next,status);}
        int sorting=recipe.layers[layer].sortingLayer;if(ImGui::InputInt("Sorting layer (Enter)",&sorting,1,10,ImGuiInputTextFlags_EnterReturnsTrue)){next=recipe;next.layers[layer].sortingLayer=sorting;Apply(doc,*state.assets,next,status);}
        ImGui::InputFloat2("Cell size (metres)",&cellSize.x);if(ImGui::Button("Resize grid")){next=recipe;next.cellSize=cellSize;Apply(doc,*state.assets,next,status);}
        if(ImGui::Button("Rebuild from palette")){next=recipe;next.sheet=palette;if(Apply(doc,*state.assets,next,status)&&state.resources)state.resources->Invalidate(recipe.sheet);}
        if(recipe.layers.size()>1&&ImGui::Button("Delete layer and its cells")){next=recipe;const auto id=next.layers[layer].id;next.layers.erase(next.layers.begin()+layer);next.cells.erase(std::remove_if(next.cells.begin(),next.cells.end(),[&](const auto& c){return c.layer==id;}),next.cells.end());layer=0;Apply(doc,*state.assets,next,status);}
        if(ImGui::Button("Detach grid; retain ordinary objects")){doc.BeginEdit();doc.GetScene().Settings().authoringRecipes.erase(TilemapKey(recipe.root));doc.CommitEdit();generation=~uint64_t(0);Sync(doc);painting=false;}
        ImGui::TextWrapped("Static grid, full-cell collision, 2,048 occupied cells maximum. Frame pivots do not move cell centres. Prefabs contain compiled ordinary objects; paint the source scene.");
    }
    ImGui::EndDisabled();
    if(recipe.root&&state.resources){std::string error;auto data=state.resources->GetSpriteSheet(recipe.sheet,error);
        if(data){const auto& sheet=*data;
            if(brushSheet!=recipe.sheet){brushSheet=recipe.sheet;brushFrame.clear();}
            if(brushFrame.empty()&&frame>=0)brushFrame=sheet.frames[std::clamp(frame,0,int(sheet.frames.size())-1)].id;
            auto found=std::find_if(sheet.frames.begin(),sheet.frames.end(),[&](const auto& f){return f.id==brushFrame;});
            frame=found==sheet.frames.end()?-1:int(found-sheet.frames.begin());
            if(frame<0){painting=false;status="Selected brush frame was removed; choose a tile. Existing cells were not retargeted.";}
            state.resources->RequestTexture(sheet.texture);auto texture=state.resources->TryGetTexture(sheet.texture);
            if(texture.IsValid()){const auto token=state.resources->GetRenderer()->EditorImageToken(texture);const float size=56*ImGui::GetIO().FontGlobalScale;const int columns=std::max(1,int(ImGui::GetContentRegionAvail().x/(size+10)));
                ImGui::SeparatorText("Tile palette");ImGui::BeginChild("Palette",{0,180},ImGuiChildFlags_Borders);ImGuiListClipper clipper;clipper.Begin((int(sheet.frames.size())+columns-1)/columns,size+10);
                while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row)for(int col=0;col<columns;++col){int index=row*columns+col;if(index>=int(sheet.frames.size()))break;const auto& f=sheet.frames[index];if(col)ImGui::SameLine();ImGui::PushID(index);const bool selectedFrame=index==frame;if(selectedFrame)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
                    if(ImGui::ImageButton("tile",ImTextureID(token),{size,size},{float(f.pixels.x)/sheet.width,1-float(f.pixels.y)/sheet.height},{float(f.pixels.x+f.pixels.z)/sheet.width,1-float(f.pixels.y+f.pixels.w)/sheet.height})){frame=index;brushFrame=f.id;}
                    if(selectedFrame)ImGui::PopStyleColor();
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s",f.name.c_str(),f.id.c_str());
                    ImGui::PopID();}
                ImGui::EndChild();ImGui::Text("%s | %zu painted cells",frame>=0?sheet.frames[frame].name.c_str():"Choose a tile",recipe.cells.size());
            }else ImGui::TextUnformatted("Loading palette image...");
        }else ImGui::TextWrapped("%s",error=="loading"?"Loading palette...":error.c_str());
    }
    ImGui::TextWrapped("%s",status.c_str());state.status=status.empty()?state.status:status;ImGui::End();
}

void TilemapEditor::Autotest(int frameNumber,EditorDocument& doc,EditorPanelState& state,EditorCamera& camera,Window& window,const std::function<void()>& capture){
    auto check=[&](bool ok,const char* label){testFailures+=!ok;std::fprintf(stderr,"[Tilemap editor] %s: %s\n",ok?"PASS":"FAIL",label);};
    if(frameNumber==3){
        for(const auto& [key,text]:doc.GetScene().Settings().authoringRecipes){TilemapRecipe r;std::string error;if(ParseTilemap(text,r,error)){recipe=r;doc.Select(r.root);generation=~uint64_t(0);Sync(doc);break;}}
        state.showTilemapEditor=true;state.showSpriteSheetEditor=false;state.view2D=true;
        check(recipe.root!=0,"existing authored grid is discoverable");
    }
    if(frameNumber==11){state.showProfiler=false;state.debug={};state.showWorldBuilding=false;
        camera.Set2D(true);camera.LookAt({-8,3,0},10);painting=true;tool=0;frame=0;
        testCells=recipe.cells.size();SaveSceneToString(doc.GetScene(),testSource);
    }
    if(frameNumber==12){const auto& v=state.workspace.viewport;ImGui::SetWindowFocus(nullptr);
        testPointer={int(v.position.x+v.size.x*.5f),int(v.position.y+v.size.y*.5f)};
        SDL_GetGlobalMouseState(&previousPointer.x,&previousPointer.y);SDL_WarpMouseInWindow(window.NativeWindow(),testPointer.x,testPointer.y);
        glm::vec3 origin,direction;camera.PixelRay(int(v.size.x*.5f),int(v.size.y*.5f),int(v.size.x),int(v.size.y),origin,direction);std::string error;
        check(CellAt(doc.GetScene(),origin,direction,testCell,error),"native viewport ray reaches the intended cell");
    }
    if(frameNumber==13||frameNumber==15){SDL_Event event{};event.type=frameNumber==13?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;event.button.windowID=SDL_GetWindowID(window.NativeWindow());event.button.button=SDL_BUTTON_LEFT;event.button.x=testPointer.x;event.button.y=testPointer.y;SDL_PushEvent(&event);}
    if(frameNumber==17){check(recipe.cells.size()==testCells+1&&doc.IsDirty()&&std::any_of(recipe.cells.begin(),recipe.cells.end(),[&](const auto& c){return c.layer==recipe.layers[layer].id&&c.x==testCell.x&&c.y==testCell.y;}),"real SDL/ImGui pointer paints the intended snapped viewport cell");ImGui::SetWindowFocus("Tilemap painter");}
    if(frameNumber==18){capture();SDL_WarpMouseGlobal(previousPointer.x,previousPointer.y);if(recipe.cells.size()==testCells+1)doc.Undo();Sync(doc);painting=false;
        std::string text;SaveSceneToString(doc.GetScene(),text);check(text==testSource,"viewport stroke Undo restores exact authored source before Play");}
}
