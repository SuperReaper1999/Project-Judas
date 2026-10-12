#pragma once
#include "Project.h"
#include "AssetDatabase.h"
#include "Scene.h"
#include <functional>
#include <vector>
struct NavigationDependency {
 std::string scene, reason; SceneObjectId surface=0;
 bool stale=false, readOnly=false;
};
// Checks every registered owned scene and every explicitly published kit scene.
// No inference of unpublished/private base content. Empty changedAsset checks all.
bool InspectNavigationDependencies(const Project&,const AssetDatabase&,const AssetId& changedAsset,
 std::vector<NavigationDependency>&,std::string& error);
// Explicit operation on saved owned scenes. Stages the complete batch using the
// normal baker and rollback-capable authoring publication; cancellation before
// publication preserves all previous assets/scenes. Published kit stays read-only.
bool RebuildNavigationDependencies(const Project&,AssetDatabase&,const std::vector<NavigationDependency>&,
 std::string& error,const std::function<bool()>& cancelled={});
