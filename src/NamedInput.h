#pragma once
#include "InputSystem.h"
#include "AuthoringJSON.h"
namespace AuthoringJSON {
inline J input(const InputMap& m){J a=J::array();for(auto& e:m.entries){J bs=J::array();for(auto& b:e.bindings)bs.push_back({{"control",b.control},{"scale",b.scale},{"deadzone",b.deadzone}});a.push_back({{"name",e.name},{"axis",e.axis},{"bindings",bs}});}return a;}
inline InputMap input(const J& a){require(a.is_array()&&a.size()<=10000,"input entries limit/type");InputMap m;for(auto& e:a){keys(e,{"name","axis","bindings"},"input");InputEntry r;r.name=e.at("name").get<std::string>();r.axis=e.at("axis").get<bool>();require(e.at("bindings").is_array()&&e.at("bindings").size()<=10000,"input bindings limit/type");for(auto& b:e.at("bindings")){keys(b,{"control","scale","deadzone"},"input/binding");r.bindings.push_back({b.at("control").get<std::string>(),typed<float>(b.at("scale")),typed<float>(b.at("deadzone"))});}m.entries.push_back(r);}std::string error;require(m.Validate(error),"input: "+error);return m;}
inline bool ParseInputDocument(const std::string& text,InputMap& out,std::string& error){
 try {auto j=parse(text);keys(j,{"kind","schema","data"},"document");require(j.at("kind")=="input","document kind mismatch");require(j.at("schema").is_number_integer()&&j.at("schema")==1,"unsupported authoring schema");auto candidate=input(j.at("data"));out=std::move(candidate);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}
}
}
