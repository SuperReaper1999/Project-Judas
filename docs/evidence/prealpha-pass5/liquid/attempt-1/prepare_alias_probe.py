from pathlib import Path
import hashlib, json
root=Path('/home/conner/Documents/GitHub/Project-Judas')
source=root/'tests/OriginEnvironmentTests.cpp'
text=source.read_text()
helper=r'''
// Cache-only Pass 5 physical oracle. Original fixture, inputs and failing check
// remain unmodified in tests/OriginEnvironmentTests.cpp.
double ActualMeshCap(const LiquidSurface& surface, glm::dvec3 p) {
    auto mesh=surface.Mesh(1);
    if(std::getenv("JUDAS_LIQUID_NEGATIVE_EMPTY_MESH"))mesh.vertices.clear();
    const auto up=surface.data->equilibrium.Up(p);
    double nearest=1e30;
    for(size_t i=0;i+2<mesh.vertices.size();i+=3){
        const glm::dvec3 a(mesh.vertices[i].position),b(mesh.vertices[i+1].position),c(mesh.vertices[i+2].position);
        const auto e=b-a,f=c-a,n=glm::cross(e,f);
        const double denominator=glm::dot(n,up);
        if(std::abs(denominator)<1e-14)continue;
        const double distance=glm::dot(n,a-p)/denominator;
        if(distance<0)continue;
        const auto v=p+up*distance-a;
        const double ee=glm::dot(e,e),ef=glm::dot(e,f),ff=glm::dot(f,f),ve=glm::dot(v,e),vf=glm::dot(v,f);
        const double d=ee*ff-ef*ef;
        if(d<=1e-25)continue;
        const double u=(ve*ff-vf*ef)/d,w=(vf*ee-ve*ef)/d;
        if(u>=-1e-14&&w>=-1e-14&&u+w<=1+1e-14)nearest=std::min(nearest,distance);
    }
    return nearest;
}
void AliasAndOccupancyOracle(RuntimeWorld& world,LiquidSystem& liquid,LiquidHandle source,LiquidHandle target,glm::vec3 point) {
    const glm::vec3 delta(512,-512,512);
    auto rounded=(point-delta)+delta;
    auto a=liquid.Sample(point),b=liquid.Sample(rounded);
    if(point==rounded){
        // Additional radial control only: unchanged original radial fixture,
        // with explicitly separate opposite-sided query inputs.
        const auto* state=liquid.Get(source);const auto origin=point;
        bool found=false;
        for(const auto tangent:{state->dynamicSurface->data->x,state->dynamicSurface->data->y}){
            const auto direction=glm::vec3(glm::dquat(state->pose.rotation)*tangent);
            for(int exponent=-12;exponent>=-24&&!found;--exponent){
                const float epsilon=std::ldexp(1.f,exponent);const auto p=origin+direction*epsilon,q=origin-direction*epsilon;
                const auto pSample=liquid.Sample(p),qSample=liquid.Sample(q);
                if(p!=q&&(p-delta)==(q-delta)&&pSample&&qSample&&
                   glm::length(pSample->surfacePoint-qSample->surfacePoint)>.0006f&&
                   glm::length(pSample->velocity-qSample->velocity)>.0006f){point=p;rounded=q;a=pSample;b=qSample;found=true;}
            }
            if(found)break;
        }
        Check(found,"separate radial case finds two opposite physical sides with identical translated float identity");
    }
    Check(a&&b&&a->handle==b->handle,"both original-coordinate queries hit the same real owner");
    Check(point!=rounded&&(point-delta)==(rounded-delta),"different pre-shift query identities collapse to exactly the same post-shift binary32 input");
    const auto* owner=liquid.Get(source);
    const auto inverse=glm::inverse(glm::dquat(owner->pose.rotation));
    auto local=[&](glm::vec3 p){return inverse*(glm::dvec3(p)-glm::dvec3(owner->pose.position));};
    const auto pa=local(point),pb=local(rounded);
    const auto ca=owner->dynamicSurface->Cell(pa),cb=owner->dynamicSurface->Cell(pb);
    Check(ca>=0&&cb>=0&&ca!=cb,"aliased query identities occupy opposite represented finite-volume cells");
    const auto capA=ActualMeshCap(*owner->dynamicSurface,pa),capB=ActualMeshCap(*owner->dynamicSurface,pb);
    Check(std::isfinite(capA)&&std::isfinite(capB)&&capA<1e29&&capB<1e29,"actual triangle geometry supplies both independently intersected caps");
    const auto geometricA=pa+owner->equilibrium.Up(pa)*capA,geometricB=pb+owner->equilibrium.Up(pb)*capB;
    const auto sampledA=local(a->surfacePoint),sampledB=local(b->surfacePoint);
    const double meshA=glm::length(geometricA-sampledA),meshB=glm::length(geometricB-sampledB);
    Check(meshA<.0003&&meshB<.0003,"both reported sample caps agree with separately intersected rendered physical geometry");
    const double capDifference=glm::length(glm::dvec3(a->surfacePoint)-glm::dvec3(b->surfacePoint));
    const double velocityDifference=glm::length(glm::dvec3(a->velocity)-glm::dvec3(b->velocity));
    Check(capDifference>2*.0003&&velocityDifference>2*.0003,"triangle inequality prevents any single post-shift result from satisfying both unchanged tolerances");
    const auto state=SurfaceState(*owner);const auto ledger=liquid.Accounting("water");
    liquid.SetEnabled(source,false);Check(!liquid.Sample(point),"negative removed occupancy cannot produce the former sample");liquid.SetEnabled(source,true);
    Check(SurfaceState(*owner)==state&&liquid.Accounting("water").total==ledger.total,"read-only oracle and enable control do not alter wave history or owner quantity");
    std::cout<<std::setprecision(17)<<"ALIAS point_a="<<point.x<<','<<point.y<<','<<point.z<<" point_b="<<rounded.x<<','<<rounded.y<<','<<rounded.z
      <<" common_shifted="<<(point-delta).x<<','<<(point-delta).y<<','<<(point-delta).z<<" local_error="<<glm::length(pa-pb)
      <<" cell_a="<<ca<<" cell_b="<<cb<<" cap_difference="<<capDifference<<" velocity_difference="<<velocityDifference<<" mesh_a_error="<<meshA<<" mesh_b_error="<<meshB<<'\n';
    auto body=LiquidBox({-.1,.45,-.1},{.1,.55,.1});
    for(auto& tet:body.cells)for(auto& p:tet)p=glm::dvec3(owner->pose.position)+glm::dquat(owner->pose.rotation)*p;
    const auto loaded=liquid.Submerged(body);
    Check(loaded.volume>0&&loaded.volume<.004,"independent finite body crosses actual unequal caps and is partially submerged");
    std::string shiftError;
    Check(world.RequestOrigin(glm::dvec3(delta),shiftError)&&world.CommitOrigin(),"both query identities pass through the real authoritative world-origin transaction");
    const auto shifted=liquid.Sample(point-delta);
    Check(bool(shifted),"actual translated world still supplies one physical sample for the common represented point");
    const double surfaceErrorA=glm::length(glm::dvec3(shifted->surfacePoint+delta)-glm::dvec3(a->surfacePoint));
    const double surfaceErrorB=glm::length(glm::dvec3(shifted->surfacePoint+delta)-glm::dvec3(b->surfacePoint));
    Check(!(Near(shifted->surfacePoint+delta,a->surfacePoint)&&Near(shifted->surfacePoint+delta,b->surfacePoint)),"actual translated physical output cannot retain both distinct pre-shift caps under the unchanged tolerance");
    std::cout<<"SHIFT alias_surface_error_a="<<surfaceErrorA<<" alias_surface_error_b="<<surfaceErrorB<<'\n';
    for(auto& tet:body.cells)for(auto& p:tet)p-=glm::dvec3(delta);
    const auto loadedShifted=liquid.Submerged(body);
    Check(std::abs(loadedShifted.volume-loaded.volume)<1e-7&&Near(loadedShifted.buoyancy,loaded.buoyancy,.003)&&Near(loadedShifted.center+delta,loaded.center),"actual partial-volume solid loading remains physically equivalent across the same half-turn chart shift");
    std::cout<<"LOADING volume_before="<<loaded.volume<<" volume_after="<<loadedShifted.volume<<" absolute_error="<<std::abs(loaded.volume-loadedShifted.volume)<<'\n';
    std::map<std::string,SaveChunk> chunks;
    Check(WorldPersistence::Capture(world,chunks,shiftError),"real disturbed half-turn world captures through the ordinary persistence participants");
    Scene saved;RuntimeWorld cold;cold.legacyGameplay=false;
    Check(WorldPersistence::SceneFrom(chunks,saved,shiftError)&&cold.Build(saved,world.Resources(),shiftError,nullptr,nullptr,true)&&WorldPersistence::Prepare(cold,shiftError)&&WorldPersistence::Restore(cold,chunks,shiftError),"independent runtime reconstructs the exact shifted geometry and owned wave state");
    const auto* restored=cold.Liquids().Get(cold.Liquids().Handle(owner->entity));
    const auto restoredSample=cold.Liquids().Sample(point-delta);
    Check(restored&&SurfaceState(*restored)==state&&restoredSample&&Near(restoredSample->surfacePoint,shifted->surfacePoint)&&Near(restoredSample->velocity,shifted->velocity)&&cold.Liquids().Accounting("water").total==ledger.total,"restored physical query endpoint matches the actual represented shifted state, without claiming the lost pre-shift identity");
    Check(world.RequestOrigin({0,0,0},shiftError)&&world.CommitOrigin()&&SurfaceState(*owner)==state&&liquid.Accounting("water").total==ledger.total,"return transaction preserves exact wave history and owner total");
    // Separate conservative transaction witness: a continuous height graph is
    // not universally compatible with full/empty neighbouring cell quantities.
    auto* receiver=liquid.Get(target);auto& surface=*receiver->dynamicSurface;const auto& face=surface.data->faces.front();
    const auto fillCell=face.a,emptyCell=face.b;
    auto worldPoint=[&](unsigned c){const auto p=surface.data->cells[c].centre;return glm::vec3(glm::dvec3(receiver->pose.position)+glm::dquat(receiver->pose.rotation)*p);};
    Check(surface.Cell(inverse*(glm::dvec3(worldPoint(fillCell))-glm::dvec3(receiver->pose.position)))==int(fillCell)&&
          surface.Cell(inverse*(glm::dvec3(worldPoint(emptyCell))-glm::dvec3(receiver->pose.position)))==int(emptyCell),"authored centre points robustly select the actual adjacent cells");
    const auto total=liquid.Accounting("water").total;
    const double add=surface.cells[fillCell].storage.capacity-surface.cells[fillCell].volume;
    Check(liquid.Transfer(source,target,add,{},worldPoint(fillCell))==add,"legitimate paired transfer fills selected cell without special-case state edits");
    const double remove=surface.cells[emptyCell].volume;
    Check(liquid.Transfer(target,source,remove,worldPoint(emptyCell),{})==remove,"legitimate paired transfer empties adjacent selected cell");
    Check(surface.cells[fillCell].volume==surface.cells[fillCell].storage.capacity&&surface.cells[emptyCell].volume==0,"open-face neighbouring cells can legitimately be full and dry before the next physical step");
    Check(std::abs(liquid.Accounting("water").total-total)<=liquid.Accounting("water").tolerance&&std::abs(liquid.Accounting("water").error)<=liquid.Accounting("water").tolerance,"full/dry construction preserves the authoritative M54 ledger");
    std::cout<<"FULL_DRY full_cell="<<fillCell<<" dry_cell="<<emptyCell<<" full_volume="<<surface.cells[fillCell].volume<<" dry_volume="<<surface.cells[emptyCell].volume
      <<" full_q="<<surface.cells[fillCell].q<<" dry_q="<<surface.cells[emptyCell].q<<" ledger_error="<<liquid.Accounting("water").error<<'\n';
}
'''
text=text.replace('#include "../third_party/nlohmann/json.hpp"','#include "nlohmann/json.hpp"\n#include <cstdlib>')
needle='void LiquidContract(const fs::path& output, bool radial,'
assert text.count(needle)==1
text=text.replace(needle,helper+'\n'+needle)
needle='    Check(bool(sample), "positive pre-shift physical submersion sample");'
assert text.count(needle)==1
text=text.replace(needle,needle+'\n    if(std::getenv("JUDAS_LIQUID_ALIAS_PROBE")){AliasAndOccupancyOracle(world,liquid,handle,liquid.Handle(3),point);return;}')
needle='        if (argc == 4 && (std::string(argv[1])'
assert text.count(needle)==1
text=text.replace(needle,'        if(argc==3 && std::string(argv[1])=="--alias-radial"){const auto output=fs::absolute(argv[2]);Check(!fs::exists(output),"fresh separate radial output");LiquidContract(output,true);std::cout<<"SUMMARY "<<checks<<" checks 0 failures\\n";return 0;}\n'+needle)
out=root/'.cache/prealpha-pass5/liquid/AliasProbe.cpp';out.write_text(text)
(root/'.cache/prealpha-pass5/liquid/SOURCE.json').write_text(json.dumps({'original_source':str(source.relative_to(root)),'original_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'generated_probe_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'transformation':'Unchanged original fixture and test copied outside production; helper and opt-in environment branch added before original failing query. Original source file untouched.'},indent=2)+'\n')
