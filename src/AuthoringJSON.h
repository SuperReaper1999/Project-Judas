#pragma once
// Shared bounded JSON checks; lightweight input consumers need no scene/runtime codec.
#include "../third_party/nlohmann/json.hpp"
#include <set>
#include <limits>
#include <cmath>
#include <type_traits>
#include <stdexcept>
namespace AuthoringJSON {
using J=nlohmann::ordered_json;
inline void require(bool ok,const std::string& path){if(!ok)throw std::runtime_error(path);}
inline void keys(const J& j,std::initializer_list<const char*> allowed,const std::string& path){require(j.is_object(),path+": object required");for(auto it=j.begin();it!=j.end();++it){bool found=false;for(auto k:allowed)found|=it.key()==k;require(found,path+"/"+it.key()+": unknown property");}}
template<class T>T typed(const J& value){
 if constexpr(std::is_integral_v<T>&&!std::is_same_v<T,bool>){require(value.is_number_integer(),"integer value required");auto n=value.get<long double>();require(n>=std::numeric_limits<T>::lowest()&&n<=std::numeric_limits<T>::max(),"integer out of range");}
 if constexpr(std::is_floating_point_v<T>){require(value.is_number(),"number required");auto n=value.get<double>();require(std::isfinite(n)&&std::abs(n)<=std::numeric_limits<T>::max(),"finite bounded number required");}
 return value.get<T>();
}
inline J parse(const std::string& text){require(text.size()<=32*1024*1024,"document exceeds 32 MiB");std::vector<std::set<std::string>> stack;return J::parse(text,[&](int depth,J::parse_event_t e,J& v){require(depth<=64,"document nesting exceeds 64");if(e==J::parse_event_t::object_start)stack.emplace_back();if(e==J::parse_event_t::key)require(stack.back().insert(v.get<std::string>()).second,"duplicate key: "+v.get<std::string>());if(e==J::parse_event_t::object_end)stack.pop_back();return true;});}
}
