#pragma once
#include <algorithm>
#include <functional>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
namespace LifecycleAudit {
namespace fs=std::filesystem;
struct Receipt {
    nlohmann::json checks=nlohmann::json::array();
    unsigned failures=0;
    void Check(bool ok,const std::string& name,nlohmann::json details=nlohmann::json::object()) {
        checks.push_back({{"name",name},{"passed",ok},{"details",std::move(details)}});
        failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';
    }
    void Require(bool ok,const std::string& name,const std::string& error={}) {
        Check(ok,name,{{"error",error}});if(!ok)throw std::runtime_error(name+": "+error);
    }
    int Finish(const fs::path& out,const std::string& contract)const {
        std::ofstream(out/"RESULT.json")<<nlohmann::json{{"contract",contract},{"checks",checks},{"failures",failures},{"scope","Standalone M78 lifecycle audit; original fixtures and source unchanged."}}.dump(2)<<'\n';
        std::cout<<"SUMMARY "<<checks.size()<<" checks "<<failures<<" failures\n";return failures?1:0;
    }
};
inline std::string Read(const fs::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot read "+path.string());return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
inline fs::path FreshOutput(int argc,char** argv){if(argc!=2)throw std::runtime_error("usage: fixture fresh-owned-output-directory");fs::path out=fs::absolute(fs::u8path(argv[1])).lexically_normal();if(fs::exists(out))throw std::runtime_error("output already exists; choose a fresh owned path");fs::create_directories(out);return out;}
}
