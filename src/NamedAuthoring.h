#pragma once
#include <string>
// Named authoring schema is separate from scene/save/cooked schemas. The normal
// loaders validate decoded data; this codec never creates runtime objects.
inline bool IsNamedDocument(const std::string& text){auto p=text.find_first_not_of(" \t\r\n");return p!=std::string::npos&&text[p]=='{';}
bool NamedToLegacy(const std::string& text,const std::string& expectedKind,std::string& legacy,std::string& error);
bool LegacyToNamed(const std::string& legacy,const std::string& kind,std::string& named,std::string& error);
// Preserve a selected named source on ordinary editor saves. Stage and validate
// before replacement; existing external-edit protection belongs to the document.
bool WriteAuthoredDocument(const std::string& path,const std::string& legacy,const std::string& kind,std::string& error,bool forceNamed=false,bool preserveExisting=true);
bool ValidateAuthoredDocument(const std::string& text,const std::string& kind,std::string& error);
