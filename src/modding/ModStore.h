#pragma once
#include "ModContent.h"
#include "ContentStorage.h"
namespace JudasMods {
struct Installed {std::string id,version,hash,file;};
struct Loadout {std::string id,version,hash,trustedHash;};
// A snapshot retains leases and immutable cache files for the entire resource and
// world lifetime. It contains no VM and installation never constructs one.
struct Snapshot {
 Resolution resolution;
 std::map<std::string,std::string> files;
 std::vector<std::shared_ptr<void>> leases;
};
class Store {
public:
 explicit Store(std::string gameRoot);
 bool List(std::vector<Installed>&,std::vector<Loadout>&,std::string&);
 bool Install(const Contract&,const Package&,std::string&,
              const std::function<bool()>& cancelled={});
 bool Enable(const Contract&,const std::string& id,const std::string& version,
             const std::string& hash,bool trustExecutable,std::string&);
 bool Disable(const std::string& id,std::string&);
 bool Remove(const std::string& id,const std::string& version,const std::string& hash,std::string&);
 bool Preview(const Contract&,const std::vector<Selection>&,Snapshot&,std::string&);
 bool Freeze(const Contract&,Snapshot&,std::string&);
 std::function<void(const char*)> boundary;
private:
 ContentStorage m_installed,m_selection,m_cache,m_transaction;
 bool Materialize(Snapshot&,bool installedLeases,std::string&);
 bool CollectCache(const std::vector<Installed>&,std::string&);
 bool ReadIndex(std::vector<Installed>&,std::string&);
 bool ReadLoadout(std::vector<Loadout>&,std::string&);
 bool WriteIndex(const std::vector<Installed>&,std::string&);
 bool WriteLoadout(const std::vector<Loadout>&,std::string&);
 bool Selected(const std::vector<Installed>&,const std::vector<Loadout>&,
               std::vector<Selection>&,std::string&);
};
bool GameModDirectory(const Contract&,std::string&,std::string&);
}
