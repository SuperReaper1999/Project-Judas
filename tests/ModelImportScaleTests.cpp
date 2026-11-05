#include "ModelArchive.h"
#include "ModelCook.h"
#include "ModelImportScale.h"
#include "ModelLoader.h"
#include "SceneFingerprint.h"
#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
unsigned checks = 0, failures = 0;
void Check(bool okay, const char *name) {
    ++checks; failures += !okay;
    std::printf("%s %s\n", okay ? "PASS" : "FAIL", name);
}
MeshData Triangle(float size = 2) {
    MeshData mesh; mesh.vertices.resize(3);
    mesh.vertices[0].position = {0,0,0};
    mesh.vertices[1].position = {size,0,0};
    mesh.vertices[2].position = {0,size,0};
    mesh.indices = {0,1,2};
    return mesh;
}
std::shared_ptr<SkeletalAsset> Rig(float scale) {
    auto asset = std::make_shared<SkeletalAsset>(); auto &s = asset->skeleton;
    s.names = {"Source"}; s.parents = {-1}; s.order = {0}; s.rest.local.resize(1);
    s.rest.local[0].scale = glm::vec3(scale); s.skinNodes = {0}; s.inverseBind = {glm::mat4(1)};
    return asset;
}
Json Read(const fs::path &path) {std::ifstream file(path); return Json::parse(file);}
void Write(const fs::path &path, const Json &value) {std::ofstream(path) << value.dump(2) << '\n';}
std::string Hash(const fs::path &path) {
    std::string hash, error; if (!SceneFingerprintSha256File(path.string(),hash,error)) throw std::runtime_error(error);
    return hash;
}
bool Cook(const std::string &recipe, ModelCookTask &task) {
    std::string error; const bool okay = CookModelRecipe(recipe,task) && PublishModelImport(task,error);
    if(!okay)
        std::fprintf(stderr,"cook: %s %s\n",task.error.c_str(),error.c_str());
    return okay;
}
void Pure() {
    ModelImportSettings settings; ModelImportReport report; auto mesh = Triangle();
    JudasImport::InspectScale("normal.obj",settings,mesh,report);
    Check(!JudasImport::ScaleWarning(report),"ordinary two-metre geometry needs no scale warning");
    mesh = Triangle(200); const auto before = mesh.vertices[1].position;
    JudasImport::InspectScale("large.obj",settings,mesh,report);
    auto *warning = JudasImport::ScaleWarning(report);
    Check(warning && warning->message.find("200 x 200 x 0 metres")!=std::string::npos &&
              warning->message.find("100x")!=std::string::npos && warning->action.find("unit")!=std::string::npos,
          "large imported geometry has dimensions, units, 100x guidance and corrective choices");
    Check(mesh.vertices[1].position==before && warning->message.find("can be intentional")!=std::string::npos,
          "warning is advisory and cannot rescale a legitimate large level");
    JudasImport::InspectScale("large.obj",settings,mesh,report);
    Check(report.diagnostics.size()==1,"repeated scale inspection replaces its advice rather than duplicating warnings");
    mesh = Triangle(.005f); JudasImport::InspectScale("tiny.obj",settings,mesh,report);
    Check(JudasImport::ScaleWarning(report),"millimetre-size imported objects get an explicit review rather than automatic enlargement");
    mesh = Triangle(); for(auto &v:mesh.vertices)v.position+=glm::vec3(1000000,2000000,-1000000);
    JudasImport::InspectScale("offset.obj",settings,mesh,report);
    Check(!JudasImport::ScaleWarning(report),"large world-coordinate offset is not confused with large physical dimensions");
    mesh = Triangle(.02f); mesh.skeletal=Rig(100); mesh.skinVertices.resize(3);
    report.sourceUnitMeters=1;
    JudasImport::InspectScale("scaled.gltf",settings,mesh,report);
    Check(JudasImport::ScaleWarning(report),"an actual 100x geometry transform is visible even when its source vertices are small");
    report.sourceUnitMeters=.01;
    JudasImport::InspectScale("centimetre-character.fbx",settings,mesh,report);
    Check(!JudasImport::ScaleWarning(report),"correct centimetre FBX with 100x rig transform and metre-size result is not falsely warned");
    report.sourceUnitMeters=1; auto rig=Rig(100); rig->skeleton.inverseBind[0]=glm::scale(glm::mat4(1),glm::vec3(.01f));
    mesh=Triangle();mesh.skeletal=rig;mesh.skinVertices.resize(3);
    JudasImport::InspectScale("compensated.gltf",settings,mesh,report);
    Check(!JudasImport::ScaleWarning(report),"inverse-bind compensation is respected instead of warning on raw bone scale");
    mesh=Triangle();mesh.vertices.push_back({});mesh.vertices.back().position={10000,10000,0};
    mesh.primitives.push_back({0,3,0,"selected",-1});
    JudasImport::InspectScale("selected.gltf",settings,mesh,report);
    Check(!JudasImport::ScaleWarning(report),"unselected/unreferenced vertices do not inflate the reported selected part bounds");
}
void Real(const fs::path &root, const fs::path &fbxFixture) {
    fs::create_directories(root / "Assets"); const auto source=root/"large.obj";
    std::ofstream(source)<<"o wall\nv 0 0 0\nv 200 0 0\nv 0 200 0\nf 1 2 3\n";
    MeshData mesh;ModelImportSettings settings;ModelImportReport report;std::string error;
    Check(ImportModelSource(source.string(),settings,mesh,report,error)&&JudasImport::ScaleWarning(report),
          "real OBJ source import exposes the shared scale diagnostic");
    settings.sourceUnitMeters=.01; ModelImportReport fixed;
    Check(ImportModelSource(source.string(),settings,mesh,fixed,error)&&!JudasImport::ScaleWarning(fixed)&&
              glm::length(mesh.vertices[1].position-mesh.vertices[0].position)<=2.0001f,
          "explicit centimetre correction produces metre geometry and clears the size warning");
    ModelImportReport fbx;
    Check(ImportModelSource(fbxFixture.string(),{},mesh,fbx,error)&&
              !JudasImport::ScaleWarning(fbx),
          "real metadata-normalized centimetre/pivot FBX remains a no-scale-warning negative control");
    std::string recipe;Check(CreateModelRecipe(root.string(),source.string(),"Assets/large.judasmodel",recipe,error),
                             "large-source recipe is an ordinary editable import recipe");
    ModelCookTask first;Check(Cook(recipe,first)&&JudasImport::ScaleWarning(first.report),
                              "fresh cook preserves scale advice and still permits explicit CLI publication");
    const auto original=Hash(first.output);ModelCookTask unchanged;unchanged.previewRequired=false;
    Check(Cook(recipe,unchanged)&&unchanged.receiptHit&&unchanged.decodedProducts==0&&
              JudasImport::ScaleWarning(unchanged.report)&&Hash(first.output)==original,
          "zero-decode receipt reuse retains diagnostics and exact accepted model bytes");
    fs::path cache;
    for(const auto &entry:fs::directory_iterator(root/".cache/model-import"))
        if(entry.path().extension()==".json")cache=entry.path();
    auto info=Read(cache);info["diagnostics"].push_back({{"severity","warning"},{"code","cache-negative-control"},
        {"source","large.obj"},{"node",""},{"message","Known cached advice"},{"action","Retain this metadata"}});
    Write(cache,info);fs::remove(first.output);fs::remove(first.output+".import-report.json");
    ModelCookTask cached;bool marker=false;const bool restored=Cook(recipe,cached);
    for(const auto &d:cached.report.diagnostics)marker|=d.code=="cache-negative-control";
    Check(restored&&marker&&JudasImport::ScaleWarning(cached.report),
          "decoded-product cache preserves stored diagnostics and scale context on a cold publication");
    Check(LoadModelMesh(first.output,mesh,error)&&mesh.vertices[1].position.x==200,
          "successful large-model publication retains original geometry without automatic correction");
}
}
int main(int argc,char **argv) {
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    try {
        if(argc>3)throw std::invalid_argument("usage: ModelImportScaleTests [FRESH_OUTPUT [FBX_FIXTURE]]");
        const auto root=argc>=2?fs::absolute(argv[1]):fs::temp_directory_path()/
            ("judas-scale-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        const auto fbxFixture=argc==3?fs::absolute(argv[2]):
            fs::path(__FILE__).parent_path()/"fixtures/m66/pivot-centimetres.fbx";
        if(fs::exists(root))throw std::invalid_argument("scale fixture requires a fresh output directory");
        Pure();Real(root,fbxFixture);
    } catch(const std::exception &e){Check(false,e.what());}
    std::printf("Model scale: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
