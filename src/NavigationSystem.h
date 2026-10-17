#pragma once
#include "NavigationAsset.h"
#include "DebugDraw.h"
#include <memory>
class RuntimeWorld;
struct NavigationFilter {unsigned profile=0;CategoryMask include=kAllCategories,exclude=0;std::map<unsigned,float> costs;};
struct NavigationLocation {glm::vec3 position{0};SceneObjectId surface=0;unsigned area=0;};
struct NavigationCorner {glm::vec3 position{0};SceneObjectId link=0;glm::vec3 linkEnd{0};};
struct NavigationPath {enum class Status {Failed,Partial,Complete};Status status=Status::Failed;std::vector<NavigationCorner> corners;std::vector<unsigned> areas;float distance=0,cost=0;unsigned revision=0;};
struct NavigationAgentState {bool stopped=false,hasDestination=false,onLink=false;glm::vec3 destination{0},steering{0};NavigationPath path;size_t corner=0;float remaining=0,elapsed=0;};
struct NavigationStatistics {size_t surfaces=0,tiles=0,polygons=0,agents=0,obstacles=0;};
// World-owned query/guidance state. It never writes authoritative entity poses.
class NavigationSystem {
public:
 explicit NavigationSystem(ProjectNavigation config={});~NavigationSystem();
 NavigationSystem(const NavigationSystem&)=delete;NavigationSystem& operator=(const NavigationSystem&)=delete;
 const ProjectNavigation& Configuration()const;
 bool LoadSurface(SceneObjectId,const SceneTransform&,std::shared_ptr<const NavigationData>,std::string&,bool publish=true);
 void PublishSurfaces(const std::vector<SceneObjectId>&);
 void DiscardStagedSurface(SceneObjectId);
 void RemoveSurface(SceneObjectId);
 std::optional<NavigationLocation> Sample(glm::vec3,float,const NavigationFilter& = {})const;
 NavigationPath FindPath(glm::vec3,glm::vec3,const NavigationFilter& = {})const;
 std::optional<NavigationLocation> Raycast(glm::vec3,glm::vec3,const NavigationFilter& = {})const;
 bool SetDestination(SceneObjectId,glm::vec3);void ClearDestination(SceneObjectId);
 void RegisterAgent(SceneObjectId id);
 NavigationAgentState* Agent(SceneObjectId);const NavigationAgentState* Agent(SceneObjectId)const;
 bool CompleteLink(SceneObjectId);
 void Update(RuntimeWorld&,float);
 // Detour geometry is surface-local. Retain polygon refs, pending obstacles,
 // link progress and revision while translating only the enclosing world frames.
 void ShiftOrigin(glm::dvec3 delta);
 void Debug(DebugLineList&)const;
 NavigationStatistics Statistics()const;
 unsigned Revision()const;const std::map<SceneObjectId,std::string>& Errors()const;
private:struct Impl;std::unique_ptr<Impl> m;
};
