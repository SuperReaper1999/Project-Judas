#pragma once
#include "InputSystem.h"
#include "AuthoringJSON.h"
namespace AuthoringJSON {
inline J input(const InputMap& m){
 J a=J::array();for(const auto& e:m.entries){J bs=J::array();for(const auto& b:e.bindings){J value={{"control",b.control},{"scale",b.scale},{"deadzone",b.deadzone}};if(e.vector){value["scaleY"]=b.scaleY;value["circular"]=b.circular;}bs.push_back(std::move(value));}J value={{"name",e.name},{"axis",e.axis},{"bindings",bs}};if(e.vector)value["vector"]=true;a.push_back(std::move(value));}return a;
}
inline InputMap input(const J& a){
 require(a.is_array()&&a.size()<=10000,"input entries limit/type");InputMap m;
 for(const auto& e:a){keys(e,{"name","axis","vector","bindings"},"input");InputEntry r;r.name=e.at("name").get<std::string>();r.axis=e.at("axis").get<bool>();r.vector=e.value("vector",false);
  require(e.at("bindings").is_array()&&e.at("bindings").size()<=10000,"input bindings limit/type");
  for(const auto& b:e.at("bindings")){keys(b,{"control","scale","deadzone","scaleY","circular"},"input/binding");InputBinding value{b.at("control").get<std::string>(),typed<float>(b.at("scale")),typed<float>(b.at("deadzone"))};if(b.contains("scaleY"))value.scaleY=typed<float>(b.at("scaleY"));value.circular=b.value("circular",false);r.bindings.push_back(value);}
  m.entries.push_back(std::move(r));}
 std::string error;require(m.Validate(error),"input: "+error);return m;
}
inline bool ParseInputDocument(const std::string& text,InputMap& out,std::string& error){
 try {auto j=parse(text);keys(j,{"kind","schema","data"},"document");require(j.at("kind")=="input","document kind mismatch");require(j.at("schema").is_number_integer()&&j.at("schema")==1,"unsupported authoring schema");auto candidate=input(j.at("data"));out=std::move(candidate);error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}
}
}
