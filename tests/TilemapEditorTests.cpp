#include "editor/TilemapEditor.h"
#include "editor/EditorDocument.h"
#include "SpriteAnimation.h"
#include "ScreenshotWriter.h"
#include "SceneSerialization.h"
#include <filesystem>
#include <stdexcept>
#include <cstdio>
namespace fs=std::filesystem;
unsigned checks=0;
void Check(bool good,const std::string& label){++checks;std::printf("%s %s\n",good?"PASS":"FAIL",label.c_str());if(!good)throw std::runtime_error(label);}
int main(int argc,char** argv){try{
 auto root=fs::absolute(argc>1?argv[1]:".cache/tilemap-editor");fs::create_directories(root/"Assets");std::string error;
 Check(WriteRgbPng((root/"Assets/tile.png").string(),4,4,std::vector<unsigned char>(4*4*3,180)),"real palette image");
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord image,palette;
 auto track=[&](const fs::path& path,AssetRecord& out){if(auto* a=assets.FindByRelativePath(path.lexically_relative(root).generic_string())){out=*a;return true;}return assets.Track(path.string(),out,error);};
 Check(track(root/"Assets/tile.png",image),"track image");SpriteSheet sheet;sheet.width=sheet.height=4;sheet.texture=image.id;sheet.frames={{"tile","Tile",{0,0,4,4},{.5,.5}}};
 Check(SaveSpriteSheet((root/"Assets/tile.judassprite").string(),sheet,error)&&track(root/"Assets/tile.judassprite",palette),"ordinary tracked palette");
 EditorDocument doc;doc.NewScene();TilemapEditor editor;Check(editor.Create(doc,assets,palette.id,{0,0,0},error),"create via real document transaction: "+error);auto owner=editor.recipe.root;
 auto ray=[&](float x,float y,bool down,bool pressed=false,bool cancel=false,bool right=false){return editor.Stroke(doc,assets,{x,y,10},{0,0,-1},true,pressed&&!right,pressed&&right,down&&!right,down&&right,cancel,error);};
 Check(ray(-2.5f,.5f,true,true)&&ray(4.5f,.5f,true)&&ray(4.5f,.5f,false),"viewport ray brush drag commits");
 Check(editor.recipe.cells.size()==8&&doc.IsDirty()&&doc.CanUndo(),"fast drag paints every crossed negative/positive cell in one undoable edit");
 const auto painted=doc.GetScene();doc.Undo();editor.Sync(doc);Check(editor.recipe.root==owner&&editor.recipe.cells.empty(),"one Undo restores grid and collision together");doc.Redo();editor.Sync(doc);Check(ScenesEqual(painted,doc.GetScene())&&editor.recipe.cells.size()==8,"Redo restores exact stable identities");
 std::string before;SaveSceneToString(doc.GetScene(),before);ray(0,1,true,true);ray(2,3,true);ray(2,3,false,false,true);std::string after;SaveSceneToString(doc.GetScene(),after);
 Check(before==after,"Escape cancels draft without publishing partial tiles or dirty history");
 editor.tool=2;ray(1.2f,1.2f,true,true);ray(3.2f,2.2f,true);ray(3.2f,2.2f,false);
 Check(editor.recipe.cells.size()==14,"rectangle fill uses inclusive snapped cells");
 ray(1.2f,1.2f,true,true,false,true);ray(3.2f,2.2f,true,false,false,true);ray(3.2f,2.2f,false,false,false,true);
 Check(editor.recipe.cells.size()==8,"right-drag rectangle erases real generated geometry");
 editor.tool=0;ray(0,4,true,true);doc.BeginEdit();doc.GetScene().Find(owner)->name="Rename during draft";doc.CommitEdit();ray(3,4,false);
 editor.Sync(doc);Check(editor.recipe.cells.size()==8,"intervening document edit invalidates stale paint transaction");
 glm::ivec2 cell;Check(!editor.CellAt(doc.GetScene(),{0,0,10},{1,0,0},cell,error),"parallel ray refuses to paint an invented surface");
 Check(doc.SaveAs((root/"map.judas").string(),error),"save ordinary scene");editor.Sync(doc);Check(editor.recipe.root==owner&&editor.painting,"Save As retains the current grid editing context");EditorDocument reopened;Check(reopened.Load((root/"map.judas").string(),error),"new document reopens the grid");
 reopened.Select(owner);Check(reopened.DuplicateSelection(error),"duplicate whole grid with normal hierarchy command: "+error);
 const auto duplicate=reopened.Selected();TilemapRecipe copied;
 Check(ParseTilemap(reopened.GetScene().Settings().authoringRecipes.at(TilemapKey(duplicate)),copied,error)&&copied.root==duplicate&&copied.cells.size()==8,"duplicate retains independently editable recipe and remapped children");
 Scene rebuilt;TilemapRecipe rebuiltRecipe;Check(BakeTilemap(reopened.GetScene(),copied,assets,rebuilt,rebuiltRecipe,error),"copied map passes managed ownership checks");
 Check(reopened.DeleteSelection(error)&&!reopened.GetScene().Settings().authoringRecipes.count(TilemapKey(duplicate)),"deleting a grid retires metadata without orphaned recipes");
 reopened.Undo();Check(reopened.GetScene().Settings().authoringRecipes.count(TilemapKey(duplicate))&&reopened.GetScene().Find(duplicate),"Undo deletion restores grid and authoring identity");
 std::printf("TILEMAP_EDITOR checks=%u failures=0\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
