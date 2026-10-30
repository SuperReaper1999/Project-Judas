#include "EngineHost.h"
#include "CameraLens.h"
#include "RuntimeWorld.h"
#include "LocalMultiplayer.h"
#include "InputSystem.h"
#include "WorldPresentation.h"
#include <chrono>
#include <algorithm>
#include <numeric>
#include "ScreenshotWriter.h"
#include <iostream>
#include <stdexcept>
#include <filesystem>
unsigned checks=0;
void Check(bool pass,const char* name){++checks;if(!pass)throw std::runtime_error(name);std::cout<<"PASS "<<name<<'\n';}
int main(int argc,char** argv){try{
    std::string error;EngineHost host;
    Check(host.Init("Native sprite pixel proof",320,240,false,error),"GL engine host starts");
    auto& r=host.GetRenderer();glClearColor(0,0,0,1);CameraLens lens;lens.projection=CameraProjectionKind::Orthographic;lens.height=4;
    const auto frame=BuildCameraFrame({0,0,5},glm::quat(1,0,0,0),60,.01f,100,lens,320,240);
    auto draw=[&](int order,bool flip,bool alpha){
        r.BeginFrame(320,240);r.SetSceneAppearance(false,1,{},0,glm::quat(1,0,0,0),false,{0,0,0});r.SetCameraFrame(frame);
        SpriteSettings back;back.size={2,2};back.sortingLayer=1;
        SpriteSettings front=back;front.order=order;front.flipX=flip;
        r.QueueSprite(back,{0,0,0},{1,0,0,0},{1,1,1},{},{1,0,0,1},10);
        r.QueueSprite(front,{.5f,0,0},{1,0,0,0},{1,1,1},{},{0,1,0,alpha?.5f:1.f},11);
        r.FlushSprites();std::vector<unsigned char> pixels;r.CaptureFrame(320,240,pixels);Check(pixels.size()==320u*240u*3u,"capture real GL framebuffer");return pixels;
    };
    auto pixel=[](const auto& p,int x,int y){const auto i=std::size_t(y*320+x)*3;return glm::ivec3(p[i],p[i+1],p[i+2]);};
    auto a=draw(1,false,false);auto center=pixel(a,160,120);
    Check(center.g>240&&center.r<5,"sorted front sprite actually covers red sprite");
    auto b=draw(-1,false,false);center=pixel(b,160,120);
    Check(center.r>240&&center.g<5,"negative sorting control reverses actual visible output");
    auto blended=draw(1,false,true);center=pixel(blended,160,120);
    Check(center.r>110&&center.r<145&&center.g>110&&center.g<145,"sprite opacity blends actual pixels");
    Check(pixel(a,10,10)==glm::ivec3(0),"outside sprite remains clear");
    auto ySort=[&](float y){r.BeginFrame(320,240);r.SetCameraFrame(frame);SpriteSettings sprite;sprite.size={2,2};sprite.ySort=true;
        r.QueueSprite(sprite,{0,0,0},{1,0,0,0},{1,1,1},{},{1,0,0,1},1);
        r.QueueSprite(sprite,{0,y,0},{1,0,0,0},{1,1,1},{},{0,1,0,1},2);r.FlushSprites();
        std::vector<unsigned char> p;r.CaptureFrame(320,240,p);return p;};
    Check(pixel(ySort(.1f),160,120)==glm::ivec3(255,0,0)&&pixel(ySort(-.1f),160,120)==glm::ivec3(0,255,0),"top-down Y sort changes actual overlap consistently when actors cross");
    TextureData image;image.width=2;image.height=2;
    image.pixels={0,0,255,255, 255,255,255,0, 255,0,0,255, 0,255,0,255};
    const auto texture=r.CreateTexture(image);
    auto textured=[&](SpriteSettings sprite){r.BeginFrame(320,240);r.SetCameraFrame(frame);
        r.QueueSprite(sprite,{0,0,0},{1,0,0,0},{1,1,1},texture,{1,1,1,1},1);r.FlushSprites();
        std::vector<unsigned char> p;r.CaptureFrame(320,240,p);return p;};
    SpriteSettings atlas;atlas.size={2,2};const auto imagePixels=textured(atlas);
    Check(pixel(imagePixels,130,90)==glm::ivec3(255,0,0)&&pixel(imagePixels,190,90)==glm::ivec3(0,255,0)&&pixel(imagePixels,130,150)==glm::ivec3(0,0,255),"loose RGBA texture preserves top-left image orientation");
    Check(pixel(imagePixels,190,150)==glm::ivec3(0),"transparent texture texel reveals clear background");
    atlas.flipX=true;const auto mirrored=textured(atlas);
    Check(pixel(mirrored,130,90)==glm::ivec3(0,255,0)&&pixel(mirrored,190,90)==glm::ivec3(255,0,0),"flip mirrors real image around pivot");
    atlas.flipX=false;atlas.rect={0,0,.5f,.5f};const auto cropped=textured(atlas);
    Check(pixel(cropped,130,90)==glm::ivec3(255,0,0)&&pixel(cropped,190,150)==glm::ivec3(255,0,0),"normalized top-left source rect selects atlas cell without UV inversion");
    atlas.rect={0,0,1,1};atlas.pivot={0,0};const auto pivoted=textured(atlas);
    Check(pixel(pivoted,130,90)==glm::ivec3(0)&&pixel(pivoted,190,150)==glm::ivec3(255,0,0),"pivot changes physical visual extent without moving entity origin");
    Scene scene;scene.Settings().backgroundColor={0,0,0};scene.Settings().sunEnabled=false;
    auto& native=scene.CreateObject("native world sprite");native.render=SceneRenderComponent{};native.render->shape=SceneShape::Sprite;
    native.render->sprite.size={2,2};native.render->color={0,1,0};
    auto& secondary=scene.CreateObject("independent orthographic target");secondary.transform.position={0,0,5};
    secondary.renderCamera=SceneRenderCameraComponent{};secondary.renderCamera->width=secondary.renderCamera->height=80;
    secondary.renderCamera->lens=lens;
    RuntimeWorld world;world.legacyGameplay=false;
    Check(world.Build(scene,nullptr,error),"native sprite world builds without hidden resources or physics");
    SceneTransform camera;camera.position={0,0,5};Check(world.SetRuntimeView(camera,60,.01f,100,lens),"world accepts orthographic lens");
    RenderWorldFrame(r,320,240,world,nullptr,frame.view,frame.projection,{0,0,0},1);
    std::vector<unsigned char> worldPixels;r.CaptureFrame(320,240,worldPixels);
    Check(pixel(worldPixels,160,120)==glm::ivec3(0,255,0),"ordinary runtime WorldPresentation draws actual native sprite");
    const auto& target=world.PresentationCameras().front().target;
    r.BeginUIFrame(320,240);r.DrawUIImage({0,0},{80,80},r.RenderTargetTexture(target),{1,1,1,1},false,true);r.EndUIFrame();
    std::vector<unsigned char> secondaryPixels;r.CaptureFrame(320,240,secondaryPixels);
    Check(pixel(secondaryPixels,40,40)==glm::ivec3(0,255,0),"independent orthographic secondary camera renders native sprite to real target");
    auto& players=world.Players();const auto left=players.Create("left",InputMap::Defaults(),error)->id;
    const auto right=players.Create("right",InputMap::Defaults(),error)->id;
    players.SetViewport(left,{0,0,.5f,1},error);players.SetViewport(right,{.5f,0,.5f,1},error);
    players.SetView(left,camera,60,.01f,100,error,lens);camera.position.x=10;
    players.SetView(right,camera,60,.01f,100,error,lens);
    RenderWorldFrame(r,320,240,world,nullptr,frame.view,frame.projection,{0,0,0},1);
    std::vector<unsigned char> split;r.CaptureFrame(320,240,split);
    Check(pixel(split,80,120)==glm::ivec3(0,255,0)&&pixel(split,240,120)==glm::ivec3(0),"two independently positioned orthographic player views show distinct actual world contents");
    std::vector<double> times;
    for(int iteration=0;iteration<70;++iteration){const auto start=std::chrono::steady_clock::now();
        r.BeginFrame(320,240);r.SetCameraFrame(frame);
        for(int i=0;i<200;++i){SpriteSettings sprite;sprite.size={.2f,.2f};r.QueueSprite(sprite,{float(i%20)*.2f-2,float(i/20)*.2f-1,0},{1,0,0,0},{1,1,1},{},{1,1,1,1},i+1);}
        r.FlushSprites();glFinish();
        if(iteration>=10)times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    }
    std::sort(times.begin(),times.end());std::cout<<"PERF sprites=200 frames="<<times.size()<<" mean_ms="<<std::accumulate(times.begin(),times.end(),0.0)/times.size()<<" median_ms="<<times[times.size()/2]<<" p95_ms="<<times[56]<<" worst_ms="<<times.back()<<" (submission+GPU completion, not separate CPU/GPU)\n";
    lens.pixelPerfect=true;lens.referenceWidth=80;lens.referenceHeight=40;lens.pixelsPerUnit=20;
    const auto letterbox=BuildCameraFrame({0,0,5},{1,0,0,0},60,.01f,100,lens,320,240);
    auto stable=[&](float dx){r.BeginFrame(320,240);r.SetCameraFrame(letterbox);SpriteSettings sprite;sprite.size={1,1};
        r.QueueSprite(sprite,{dx,0,0},{1,0,0,0},{1,1,1},{},{1,1,1,1},1);r.FlushSprites();std::vector<unsigned char> p;r.CaptureFrame(320,240,p);return p;};
    const auto initial=stable(0);const auto subpixel=stable(.001f);
    Check(initial==subpixel,"subpixel sprite motion leaves pixel-perfect frame byte identical");
    Check(pixel(initial,160,10)==glm::ivec3(0)&&pixel(initial,160,120)==glm::ivec3(255),"integer content viewport has empty letterbox and visible center");
    r.BeginUIFrame(320,240);r.DrawUIRect({0,0},{320,20},{1,0,0,1});r.EndUIFrame();
    std::vector<unsigned char> hud;r.CaptureFrame(320,240,hud);
    Check(pixel(hud,160,10)==glm::ivec3(255,0,0),"full-output UI is not squeezed into the world's letterbox viewport");
    if(argc>1){std::filesystem::create_directories(argv[1]);Check(WriteRgbPng((std::filesystem::path(argv[1])/"sorting.png").string(),320,240,a),"save rendered evidence");}
    std::cout<<"SUMMARY checks="<<checks<<" failures=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
