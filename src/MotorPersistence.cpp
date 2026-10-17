#include "CharacterMotor.h"
#include "SaveArchive.h"
void CharacterMotor::Persist(SaveArchive& a){
 a(position,velocity,orientation,acceleration,followingSupport,supportOrigin);
 a(result.displacement,result.velocity,result.supportNormal,result.supportVelocity,result.gravity,result.up,result.supported,result.collided,result.recovered,result.stepped);
 // Supporting/ignored bodies are stable entity keys in the owning participant,
 // never native body-slot IDs here. Settings use normal Scene serialization.
 if(a.reading){displacementIntent={0,0,0};orientationIntent.reset();std::string error;a.Require(ValidCharacterMotor(settings,error),"invalid restored motor");}
}
