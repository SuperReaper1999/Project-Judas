#include "CameraLens.h"
#include "Sprite.h"
#include <cstdio>
#include <limits>
#include <stdexcept>
namespace {
int checks=0;
void Check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
}
int main(){
 try {
    SpriteSettings sprite;sprite.size={2,4};sprite.pivot={.25f,.75f};sprite.rect={.5f,0,.5f,.5f};
    const auto text=EncodeSprite(sprite);SpriteSettings restored;std::string error;
    Check(DecodeSprite(text,restored,error)&&sprite==restored,"sprite roundtrip");
    const auto uv=SpriteUV(sprite,SpriteCentre(sprite));
    Check(glm::length(uv-glm::vec2(.75f,.75f))<1e-6f,"source top-left image rect converted to bottom-origin GPU UV");
    sprite.flipX=true;Check(SpriteCentre(sprite).x==-.5f,"non-centred horizontal flip moves geometry about pivot");
    auto before=restored;Check(!DecodeSprite("{}",restored,error)&&restored==before,"failed decode preserves caller state");
    for(int which=0;which<5;++which){auto bad=sprite;if(which==0)bad.size.x=0;if(which==1)bad.pivot.y=2;if(which==2)bad.rect.x=.9f;if(which==3)bad.tint.w=-1;if(which==4)bad.size.y=std::numeric_limits<float>::infinity();Check(!ValidSprite(bad),"invalid sprite state rejects");}
    CameraLens lens;lens.projection=CameraProjectionKind::Orthographic;lens.height=10;
    auto a=BuildCameraFrame({0,0,10},{1,0,0,0},60,.1f,100,lens,800,400);
    glm::vec3 o1,d1,o2,d2;
    Check(a.Ray({.25f,.5f},o1,d1)&&a.Ray({.75f,.5f},o2,d2)&&glm::length(d1-d2)<1e-6f&&glm::length(o2-o1)>5,"orthographic parallel rays with distinct origins");
    Check(a.Project({0,0,20}).behind&&!a.Project({0,0,20}).inside,"orthographic behind uses signed eye depth");
    for(auto pixel:{glm::vec2(.2f,.8f),glm::vec2(.8f,.2f)}){Check(a.Ray(pixel,o1,d1),"viewport ray");Check(glm::length(a.Project(o1+d1*5.f).position-pixel)<1e-5f,"projection/ray roundtrip");}
    lens.pixelPerfect=true;auto p=BuildCameraFrame({0,0,10},{1,0,0,0},60,.1f,100,lens,801,601);
    Check(p.content==glm::ivec4(80,120,640,360),"integer canvas fits and centres at odd window size");
    Check(!p.Ray({.01f,.01f},o1,d1),"letterbox cannot generate gameplay rays");
    auto shifted=BuildCameraFrame({.001f,.002f,10},{1,0,0,0},60,.1f,100,lens,801,601);
    Check(p.view==shifted.view,"subpixel camera motion stays on same presentation grid");
    lens=CameraLens{};auto legacy=BuildCameraFrame({0,0,10},{1,0,0,0},60,.1f,100,lens,800,400);
    auto expected=glm::perspective(glm::radians(60.f),2.f,.1f,100.f);
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)if(legacy.projection[c][r]!=expected[c][r])printf("DIFF %d %d actual=%.12g expected=%.12g delta=%.12g\n",c,r,legacy.projection[c][r],expected[c][r],legacy.projection[c][r]-expected[c][r]);
    Check(legacy.projection==expected,"legacy perspective matrix identical");
    lens.pixelPerfect=true;Check(!ValidCameraLens(lens),"pixel-perfect perspective rejects");
    std::printf("Sprite/camera math: %d checks passed\n",checks);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL after %d: %s\n",checks,e.what());return 1;}
}
