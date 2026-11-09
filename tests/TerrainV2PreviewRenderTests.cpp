#include "EngineHost.h"
#include "editor/TerrainV2PreviewRenderer.h"
#include "terrain/TerrainBrush.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

int main() {
    unsigned checks = 0;
    try {
        auto check = [&](bool value, const char *message) {
            ++checks;
            if (!value) throw std::runtime_error(message);
        };
        EngineHost host; std::string error;
        check(host.Init("Terrain editing LOD parity", 160, 160, false, error), error.c_str());
        auto &renderer = host.GetRenderer();
        auto source = TerrainV2::Create(65, 65, 1, 16);
        std::fill(source.heights.begin(), source.heights.end(), 2);
        std::vector<TerrainV2::ChunkKey> keys;
        for (unsigned z=0;z<source.ChunksZ();++z)
            for (unsigned x=0;x<source.ChunksX();++x) keys.push_back({x,z});
        std::shared_ptr<const TerrainV2::EditorPreview> product;
        TerrainV2PreviewRenderer preview;
        TextureHandle materialWeights;
        float measuredDepth=1;
        auto compare = [&](glm::vec3 eye, glm::vec3 up, std::optional<TerrainV2::Edit> brush) {
            auto image = [&](bool simplified) {
                renderer.BeginFrame(160,160);
                renderer.SetCamera(glm::lookAt(eye,glm::vec3(0),up),glm::ortho(-36.f,36.f,-36.f,36.f,.1f,150.f));
                renderer.SetLighting({0,1,0},{0,0,0},{1,1,1});
                renderer.BeginTerrainMaterial(materialWeights, {}, source.layerColours, glm::mat4(1));
                if (simplified) preview.Draw(renderer,{},glm::quat(1,0,0,0),{1,1,1},brush);
                else for (const auto &[_,chunk]:product->Chunks()) {
                    auto mesh=renderer.CreateMesh(chunk->mesh);
                    renderer.DrawMesh(mesh,chunk->anchor,glm::quat(1,0,0,0),glm::vec3(1),{},glm::vec3(1));
                    renderer.EndMeshBatch();renderer.DestroyMesh(mesh);
                }
                renderer.EndTerrainMaterial();renderer.EndFrame();
                glReadPixels(80,80,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&measuredDepth);
                std::vector<unsigned char> pixels;renderer.CaptureFrame(160,160,pixels);
                return pixels;
            };
            auto exact=image(false);const float exactDepth=measuredDepth;
            auto lod=image(true);
            if(brush)check(std::abs(measuredDepth-exactDepth)<2e-5f,
                           "brush-pinned surface depth matches full geometry after publication");
            size_t differences=0,visible=0;
            for (size_t i=0;i<exact.size();++i) {
                differences+=std::abs(int(exact[i])-int(lod[i]))>3;
                visible+=exact[i]>50;
            }
            check(visible>1000,"comparison renders real terrain");
            check(differences<exact.size()/100,"LOD, pinned brush and cave pixels agree with full geometry");
            return lod;
        };
        check(TerrainV2::PrepareEditorPreview(source,keys,{},product,error),error.c_str());
        preview.Publish(renderer,product);
        auto originalColour = compare({0,70,0},{0,0,-1},{});
        TextureData paint;paint.width=paint.height=1;paint.pixels={0,255,0,0};
        materialWeights=renderer.CreateTexture(paint);
        auto paintedColour=compare({0,70,0},{0,0,-1},{});
        check(paintedColour!=originalColour,"painting changes cached LOD colours without rebuilding geometry");
        TerrainV2::SurfaceBrush raise;
        raise.radius=4;raise.strength=2;
        std::vector<TerrainV2::ChunkKey> dirty;
        check(TerrainV2::ApplySurfaceBrush(source,{0,2,0},raise,dirty,error),error.c_str());
        auto flat=product;
        check(TerrainV2::PrepareEditorPreview(source,dirty,flat,product,error),error.c_str());
        check(flat->Lod()!=product->Lod(),"sculpt replaces the immutable height hierarchy");
        preview.Publish(renderer,product);
        compare({0,70,0},{0,0,-1},TerrainV2::Edit{TerrainV2::Edit::Kind::Remove,{0,2,0},4});
        TerrainV2::Edit cut{TerrainV2::Edit::Kind::Remove,{0,1,0},4};
        source.edits.push_back(cut);
        auto previous=product;
        check(TerrainV2::PrepareEditorPreview(source,TerrainV2::Affected(source,cut),previous,product,error),error.c_str());
        check(previous->Lod()==product->Lod(),"excavation reuses unchanged immutable height LOD metadata");
        preview.Publish(renderer,product);
        compare({0,70,0},{0,0,-1},cut);
        compare({0,-40,0},{0,0,1},{});
        // Paint and full-resolution picking remain independent of render LOD.
        glm::vec3 hit;
        check(product->Pick({20,20,20},{0,-1,0},hit)&&std::abs(hit.y-2)<1e-5,
              "LOD presentation retains exact physical source picking");
        auto stats=preview.Statistics();
        check(stats.bytes<=stats.budget,"editor GPU pages obey their bounded budget");
        renderer.DestroyTexture(materialWeights);
        preview.Clear(renderer);
        check(preview.Statistics().bytes==0,"preview release returns all owned GPU pages");
        check(glGetError()==GL_NO_ERROR,"preview rendering produces no GL error");
        std::cout<<"SUMMARY "<<checks<<" checks 0 failures\n";
        return 0;
    } catch(const std::exception &e) {
        std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;
    }
}
