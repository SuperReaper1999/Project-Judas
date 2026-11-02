#pragma once
#include <array>
#include <exception>

// Uses the FluidWorld test helpers. These controls require
// real geometric exclusion; a no-contact/unchanged-particle shortcut fails.
void TestSphereInitializationContract() {
    std::printf("Sphere initial-overlap direction and energy\n");
    const ConstantGravity zero(glm::vec3(0));
    const glm::quat turn=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3)));
    struct Frame { glm::quat rotation; glm::vec3 translation; };
    const std::array<Frame,4> frames{{
        {glm::quat(1,0,0,0),glm::vec3(0)}, {turn,glm::vec3(0)},
        {-turn,glm::vec3(0)}, {turn,glm::vec3(2,-1,3)}
    }};
    const glm::vec3 ray=glm::normalize(glm::vec3(1,2,-3));
    const std::array<float,8> distances{{0,1e-40f,1e-8f,5e-8f,9.99e-8f,1.001e-7f,2e-7f,1e-4f}};
    for(bool defaults:{false,true})for(const auto& frame:frames)for(const float distance:distances) {
        FluidSettings settings;
        if(!defaults){settings.substeps=1;settings.densityIterations=0;settings.velocitySmoothing=0;}
        FluidWorld fluid(settings);
        const auto point=frame.translation+frame.rotation*(ray*distance);
        fluid.AddParticle(point,glm::vec3(0),1.953125f);
        FluidSphereCollider sphere;
        sphere.owner={73,401};sphere.radius=.3f;
        sphere.previousPose={frame.translation,frame.rotation};sphere.currentPose=sphere.previousPose;
        // The input is the represented point, including any cancellation in
        // translated float coordinates. Every NONZERO radial ray must survive.
        const glm::dvec3 represented(point-frame.translation);
        const glm::dvec3 preferred=glm::dot(represented,represented)>0 ? represented :
            glm::dvec3(frame.rotation*glm::vec3(1,0,0));
        const auto direction=glm::normalize(preferred);
        bool completed=true;
        std::vector<FluidContactImpulse> impulses;
        try{fluid.Step(1.f/60,zero,{}, {sphere},&impulses);}
        catch(const std::exception& error){completed=false;std::printf("    exception: %s\n",error.what());}
        Check(completed,"stationary sphere initial overlap always supplies a defined geometric exit");
        if(!completed)continue;
        const auto& particle=fluid.Particles()[0];
        const glm::dvec3 radial=glm::dvec3(particle.position)-glm::dvec3(frame.translation);
        const bool finite=std::isfinite(particle.position.x)&&std::isfinite(particle.position.y)&&std::isfinite(particle.position.z);
        Check(finite&&glm::dot(radial,radial)>=double(sphere.radius)*sphere.radius,
              "sphere initial overlap resolves outside the real solid");
        Check(glm::dot(glm::normalize(radial),direction)>1-2e-6,
              "nonzero radial rays retain their direction; only the exact centre uses body-local X");
        Check(particle.velocity==glm::vec3(0)&&fluid.GetDiagnostics().kineticEnergy==0&&impulses.empty(),
              "stationary sphere depenetration creates no velocity kinetic energy or contact impulse");
        Check(particle.mass==1.953125f&&fluid.GetDiagnostics().totalMass==1.953125f,
              "sphere depenetration preserves actual fluid mass");
    }
    // q and -q encode precisely the same body orientation. Use exact centre
    // and a genuine small radial direction without translated quantization.
    for(const auto point:{glm::vec3(0),ray*5e-8f}) {
        std::array<glm::vec3,2> final;
        for(int sign=0;sign<2;++sign) {
            FluidSettings settings;settings.substeps=1;settings.densityIterations=0;settings.velocitySmoothing=0;
            FluidWorld fluid(settings);fluid.AddParticle(turn*point,glm::vec3(0),1.953125f);
            const BodyTransform pose{{0,0,0},sign?-turn:turn};
            const FluidSphereCollider sphere{{73,401},pose,pose,.3f};
            bool completed=true;
            try{fluid.Step(1.f/60,zero,{}, {sphere});}catch(const std::exception&){completed=false;}
            Check(completed,"equivalent quaternion sphere initialization completes");
            final[sign]=fluid.Particles()[0].position;
        }
        Check(final[0]==final[1],"sphere initialization is identical for quaternion q and minus q");
    }
}
