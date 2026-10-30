#include "Tilemap.h"
#include "SpriteAnimation.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "TextureLoader.h"
#include "../../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <set>
#include <tuple>
#include <stdexcept>
using Json=nlohmann::ordered_json;
namespace {
bool Fail(std::string& error,const std::string& message){error="Tilemap: "+message;return false;}
bool Key(const std::string& text){return !text.empty()&&text.size()<=96&&text.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.")==std::string::npos;}
auto CellKey(const TilemapCell& c){return std::make_tuple(c.layer,c.y,c.x);}
std::string Hash(const SceneObject& o){std::string text;WriteSceneObjectBlock(o,text);return SceneFingerprintSha256(text);}
int Integer(const Json& j){if(!j.is_number_integer()||j.get<double>() < -512||j.get<double>()>512)throw std::runtime_error("cell coordinate outside -512..512");return j.get<int>();}
void Fields(const Json& j,std::initializer_list<const char*> names){
    if(!j.is_object()||j.size()!=names.size())throw std::runtime_error("unknown or missing field");
    for(auto name:names)if(!j.contains(name))throw std::runtime_error(std::string("missing ")+name);
}
int Chunk(int x){return x>=0?x/16:(x-15)/16;}
}
std::string TilemapKey(SceneObjectId id){return "tilemap."+std::to_string(id);}
bool ValidateTilemap(const TilemapRecipe& r,std::string& error){
    if(!r.root||!IsValidAssetId(r.sheet)||r.layers.empty()||r.layers.size()>8||r.cells.size()>kTilemapMaxCells||r.generated.size()>kTilemapMaxCells*2)
        return Fail(error,"root, palette, layer or cell bounds invalid");
    if(!std::isfinite(r.cellSize.x)||!std::isfinite(r.cellSize.y)||r.cellSize.x<.01f||r.cellSize.y<.01f||r.cellSize.x>100||r.cellSize.y>100)
        return Fail(error,"cell size must be finite, 0.01..100 metres");
    std::set<std::string> layers,keys;std::set<SceneObjectId> ids;
    for(const auto& layer:r.layers){
        if(!Key(layer.id)||layer.name.empty()||layer.name.size()>128||!layers.insert(layer.id).second||std::abs(int64_t(layer.order))>32767||std::abs(int64_t(layer.sortingLayer))>32767)
            return Fail(error,"invalid/duplicate layer identity, label or sort order");
    }
    std::set<std::tuple<std::string,int,int>> cells;
    std::set<std::string> solidLayers;for(const auto& l:r.layers)if(l.collision)solidLayers.insert(l.id);
    std::set<std::pair<int,int>> solidCells;
    for(const auto& c:r.cells)if(!layers.count(c.layer)||c.frame.empty()||c.frame.size()>128||std::abs(int64_t(c.x))>512||std::abs(int64_t(c.y))>512||!cells.insert(CellKey(c)).second)
        return Fail(error,"duplicate/out-of-range cell or missing layer/frame");
    for(const auto& c:r.cells)if(solidLayers.count(c.layer)&&!solidCells.insert({c.x,c.y}).second)return Fail(error,"overlapping solid layers; disable collision on decorative layers");
    for(const auto& g:r.generated)if(g.key.empty()||g.key.size()>160||!keys.insert(g.key).second||!g.entity||g.entity==r.root||!ids.insert(g.entity).second||g.hash.size()!=64||g.hash.find_first_not_of("0123456789abcdef")!=std::string::npos)
        return Fail(error,"invalid/duplicate managed child");
    error.clear();return true;
}
std::string EncodeTilemap(const TilemapRecipe& r){
    Json d={{"root",r.root},{"sheet",r.sheet},{"cellSize",{r.cellSize.x,r.cellSize.y}},{"layers",Json::array()},{"cells",Json::array()},{"generated",Json::array()}};
    for(const auto& l:r.layers)d["layers"].push_back({{"id",l.id},{"name",l.name},{"sortingLayer",l.sortingLayer},{"order",l.order},{"collision",l.collision}});
    auto cells=r.cells;std::sort(cells.begin(),cells.end(),[](const auto& a,const auto& b){return CellKey(a)<CellKey(b);});
    for(const auto& c:cells)d["cells"].push_back({{"layer",c.layer},{"frame",c.frame},{"x",c.x},{"y",c.y},{"flipX",c.flipX},{"flipY",c.flipY}});
    auto generated=r.generated;std::sort(generated.begin(),generated.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    for(const auto& g:generated)d["generated"].push_back({{"key",g.key},{"entity",g.entity},{"hash",g.hash}});
    return Json{{"kind","tilemap"},{"schema",1},{"data",d}}.dump();
}
bool ParseTilemap(const std::string& text,TilemapRecipe& out,std::string& error){try{
    if(text.size()>4*1024*1024)throw std::runtime_error("recipe exceeds 4 MiB");
    auto j=Json::parse(text,[](int depth,Json::parse_event_t,const Json&){if(depth>16)throw std::runtime_error("recipe nesting bound");return true;});
    Fields(j,{"kind","schema","data"});if(j.at("kind")!="tilemap"||j.at("schema")!=1)throw std::runtime_error("unsupported recipe identity/version");
    auto& d=j.at("data");Fields(d,{"root","sheet","cellSize","layers","cells","generated"});TilemapRecipe r;
    if(!d.at("root").is_number_unsigned())throw std::runtime_error("unsigned root identity required");
    r.root=d.at("root").get<SceneObjectId>();r.sheet=d.at("sheet").get<std::string>();
    if(!d.at("cellSize").is_array()||d.at("cellSize").size()!=2)throw std::runtime_error("cellSize needs two numbers");
    r.cellSize={d.at("cellSize")[0].get<float>(),d.at("cellSize")[1].get<float>()};
    if(!d.at("layers").is_array()||d.at("layers").size()>8||!d.at("cells").is_array()||d.at("cells").size()>kTilemapMaxCells||!d.at("generated").is_array()||d.at("generated").size()>kTilemapMaxCells*2)throw std::runtime_error("bounded layer/cell/child arrays required");
    r.layers.clear();for(const auto& l:d.at("layers")){Fields(l,{"id","name","sortingLayer","order","collision"});
        if(!l.at("order").is_number_integer()||!l.at("sortingLayer").is_number_integer()||std::abs(l.at("order").get<double>())>32767||std::abs(l.at("sortingLayer").get<double>())>32767)throw std::runtime_error("integer sort values required");
        r.layers.push_back({l.at("id").get<std::string>(),l.at("name").get<std::string>(),l.at("sortingLayer").get<int>(),l.at("order").get<int>(),l.at("collision").get<bool>()});}
    for(const auto& c:d.at("cells")){Fields(c,{"layer","frame","x","y","flipX","flipY"});r.cells.push_back({c.at("layer").get<std::string>(),c.at("frame").get<std::string>(),Integer(c.at("x")),Integer(c.at("y")),c.at("flipX").get<bool>(),c.at("flipY").get<bool>()});}
    for(const auto& g:d.at("generated")){Fields(g,{"key","entity","hash"});if(!g.at("entity").is_number_unsigned())throw std::runtime_error("unsigned child identity required");r.generated.push_back({g.at("key").get<std::string>(),g.at("hash").get<std::string>(),g.at("entity").get<SceneObjectId>()});}
    if(!ValidateTilemap(r,error))return false;
    out=std::move(r);error.clear();return true;
}catch(const std::exception& e){return Fail(error,e.what());}}
bool PaintTilemap(TilemapRecipe& r,const std::string& layer,glm::ivec2 from,glm::ivec2 to,const std::string& frame,bool erase,bool flipX,bool flipY,std::string& error){
    if(!ValidateTilemap(r,error))return false;
    if(std::none_of(r.layers.begin(),r.layers.end(),[&](const auto& l){return l.id==layer;}))return Fail(error,"select a valid layer");
    auto low=glm::min(from,to),high=glm::max(from,to);
    if(low.x < -512||low.y < -512||high.x>512||high.y>512||int64_t(high.x-low.x+1)*(high.y-low.y+1)>int64_t(kTilemapMaxCells)||(!erase&&frame.empty()))return Fail(error,"paint rectangle exceeds bounded grid or frame is missing");
    auto next=r;next.cells.erase(std::remove_if(next.cells.begin(),next.cells.end(),[&](const auto& c){return c.layer==layer&&c.x>=low.x&&c.x<=high.x&&c.y>=low.y&&c.y<=high.y;}),next.cells.end());
    if(!erase)for(int y=low.y;y<=high.y;++y)for(int x=low.x;x<=high.x;++x)next.cells.push_back({layer,frame,x,y,flipX,flipY});
    if(!ValidateTilemap(next,error))return false;
    r=std::move(next);return true;
}
bool BakeTilemap(const Scene& source,const TilemapRecipe& r,const AssetDatabase& assets,Scene& output,TilemapRecipe& baked,std::string& error){try{
    if(!ValidateTilemap(r,error))return false;
    if(!ValidateHierarchy(source,error))return false;
    const auto* root=source.Find(r.root);if(!root)return Fail(error,"root no longer exists");
    if(root->prefabRoot||!root->prefabAsset.empty())return Fail(error,"paint the source scene; prefab instances contain baked ordinary objects");
    for(auto id=r.root;id;){auto* o=source.Find(id);if(!o)return Fail(error,"missing ancestor");if(o->body||o->characterMotor||o->ragdoll||o->deformable||o->wheel||o->socket)return Fail(error,"tilemap frame/ancestors must be static transform groups");id=o->parent;}
    const auto* asset=assets.Find(r.sheet);if(!asset||asset->missing||asset->type!=AssetType::SpriteSheet)return Fail(error,"tracked sprite-sheet palette required");
    SpriteSheet sheet;if(!LoadSpriteSheet(asset->path,sheet,error))return false;
    const auto* texture=assets.Find(sheet.texture);if(!texture||texture->missing||texture->type!=AssetType::Texture)return Fail(error,"palette image is missing");
    TextureData image;if(!LoadTextureFromFile(texture->path,image,error))return false;
    if(image.width!=sheet.width||image.height!=sheet.height)return Fail(error,"palette image dimensions changed; explicitly re-slice the sheet");
    Scene flat;if(!FlattenHierarchy(source,flat,error))return false;
    if(std::any_of(r.layers.begin(),r.layers.end(),[](const auto& l){return l.collision;})&&flat.Find(r.root)->transform.scale!=glm::vec3(1))return Fail(error,"collision grids require unit world scale; change Cell size instead");
    std::map<std::string,TilemapGenerated> old;
    for(const auto& g:r.generated){const auto* o=source.Find(g.entity);if(!o||o->parent!=r.root||Hash(*o)!=g.hash)return Fail(error,"managed tile changed/removed outside painter; undo that edit or Detach grid to retain ordinary objects");
        for(const auto& child:source.Objects())if(child.parent==g.entity)return Fail(error,"managed tile has user children; Detach grid before editing them");
        old.emplace(g.key,g);}
    std::map<std::string,const SpriteFrame*> frames;for(const auto& f:sheet.frames)frames.emplace(f.id,&f);
    auto scene=source;auto recipe=r;recipe.generated.clear();
    auto publish=[&](const std::string& key,SceneObject o){
        auto found=old.find(key);if(found!=old.end()){o.id=found->second.entity;old.erase(found);}else o.id=scene.CreateObject(o.name).id;
        o.parent=r.root;o.authoringFolder=root->name;*scene.Find(o.id)=o;recipe.generated.push_back({key,Hash(o),o.id});
    };
    for(const auto& layer:r.layers){
        std::map<std::pair<int,int>,std::array<bool,256>> chunks;
        for(const auto& cell:r.cells)if(cell.layer==layer.id){
            auto found=frames.find(cell.frame);if(found==frames.end())return Fail(error,"palette frame removed: "+cell.frame);
            const auto& f=*found->second;SceneObject o;o.name="Tile "+layer.name+" ("+std::to_string(cell.x)+","+std::to_string(cell.y)+")";
            o.transform.position={r.cellSize*glm::vec2(cell.x+.5f,cell.y+.5f),0};o.render=SceneRenderComponent{};auto& render=*o.render;
            render.shape=SceneShape::Sprite;render.color={1,1,1};render.textureAsset=sheet.texture;render.sprite.size=r.cellSize;render.sprite.pivot={.5f,.5f};render.sprite.rect=glm::vec4(f.pixels)/glm::vec4(sheet.width,sheet.height,sheet.width,sheet.height);render.sprite.flipX=cell.flipX;render.sprite.flipY=cell.flipY;render.sprite.sortingLayer=layer.sortingLayer;render.sprite.order=layer.order;o.renderLayer=root->renderLayer;o.tags=root->tags;
            publish("tile/"+layer.id+"/"+std::to_string(cell.x)+"/"+std::to_string(cell.y),o);
            if(layer.collision){auto cx=Chunk(cell.x),cy=Chunk(cell.y);chunks[{cx,cy}][(cell.y-cy*16)*16+cell.x-cx*16]=true;}
        }
        for(auto& [chunk,grid]:chunks)for(int y=0;y<16;++y)for(int x=0;x<16;++x)if(grid[y*16+x]){
            int width=1,height=1;while(x+width<16&&grid[y*16+x+width])++width;
            for(;y+height<16;++height){bool full=true;for(int dx=0;dx<width;++dx)full&=grid[(y+height)*16+x+dx];if(!full)break;}
            for(int dy=0;dy<height;++dy)for(int dx=0;dx<width;++dx)grid[(y+dy)*16+x+dx]=false;
            const int gx=chunk.first*16+x,gy=chunk.second*16+y;SceneObject o;o.name="Tile collision "+layer.name;
            o.transform.position={r.cellSize*glm::vec2(gx+width*.5f,gy+height*.5f),0};o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;o.body->planar=PlanarBox(glm::dvec2(r.cellSize)*glm::dvec2(width,height)*.5);o.body->motion=SceneBodyMotion::Static;o.body->friction=.7f;o.tags=root->tags;
            publish("collision/"+layer.id+"/"+std::to_string(gx)+"/"+std::to_string(gy)+"/"+std::to_string(width)+"/"+std::to_string(height),o);
        }
    }
    for(const auto& [key,g]:old){for(const auto& o:scene.Objects())if(o.joint&&(o.joint->bodyA==g.entity||o.joint->bodyB==g.entity))return Fail(error,"removed tile is a joint anchor; detach the grid before changing this assembly");scene.DestroyObject(g.entity);}
    const auto key=TilemapKey(r.root);
    if(auto it=source.Settings().authoringRecipes.find(key);it!=source.Settings().authoringRecipes.end()){TilemapRecipe prior;if(!ParseTilemap(it->second,prior,error)||prior.root!=r.root)return Fail(error,"recipe key belongs to another authoring operation");}
    if(!scene.Settings().authoringRecipes.count(key)&&scene.Settings().authoringRecipes.size()>=128)return Fail(error,"scene recipe limit reached");
    scene.Settings().authoringRecipes[key]=EncodeTilemap(recipe);if(!ValidateHierarchy(scene,error))return false;
    std::string text;SaveSceneToString(scene,text);Scene check;if(!LoadSceneFromString(text,check,error))return false;
    output=std::move(scene);baked=std::move(recipe);error.clear();return true;
}catch(const std::exception& e){return Fail(error,e.what());}}
bool DuplicateTilemapRecipes(const Scene& source,Scene& result,const std::map<SceneObjectId,SceneObjectId>& ids,std::string& error){
    for(const auto& [key,text]:source.Settings().authoringRecipes){
        TilemapRecipe r;std::string ignored;if(!ParseTilemap(text,r,ignored)||!ids.count(r.root))continue;
        r.root=ids.at(r.root);
        for(auto& g:r.generated){const auto* old=source.Find(g.entity);
            if(!old||Hash(*old)!=g.hash||!ids.count(g.entity))return Fail(error,"detach the grid or undo manual child edits before duplicating it");
            g.entity=ids.at(g.entity);const auto* copied=result.Find(g.entity);if(!copied)return Fail(error,"incomplete copied tile hierarchy");g.hash=Hash(*copied);
        }
        if(result.Settings().authoringRecipes.size()>=128)return Fail(error,"scene recipe limit reached");
        result.Settings().authoringRecipes[TilemapKey(r.root)]=EncodeTilemap(r);
    }
    error.clear();return true;
}
void PruneTilemapRecipes(Scene& scene){
    for(auto it=scene.Settings().authoringRecipes.begin();it!=scene.Settings().authoringRecipes.end();){
        TilemapRecipe r;std::string error;
        if(ParseTilemap(it->second,r,error)&&!scene.Find(r.root))it=scene.Settings().authoringRecipes.erase(it);else ++it;
    }
}
