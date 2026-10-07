#pragma once
#include "Ragdoll.h"
#include <string>
#include <vector>
// Preview-only geometry fit in skeleton-local metres. Caller explicitly accepts
// the returned mapping via the normal document transaction. No anatomical rules.
bool FitSkeletonChain(const Skeleton&,const RagdollDefinition&,const std::vector<std::string>& joints,float thickness,RagdollDefinition& out,std::string& error);
