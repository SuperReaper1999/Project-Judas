#pragma once
#include <memory>
#include <string>
#include <vector>
#include <functional>

// Flat, scoped content storage. Logical archive paths NEVER become disk paths.
// Directory handles pin every component; POSIX operations are descriptor-relative,
// Windows refuses reparse points and holds directories without delete sharing.
// Files are single-link regular objects. Publisher uses synchronized temporary
// files and atomic per-file replacement, not a filesystem-wide transaction.
class ContentStorage {
public:
 explicit ContentStorage(std::string root);~ContentStorage();
 ContentStorage(const ContentStorage&)=delete;
 bool Open(std::string& error);
 bool Read(const std::string& name,std::string& bytes,size_t limit,std::string& error);
 bool Publish(const std::string& name,const std::string& bytes,std::string& error,bool immutable=false);
 bool Remove(const std::string& name,std::string& error);
 bool List(std::vector<std::string>& names,size_t limit,std::string& error);
 // A version lease denies deletion/replacement during a session. Different store
 // instances/processes share the OS lock; an in-use uninstall fails explicitly.
 std::shared_ptr<void> Lease(const std::string& name,std::string& error);
 std::shared_ptr<void> Exclusive(const std::string& lockName,std::string& error);
 std::string Path(const std::string& name)const;
 const std::string& Root()const;
 std::function<void(const char*)> boundary;
private:struct Impl;std::unique_ptr<Impl> m;
};
// Tool input reader: pins existing ancestors without creating directories;
// rejects link/reparse/hardlink/nonregular input before bounded allocation.
bool ReadRegularContentFile(const std::string& path,std::string& bytes,size_t limit,std::string& error);
