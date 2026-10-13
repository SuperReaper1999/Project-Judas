#include "Deformable.h"
#include "ModelLoader.h"
#include "SceneFingerprint.h"
#include <fstream>
#include <cstdio>
// Developer/editor-compatible helpers. Outputs immutable assets, never runtime
// geometry rebuilt every frame. Generic indexed import preserves source seams.
int main(int argc,char** argv){
    try{
        if(argc<4)throw std::runtime_error("usage: judas_deformable_bake sheet output columns rows width height subdivision | block output x y z width height depth | cloth output source [sourceAssetId]");
        DeformableAsset asset;std::string error,kind=argv[1];
        if(kind=="sheet"&&argc==8)asset=MakeDeformableSheet(std::stoul(argv[3]),std::stoul(argv[4]),std::stod(argv[5]),std::stod(argv[6]),std::stoul(argv[7]));
        else if((kind=="fracture"||kind=="fracture-rigid"||kind=="structure")&&(argc==9||argc==12))asset=MakeFractureBlock({std::stoul(argv[3]),std::stoul(argv[4]),std::stoul(argv[5])},{std::stod(argv[6]),std::stod(argv[7]),std::stod(argv[8])},kind!="fracture");
        else if(kind=="partition"&&argc==4){std::ifstream source(argv[3]);std::string text{std::istreambuf_iterator<char>(source),{}};if(!source||!ImportFracturePartition(text,asset,error))throw std::runtime_error("partition import: "+error);}
        else if(kind=="block"&&argc==9)asset=MakeDeformableBlock({std::stoul(argv[3]),std::stoul(argv[4]),std::stoul(argv[5])},{std::stod(argv[6]),std::stod(argv[7]),std::stod(argv[8])});
        else if(kind=="cloth"&&(argc==4||argc==5)){MeshData mesh;if(!LoadModelMesh(argv[3],mesh,error)||!ImportDeformableCloth(mesh,asset,error))throw std::runtime_error(error);if(argc==5){asset.sourceAsset=argv[4];if(!SceneFingerprintSha256File(argv[3],asset.sourceFingerprint,error))throw std::runtime_error(error);}}
        else throw std::runtime_error("invalid bake arguments");
        if((kind=="fracture"||kind=="fracture-rigid"||kind=="structure")&&argc==12){asset.fracture->tension=std::stod(argv[9]);asset.fracture->shear=std::stod(argv[10]);asset.fracture->compliance=std::stod(argv[11]);if(!PrepareDeformableAsset(asset,error))throw std::runtime_error(error);}
        if(kind=="structure"){asset.fracture->structural=true;asset.groups["middle"]=asset.fracture->parts[asset.fracture->parts.size()/2].nodes;if(!PrepareDeformableAsset(asset,error))throw std::runtime_error(error);}
        std::ofstream output(argv[2],std::ios::binary);auto bytes=EncodeDeformableAsset(asset);output.write(bytes.data(),std::streamsize(bytes.size()));output.close();if(!output)throw std::runtime_error("cannot write output");
        std::printf("%s: %zu nodes %zu triangles %zu tetrahedra %zu render vertices\n",argv[2],asset.nodes.size(),asset.triangles.size(),asset.tetrahedra.size(),asset.render.vertices.size());return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
