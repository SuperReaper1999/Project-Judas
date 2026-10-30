// Included inside Impl::Native, sharing validated value helpers. No gameplay.
    if(op=="playerDevices"||op=="playerCreate"||op=="playerList"||op=="playerExists"||op=="playerRemove"||op=="playerInfo"||op=="playerEntity"||op=="playerViewport"||op=="playerAssign"||op=="playerUnassign"||op=="playerMap"||op=="playerView"||op=="playerInput"||op=="playerProject"||op=="playerRay"||op=="uiPlayer"){
        auto token=[&](JSValueConst v)->uint64_t{if(!JS_IsString(v))return 0;auto text=String(c,v);if(text.empty()||text.size()>20||text.find_first_not_of("0123456789")!=std::string::npos)return 0;try{return std::stoull(text);}catch(...){return 0;}};
        auto hub=[&]()->InputDeviceHub*{if(!s->input)return nullptr;auto& owned=world.Players().devices;if(!owned)owned=std::make_shared<InputDeviceHub>();s->input->localDevices=owned;return owned.get();};
        auto rect=[&](JSValueConst v,glm::vec4& out){return Number(c,v,"x",out.x)&&Number(c,v,"y",out.y)&&Number(c,v,"width",out.z)&&Number(c,v,"height",out.w)&&LocalPlayers::ValidRect(out);};
        auto rectValue=[&](glm::vec4 r){auto out=JS_NewObject(c);for(auto field:{std::pair<const char*,float>{"x",r.x},{"y",r.y},{"width",r.z},{"height",r.w}})JS_SetPropertyStr(c,out,field.first,JS_NewFloat64(c,field.second));return out;};
        auto readMap=[&](JSValueConst v,InputMap& map,std::string& error){std::string json;if(!s->Json(v,json,error))return false;if(!InputMap::Parse("{\"kind\":\"input\",\"schema\":1,\"data\":"+json+"}",map,error))return false;if(map.entries.size()>256){error="At most 256 player bindings";return false;}return true;};
        auto* service=world.PlayersIfLoaded();auto* p=service?service->Find(token(arg(1))):nullptr;std::string error;
        if(op=="playerDevices"){auto* devices=hub();auto out=JS_NewArray(c);if(!devices)return out;uint32_t n=0;for(auto& pair:devices->Devices()){auto& d=pair.second;auto value=JS_NewObject(c);for(auto field:{std::pair<const char*,std::string>{"id",d.id},{"kind",d.kind},{"name",d.name}})JS_SetPropertyStr(c,value,field.first,JS_NewString(c,field.second.c_str()));JS_SetPropertyStr(c,value,"connected",JS_NewBool(c,d.connected));JS_SetPropertyUint32(c,out,n++,value);}return out;}
        if(op=="playerList"){auto out=JS_NewArray(c);uint32_t n=0;if(service)for(auto& pair:service->All())JS_SetPropertyUint32(c,out,n++,JS_NewString(c,std::to_string(pair.first).c_str()));return out;}
        if(op=="playerCreate"){if(!JS_IsString(arg(1)))return JS_ThrowTypeError(c,"player name string required");if(s->input)hub();InputMap map=s->input?s->input->Map():InputMap::Defaults();if(!JS_IsNull(arg(2))&&!readMap(arg(2),map,error))return JS_ThrowTypeError(c,"input map: %s",error.c_str());auto* created=world.Players().Create(String(c,arg(1)),map,error);return created?JS_NewString(c,std::to_string(created->id).c_str()):JS_ThrowTypeError(c,"%s",error.c_str());}
        if(op=="playerExists")return JS_NewBool(c,p!=nullptr);
        if(op=="uiPlayer"){uint32_t handle=0;JS_ToUint32(c,&handle,arg(1));if(!world.UI().Document(handle))return JS_ThrowReferenceError(c,"stale UI document");if(argc>2){const auto id=JS_IsNull(arg(2))?0:token(arg(2));if(!JS_IsNull(arg(2))&&(!service||!service->Find(id)))return JS_ThrowReferenceError(c,"stale player");world.UI().SetPlayer(handle,id);if(id)world.UI().PlayerViewport(id,service->Find(id)->viewport);}auto id=world.UI().Player(handle);return id?JS_NewString(c,std::to_string(id).c_str()):JS_NULL;}
        if(!p)return JS_ThrowReferenceError(c,"stale local player");
        if(op=="playerRemove"){world.UI().RemovePlayer(p->id);return JS_NewBool(c,service->Remove(p->id));}
        if(op=="playerInfo"){auto out=JS_NewObject(c),devices=JS_NewArray(c);uint32_t n=0;for(auto& a:p->devices)JS_SetPropertyUint32(c,devices,n++,JS_NewString(c,a.device.c_str()));JS_SetPropertyStr(c,out,"name",JS_NewString(c,p->name.c_str()));JS_SetPropertyStr(c,out,"devices",devices);JS_SetPropertyStr(c,out,"viewError",JS_NewString(c,p->renderError.c_str()));return out;}
        if(op=="playerEntity"){if(argc>2){const auto id=JS_IsNull(arg(2))?0:token(arg(2));if(!JS_IsNull(arg(2))&&(!id||!world.RuntimeDefinition(id)))return JS_ThrowReferenceError(c,"stale controlled entity");p->entity=id;}return p->entity&&world.RuntimeDefinition(p->entity)?JS_NewString(c,std::to_string(p->entity).c_str()):JS_NULL;}
        if(op=="playerViewport"){if(argc>2){glm::vec4 r;if(!rect(arg(2),r)||!service->SetViewport(p->id,r,error))return JS_ThrowTypeError(c,"normalized viewport required");if(world.UIIfLoaded())world.UI().PlayerViewport(p->id,r);}return rectValue(p->viewport);}
        if(op=="playerMap"){InputMap map;if(!readMap(arg(2),map,error)||!p->input.SetMap(map,error))return JS_ThrowTypeError(c,"input map: %s",error.c_str());p->baseline=true;p->touchSources.clear();return JS_TRUE;}
        if(op=="playerUnassign")return JS_NewBool(c,service->Unassign(p->id,String(c,arg(2))));
        if(op=="playerAssign"){
            if(!hub()||!JS_IsString(arg(2))||!KnownOptions(c,arg(3),{"controls","region"}))return JS_ThrowTypeError(c,"device and controls/region options required");
            LocalPlayers::Assignment a;a.device=String(c,arg(2));auto controls=JS_GetPropertyStr(c,arg(3),"controls"),region=JS_GetPropertyStr(c,arg(3),"region");bool valid=true;
            if(!JS_IsUndefined(controls)){auto length=JS_GetPropertyStr(c,controls,"length");uint32_t n=0;valid=JS_IsArray(controls)&&JS_ToUint32(c,&n,length)==0&&n<=128;JS_FreeValue(c,length);for(uint32_t i=0;valid&&i<n;++i){auto v=JS_GetPropertyUint32(c,controls,i);valid=JS_IsString(v);if(valid)a.controls.insert(String(c,v));JS_FreeValue(c,v);}}
            if(!JS_IsUndefined(region))valid&=rect(region,a.region);
            JS_FreeValue(c,controls);JS_FreeValue(c,region);
            if(!valid||!service->Assign(p->id,*hub(),std::move(a),error))return JS_ThrowTypeError(c,"device assignment: %s",error.c_str());
            return JS_TRUE;
        }
        if(op=="playerView"){
            if(JS_IsNull(arg(2))){p->view.reset();p->renderError.clear();p->attemptedSize={0,0};return JS_TRUE;}SceneTransform pose;double fov=70;float nearPlane=.1f,farPlane=500;bool valid=JS_IsObject(arg(2))&&readTransform(arg(2),pose)&&JS_ToFloat64(c,&fov,arg(3))==0&&KnownOptions(c,arg(4),{"near","far","projection","height","pixelPerfect","referenceWidth","referenceHeight","pixelsPerUnit"});CameraLens lens;valid&=ReadCameraLens(c,arg(4),lens);
            for(auto field:{std::pair<const char*,float*>{"near",&nearPlane},{"far",&farPlane}}){auto value=JS_GetPropertyStr(c,arg(4),field.first);if(!JS_IsUndefined(value))valid&=Number(c,arg(4),field.first,*field.second);JS_FreeValue(c,value);}
            if(!valid||!service->SetView(p->id,pose,float(fov),nearPlane,farPlane,error,lens))return JS_ThrowTypeError(c,"invalid player view/range");
            return JS_TRUE;
        }
        if(op=="playerProject"||op=="playerRay"){
            if(!p->view||world.viewportWidth<=0||world.viewportHeight<=0)return JS_NULL;
            const auto& pose=*p->view;const auto size=LocalPlayers::ViewSize(p->viewport,world.viewportWidth,world.viewportHeight);
            const auto frame=BuildCameraFrame(pose.position,pose.rotation,p->fov,p->nearPlane,p->farPlane,p->lens,size.x,size.y);
            if(op=="playerRay") {
                glm::vec2 point;glm::vec3 origin,direction;
                if(!Number(c,arg(2),"x",point.x)||!Number(c,arg(2),"y",point.y)||point.x<0||point.x>1||point.y<0||point.y>1)return JS_ThrowTypeError(c,"normalized viewport-local point required");
                if(!frame.Ray(point,origin,direction))return JS_NULL;
                auto out=JS_NewObject(c);JS_SetPropertyStr(c,out,"origin",Vec(c,origin));JS_SetPropertyStr(c,out,"direction",Vec(c,direction));return out;
            }
            glm::vec3 point;if(!ReadVec(c,arg(2),point))return JS_ThrowTypeError(c,"finite world point required");const auto projected=frame.Project(point);auto out=JS_NewObject(c);
            for(auto field:{std::pair<const char*,double>{"x",p->viewport.x+projected.position.x*p->viewport.z},{"y",p->viewport.y+projected.position.y*p->viewport.w},{"depth",projected.depth},{"distance",projected.distance}})JS_SetPropertyStr(c,out,field.first,JS_NewFloat64(c,field.second));
            JS_SetPropertyStr(c,out,"behind",JS_NewBool(c,projected.behind));JS_SetPropertyStr(c,out,"inside",JS_NewBool(c,projected.inside));return out;
        }
        if(op=="playerInput"){
            const auto method=String(c,arg(2)),name=String(c,arg(3));const bool blocked=world.UIIfLoaded()&&(world.UIIfLoaded()->Paused()||world.UIIfLoaded()->Paused(p->id))&&!s->inUI;
            if(method=="held"||method=="pressed"||method=="released"){const auto state=blocked?InputActionState{}:s->fixed?p->input.FixedAction(name):p->input.Action(name);return JS_NewBool(c,method=="held"?state.held:method=="pressed"?state.pressed:state.released);}
            if(method=="axis")return JS_NewFloat64(c,blocked?0:p->input.Axis(name));
            if(method=="vector"||method=="stick"){if(method=="stick"&&name!="left"&&name!="right")return JS_ThrowTypeError(c,"stick side left/right required");const auto v=blocked?InputVector{}:method=="vector"?p->input.Vector(name):p->input.Stick(name);auto out=JS_NewObject(c);JS_SetPropertyStr(c,out,"x",JS_NewFloat64(c,v.x));JS_SetPropertyStr(c,out,"y",JS_NewFloat64(c,v.y));return out;}
            if(method=="pointer"){const auto v=p->input.Pointer();auto out=JS_NewObject(c);JS_SetPropertyStr(c,out,"x",JS_NewFloat64(c,v.x));JS_SetPropertyStr(c,out,"y",JS_NewFloat64(c,v.y));JS_SetPropertyStr(c,out,"down",JS_NewBool(c,!blocked&&v.down));return out;}
            if(method=="touches"){auto out=JS_NewArray(c);uint32_t n=0;if(!blocked)for(auto& t:p->input.Touches()){auto v=JS_NewObject(c);JS_SetPropertyStr(c,v,"id",JS_NewFloat64(c,double(t.id)));for(auto pair:{std::pair<const char*,float>{"x",t.x},{"y",t.y},{"pressure",t.pressure}})JS_SetPropertyStr(c,v,pair.first,JS_NewFloat64(c,pair.second));JS_SetPropertyUint32(c,out,n++,v);}return out;}
            return JS_ThrowTypeError(c,"unknown player input observation");
        }
    }
