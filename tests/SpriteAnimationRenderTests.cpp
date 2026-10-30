#include "EngineHost.h"
#include "CameraLens.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPresentation.h"
#include "ScreenshotWriter.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
unsigned checks=0;
void Check(bool ok,const std::string& label){
    ++checks;
    if(!ok)throw std::runtime_error(label);
    std::cout<<"PASS "<<label<<'\n';
}
int main(int argc,char** argv){
    try{
        if(argc!=2)return 2;
        const auto root=std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(root/"Assets");
        std::string error;
        std::vector<unsigned char> image(8*4*3,0);
        for(int y=0;y<4;++y)for(int x=0;x<8;++x)image[(y*8+x)*3+(x<4?0:1)]=255;
        Check(WriteRgbPng((root/"Assets/atlas.png").string(),8,4,image),"create independently coloured atlas fixture");
        AssetDatabase assets;
        assets.Scan(root.string(),(root/"Assets").string());
        AssetRecord texture;
        Check(assets.Track((root/"Assets/atlas.png").string(),texture,error),"track actual PNG");
        SpriteSheet sheet;
        sheet.texture=texture.id;
        sheet.width=8;
        sheet.height=4;
        sheet.pixelsPerUnit=2;
        Check(SliceSpriteSheet(sheet,{
            4,4
        },{
            0,0
        },{
            0,0
        },error),"slice two adjacent cells without gutters");
        sheet.clips.push_back({
            "colors",{
                {
                    "cell-0-0",.25
                },{
                    "cell-0-1",.25
                }
            }, {
            }
        });
        Check(SaveSpriteSheet((root/"Assets/atlas.judassprite").string(),sheet,error),"write typed sheet");
        AssetRecord record;
        Check(assets.Track((root/"Assets/atlas.judassprite").string(),record,error),"track sheet resource");
        EngineHost host;
        Check(host.Init("Sprite animation raster proof",320,240,false,error),"start actual GL runtime");
        auto& renderer=host.GetRenderer();
        ResourceManager resources(&renderer,&assets);
        resources.SetBlockingMode(true);
        Scene scene;
        scene.Settings().backgroundColor={
            0,0,0
        };
        scene.Settings().sunEnabled=false;
        auto& o=scene.CreateObject("Atlas actor");
        o.render=SceneRenderComponent{
        };
        o.render->shape=SceneShape::Sprite;
        o.render->color={
            1,1,1
        };
        o.render->spritePlayback=SpritePlayback{
        };
        o.render->spritePlayback->sheet=record.id;
        o.render->spritePlayback->clip="colors";
        RuntimeWorld world;
        world.legacyGameplay=false;
        Check(world.Build(scene,&resources,error),"normal scene requests sheet and image");
        CameraLens lens;
        lens.projection=CameraProjectionKind::Orthographic;
        lens.height=4;
        const auto frame=BuildCameraFrame({
            0,0,5
        },glm::quat(1,0,0,0),60,.01f,100,lens,320,240);
        auto capture=[&](){
            RenderWorldFrame(renderer,320,240,world,nullptr,frame.view,frame.projection,{
                0,0,0
            },1);
            std::vector<unsigned char> pixels;
            renderer.CaptureFrame(320,240,pixels);
            return pixels;
        };
        auto color=[&](const auto& pixels,int channel){
            for(int y=61;y<179;++y)for(int x=100;x<220;++x){
                const auto n=(y*320+x)*3;
                for(int c=0;c<3;++c)if(pixels[n+c]!=(c==channel?255:0))return false;
            }
            return true;
        };
        auto red=capture();
        Check(color(red,0),"first frame is red over its entire physical visual extent, not whole-sheet red/green");
        WriteRgbPng((root/"red.png").string(),320,240,red);
        world.UpdateSpriteAnimations(.25);
        auto green=capture();
        Check(color(green,1),"exact frame boundary changes actual runtime pixels to green");
        WriteRgbPng((root/"green.png").string(),320,240,green);
        auto style=world.RuntimeDefinition(1)->render->sprite;
        style.nearest=false;
        world.SetSprite(1,style);
        auto p=*world.RuntimeDefinition(1)->render->spritePlayback;
        p.time=0;
        p.playing=false;
        Check(world.SetSpritePlayback(1,p,error),"seek/pause smooth sprite without gameplay events");
        auto smooth=capture();
        Check(color(smooth,0),"linear sampling cannot bleed adjacent green cell at either sprite edge");
        WriteRgbPng((root/"smooth.png").string(),320,240,smooth);
        const auto phase=world.RuntimeDefinition(1)->render->spritePlayback->time;
        capture();
        capture();
        Check(world.RuntimeDefinition(1)->render->spritePlayback->time==phase,"multiple render views never advance animation or emit events");
        resources.Shutdown();
        std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";
        return 0;
    }
    catch(const std::exception& e){
        std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}
