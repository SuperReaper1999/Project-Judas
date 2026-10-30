#pragma once
#include "authoring/Tilemap.h"
#include "DebugDraw.h"
#include <array>
#include <functional>
class EditorDocument;
class EditorCamera;
class Window;
struct EditorPanelState;
struct TilemapEditor {
    TilemapRecipe recipe;
    bool painting=false,flipX=false,flipY=false;
    int layer=0,frame=0,tool=0;
    DebugLineList overlay;
    void Panel(EditorDocument&,EditorPanelState&);
    bool Create(EditorDocument&,const AssetDatabase&,const AssetId&,glm::vec3 position,std::string& error);
    bool Apply(EditorDocument&,const AssetDatabase&,const TilemapRecipe&,std::string& error);
    bool Stroke(EditorDocument&,const AssetDatabase&,glm::vec3 origin,glm::vec3 direction,
                bool canPaint,bool leftPressed,bool rightPressed,bool leftHeld,bool rightHeld,bool cancel,std::string& error);
    bool CellAt(const Scene&,glm::vec3 origin,glm::vec3 direction,glm::ivec2&,std::string& error)const;
    void Sync(const EditorDocument&);
    void Autotest(int frame,EditorDocument&,EditorPanelState&,EditorCamera&,Window&,const std::function<void()>& capture);
    bool StrokeActive()const{return active;}
    unsigned TestFailures()const{return testFailures;}
private:
    uint64_t generation=~uint64_t(0),strokeGeneration=0;
    std::string path,palette,status,brushFrame,brushSheet,strokeFrame;
    glm::vec2 cellSize{1};
    std::array<char,129> name{};
    unsigned testFailures=0;size_t testCells=0;std::string testSource;glm::ivec2 testCell{0},testPointer{0},previousPointer{0};
    bool active=false,right=false;
    glm::ivec2 first{0},last{0};
    TilemapRecipe stroke;
};
