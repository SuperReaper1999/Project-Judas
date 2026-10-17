    bool CompileFor(Impl& target,std::string& error) const {
        if(!world||!assets){error="script source check requires a project world";return false;}
        for(const auto& o:world->ScriptSlots())for(const auto& slot:o.scripts)if(slot.enabled){
            auto* record=assets->Find(slot.asset);
            if(!record){error="entity "+std::to_string(o.id)+": missing script asset "+slot.asset;return false;}
            auto* module=target.Module(record->relativePath);
            if(!module||JS_PrepareModule(target.ctx,JS_MKPTR(JS_TAG_MODULE,module))<0){error=record->relativePath+": "+Exception(target.ctx);return false;}
        }
        return true;
    }
    ScriptDebugVariable DebugValue(const std::string& name,JSValueConst value,unsigned depth,unsigned& nodes){
        ScriptDebugVariable out;out.name=name;
        if(++nodes>512){out.value="<snapshot limit>";return out;}
        if(JS_IsProxy(value)){out.value="<proxy: not evaluated>";return out;}
        if(JS_IsObject(value)){
            if(JS_IsFunction(ctx,value)){out.value="<function>";return out;}
            out.value=JS_IsArray(value)?"Array":"Object";
            if(depth>=3)return out;
            JSPropertyEnum* props=nullptr;uint32_t count=0;
            if(JS_GetOwnPropertyNames(ctx,&props,&count,value,JS_GPN_STRING_MASK)==0){
                for(uint32_t i=0;i<count&&i<32&&nodes<512;++i){
                    auto key=JS_AtomToValue(ctx,props[i].atom);auto label=String(ctx,key);JS_FreeValue(ctx,key);
                    JSPropertyDescriptor d{};
                    if(JS_GetOwnProperty(ctx,&d,value,props[i].atom)>0){
                        if(!JS_IsUndefined(d.getter)||!JS_IsUndefined(d.setter))out.children.push_back({label,"<accessor: not evaluated>",{}});
                        else out.children.push_back(DebugValue(label,d.value,depth+1,nodes));
                        JS_FreeValue(ctx,d.value);JS_FreeValue(ctx,d.getter);JS_FreeValue(ctx,d.setter);
                    }
                }
                JS_FreePropertyEnum(ctx,props,count);
            }
        }else if(JS_IsUninitialized(value))out.value="<uninitialized>";
        else if(JS_IsString(value))out.value="\""+String(ctx,value).substr(0,1024)+"\"";
        else if(JS_IsSymbol(value))out.value="<symbol>";
        else out.value=String(ctx,value).substr(0,1024);
        return out;
    }
    static int DebugHook(JSContext* ctx,JSAtom file,int line,int,int depth,void* opaque){
        auto& s=*static_cast<Impl*>(opaque);auto& d=*s.debug;
        if(d.inside)return 0;
        auto it=d.files.find(file);if(it==d.files.end()){
            auto v=JS_AtomToValue(ctx,file);auto name=String(ctx,v);JS_FreeValue(ctx,v);
            // Hold the atom so cache entries cannot alias after function collection.
            JS_DupAtom(ctx,file);it=d.files.emplace(file,std::move(name)).first;
        }
        if(it->second=="judas")return 0;
        const bool changed=d.lastFile!=file||d.lastLine!=line||d.lastDepth!=depth;
        d.linesByDepth.erase(d.linesByDepth.upper_bound(depth),d.linesByDepth.end());
        const bool newLine=d.linesByDepth[depth]!=std::make_pair(file,line);
        d.linesByDepth[depth]={file,line};
        d.lastFile=file;d.lastLine=line;d.lastDepth=depth;
        if(!changed)return 0;
        bool hit=d.pauseNext;
        for(const auto& p:d.points)if(newLine&&p.enabled&&p.file==it->second&&p.line==line)hit=true;
        if(d.step==ScriptDebugAction::Into||(d.step==ScriptDebugAction::Over&&depth<=d.stepDepth&&(newLine||depth<d.stepDepth))||(d.step==ScriptDebugAction::Out&&depth<d.stepDepth))hit=true;
        if(!hit)return 0;
        d.pauseNext=false;d.inside=true;
        ScriptDebugStop stop;stop.entity=s.currentOwner;stop.slot=s.currentSlot;
        unsigned nodes=0;
        for(unsigned n=0;n<32;++n){
            auto frame=JS_GetDebugFrame(ctx,n);if(JS_IsNull(frame))break;
            ScriptDebugFrame f;
            auto text=[&](const char* key){auto v=JS_GetPropertyStr(ctx,frame,key);auto r=String(ctx,v);JS_FreeValue(ctx,v);return r;};
            f.file=text("file");f.function=text("function");
            if(auto source=d.sources.find(f.file);source!=d.sources.end()){f.sourceKnown=true;f.source=source->second;}
            auto integer=[&](const char* key){auto v=JS_GetPropertyStr(ctx,frame,key);int32_t r=0;JS_ToInt32(ctx,&r,v);JS_FreeValue(ctx,v);return r;};
            f.line=integer("line");f.column=integer("column");
            auto vars=JS_GetPropertyStr(ctx,frame,"variables");
            for(unsigned i=0;i<256&&nodes<512;++i){
                auto entry=JS_GetPropertyUint32(ctx,vars,i);if(JS_IsUndefined(entry))break;
                auto name=JS_GetPropertyStr(ctx,entry,"name");auto scope=JS_GetPropertyStr(ctx,entry,"scope");auto value=JS_GetPropertyStr(ctx,entry,"value");
                f.variables.push_back(s.DebugValue(String(ctx,scope)+" "+String(ctx,name),value,0,nodes));
                JS_FreeValue(ctx,name);JS_FreeValue(ctx,scope);JS_FreeValue(ctx,value);JS_FreeValue(ctx,entry);
            }
            JS_FreeValue(ctx,vars);JS_FreeValue(ctx,frame);stop.frames.push_back(std::move(f));
        }
        // Never let a C++ tool exception unwind through the C interpreter.
        ScriptDebugAction action=ScriptDebugAction::Cancel;
        try{action=d.handler(stop);}catch(...){std::fprintf(stderr,"Script debugger handler failed; cancelling callback\n");}
        d.inside=false;d.step=action;d.stepDepth=depth;
        if(action==ScriptDebugAction::Cancel)JS_SetExecutionHook(s.rt,nullptr,nullptr);
        return action==ScriptDebugAction::Cancel;
    }
