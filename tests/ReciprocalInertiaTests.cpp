// Independent tensor construction and actual public ContactSolver controls.
#include "RigidBody.h"
#include "ContactSolver.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
unsigned checks=0,failures=0;
void Check(bool pass,const char* label){++checks;failures+=!pass;if(!pass)std::printf("FAIL %s\n",label);}
bool Bits(float a,float b){return std::memcmp(&a,&b,sizeof(a))==0;}
bool Bits(const glm::mat3& a,const glm::mat3& b){
    for(unsigned c=0;c<3;++c)for(unsigned r=0;r<3;++r)if(!Bits(a[c][r],b[c][r]))return false;
    return true;
}
// Preserved BEFORE expression. Never route a negative-control tensor through
// the method under test: its implementation is intentionally being corrected.
glm::mat3 OriginalWorld(const RigidBody& body){
    const glm::mat3 R=glm::mat3_cast(body.orientation);
    return R*body.inverseInertiaLocal*glm::transpose(R);
}
using ReferenceMatrix=std::array<std::array<long double,3>,3>;
ReferenceMatrix IndependentWorld(const RigidBody& body){
    const glm::mat3 R=glm::mat3_cast(body.orientation);ReferenceMatrix out{};
    for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column)
        for(unsigned k=0;k<3;++k)for(unsigned l=0;l<3;++l)
            out[row][column]+=(long double)R[k][row]*(long double)body.inverseInertiaLocal[l][k]*(long double)R[l][column];
    return out;
}
void CheckAngularResponse(const RigidBody& actual,const glm::vec3& before,
                          const ReferenceMatrix& reference,const glm::vec3& effort,
                          long double duration,const char* label){
    for(unsigned row=0;row<3;++row){
        long double expected=before[row],scale=std::abs((long double)before[row]);
        for(unsigned column=0;column<3;++column){
            const long double term=reference[row][column]*(long double)effort[column]*duration;
            expected+=term;scale+=std::abs(term);
        }
        const long double underflow=64*(long double)std::numeric_limits<float>::denorm_min();
        Check(std::abs((long double)actual.angularVelocity[row]-expected)<=
              64*std::numeric_limits<float>::epsilon()*scale+underflow,label);
    }
}

contact_block::Matrix Gram(const glm::mat3& inertia,const std::array<glm::vec3,4>& offsets){
    contact_block::Matrix K{};
    for(unsigned i=0;i<4;++i)for(unsigned j=i;j<4;++j){
        const auto ai=glm::cross(glm::dvec3(offsets[i]),glm::dvec3(0,1,0));
        const auto aj=glm::cross(glm::dvec3(offsets[j]),glm::dvec3(0,1,0));
        K[i][j]=K[j][i]=1+glm::dot(ai,glm::dmat3(inertia)*aj);
    }
    return K;
}
bool Valid(const contact_block::Matrix& K){
    contact_block::NormalPatch patch;patch.Prepare(K,4);
    contact_block::Vector answer{};return patch.Solve({},{},answer);
}
void Construction(){
    RigidBody body;
    body.inverseMass=1;
    body.orientation=glm::angleAxis(.7f,glm::normalize(glm::vec3(1,2,3)));
    body.inverseInertiaLocal=glm::mat3(0);
    body.inverseInertiaLocal[0][0]=6;body.inverseInertiaLocal[1][1]=3;body.inverseInertiaLocal[2][2]=12;
    body.inverseInertiaLocal[0][1]=body.inverseInertiaLocal[1][0]=.25f;
    body.inverseInertiaLocal[0][2]=body.inverseInertiaLocal[2][0]=-.125f;
    body.inverseInertiaLocal[1][2]=body.inverseInertiaLocal[2][1]=.5f;
    const auto physical=body.InverseInertiaWorld(),R=glm::mat3_cast(body.orientation);
    for(unsigned c=0;c<3;++c)for(unsigned r=0;r<3;++r){
        Check(Bits(physical[c][r],physical[r][c]),"published tensor is exactly reciprocal");
        long double reference=0,magnitude=0;
        for(unsigned k=0;k<3;++k)for(unsigned l=0;l<3;++l){
            const long double term=(long double)R[k][r]*(long double)body.inverseInertiaLocal[l][k]*(long double)R[l][c];
            reference+=term;magnitude+=std::abs(term);
        }
        Check(std::abs((long double)physical[c][r]-reference)<=
              2*std::numeric_limits<float>::epsilon()*magnitude,
              "native tensor agrees with independent long-double physical product");
    }
    for(glm::dvec3 v:{glm::dvec3(1,0,0),glm::dvec3(0,1,0),glm::dvec3(0,0,1),
                      glm::dvec3(1,.5,-.25),glm::dvec3(-.125,2,1)})
        Check(glm::dot(v,glm::dmat3(physical)*v)>0,"authored SPD mobility retains positive quadratic work");
    // Exact axis permutation represents the SAME local tensor in another basis.
    const glm::quat basis(.5f,.5f,.5f,.5f);
    const glm::mat3 P=glm::mat3_cast(basis);
    RigidBody permuted=body;
    permuted.orientation=body.orientation*glm::conjugate(basis);
    permuted.inverseInertiaLocal=P*body.inverseInertiaLocal*glm::transpose(P);
    const auto equivalent=permuted.InverseInertiaWorld();
    for(unsigned c=0;c<3;++c)for(unsigned r=0;r<3;++r)
        Check(std::abs(double(equivalent[c][r])-double(physical[c][r]))<=
              64*std::numeric_limits<float>::epsilon()*
              std::max(std::abs(double(equivalent[c][r])),std::abs(double(physical[c][r]))),
              "local-basis permutation preserves the physical world operator to native accuracy");
    RigidBody invalid=body;
    invalid.inverseInertiaLocal[0][1]+=.125f;
    Check(Bits(invalid.InverseInertiaWorld(),OriginalWorld(invalid)),
          "nonsymmetric authored tensor is not silently repaired");
    invalid=body;invalid.inverseInertiaLocal[0][0]=std::numeric_limits<float>::quiet_NaN();
    Check(Bits(invalid.InverseInertiaWorld(),OriginalWorld(invalid)),
          "nonfinite authored tensor follows exactly the original unsupported path");
    invalid=RigidBody{};invalid.inverseInertiaLocal=glm::mat3(1);invalid.inverseInertiaLocal[0][0]=-1;
    Check(invalid.InverseInertiaWorld()[0][0]==-1,
          "negative authored mobility is never eigenvalue-clamped or regularized");
}
void ActualPatches(){
    unsigned oldRejected=0,newAccepted=0;
    for(unsigned sample=0;sample<64;++sample){
        RigidBody body,ground;
        body.inverseMass=1;body.inverseInertiaLocal=SolidBoxInverseInertia(1,{.2f,.3f,.4f});
        body.position={0,.3f,0};body.linearVelocity={0,-1,0};
        body.orientation=glm::angleAxis(float(sample+1)*.03125f,glm::vec3(0,1,0));
        const auto R=ContactRotation(body.orientation);
        const std::array<glm::dvec3,4> local{{{-.2f,-.3f,-.4f},{.2f,-.3f,-.4f},
                                            {.2f,-.3f,.4f},{-.2f,-.3f,.4f}}};
        std::array<glm::vec3,4> offsets{};
        ContactSolver solver;solver.EnableBlockNormals(true);
        for(unsigned i=0;i<4;++i){
            Contact c;c.hit=true;c.normal={0,1,0};c.preciseNormal={0,1,0};
            c.hasLocalAnchors=true;c.signedSeparation=0;c.localAnchorA=local[i];
            const auto world=glm::dvec3(body.position)+R*local[i];
            c.localAnchorB=world;c.point=glm::vec3(world);
            offsets[i]=glm::vec3(R*local[i]);
            solver.AddContact(body,ground,c,0,0);
        }
        const auto oldK=Gram(OriginalWorld(body),offsets);
        const auto newK=Gram(body.InverseInertiaWorld(),offsets);
        const bool oldValid=Valid(oldK),newValid=Valid(newK);
        oldRejected+=!oldValid;newAccepted+=newValid;
        if(!oldValid)
            std::printf("NATIVE_FALSE_PSD sample=%u angle=%a old_IAxz=%a old_IAzx=%a\n",sample,
                double(float(sample+1)*.03125f),double(OriginalWorld(body)[0][2]),
                double(OriginalWorld(body)[2][0]));
        Check(newValid,"four-point cyclic patch uses an independently valid PSD response");
        solver.Prepare(0);
        Check(Bits(solver.Constraints().front().inverseInertiaA,body.InverseInertiaWorld()),
              "actual ContactSolver frame uses coherent corrected mobility for impulse application");
        const auto before=contact_block::diagnostics;
        solver.SolveVelocities(1);
        Check(contact_block::diagnostics.solves==before.solves+1&&
              contact_block::diagnostics.fallbacks==before.fallbacks,
              "actual four-point solver no longer degrades to scalar fallback on this patch");
        double load=0;
        for(const auto& row:solver.Constraints()){
            Check(row.normalImpulse>=0&&row.tangentImpulse==glm::vec3(0),
                  "all actual normal reactions remain unilateral without invented friction");
            load+=row.normalImpulse;
        }
        Check(std::abs(load-1)<64*std::numeric_limits<float>::epsilon(),
              "real inelastic normal impulse balances the incoming unit momentum");
        Check(glm::length(body.linearVelocity)<64*std::numeric_limits<float>::epsilon()&&
              glm::length(body.angularVelocity)<64*std::numeric_limits<float>::epsilon(),
              "actual represented normal response stops without artificial torque");
    }
    Check(oldRejected>0,"literal old native tensor independently reproduces a real four-point PSD failure");
    // The preserved GCC/Linux run rejected 19/64. The old float expression's
    // exact count may vary with compiler association; the physical contract is
    // that every corrected patch succeeds, checked independently below.
    Check(newAccepted==64,"corrected coherent construction handles all unchanged cyclic fixtures");
    std::printf("cyclic fixtures: native rejected %u/64, coherent accepted %u/64\n",oldRejected,newAccepted);
}
void DegenerateAndInvalidInputs(){
    RigidBody body;body.inverseMass=1;
    body.inverseInertiaLocal=glm::mat3(0);
    Check(Bits(body.InverseInertiaWorld(),glm::mat3(0)),"zero local inertia is not invented or regularized");
    body.inverseInertiaLocal=glm::mat3(0);body.inverseInertiaLocal[0][0]=1;body.inverseInertiaLocal[2][2]=2;
    Check(Bits(body.InverseInertiaWorld(),body.inverseInertiaLocal),"singular locked-axis tensor remains singular exactly at identity");
    body.inverseInertiaLocal[1][1]=-3;
    Check(Bits(body.InverseInertiaWorld(),body.inverseInertiaLocal),"symmetric indefinite local tensor retains its negative eigenvalue");
    body.orientation=glm::quat(1.5f,.25f,-.5f,.75f);
    body.inverseInertiaLocal=glm::mat3(1);body.inverseInertiaLocal[0][1]=.25f;
    Check(Bits(body.InverseInertiaWorld(),OriginalWorld(body)),"asymmetric authored local tensor preserves literal native unsupported path");
    body.inverseInertiaLocal=glm::mat3(1);body.inverseInertiaLocal[2][2]=std::numeric_limits<float>::infinity();
    Check(Bits(body.InverseInertiaWorld(),OriginalWorld(body)),"nonfinite authored tensor is never silently replaced by valid mobility");
    body.inverseInertiaLocal=glm::mat3(1);body.orientation.x=std::numeric_limits<float>::quiet_NaN();
    Check(Bits(body.InverseInertiaWorld(),OriginalWorld(body)),"nonfinite native rotation preserves old unsupported expression");
    body.orientation=glm::quat(2,1,-1,1);body.inverseInertiaLocal=glm::mat3(std::numeric_limits<float>::max());
    Check(Bits(body.InverseInertiaWorld(),OriginalWorld(body)),"float publication overflow follows old expression without clamping");
}
void NativeQuaternionAndDirectApplications(){
    RigidBody body;body.inverseMass=.5f;body.orientation=glm::quat(1.5f,.25f,-.5f,.75f);
    body.inverseInertiaLocal=glm::mat3(0);
    body.inverseInertiaLocal[0][0]=6;body.inverseInertiaLocal[1][1]=3;body.inverseInertiaLocal[2][2]=12;
    body.inverseInertiaLocal[0][1]=body.inverseInertiaLocal[1][0]=.25f;
    body.inverseInertiaLocal[0][2]=body.inverseInertiaLocal[2][0]=-.125f;
    body.inverseInertiaLocal[1][2]=body.inverseInertiaLocal[2][1]=.5f;
    const auto reference=IndependentWorld(body);const auto actual=body.InverseInertiaWorld();
    const glm::mat3 R=glm::mat3_cast(body.orientation);
    for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column){
        long double scale=0;
        for(unsigned k=0;k<3;++k)for(unsigned l=0;l<3;++l)
            scale+=std::abs((long double)R[k][row]*(long double)body.inverseInertiaLocal[l][k]*(long double)R[l][column]);
        Check(std::abs((long double)actual[column][row]-reference[row][column])<=
              2*std::numeric_limits<float>::epsilon()*scale,
              "deliberately nonunit quaternion retains the independent NATIVE float rotation mapping");
        Check(Bits(actual[column][row],actual[row][column]),"nonunit native rotation still publishes exactly reciprocal mobility");
    }
    RigidBody normalized=body;normalized.orientation=glm::normalize(normalized.orientation);
    Check(!Bits(actual,normalized.InverseInertiaWorld()),"normalizing the quaternion is a demonstrably different rejected control");
    const glm::vec3 incoming(.125f,-.25f,.5f),impulse(.25f,-.5f,.125f),torque(-.5f,.125f,.25f);
    for(float scale:{1.f,.0009765625f,1024.f}){
        RigidBody applied=body;applied.angularVelocity=incoming*scale;
        const auto before=applied.angularVelocity;
        applied.ApplyAngularImpulse(impulse*scale);
        CheckAngularResponse(applied,before,reference,impulse*scale,1,
            "actual direct angular impulse agrees with independent native-rotation physical tensor");
        RigidBody kicked=body;kicked.angularVelocity=incoming*scale;kicked.linearVelocity={1,2,3};
        kicked.torqueAccumulator=torque*scale;
        const float dt=1.f/60;
        IntegrateRigidBodyVelocity(kicked,dt);
        CheckAngularResponse(kicked,incoming*scale,reference,torque*scale,(long double)dt,
            "actual torque kick agrees with independent native-rotation physical tensor");
        Check(kicked.torqueAccumulator==glm::vec3(0)&&kicked.forceAccumulator==glm::vec3(0),
              "shared tensor publication preserves force/torque accumulator ownership");
        Check(kicked.linearVelocity==glm::vec3(1,2,3),"angular correction does not alter independent linear motion");
    }
    RigidBody immovable=body;immovable.inverseMass=0;immovable.angularVelocity=incoming;
    immovable.ApplyAngularImpulse(impulse);
    Check(immovable.angularVelocity==incoming,"static authority still rejects direct angular impulse");
    immovable.torqueAccumulator=torque;IntegrateRigidBodyVelocity(immovable,1.f/60);
    Check(immovable.angularVelocity==incoming&&immovable.torqueAccumulator==glm::vec3(0),
          "static torque kick preserves velocity and clears only owned accumulators");
}
}
int main(){Construction();ActualPatches();DegenerateAndInvalidInputs();NativeQuaternionAndDirectApplications();
    std::printf("shared reciprocal inertia: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
