#include "SaveStorage.h"
#include "SceneFingerprint.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <sys/file.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
int checks=0,failures=0;void Check(bool ok,const char* label){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",label);}
// Independently construct the documented container with a correct checksum.
// This bypasses only encoder validation so decoder negatives reach the actual
// participant-count guard, rather than failing an unrelated checksum/header.
std::string EnvelopeFixture(const GameSnapshot& source,bool includeDiagnostic=true){
 auto participants=source.participants;SaveArchive payload;payload(participants);
 SaveArchive header;uint32_t version=2;auto project=source.project,scene=source.scene,content=source.content,name=source.displayName,metadata=source.metadata,build=source.diagnosticBuild;
 auto timestamp=source.timestamp;auto gameVersion=source.gameVersion;auto hash=SceneFingerprintSha256(payload.bytes);
 header(version,project,scene,content,name,metadata,timestamp,gameVersion,hash);
 if(includeDiagnostic)header(build);
 SaveArchive envelope;std::string magic="JudasGameSave";envelope(magic,header.bytes,payload.bytes);return envelope.bytes;
}
}
int main(int argc,char** argv){namespace fs=std::filesystem;fs::path root=argc>1?argv[1]:".cache/m61-storage";if(root.filename().string().find("m61-storage")!=0){std::puts("Use an isolated m61-storage* directory");return 2;}fs::remove_all(root);fs::create_directories(root);SaveStorage store(root.string());GameSnapshot snapshot;snapshot.project="test-project";snapshot.scene="Scenes/main.judas";snapshot.content=SceneFingerprintSha256("fixture");snapshot.displayName="日本語 — résumé";snapshot.timestamp=1;snapshot.participants["fixture"]={1,"data"};std::string error;GameSnapshot loaded;bool recovered=false;
 Check(snapshot.diagnosticBuild=="unknown","an unrecorded build diagnostic defaults to unknown rather than inventing a milestone");
 auto historical=snapshot;historical.diagnosticBuild="sentinel previous output";
 Check(DecodeGameSnapshot(EnvelopeFixture(snapshot,false),historical,error)&&historical.diagnosticBuild=="unknown"&&historical.participants.at("fixture").data=="data",
       "independent old header without build diagnostic restores unknown and preserves participant bytes");
 auto explicitBuild=snapshot;explicitBuild.diagnosticBuild="fixture exact build identity";GameSnapshot explicitRoundTrip;
 Check(DecodeGameSnapshot(EncodeGameSnapshot(explicitBuild),explicitRoundTrip,error)&&explicitRoundTrip.diagnosticBuild==explicitBuild.diagnosticBuild,
       "an explicitly recorded build diagnostic round-trips without becoming a compatibility gate");
 size_t encodedBytes=123;
 Check(store.Write("one",snapshot,error,false,&encodedBytes),"first slot durably commits");
 Check(encodedBytes==EncodeGameSnapshot(snapshot).size()&&encodedBytes==fs::file_size(root/"one.save"),"write reports exact encoded bytes without a second encoding by caller");
 encodedBytes=123;Check(!store.Write("../invalid",snapshot,error,false,&encodedBytes)&&encodedBytes==123,"early validation failure does not invent an encoded size");
Check(store.Read("one",loaded,recovered,error)&&!recovered&&loaded.displayName==snapshot.displayName,"Unicode metadata round trip");snapshot.timestamp=2;Check(store.Write("two",snapshot,error),"independent second slot");Check(store.List(snapshot.project,snapshot.content,error).size()==2,"bounded metadata listing");Check(!store.Write("../escape",snapshot,error)&&!fs::exists(root.parent_path()/"escape.save"),"slot path traversal rejected");
 snapshot.timestamp=3;Check(store.Write("one",snapshot,error),"overwrite publishes previous-good first");std::ofstream(root/"one.save",std::ios::binary|std::ios::trunc)<<"broken";Check(store.Read("one",loaded,recovered,error)&&recovered&&loaded.timestamp==1,"corruption recovers previous committed generation explicitly");Check(store.Write("one",snapshot,error),"corrupt current repaired without destroying previous good");
 store.boundary=[](const char* stage){if(std::string(stage)=="file-synced")throw std::runtime_error("controlled write failure");};snapshot.timestamp=4;Check(!store.Write("one",snapshot,error),"controlled prepublication failure reported");store.boundary={};Check(store.Read("one",loaded,recovered,error)&&loaded.timestamp==3,"failed overwrite leaves current valid");
 auto payload=EncodeGameSnapshot(snapshot);payload.back()^=1;Check(!DecodeGameSnapshot(payload,loaded,error),"payload checksum detects corruption");Check(!DecodeGameSnapshot(EncodeGameSnapshot(snapshot).substr(0,30),loaded,error),"truncation rejected");SaveArchive archive;uint32_t huge=0xffffffff;archive(huge);std::string value;try{SaveArchive read(archive.bytes);read(value);Check(false,"oversized string rejected before allocation");}catch(...){Check(true,"oversized string rejected before allocation");}
 int fd=open((root/".lock").c_str(),O_RDWR);flock(fd,LOCK_EX|LOCK_NB);Check(!store.Write("one",snapshot,error)&&error.find("busy")!=std::string::npos,"concurrent process lock conflict explicit");flock(fd,LOCK_UN);close(fd);
 for(const char* stage:{"temporary","file-synced","previous-committed","published","directory-synced"}){pid_t child=fork();if(!child){SaveStorage isolated(root.string());isolated.boundary=[stage](const char* point){if(std::string(point)==stage)_exit(77);};std::string e;isolated.Write("one",snapshot,e);_exit(0);}int status=0;waitpid(child,&status,0);Check(store.Read("one",loaded,recovered,error)&&(loaded.timestamp==3||loaded.timestamp==4),"controlled child termination retains recoverable committed generation");}
 Check(store.Delete("one",error)&&!store.Read("one",loaded,recovered,error),"delete selected slot includes recovery generation");Check(store.Read("two",loaded,recovered,error)&&loaded.timestamp==2,"delete preserves other slot");std::ofstream(root/".tmp-abandoned")<<"partial";Check(store.List(snapshot.project,snapshot.content,error).size()==1,"abandoned temporary not advertised as slot");
 auto newer=EncodeGameSnapshot(snapshot);SaveArchive env(newer);std::string magic,header,data;env(magic,header,data);header[0]=3;SaveArchive output;output(magic,header,data);Check(!DecodeGameSnapshot(output.bytes,loaded,error),"unknown/newer container rejects");header[0]=1;SaveArchive older;older(magic,header,data);Check(DecodeGameSnapshot(older.bytes,loaded,error)&&loaded.timestamp==4000,"explicit seconds-to-milliseconds envelope migration retains semantic records");
 fs::create_directory(root/"real");fs::create_directory_symlink(root/"real",root/"redirect");SaveStorage escaped((root/"redirect"/"nested").string());Check(!escaped.Write("safe",snapshot,error),"intermediate directory symlink rejected");fs::create_symlink("two.save",root/"link.save");Check(!store.Write("link",snapshot,error)&&!store.Delete("link",error),"slot symlink rejected for write/delete");
 store.boundary=[](const char* stage){if(std::string(stage)=="published")throw std::runtime_error("controlled post-rename sync failure");};Check(!store.Write("late",snapshot,error)&&error.find("durability uncertain")!=std::string::npos,"post-rename uncertainty reported honestly");store.boundary={};Check(store.Read("late",loaded,recovered,error),"late publication remains validated readable state");
 // M95 observation state extends the optional composition to 22 participants, matching
 // SaveService's world schema. This verifies the generic envelope only; the
 // opaque fixture bytes do not pretend to be complete subsystem save records.
 for(unsigned count:{19u,20u,21u,22u}){
  auto many=snapshot;many.participants.clear();for(unsigned i=0;i<count;++i)many.participants["participant-"+std::to_string(i)]={1,"opaque fixture "+std::to_string(i)};
  const auto slot="participants-"+std::to_string(count);
  const bool wrote=store.Write(slot,many,error);std::printf("PARTICIPANTS count=%u write=%s diagnostic=%s\n",count,wrote?"accepted":"rejected",error.c_str());
  Check(wrote,("supported "+std::to_string(count)+"-participant envelope durably encodes").c_str());
  GameSnapshot decoded;const bool read=DecodeGameSnapshot(EnvelopeFixture(many),decoded,error);
  Check(read&&decoded.participants.size()==count&&EncodeGameSnapshot(decoded)==EncodeGameSnapshot(many),("independent "+std::to_string(count)+"-participant envelope decodes and round-trips exactly").c_str());
 }
 auto tooMany=snapshot;tooMany.participants.clear();for(unsigned i=0;i<23;++i)tooMany.participants["participant-"+std::to_string(i)]={1,"fixture"};
 bool oversizedRejected=false;try{EncodeGameSnapshot(tooMany);}catch(const std::exception&){oversizedRejected=true;}
 Check(oversizedRejected,"23-participant envelope remains outside the bounded supported schema");
 auto unchanged=snapshot;unchanged.project="preserve-output-on-failure";const auto before=EncodeGameSnapshot(unchanged);
 Check(!DecodeGameSnapshot(EnvelopeFixture(tooMany),unchanged,error)&&EncodeGameSnapshot(unchanged)==before,"oversized checksum-valid decoder input rejects atomically without mutating output");
 std::printf("SUMMARY checks=%d failures=%d\n",checks,failures);return failures?1:0;}
