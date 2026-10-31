#include "PhysicsWorld.h"
#include <malloc.h>
#include <cstdio>
int main(){
 {PhysicsWorld w;w.Init();w.Shutdown();}
 auto bytes=[](){return mallinfo2().uordblks;};
 const auto before=bytes();
 for(int i=0;i<64;++i){PhysicsWorld w;w.Init();w.Shutdown();w.Init();}
 const auto control=bytes();
 for(int i=0;i<64;++i){PhysicsWorld w;w.Init();w.Init();}
 const auto repeated=bytes();
 std::printf("64 explicit-shutdown controls retained: %lld bytes\n64 repeated-Init cases retained: %lld bytes\n",(long long)(control-before),(long long)(repeated-control));
 return repeated>control?0:1;
}
