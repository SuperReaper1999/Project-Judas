#pragma once
#include "Scene.h"
#include "AssetDatabase.h"
#include <map>

// Editor-authored sparse grids compile to ordinary scene sprites and static MK2
// planar boxes. No second runtime world, collision clock or save representation.
struct TilemapLayer {
    std::string id="ground",name="Ground";
    int sortingLayer=0,order=0;
    bool collision=true;
};
struct TilemapCell {
    std::string layer,frame;
    int x=0,y=0;
    bool flipX=false,flipY=false;
};
struct TilemapGenerated {std::string key,hash;SceneObjectId entity=0;};
struct TilemapRecipe {
    SceneObjectId root=0;
    AssetId sheet;
    glm::vec2 cellSize{1};
    std::vector<TilemapLayer> layers{{}};
    std::vector<TilemapCell> cells;
    std::vector<TilemapGenerated> generated;
};
constexpr size_t kTilemapMaxCells=2048;
std::string TilemapKey(SceneObjectId);
std::string EncodeTilemap(const TilemapRecipe&);
bool ParseTilemap(const std::string&,TilemapRecipe&,std::string& error);
bool ValidateTilemap(const TilemapRecipe&,std::string& error);
// Inclusive grid rectangle; atomic rejection before changing the caller's draft.
bool PaintTilemap(TilemapRecipe&,const std::string& layer,glm::ivec2 from,
                  glm::ivec2 to,const std::string& frame,bool erase,bool flipX,bool flipY,std::string& error);
// Pure CPU, transactional. Existing managed edits/missing children reject rather
// than silently deleting user work. Stable cell keys retain ordinary entity IDs.
bool BakeTilemap(const Scene&,const TilemapRecipe&,const AssetDatabase&,Scene& result,
                 TilemapRecipe& baked,std::string& error);
// Editor lifecycle hooks: copying/deleting a grid must retain/retire its recipe
// in the same undo transaction as the ordinary generated object hierarchy.
bool DuplicateTilemapRecipes(const Scene& source,Scene& result,
                             const std::map<SceneObjectId,SceneObjectId>& ids,std::string& error);
void PruneTilemapRecipes(Scene&);
