#pragma once
#include "ModelImport.h"
#include "JobSystem.h"
#include <atomic>
// The recipe is author-owned named JSON. Work runs on the existing job system;
// accepted self-contained asset publication remains an explicit outer operation.
struct ModelCookTask {
 std::atomic<bool> cancel{false},done{false};std::atomic<unsigned> progress{0};
 JobHandle job;
 bool success=false,unchanged=false;std::string error,recipe,output,assetId,temporary;
 ModelImportReport report;
 std::shared_ptr<const MeshData> preview;
 // CLI/build tools need no decoded unchanged preview; editor defaults to one.
 bool previewRequired=true,receiptHit=false;unsigned decodedProducts=0;
 std::string importRecord,outputHash,verifiedOutputStamp;
 std::map<std::string,std::string> verifiedInputStamps;
 ~ModelCookTask();
 std::function<bool()> workerCancelled;
};
std::shared_ptr<ModelCookTask> QueueModelImport(JobSystem&,const std::string& recipe);
bool CookModelRecipe(const std::string&,ModelCookTask&);
bool PublishModelImport(ModelCookTask&,std::string& error);
bool CreateModelRecipe(const std::string& projectRoot,const std::string& source,const std::string& relativeOutput,std::string& recipe,std::string& error);
