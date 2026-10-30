#pragma once
#include "SpriteAnimation.h"
#include "AssetDatabase.h"
struct EditorPanelState;
// Disk edits are explicit; the source image is never rewritten. Unsaved drafts
// survive panel closing and block switching to another asset/project.
struct SpriteSheetEditor {
    std::string path,asset,saved,status;
    SpriteSheet draft;
    std::vector<SpriteSheet> undo;
    int frame=0,clip=0;
    glm::ivec2 cell{32},margin{0},spacing{0};
    double previewTime=0;
    bool previewPlaying=false,previewClip=true;
    void Remember();
    bool Open(const AssetRecord&,std::string& error);
    bool Save(std::string& error);
    bool Dirty()const;
    void Panel(EditorPanelState&);
};
bool CreateSpriteSheetAsset(AssetDatabase&,const AssetId& image,AssetRecord&,std::string& error);
