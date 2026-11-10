#include "../tools/crash_proof/WheelAssembly.h"
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
unsigned RunCrashWheelAssemblyChecks(){
    unsigned checks=0;auto check=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';};
    for(float sign:{-1.f,1.f}){
        MeshData mesh,brakes;const glm::vec3 center{sign*.9f,-.42f,-1.54f};
        auto orientation=glm::angleAxis(sign*.07f,glm::vec3(0,0,1));
        for(unsigned i=0;i<64;++i)for(unsigned side=0;side<2;++side){
            float a=float(i)*glm::two_pi<float>()/64;
            MeshVertex v;v.position=center+orientation*glm::vec3(side?.16f:-.16f,.37f*std::cos(a),.37f*std::sin(a));
            v.normal=orientation*glm::vec3(0,std::cos(a),std::sin(a));mesh.vertices.push_back(v);
        }
        for(unsigned i=0;i<64;++i){unsigned j=(i+1)%64;for(auto k:{i*2,j*2,i*2+1,j*2,j*2+1,i*2+1})mesh.indices.push_back(k);}
        auto original=mesh;MeshVertex caliper;caliper.position=center+orientation*glm::vec3(.1f,.05f,.2f);brakes.vertices.push_back(caliper);
        auto fit=PrepareCrashWheel(mesh,brakes);
        check(glm::length(fit.center-center)<1e-6f,"wheel fit recovers translated hub center for each side");
        check(glm::dot(fit.axleFrame*glm::vec3(1,0,0),orientation*glm::vec3(1,0,0))>1-1e-6f,"wheel fit recovers cambered axle without reflection");
        check(std::abs(fit.radius-.37f)<1e-6f&&std::abs(fit.width-.32f)<1e-6f,"wheel dimensions come from geometry");
        float error=0,radialError=0;
        for(unsigned i=0;i<mesh.vertices.size();++i){
            auto local=mesh.vertices[i].position;
            error=std::max(error,glm::length(fit.center+fit.axleFrame*local-original.vertices[i].position));
            for(float angle:{-6.1f,-1.f,0.f,1.4f,6.1f}){
                auto spun=glm::angleAxis(angle,glm::vec3(1,0,0))*local;
                radialError=std::max(radialError,std::abs(glm::length(glm::vec2(spun.y,spun.z))-.37f));
            }
        }
        check(error<2e-6f,"rest wheel preparation preserves original geometry including opposite-side orientation");
        check(radialError<2e-6f,"forward and reverse wheel rotation stays concentric at all sampled phases");
        check(glm::length(fit.center+fit.axleFrame*brakes.vertices[0].position-caliper.position)<1e-6f,"nonrotating brake hardware preserves original rest alignment");
    }
    bool rejected=false;try{MeshData a,b;PrepareCrashWheel(a,b);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"missing rotating geometry is rejected rather than inventing an axle");
    check(ResolveCrashPartRole("Vehicle_M_Caliper_High/material/metal", {}) == CrashPartRole::Caliper,
          "stationary caliper is identified even without wheel in its source name");
    check(ResolveCrashPartRole("Vehicle_M_Brake_High/material/disc", {}) == CrashPartRole::Wheel,
          "brake discs rotate with their wheel rather than remaining on the chassis");
    CrashPartRoles roles;std::string error;
    check(ParseCrashPartRoles(R"({"schema":1,"parts":{"opaque-name":"wheel","other-name":"caliper","cabin":"body"}})",roles,error),
          "explicit source identity authoring accepts all three assembly roles");
    check(ResolveCrashPartRole("opaque-name", roles)==CrashPartRole::Wheel &&
          ResolveCrashPartRole("other-name", roles)==CrashPartRole::Caliper &&
          ResolveCrashPartRole("cabin", roles)==CrashPartRole::Body,
          "authored roles override source naming without removing cabin geometry");
    rejected=false;try{ResolveCrashPartRole("unmapped",roles);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"incomplete authored assembly fails instead of silently misclassifying a part");
    const auto previous=roles;
    check(!ParseCrashPartRoles(R"({"schema":1,"parts":{"cabin":"ignore"}})",roles,error)&&roles==previous,
          "invalid role cannot discard geometry or replace a valid assembly map");
    return checks;
}
