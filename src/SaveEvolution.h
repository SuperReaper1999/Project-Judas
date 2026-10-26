#pragma once
#include "SaveStorage.h"
// Bounded, declarative, project-owned migrations. No world, VM or native cache
// pointers enter this service. Failure leaves both input and output untouched.
struct SaveEvolutionIdentity {std::string compatibility;uint32_t version=0;};
bool ValidateSaveEvolution(const std::string& policy,std::string& error);
bool SaveEvolutionCurrent(const std::string& policy,SaveEvolutionIdentity&,std::string& error);
bool EvolveSave(const std::string& policy,const std::string& currentContent,
 const std::string& currentMods,const GameSnapshot& input,bool allowMigration,
 GameSnapshot& output,std::string& report,std::string& error);

// Describes identity differences without reading participant payloads or migrating.
// Empty means the identities match; it does not certify native payload validity.
std::string DescribeSaveCompatibility(const GameSnapshot& saved,
 const std::string& currentContent,const SaveEvolutionIdentity& current,
 const std::string& currentMods);
