#include "deformables/PanelBending.h"
#include "SaveArchive.h"
#include "Deformable.h"
#include "UniformGravity.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
unsigned RunPanelBendingChecks(){
    unsigned checks=0;auto check=[&](bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';};
    using V=glm::dvec3;
    V p[4]={{0,0,0},{1,0,0},{.2,1,.4},{.7,-.8,.1}},gradient[4];double angle;
    check(PanelBending::AngleGradient(p,angle,gradient),"signed physical hinge has a valid analytic gradient");
    double maximum=0;V force(0),torque(0);
    for(int i=0;i<4;++i){force+=gradient[i];torque+=glm::cross(p[i],gradient[i]);for(int k=0;k<3;++k){
        double value=p[i][k],positive,negative;V scratch[4];p[i][k]=value+1e-6;PanelBending::AngleGradient(p,positive,scratch);p[i][k]=value-1e-6;PanelBending::AngleGradient(p,negative,scratch);p[i][k]=value;
        maximum=std::max(maximum,std::abs((positive-negative)/2e-6-gradient[i][k]));}}
    check(maximum<1e-7,"hinge gradient matches independent central finite differences");
    check(glm::length(force)<1e-12&&glm::length(torque)<1e-12,"hinge material impulses have zero net internal force and torque");
    std::vector<V> rest={{0,0,0},{1,0,0},{.2,1,0},{.7,-.8,0}};
    PanelBending bend;bend.Initialize(rest,{{0,1,2},{1,0,3}});
    V flat[4];for(int i=0;i<4;++i)flat[i]=rest[i];PanelBending::AngleGradient(flat,angle,gradient);
    check(std::abs(angle)<1e-12&&glm::length(gradient[2])>.5,"flat material has first-order bending stiffness rather than a distance-spring null mode");
    auto positions=rest;positions[2].z=.5;
    V loaded[4];for(int i=0;i<4;++i)loaded[i]=positions[i];double before;PanelBending::AngleGradient(loaded,before,gradient);
    bend.BeginSubstep();for(int i=0;i<10;++i)bend.Solve(positions,{0,0,1,1},1./60);
    for(int i=0;i<4;++i)loaded[i]=positions[i];
    PanelBending::AngleGradient(loaded,angle,gradient);
    check(std::abs(angle)<std::abs(before),"elastic physical hinge restores a displaced panel toward its rest curvature");
    positions=rest;positions[2].z=1;
    bend.Plastic(positions,1./60);
    check(bend.hinges[0].plastic>0&&bend.hinges[0].work>0,"over-yield local bending creates permanent curvature and irreversible work");
    const double plastic=bend.hinges[0].plastic,work=bend.hinges[0].work;
    bend.Plastic(positions,1./60);
    check(std::abs(bend.hinges[0].plastic-plastic)<1e-12&&std::abs(bend.hinges[0].work-work)<1e-12,"return mapping does not creep under an unchanged solved load");
    SaveArchive saved;bend.Persist(saved);PanelBending restored;restored.Initialize(rest,{{0,1,2},{1,0,3}});SaveArchive reader(saved.bytes);restored.Persist(reader);reader.Finish();
    SaveArchive roundtrip;restored.Persist(roundtrip);check(saved.bytes==roundtrip.bytes,"physical hinge plastic state survives exact participant roundtrip");
    restored.Reset();check(restored.hinges[0].plastic==0&&restored.hinges[0].work==0&&restored.hinges[0].rest==restored.hinges[0].authored,"explicit material reset clears permanent strain and restores authored curvature");
    bool rejected=false;try{PanelBending invalid;invalid.stiffness=0;invalid.Initialize(rest,{{0,1,2},{1,0,3}});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"invalid panel stiffness is rejected");
    {
        PhysicsWorld physics;check(physics.Init(),"released-target sleep fixture initializes physics");
        auto sheet=std::make_shared<DeformableAsset>(MakeDeformableSheet(2,2,1,1));
        DeformableSettings settings;settings.selfContact=false;DeformableAttachment pin;pin.group="top";settings.attachments.push_back(pin);
        DeformableInstance released,active;released.Initialize(sheet,settings,glm::dmat4(1),1);active.Initialize(sheet,settings,glm::dmat4(1),2);released.Release("top");
        UniformGravity gravity({0,0,0});DeformableTarget target;target.valid=true;
        for(unsigned step=0;step<100;++step){target.previous=target.current;target.current[3].x+=.001;released.Step(1./60,physics,gravity,{target});active.Step(1./60,physics,gravity,{target});}
        check(released.sleeping&&!active.sleeping,"retired attachment targets cannot keep a detached material awake while active targets still wake it");
    }
    V invalidPoints[4]={p[0],p[1],p[2],p[3]};invalidPoints[0].x=std::numeric_limits<double>::quiet_NaN();
    check(!PanelBending::AngleGradient(invalidPoints,angle,gradient),"nonfinite panel geometry is rejected by the material gradient");
    bool badTopology=false;const auto retained=bend.hinges.size();try{bend.Initialize({{0,0,0},{1,0,0},{2,0,0},{3,0,0}},{{0,1,2},{1,0,3}});}catch(const std::invalid_argument&){badTopology=true;}
    check(badTopology&&bend.hinges.size()==retained,"rejected material preparation preserves the existing hinge state");
    return checks;
}
