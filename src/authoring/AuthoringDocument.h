#pragma once
#include <string>
#include <cstdint>
#include <vector>
// Shared source-editor/CLI file contract. Typed data still belongs to its normal
// Project/Scene/UIDocument/etc.; this stores validated source and undo revisions.
class AuthoringDocument {
public:
 bool Load(const std::string& kind,const std::string& path,std::string& error);
 bool Replace(const std::string& source,std::string& error);
 bool Save(std::string& error);
 bool ExternalChanged()const;
 bool Dirty()const{return m_source!=m_loaded;}
 bool Undo();bool Redo();
 const std::string& Source()const{return m_source;}
 const std::string& Kind()const{return m_kind;}
 const std::string& Path()const{return m_path;}
 uint64_t Generation()const{return m_generation;}
private:
 std::string m_kind,m_path,m_source,m_loaded,m_disk;
 std::vector<std::string> m_undo,m_redo;uint64_t m_generation=0;
};
struct AuthoringOutput {std::string path,bytes,expected;bool existed=false;};
// Per-file atomic replacement, validated set publication with backups and journal.
// Failure rolls back unchanged outputs. External interference retains recovery data.
// This deliberately does not claim filesystem-wide/crash atomicity.
bool PublishAuthoringOutputs(const std::string& journalDirectory,const std::vector<AuthoringOutput>& outputs,std::string& error);
