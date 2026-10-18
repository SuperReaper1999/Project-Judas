#define main PreservedImpactMeshMain
#include "ImpactMeshCostTests.cpp"
#undef main
int main(int argc,char**argv){std::cout<<std::setprecision(17);auto ground=Sheet(2,26,true);SheetOracle oracle(*ground);const int only=argc>1?std::atoi(argv[1]):-1;
 for(int i=0;i<24;++i){if(only>=0&&i!=only)continue;PhysicsWorld w;w.Init();w.CreateShape(Shape::Cooked(ground,"trace"),{},false,1,.6f,0);auto h=w.CreateDynamicBox({-1.2f+.1f*i,1.2f,-1},{.02f,.01f,.03f},.2f,.4f,0);w.SetLinearVelocity(h,{0,-40,float(i%6)*6});
  for(int step=0;step<90;++step){try{w.ApplyLinearAcceleration(h,{0,-9.81f,0},1.f/60.f);w.Step(1.f/60.f);}catch(const std::exception&e){std::cout<<"THROW i="<<i<<" step="<<step<<" error="<<e.what()<<'\n';break;}
  auto p=w.GetTransform(h).position,v=w.GetLinearVelocity(h);const auto s=w.LastStepStats();auto depth=oracle.Depth(p);if(only>=0||depth>.05)std::cout<<"TRACE i="<<i<<" step="<<step<<" p="<<p.x<<","<<p.y<<","<<p.z<<" v="<<v.x<<","<<v.y<<","<<v.z<<" depth="<<depth<<" events="<<s.impactEvents<<" q="<<s.impactQueries<<" samples="<<s.impactSamplingTests<<'\n';if(depth>.05)break;}
 }
}
