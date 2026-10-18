#define main PreservedImpactMeshMain
#include "ImpactMeshCostTests.cpp"
#undef main
int main(){const auto ground=Sheet(2,2,false);PhysicsWorld w;w.Init();auto floor=w.CreateShape(Shape::Cooked(ground,"episode lifecycle"),{},false,1,.6f,0);auto body=w.CreateDynamicBox({0,.01,0},{.02,.01,.03},.2f,.4f,0);
 // Acquire a real resolved front contact and then leave the finite mesh with
 // substantial rotation. No body teleport or reset occurs in this control.
 w.SetLinearVelocity(body,{0,-1,0});w.Step(1.f/60.f);Check(!w.LastStepTouchEvents().empty(),"actual front contact was observed");w.SetAngularVelocity(body,{0,50,0});w.SetLinearVelocity(body,{100,0,0});w.Step(1.f/60.f);w.Step(1.f/60.f);auto p=w.GetTransform(body).position;Check(p.x>3,"high-spin finite footprint departure occurs physically");
 w.SetLinearVelocity(body,{0,-10,0});w.Step(1.f/60.f);w.Step(1.f/60.f);p=w.GetTransform(body).position;Check(p.y<-.2,"departed body reaches rear-side free flight");w.SetLinearVelocity(body,{-100,0,0});w.Step(1.f/60.f);w.Step(1.f/60.f);p=w.GetTransform(body).position;Check(std::abs(p.x)<.1,"same generation returns below finite sheet");w.SetLinearVelocity(body,{0,40,0});w.Step(1.f/60.f);Check(w.LastStepStats().impactEvents==0,"expired episode does not block genuine rear reentry");Check(w.GetTransform(body).position.y>.2,"rear-return body passes through finite sheet");
 // Explicit reset invalidates front ownership before an otherwise identical
 // overlap can be mistaken for continuation.
 w.ResetBody(body,{0,.01,0},glm::quat(1,0,0,0));w.SetLinearVelocity(body,{0,-1,0});w.Step(1.f/60.f);w.ResetBody(body,{0,-.02,0},glm::quat(1,0,0,0));w.SetLinearVelocity(body,{0,40,0});w.Step(1.f/60.f);Check(w.LastStepStats().impactEvents==0&&w.GetTransform(body).position.y>.4,"explicit reset does not inherit old front ownership");
 // Destroy/reuse must not alias the generation, even at the same slot.
 auto old=body;w.DestroyBody(body);body=w.CreateDynamicBox({0,-.02,0},{.02,.01,.03},.2f,.4f,0);Check(body.id!=old.id,"destroyed slot has a new generation");w.SetLinearVelocity(body,{0,40,0});w.Step(1.f/60.f);Check(w.LastStepStats().impactEvents==0&&w.GetTransform(body).position.y>.4,"reused slot does not inherit old episode");
 std::cout<<"checks "<<checks<<" failures "<<failures<<'\n';return failures?1:0;}
