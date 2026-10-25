// Included inside the native dispatcher; input remains a borrowed read-only
// snapshot. Configuration requests publish through InputSystem::BeginFrame.
    if(op=="inputBindings"){
        auto result=JS_NewArray(c);uint32_t index=0;
        if(s->input)for(const auto& entry:s->input->Map().entries){
            auto value=JS_NewObject(c),bindings=JS_NewArray(c);uint32_t bindingIndex=0;
            JS_SetPropertyStr(c,value,"name",JS_NewString(c,entry.name.c_str()));
            JS_SetPropertyStr(c,value,"axis",JS_NewBool(c,entry.axis));
            if(entry.vector)JS_SetPropertyStr(c,value,"vector",JS_TRUE);
            for(const auto& binding:entry.bindings){
                auto item=JS_NewObject(c);
                JS_SetPropertyStr(c,item,"control",JS_NewString(c,binding.control.c_str()));
                JS_SetPropertyStr(c,item,"scale",JS_NewFloat64(c,binding.scale));
                JS_SetPropertyStr(c,item,"deadzone",JS_NewFloat64(c,binding.deadzone));
                if(entry.vector){JS_SetPropertyStr(c,item,"scaleY",JS_NewFloat64(c,binding.scaleY));JS_SetPropertyStr(c,item,"circular",JS_NewBool(c,binding.circular));}
                JS_SetPropertyUint32(c,bindings,bindingIndex++,item);
            }
            JS_SetPropertyStr(c,value,"bindings",bindings);JS_SetPropertyUint32(c,result,index++,value);
        }
        return result;
    }
    if(op=="inputSetBindings"){
        if(!s->input)return JS_FALSE;
        InputMap map;std::string json,error;const bool reset=JS_IsNull(arg(1));
        if(!reset&&(!s->Json(arg(1),json,error)||!InputMap::Parse("{\"kind\":\"input\",\"schema\":1,\"data\":"+json+"}",map,error)))
            return JS_ThrowTypeError(c,"Input bindings: %s",error.c_str());
        if(!s->input->RequestBindings(reset?nullptr:&map,error))return JS_ThrowTypeError(c,"Input bindings: %s",error.c_str());
        return JS_TRUE;
    }
