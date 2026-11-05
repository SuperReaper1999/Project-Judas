#include "RigNormalization.h"
#include "ModelArchive.h"
#include "ModelImport.h"
#include "SkeletalAnimation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cmath>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool okay, const char *name) {
    ++checks; if (!okay) ++failures;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", name);
}
MeshData Fixture(TrackInterpolation interpolation) {
    MeshData mesh;
    mesh.vertices.resize(3);
    mesh.vertices[1].position = {.01f,0,0};
    mesh.vertices[2].position = {0,.02f,0};
    mesh.indices = {0,1,2};
    mesh.skinVertices.resize(3);
    for (auto &vertex : mesh.skinVertices) vertex.joints = glm::uvec4(1,0,0,0);
    auto asset = std::make_shared<SkeletalAsset>();
    auto &rig = asset->skeleton;
    rig.names = {"Root","Hand"}; rig.parents = {-1,0}; rig.order = {0,1};
    rig.rest.local.resize(2);
    rig.rest.local[0].scale = glm::vec3(100);
    rig.rest.local[1].translation = {0,.01f,0};
    rig.skinNodes = {0,1};
    rig.inverseBind = {glm::mat4(1),glm::translate(glm::mat4(1),glm::vec3(0,-.01f,0))};
    AnimationClip clip; clip.name = "Reach"; clip.duration = 1;
    clip.events.push_back({"hit","Hit","payload",.5f});
    clip.motionTimes = {0,1}; clip.motion.resize(2);clip.motion[1].translation={2,0,0};
    AnimationTrack translation; translation.node = 1; translation.path = TrackPath::Translation;
    translation.interpolation = interpolation; translation.times = {0,1};
    if (interpolation == TrackInterpolation::CubicSpline)
        translation.values = {{0,.01f,0,0},{0,.01f,0,0},{0,.01f,0,0},
                              {0,.02f,0,0},{0,.04f,0,0},{0,.02f,0,0}};
    else translation.values = {{0,.01f,0,0},{0,.04f,0,0}};
    clip.tracks.push_back(translation);
    AnimationTrack rotation; rotation.node=0; rotation.path=TrackPath::Rotation;
    rotation.times={0,1}; rotation.values={{0,0,0,1},{0,0,.70710678f,.70710678f}};
    clip.tracks.push_back(rotation);
    AnimationTrack scale; scale.node=0; scale.path=TrackPath::Scale;
    scale.times={0,1}; scale.values={glm::vec4(100,100,100,0),glm::vec4(120,120,120,0)};
    clip.tracks.push_back(scale);
    asset->clips.push_back(clip);mesh.skeletal=asset;
    return mesh;
}
bool SameGeometry(const MeshData &a,const MeshData &b) {
    for (float t : {0.f,.13f,.25f,.5f,.83f,1.f}) {
        const auto pa=ResolveSkinMatrices(a.skeletal->skeleton,SampleClip(a.skeletal->skeleton,a.skeletal->clips[0],t));
        const auto pb=ResolveSkinMatrices(b.skeletal->skeleton,SampleClip(b.skeletal->skeleton,b.skeletal->clips[0],t));
        for(size_t i=0;i<a.vertices.size();++i) {
            const auto joint=a.skinVertices[i].joints.x;
            const auto va=pa[joint]*glm::vec4(a.vertices[i].position,1);
            const auto vb=pb[joint]*glm::vec4(b.vertices[i].position,1);
            if(glm::length(va-vb)>1e-5f)return false;
        }
    }
    return true;
}
}
int main(int argc,char **argv) {
    std::string error;
    for (auto interpolation : {TrackInterpolation::Step,TrackInterpolation::Linear,TrackInterpolation::CubicSpline}) {
        auto source=Fixture(interpolation);MeshData output;RigNormalizationReport report;
        Check(NormalizeModelRig(source,output,report,error),"normalise animated centimetre rig");
        if (!output.skeletal) continue;
        Check(source.skeletal->skeleton.rest.local[0].scale.x==100,"source remains unchanged");
        Check(std::abs(output.skeletal->skeleton.rest.local[0].scale.x-1)<1e-5f,"root rest scale becomes one");
        Check(std::abs(output.skeletal->skeleton.rest.local[1].translation.y-1)<1e-5f,"bone translation becomes metres");
        Check(std::abs(output.vertices[2].position.y-2)<1e-5f,"vertex coordinates become metres");
        Check(SameGeometry(source,output),"independent animated world-vertex comparison");
        Check(output.skeletal->clips[0].events[0].data=="payload" &&
              output.skeletal->clips[0].motion[1].translation.x==2,"events and world root motion unchanged");
        MeshData second;RigNormalizationReport again;
        Check(NormalizeModelRig(output,second,again,error)&&SameGeometry(output,second)&&
              std::abs(again.meshScale-1)<1e-5,"repeating normalisation preserves geometry and units");
        std::vector<uint8_t> bytes;MeshData decoded;
        Check(EncodeModelArchive(output,bytes,error)&&DecodeModelArchive(bytes.data(),bytes.size(),decoded,error)&&
              SameGeometry(source,decoded),"archive roundtrip retains converted animation geometry");
    }
    for (int kind=0;kind<3;++kind) {
        auto source=Fixture(TrackInterpolation::Linear);
        auto rig=std::make_shared<SkeletalAsset>(*source.skeletal);
        if(kind==0)rig->skeleton.rest.local[0].scale.x=50;
        if(kind==1)rig->skeleton.rest.local[0].scale.x=-100;
        if(kind==2)rig->skeleton.rest.local[0].scale.x=0;
        source.skeletal=rig;MeshData sentinel;sentinel.vertices.resize(7);RigNormalizationReport report;
        Check(!NormalizeModelRig(source,sentinel,report,error)&&sentinel.vertices.size()==7&&!error.empty(),
              "unsupported scale rejects transactionally without changing output");
    }
    for (float units : {.01f,1.f,100.f}) {
        auto source=Fixture(TrackInterpolation::Linear);
        auto asset=std::make_shared<SkeletalAsset>(*source.skeletal);
        // Fixed affine unit conversion, as used by FBX/glTF helper nodes.
        asset->skeleton.rest.local[0].scale=glm::vec3(1);
        // The animated local scale must use that same unit rest frame;
        // the fixed affine root now owns the centimetre/metre conversion.
        for(auto &track:asset->clips[0].tracks)
            if(track.path==TrackPath::Scale)
                for(auto &value:track.values)value/=100.f;
        asset->skeleton.affine={glm::scale(glm::mat4(1),glm::vec3(units)),glm::mat4(1)};
        source.skeletal=asset;MeshData output;RigNormalizationReport report;
        Check(NormalizeModelRig(source,output,report,error)&&SameGeometry(source,output),
              "affine conversion roots preserve animation at centimetre, metre and 100x scales");
    }
    if(argc==2) {
        MeshData source,output;ModelImportSettings settings;ModelImportReport imported;RigNormalizationReport report;
        Check(ImportModelSource(argv[1],settings,source,imported,error)&&
              NormalizeModelRig(source,output,report,error),"real character all-clip equivalence gate");
        if(!error.empty())std::fprintf(stderr,"%s\n",error.c_str());
        std::printf("real character: %zu clips, %u poses, maximum matrix error %.9g\n",
                    source.skeletal?source.skeletal->clips.size():0,report.posesChecked,report.maximumMatrixError);
        if(output.skeletal) {
            double maximum=0;
            for(size_t c=0;c<source.skeletal->clips.size();++c)
                for(float fraction : {0.f,.13f,.5f,.83f,1.f}) {
                    const auto &a=source.skeletal->clips[c];const auto &b=output.skeletal->clips[c];
                    const auto pa=ResolveSkinMatrices(source.skeletal->skeleton,SampleClip(source.skeletal->skeleton,a,a.duration*fraction));
                    const auto pb=ResolveSkinMatrices(output.skeletal->skeleton,SampleClip(output.skeletal->skeleton,b,b.duration*fraction));
                    for(size_t i=0;i<source.vertices.size();++i) {
                        glm::vec4 va(0),vb(0);const auto &skin=source.skinVertices[i];
                        for(int k=0;k<4;++k) {
                            va+=skin.weights[k]*(pa[skin.joints[k]]*glm::vec4(source.vertices[i].position,1));
                            vb+=skin.weights[k]*(pb[skin.joints[k]]*glm::vec4(output.vertices[i].position,1));
                            va+=skin.weights1[k]*(pa[skin.joints1[k]]*glm::vec4(source.vertices[i].position,1));
                            vb+=skin.weights1[k]*(pb[skin.joints1[k]]*glm::vec4(output.vertices[i].position,1));
                        }
                        maximum=std::max(maximum,double(glm::length(va-vb)));
                    }
                }
            Check(maximum<1e-5,"real character all-vertex trajectory error below 10 micrometres");
            std::printf("real character maximum sampled vertex error: %.9g metres\n",maximum);
            bool unit=true;
            for(const auto &m:ResolveJointMatrices(output.skeletal->skeleton,output.skeletal->skeleton.rest))
                for(int k=0;k<3;++k)unit &= std::abs(glm::length(glm::vec3(m[k]))-1)<1e-4;
            Check(unit,"real character rest joint frames all have unit scale");
        }
    }
    std::printf("%u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
