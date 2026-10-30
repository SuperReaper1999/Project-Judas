#include "EngineHost.h"
#include "CameraLens.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "ScreenshotWriter.h"
#include "authoring/Tilemap.h"
#include "SpriteAnimation.h"
#include <filesystem>
#include <cstdio>
#include <stdexcept>
unsigned checks=0;
void Check(bool good,const std::string& label){++checks;if(!good)throw std::runtime_error(label);std::printf("PASS %s\n",label.c_str());}
int main(int argc,char** argv){try{
 namespace fs=std::filesystem;auto root=fs::absolute(argc>1?argv[1]:".cache/tilemap-render");fs::create_directories(root/"Assets");std::string error;
 std::vector<unsigned char> image(8*4*3,0);for(int y=0;y<4;++y)for(int x=0;x<8;++x)image[(y*8+x)*3+(x<4?0:1)]=255;
 Check(WriteRgbPng((root/"Assets/colors.png").string(),8,4,image),"independent atlas pixels");
 AssetDatabase assets;assets.Scan(root.string(),(root/"Assets").string());AssetRecord texture,palette;
 auto track=[&](const fs::path& path,AssetRecord& out){if(auto* a=assets.FindByRelativePath(path.lexically_relative(root).generic_string())){out=*a;return true;}return assets.Track(path.string(),out,error);};
 Check(track(root/"Assets/colors.png",texture),"track known image");SpriteSheet sheet;sheet.texture=texture.id;sheet.width=8;sheet.height=4;sheet.frames={{"red","Red",{0,0,4,4},{.5,.5}},{"green","Green",{4,0,4,4},{.5,.5}}};
 Check(SaveSpriteSheet((root/"Assets/colors.judassprite").string(),sheet,error)&&track(root/"Assets/colors.judassprite",palette),"track typed frame palette");
 Scene source;source.Settings().backgroundColor={0,0,0};source.Settings().sunEnabled=false;auto owner=source.CreateObject("Map").id;
 TilemapRecipe r;r.root=owner;r.sheet=palette.id;r.layers[0].collision=false;r.cells={{"ground","red",0,0},{"ground","green",1,0}};
 Scene scene;TilemapRecipe baked;Check(BakeTilemap(source,r,assets,scene,baked,error),"compile tested native render recipe");
 EngineHost host;Check(host.Init("Tilemap pixel proof",320,240,false,error),"real GL window");auto& renderer=host.GetRenderer();ResourceManager resources(&renderer,&assets);resources.SetBlockingMode(true);
 CameraLens lens;lens.projection=CameraProjectionKind::Orthographic;lens.height=4;auto camera=BuildCameraFrame({0,0,5},{1,0,0,0},60,.01f,100,lens,320,240);
 auto capture=[&](const Scene& authored){RuntimeWorld world;world.legacyGameplay=false;Check(world.Build(authored,&resources,error),"normal compiled world builds");RenderWorldFrame(renderer,320,240,world,nullptr,camera.view,camera.projection,{0,0,5},1);std::vector<unsigned char> pixels;renderer.CaptureFrame(320,240,pixels);return pixels;};
 auto color=[&](const std::vector<unsigned char>& pixels,int left,int top,int channel){for(int y=top;y<top+30;++y)for(int x=left;x<left+30;++x)for(int c=0;c<3;++c)if(pixels[(y*320+x)*3+c]!=(c==channel?255:0))return false;return true;};
 auto first=capture(scene);Check(color(first,175,75,0)&&color(first,235,75,1),"painted neighbouring cells sample distinct frames without atlas bleed or unintended tint");WriteRgbPng((root/"painted.png").string(),320,240,first);
 auto layered=baked;layered.layers.push_back({"front","Foreground",0,10,false});layered.cells.push_back({"front","green",0,0});Scene front;TilemapRecipe frontRecipe;Check(BakeTilemap(scene,layered,assets,front,frontRecipe,error),"decorative layer overlaps without adding physical collision");
 auto second=capture(front);Check(color(second,175,75,1),"layer sort order changes actual framebuffer result");
 auto erased=frontRecipe;Check(PaintTilemap(erased,"front",{0,0},{0,0},"",true,false,false,error),"erase foreground");Scene restored;TilemapRecipe again;Check(BakeTilemap(front,erased,assets,restored,again,error),"rebuild erased layer");auto third=capture(restored);Check(color(third,175,75,0),"erasing foreground reveals original lower layer");
 resources.Shutdown();std::printf("TILEMAP_PIXELS checks=%u failures=0\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
