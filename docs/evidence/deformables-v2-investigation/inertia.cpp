#include "Deformable.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(){
 namespace fs=std::filesystem;const auto root=fs::absolute(".cache/deformables-v2-investigation/inertia-fixture");fs::create_directories(root/"Assets");
 auto asset=MakeFractureBlock({1,1,2},{1,1,2},true);std::ofstream(root/"Assets/cube.judasdeform",std::ios::binary)<<EncodeDeformableAsset(asset);
 AssetDatabase db;db.Scan(root.string(),(root/"Assets").string());AssetRecord record;std::string error;
 if(!db.Track((root/"Assets/cube.judasdeform").string(),record,error)){std::cerr<<error;return 1;}
 ResourceManager resources(nullptr,&db);resources.RequestDeformable(record.id);resources.WaitForAll();
 Scene scene;auto& o=scene.CreateObject("Two cubical sections");o.deformable=DeformableSettings{};o.deformable->asset=record.id;o.deformable->material.density=1;
 RuntimeWorld world;if(!world.Build(scene,&resources,error)){std::cerr<<error;return 1;}world.UpdateDeformables(1./60);
 auto id=world.FracturePartEntity(o.id,0);auto body=world.RuntimeBody(id);if(!body.IsValid()){std::cerr<<"part unavailable";return 1;}
 const auto inertia=world.Physics().GetInertiaWorld(body);float mass=world.Physics().GetMass(body);
 std::cout<<"CUBE mass="<<mass<<" exact_uniform_Ixx="<<mass/6<<" actual_Ixx="<<inertia[0][0]<<" Ixy="<<inertia[0][1]<<" Ixz="<<inertia[0][2]<<" Iyz="<<inertia[1][2]<<" diagonal_ratio="<<inertia[0][0]/(mass/6)<<'\n';
 world.Physics().ApplyAngularImpulse(body,{1,0,0});auto spin=world.Physics().GetAngularVelocity(body);std::cout<<"UNIT_X_ANGULAR_IMPULSE expected_spin="<<6/mass<<",0,0 actual_spin="<<spin.x<<","<<spin.y<<","<<spin.z<<'\n';
}
