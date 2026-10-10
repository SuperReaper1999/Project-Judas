#pragma once
#include "EditorPanels.h"
#include "ModAuthoring.h"
#include "AuthoringDocument.h"
struct ModTools {
 char kit[512]="",destination[512]="",docs[512]="docs",modId[65]="my_mod";
 char game[512]="",packagePath[512]="",output[512]="";
 std::vector<char> manifestText=std::vector<char>(JudasMods::Limits::Metadata+1,0);
 std::string projectKey,error;AuthoringDocument draft;bool loaded=false,trust=false;
 void Panel(EditorPanelState&,EditorRequests&);
};
