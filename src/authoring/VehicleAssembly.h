#pragma once
#include "Scene.h"

namespace JudasVehicles {
struct AssemblyRecipe {
    std::string name="Vehicle";
    unsigned axles=2;
    float mass=500, track=1.8f, axleSpacing=1.9f;
    float radius=.35f, width=.24f, travel=.4f, staticSag=.08f;
    bool powered=true;
    std::vector<unsigned> drivenAxles; // empty = all; explicit indices otherwise
    SceneTransform pose;
    SceneObjectId parent=0;
    std::string wheelMesh; // Optional registered Y-axis unit cylinder (r=.5, half-length=1).
    glm::vec3 color{.12f,.65f,.7f};
};
struct AssemblyResult {SceneObjectId root=0;std::vector<SceneObjectId> wheels;};
// Pure scene transaction, shared by the editor, tooling and project tests.
// Emits ordinary bodies, Wheel and Powertrain components, never native driving.
bool CreateAssembly(Scene&,const AssemblyRecipe&,AssemblyResult&,std::string& error);
}
