#include "Deformable.h"
#include "SaveArchive.h"
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <fstream>
static unsigned checks=0;
static void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
static bool close(double a,double b){return std::abs(a-b)<1e-8*std::max(1.,std::abs(b));}
int main(){try{
    auto a=MakeFractureBlock({2,1,1},{2,2,4},true);a.fracture->structural=true;
    a.fracture->tension=100;a.fracture->shear=100;std::string error;
    check(PrepareDeformableAsset(a,error),"prepare actual rectangular section");
    const auto& b=a.fracture->bonds[0];check(close(b.area,8),"known area");
    // 2m height, 4m breadth: I_z=b*h^3/12=8/3, I_y=h*b^3/12=32/3.
    check(close(b.sectionInverse[2][2],3./32)&&close(b.sectionInverse[1][1],3./8),"independent integrated area moments");
    check(close(b.polarMoment,40./3),"known polar area moment");
    FractureState s;s.Initialize(*a.fracture);
    s.ObserveSection(*a.fracture,0,{80,0,0},{0,0,0});check(close(s.tensionDemand[0],10)&&!s.pending[0],"axial tension N/A");
    s.ObserveSection(*a.fracture,0,{0,0,0},{0,0,80});check(close(s.tensionDemand[0],30)&&!s.pending[0],"rectangular bending M/Z differs from square approximation");
    s.Initialize(*a.fracture);s.ObserveSection(*a.fracture,0,{-240,0,0},{0,0,80});check(close(s.tensionDemand[0],0)&&!s.pending[0],"compression offsets bending without invented tensile failure");
    a.fracture->compression=50;s.ObserveSection(*a.fracture,0,{-240,0,0},{0,0,80});check(s.pending[0]==1,"opposite fibre fails enabled compression");
    s.Initialize(*a.fracture);s.ObserveSection(*a.fracture,0,{0,160,0},{0,0,0});check(close(s.shearDemand[0],20),"shear resultant over area");
    s.Initialize(*a.fracture);s.ObserveSection(*a.fracture,0,{0,0,0},{80,0,0});check(close(s.shearDemand[0],6*std::sqrt(5.)),"torsion polar approximation declared explicitly");
    a.fracture->bonds[0].tension=20;s.Initialize(*a.fracture);s.ObserveSection(*a.fracture,0,{168,0,0},{0,0,0});check(s.pending[0]==1&&s.Commit(*a.fracture)&&s.component[0]!=s.component[1],"authored per-connection capacity and irreversible graph publication");
    auto bytes=EncodeDeformableAsset(a);DeformableAsset restored;check(bytes.rfind("JudasDeformable3",0)==0&&DecodeDeformableAsset({bytes.begin(),bytes.end()},restored,error),"new optional asset version round trip");check(restored.fracture->structural&&restored.fracture->bonds[0].tension==20&&close(restored.fracture->bonds[0].polarMoment,b.polarMoment),"strength persists while section caches are rebuilt");
    SaveArchive save;s.Persist(save,*a.fracture);FractureState cold;cold.Initialize(*a.fracture);SaveArchive read(save.bytes);cold.Persist(read,*a.fracture);read.Finish();check(cold.broken==s.broken&&cold.component==s.component&&cold.committed.empty(),"topology cold roundtrip no event replay");
    auto old=MakeFractureBlock({2,1,1},{2,1,1},true);auto legacy=EncodeDeformableAsset(old);check(legacy.rfind("JudasDeformable2",0)==0&&DecodeDeformableAsset({legacy.begin(),legacy.end()},restored,error)&&!restored.fracture->structural,"legacy asset remains legacy model");check(EncodeDeformableAsset(restored)==legacy,"old asset bytes preserved");
    auto invalid=a;invalid.fracture=std::make_shared<FractureCook>(*a.fracture);invalid.fracture->bonds[0].centroid.y=.1;check(!PrepareDeformableAsset(invalid,error),"non-centroid structural section rejected");
    invalid.fracture->bonds[0].centroid=b.centroid;invalid.fracture->bonds[0].tension=-1;check(!PrepareDeformableAsset(invalid,error),"negative strength rejected");
    // M80 reuses the same independent section model for cohesive XPBD solids.
    // No rigid child bodies or joint assumptions belong in section preparation.
    auto soft=MakeFractureBlock({2,1,1},{2,2,4},false);soft.fracture->structural=true;
    soft.fracture->tension=100;soft.fracture->shear=100;soft.fracture->bonds[0].tension=20;
    check(PrepareDeformableAsset(soft,error)&&!soft.fracture->rigid,"soft cohesive section is an explicit supported representation");
    check(close(soft.fracture->bonds[0].area,8)&&close(soft.fracture->bonds[0].sectionInverse[1][1],3./8)&&close(soft.fracture->bonds[0].polarMoment,40./3),"soft section derives actual rectangular area and moments");
    FractureState softState;softState.Initialize(*soft.fracture);softState.ObserveSection(*soft.fracture,0,{0,0,0},{0,0,40});
    check(close(softState.tensionDemand[0],15)&&!softState.pending[0],"soft bending below authored override remains connected");
    softState.ObserveSection(*soft.fracture,0,{0,0,0},{0,0,80});
    check(close(softState.tensionDemand[0],30)&&softState.Commit(*soft.fracture)&&softState.component[0]!=softState.component[1],"soft bending exceeds capacity and publishes actual topology separation");
    auto softBytes=EncodeDeformableAsset(soft);DeformableAsset softRestored;
    check(DecodeDeformableAsset({softBytes.begin(),softBytes.end()},softRestored,error)&&softRestored.fracture->structural&&!softRestored.fracture->rigid&&softRestored.fracture->bonds[0].tension==20,"soft structural asset roundtrip preserves mode and explicit capacity");
    check(EncodeDeformableAsset(softRestored)==softBytes,"soft structural asset encoding remains deterministic");
    auto rectangular=MakeFractureBlock({2,1,1},{2,.2,4},true);rectangular.fracture->structural=true;check(PrepareDeformableAsset(rectangular,error),"slender non-square section");s.Initialize(*rectangular.fracture);s.ObserveSection(*rectangular.fracture,0,{0,0,0},{0,0,1});check(close(s.tensionDemand[0],37.5),"small bending dimension governs capacity");
    auto small=MakeFractureBlock({2,1,1},{.02,.02,.04},true);small.fracture->structural=true;check(PrepareDeformableAsset(small,error),"section conditioning is independent of world unit scale");s.Initialize(*small.fracture);s.ObserveSection(*small.fracture,0,{0,0,0},{0,0,.00008});check(close(s.tensionDemand[0],30),"scaled moment produces equal section stress");
    check(!DecodeDeformableAsset({bytes.begin(),bytes.end()-1},restored,error),"truncated structural cook rejected transactionally");
    std::ifstream input("projects/fracture_lab/Assets/deformables/L-partition.source");std::string source{std::istreambuf_iterator<char>(input),{}};auto end=source.rfind("end");check(input.good()&&end!=std::string::npos,"existing irregular partition source available");source.insert(end,"structural 1\nstrength \"import-bond-0\" 11 22 33\n");check(ImportFracturePartition(source,restored,error)&&restored.fracture->structural&&restored.fracture->bonds[0].tension==11&&restored.fracture->bonds[0].compression==33,"explicit irregular source opts in with stable-key interface strengths");auto unknown=source;unknown.replace(unknown.find("import-bond-0",unknown.find("strength")),13,"no-such-key");check(!ImportFracturePartition(unknown,restored,error),"unknown authored strength key fails cleanly");
    std::printf("M79 section checks=%u PASS\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"M79 check %u FAILED: %s\n",checks,e.what());return 1;}}
