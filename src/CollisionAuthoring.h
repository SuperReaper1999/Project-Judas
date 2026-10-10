#pragma once
#include "Scene.h"
class ResourceManager;
bool ValidateCollisionPlacement(const SceneObject&,std::string&);
bool ExtendedCompound(const SceneBodyComponent&);
bool ResolveBodyCollision(const SceneBodyComponent&,ResourceManager*,Shape&,std::string&);

bool ValidateCollisionFluid(const SceneObject&,bool legacyParticleWater,std::string&);
