#pragma once
#include <string>
#include <glm/glm.hpp>
struct SceneObject;
class Scene;
bool HasSpatialMotionProducer(const SceneObject&);
bool ValidatePlanarDefinition(const SceneObject&,std::string& error);
bool ValidatePlanarScene(const Scene&,bool& hasPlanar,std::string& error);
class PhysicsWorld;
struct BodyHandle;
struct BodyTransform;
bool PlanarCommandAllowed(const PhysicsWorld&,BodyHandle,const BodyTransform&,
                          const glm::vec3* linear=nullptr,const glm::vec3* angular=nullptr);
