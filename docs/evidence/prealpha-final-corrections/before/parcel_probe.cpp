#include "LiquidSystem.h"
#include "LiquidGeometry.h"
#include "SaveArchive.h"
#include <iostream>
int main(){
 std::string error;GravityEquilibrium eq;eq.magnitude=9.81;LiquidBasinData baked;
 if(!BakeLiquidBasin(LiquidBox({-1,0,-1},{1,1,1}),eq,1e-6,.001,"audit",baked,error)){std::cerr<<error;return 2;}
 LiquidSystem original(true);LiquidBasinSettings author;author.initialVolume=1;
 auto h=original.AddBasin(1,author,{},std::make_shared<LiquidBasinData>(baked),error);if(!h.id)return 3;
 SaveArchive withOwner;original.Persist(withOwner);
 try{LiquidSystem denied(false);SaveArchive a(withOwner.bytes);denied.Persist(a);std::cout<<"OWNER disabled unexpectedly accepted\n";}catch(const std::exception& e){std::cout<<"OWNER disabled correctly rejected: "<<e.what()<<"\n";}
 if(!original.Remove(h))return 4;
 SaveArchive detached;original.Persist(detached);
 std::cout<<"ORIGINAL owners="<<original.States().size()<<" parcels="<<original.Parcels().size()<<" total="<<original.Accounting("water").total<<"\n";
 for(bool enabled:{true,false}){LiquidSystem restored(enabled);SaveArchive a(detached.bytes);try{restored.Persist(a);a.Finish();std::cout<<"RESTORE enabled="<<enabled<<" accepted owners="<<restored.States().size()<<" parcels="<<restored.Parcels().size()<<" total="<<restored.Accounting("water").total<<"\n";}catch(const std::exception& e){std::cout<<"RESTORE enabled="<<enabled<<" rejected: "<<e.what()<<"\n";}}
 return 0;
}
