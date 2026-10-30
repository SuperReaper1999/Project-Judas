#pragma once
#include <string>
class ScriptSystem;
class AssetDatabase;
class Renderer;
struct ScriptFaultNotice {std::string source,callback,message,stack;};
ScriptFaultNotice LatestScriptFaultNotice(const ScriptSystem*,const AssetDatabase*);
bool DefaultScriptFaultNoticeEnabled();
void DrawScriptFaultNotice(Renderer&,int width,int height,const ScriptSystem*,const AssetDatabase*);
