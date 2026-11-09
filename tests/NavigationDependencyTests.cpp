#include "NavigationDependencies.h"
#include "NavigationAsset.h"
#include "ProjectExporter.h"
#include "SceneSerialization.h"
#include "CollisionAsset.h"
#include "ModelArchive.h"
#include "SceneFingerprint.h"
#include "terrain/TerrainSource.h"
#include "terrain/TerrainMesher.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
int main() {
    unsigned checks = 0;
    auto check = [&](bool okay, const std::string& message) {
        ++checks;
        if (!okay) throw std::runtime_error(message);
    };
    try {
        const auto root = fs::temp_directory_path() / ("judas-nav-dependencies-" + MintAssetId());
        Project project;
        AssetDatabase assets;
        std::string error;
        check(Project::CreateNew(root.string(), "Navigation dependencies", project, error), error);
        auto source = TerrainV2::Create(33,33,.5f);
        auto publish = [&] {
            MeshData mesh;
            for(unsigned z=0;z<source.ChunksZ();++z)for(unsigned x=0;x<source.ChunksX();++x) {
                TerrainV2::Chunk chunk;
                check(TerrainV2::BuildChunk(source,{x,z},chunk,error),error);
                const unsigned offset=mesh.vertices.size();
                for(auto v:chunk.mesh.vertices){v.position+=chunk.anchor;mesh.vertices.push_back(v);}
                for(auto i:chunk.mesh.indices)mesh.indices.push_back(offset+i);
            }
            std::vector<uint8_t> bytes;
            check(EncodeModelArchive(mesh,bytes,error),error);
            std::ofstream model(root/"Assets/ground.judasmodel",std::ios::binary);
            model.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());model.close();
            assets.Scan(project.RootDir(),project.AssetsDir());
            AssetRecord modelAsset;if(const auto* found=assets.FindByRelativePath("Assets/ground.judasmodel"))modelAsset=*found;else check(assets.Track((root/"Assets/ground.judasmodel").string(),modelAsset,error),error);
            CollisionAsset collision;
            check(CookCollision(mesh,{},collision,error),error);
            collision.sourceAsset=modelAsset.id;
            collision.settingsFingerprint=CollisionSettingsFingerprint({});
            collision.sourceFingerprint=SceneFingerprintSha256(std::string(bytes.begin(),bytes.end()));
            check(SaveCollisionAsset((root/"Assets/ground.judascollision").string(),collision,error),error);
            AssetRecord cooked;if(const auto* found=assets.FindByRelativePath("Assets/ground.judascollision"))cooked=*found;else check(assets.Track((root/"Assets/ground.judascollision").string(),cooked,error),error);
            return std::pair(modelAsset.id,cooked.id);
        };
        const auto ids=publish();
        Scene scene;
        auto& ground=scene.CreateObject("Source-owned terrain geometry");
        ground.render=SceneRenderComponent{};ground.render->shape=SceneShape::Mesh;ground.render->meshAsset=ids.first;
        ground.body=SceneBodyComponent{};ground.body->shape=SceneShape::TriangleMesh;ground.body->collisionAsset=ids.second;
        auto& nav=scene.CreateObject("Navigation");nav.navigationSurface=NavigationSurfaceSettings{};
        nav.navigationSurface->halfExtents={8,4,8};nav.navigationSurface->minRegion=0;
        for(const char* path:{"Scenes/main.judas","Scenes/second.judas"})
            check(SaveSceneToFile(scene,project.Resolve(path),error),error);
        assets.Scan(project.RootDir(),project.AssetsDir());
        std::vector<NavigationDependency> rows;
        auto inspect=[&] {check(InspectNavigationDependencies(project,assets,ids.first,rows,error),error);};
        inspect();check(rows.size()==2,"two scene dependencies discovered without a historical application");
        check(RebuildNavigationDependencies(project,assets,rows,error),error);inspect();
        for(const auto& row:rows)check(!row.stale,"initial baked navigation is fresh");
        source.heights[source.heights.size()/2]+=.25f;publish();inspect();
        for(const auto& row:rows)check(row.stale,"source geometry publication invalidates both scenes");
        auto broken=rows;broken.back().surface=99999999;
        check(!RebuildNavigationDependencies(project,assets,broken,error),"late invalid surface rejects whole batch");inspect();
        for(const auto& row:rows)check(row.stale,"failed batch preserves both previous bakes");
        unsigned calls=0;
        check(!RebuildNavigationDependencies(project,assets,rows,error,[&]{return ++calls>=3;}),"cancellation after a bake prevents publication");inspect();
        for(const auto& row:rows)check(row.stale,"cancelled batch publishes neither scene");
        check(RebuildNavigationDependencies(project,assets,{rows.front()},error),error);inspect();
        unsigned fresh=0;for(const auto& row:rows)fresh+=!row.stale;
        check(fresh==1,"explicit single-scene rebuild leaves the other stale");
        check(RebuildNavigationDependencies(project,assets,rows,error),error);inspect();
        for(const auto& row:rows) {
            check(!row.stale,"both scenes have current navigation");
            Scene loaded;check(LoadSceneFromFile(project.Resolve(row.scene),loaded,error),error);
            check(ValidateSceneContent(loaded,assets,project.Settings().classification,project.Settings().navigation,error),error);
            loaded.Objects().front().render->color={.2f,.7f,.3f};
            check(SaveSceneToFile(loaded,project.Resolve(row.scene),error),error);
        }
        inspect();for(const auto& row:rows)check(!row.stale,"appearance-only changes preserve navigation");
        fs::remove_all(root);
        std::cout<<checks<<" navigation dependency checks passed\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
