#pragma once
#include "Scene.h"
#include "AssetDatabase.h"
#include <map>
#include <optional>

struct SpriteSheet;

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
struct TilemapEditScope { std::string layer; glm::ivec2 cell; };
struct CompiledTilemapObject { std::string key; SceneObject object; };
// Shared by whole-map authoring and incremental runtime edits. A scoped compile
// emits one visual and only the edited layer's 16x16 collision chunk. The caller
// owns identities, asset admission and transactional publication.
bool CompileTilemapObjects(const SceneObject& root,const TilemapRecipe&,const SpriteSheet&,
                          const std::optional<TilemapEditScope>&,
                          std::vector<CompiledTilemapObject>&,std::string& error);
std::string TilemapObjectFingerprint(const SceneObject&);
// Pure CPU, transactional. Existing managed edits/missing children reject rather
// than silently deleting user work. Stable keys retain IDs unless the caller
// requests fresh IDs for changed children so it can stage runtime replacements.
bool BakeTilemap(const Scene&,const TilemapRecipe&,const AssetDatabase&,Scene& result,
                 TilemapRecipe& baked,std::string& error,bool replaceChanged=false);
// Editor lifecycle hooks: copying/deleting a grid must retain/retire its recipe
// in the same undo transaction as the ordinary generated object hierarchy.
bool DuplicateTilemapRecipes(const Scene& source,Scene& result,
                             const std::map<SceneObjectId,SceneObjectId>& ids,std::string& error);
void PruneTilemapRecipes(Scene&);
