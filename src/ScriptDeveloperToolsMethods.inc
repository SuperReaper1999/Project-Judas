void ScriptSystem::SetDebugger(ScriptDebugHandler handler){
    m->CheckThread();
    if(m->debug&&m->debug->inside)throw std::logic_error("cannot replace debugger inside a stop handler");
    if(!handler){
        JS_SetExecutionHook(m->rt,nullptr,nullptr);
        if(m->debug)for(auto [atom,name]:m->debug->files){(void)name;JS_FreeAtom(m->ctx,atom);}
        m->debug.reset();return;
    }
    if(!m->debug)m->debug=std::make_unique<Impl::DebugState>();
    m->debug->handler=std::move(handler);JS_SetExecutionHook(m->rt,Impl::DebugHook,m.get());
}
void ScriptSystem::SetBreakpoints(std::vector<ScriptBreakpoint> points){m->CheckThread();if(m->debug){if(points.size()>256)points.resize(256);m->debug->points=std::move(points);}}
void ScriptSystem::BreakOnNextInstruction(){m->CheckThread();if(m->debug)m->debug->pauseNext=true;}
const std::vector<std::string>& ScriptSystem::ConsoleMessages()const{return m->consoleMessages;}
bool ScriptSystem::HasReloaded()const{return m->reloaded;}
bool ScriptSystem::CheckSources(std::string& error)const{
    m->CheckThread();error.clear();
    Impl check(nullptr,m->assets);
    return m->CompileFor(check,error);
}
bool ScriptSystem::Reload(bool preserveState,std::string& error){
    m->CheckThread();error.clear();
    if(m->world&&m->world->NetworkIfLoaded()&&m->world->NetworkIfLoaded()->Active()){error="Close the network session before reloading scripts";return false;}
    if(m->invocationDepth||(m->debug&&m->debug->inside)){error="reload requires a callback boundary";return false;}
    if(m->world)if(auto session=m->world->SceneControl();session&&session->OperationInProgress()){
        error="save or scene operation is pending; let it complete before reloading scripts";return false;
    }
    auto replacement=std::make_unique<Impl>(m->world,m->assets);
    if(m->debug)replacement->debug=std::make_unique<Impl::DebugState>();
    if(!m->CompileFor(*replacement,error))return false;
    auto states=preserveState?Capture(true):std::vector<ScriptStateRecord>{};
    for(const auto& r:states)if(!ValidateJson(r.json,error)){error="cannot preserve faulted/non-JSON state; choose fresh reload or fix state: "+error;return false;}
    replacement->budget=m->budget;replacement->reloaded=true;
    replacement->hasView=m->hasView;replacement->viewOrigin=m->viewOrigin;replacement->viewDirection=m->viewDirection;
    ScriptDebugHandler handler;std::vector<ScriptBreakpoint> points;
    if(m->debug){handler=m->debug->handler;points=m->debug->points;SetDebugger({});}
    std::vector<SceneObjectId> owners;for(const auto& [key,i]:m->instances){(void)i;owners.push_back(key.first);}
    // Retire subscriptions, slot-owned UI and streaming requests through the
    // same path as destruction. Module caches and closures are never retained.
    RemoveEntities(owners);m=std::move(replacement);
    if(handler){SetDebugger(std::move(handler));SetBreakpoints(std::move(points));}
    if(!Restore(states,error,false))return false;
    Synchronize(m->world->ScriptSlots());
    return true;
}

std::vector<ScriptDiagnostic> ScriptSystem::CheckProjectSources(const AssetDatabase& assets){
    std::vector<ScriptDiagnostic> result;
    for(const auto& [id,record]:assets.Records())if(record.type==AssetType::Script){
        // Independent compile/link state keeps one failed module from poisoning
        // the following result, and never runs user top-level expressions.
        Impl check(nullptr,&assets);auto* module=check.Module(record.relativePath);
        if(!module||JS_PrepareModule(check.ctx,JS_MKPTR(JS_TAG_MODULE,module))<0){
            result.push_back({0,0,id,"compile/link",record.relativePath+": "+Exception(check.ctx)});
            if(result.size()==128)break;
        }
    }
    return result;
}
