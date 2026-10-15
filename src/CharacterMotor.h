#pragma once
#include "PhysicsWorld.h"
#include "GravityField.h"
#include <cmath>
#include <string>

// Geometry/physical settings only. Intent, input, camera and gameplay belong
// to consumers. Capsule's model axis is local +Y, never a world-up axis.
struct CharacterMotorSettings {
    bool enabled=true;
    float radius=.3f, halfHeight=.6f;
    glm::vec3 offset{0};
    float stepHeight=.55f, supportDistance=.15f, skin=.02f, maxSlopeDegrees=50;
    float gravityScale=1, reorientationDegreesPerSecond=120;
    float interactionMass=80, maxPushImpulse=20;
    unsigned collisionLayer=0;
    CategoryMask collisionMask=kAllCategories, requiredTags=0, excludedTags=0;
};
inline bool ValidCharacterMotor(const CharacterMotorSettings& s,std::string& error) {
    const float values[]={s.radius,s.halfHeight,s.stepHeight,s.supportDistance,s.skin,s.maxSlopeDegrees,s.gravityScale,s.reorientationDegreesPerSecond,s.interactionMass,s.maxPushImpulse,s.offset.x,s.offset.y,s.offset.z};
    for(float v:values)if(!std::isfinite(v)){error="nonfinite character motor setting";return false;}
    if(s.radius<=0||s.halfHeight<0||s.stepHeight<0||s.supportDistance<0||s.skin<=0||s.skin>=s.radius||s.maxSlopeDegrees<0||s.maxSlopeDegrees>=90||s.reorientationDegreesPerSecond<0||s.interactionMass<0||s.maxPushImpulse<0||s.collisionLayer>=64){error="invalid character motor dimensions/tolerances/filter";return false;}return true;
}
struct CharacterMotionResult {
    glm::vec3 displacement{0},velocity{0},supportNormal{0},supportVelocity{0},gravity{0},up{0,1,0};
    BodyHandle support;
    bool supported=false,collided=false,recovered=false,stepped=false;
};
// Shared sweep/slide primitive. Legacy velocity seeding remains only on the
// compatibility caller; new motors use finite bounded impulse transfer.
void ResolveCharacterSlide(PhysicsWorld&,glm::vec3& center,const glm::quat&,glm::vec3 displacement,
    glm::vec3& velocity,const CharacterMotorSettings&,const PhysicsQueryFilter*,bool movingGeometry,
    bool legacyPush,bool& collided,BodyHandle observer={});
class CharacterMotor {
public:
    void Persist(class SaveArchive&);
    CharacterMotorSettings settings;
    glm::vec3 position{0},velocity{0};
    glm::quat orientation{1,0,0,0};
    glm::vec3 acceleration{0}; // additional acceleration; reset after each step
    PhysicsQueryFilter filter;
    BodyHandle observationBody; // massless geometry registration owned by RuntimeWorld
    CharacterMotionResult result;
    void Reset(glm::vec3 p,glm::quat q);
    void ShiftOrigin(glm::dvec3 delta){position=glm::vec3(glm::dvec3(position)-delta);supportOrigin=glm::vec3(glm::dvec3(supportOrigin)-delta); }
    // Once after the ordinary rigid step. Velocity is WORLD-space control
    // velocity INCLUDING the last support velocity; support changes are
    // composed once. A departing motor retains its inherited world motion.
    void Step(PhysicsWorld&,const GravityField&,float dt);
private:
    bool followingSupport=false;
    glm::vec3 supportOrigin{0};
};
