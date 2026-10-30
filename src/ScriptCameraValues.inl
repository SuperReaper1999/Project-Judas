// Shared validation for main, local-player and texture cameras. Included inside
// ScriptSystem's value-helper namespace, never another native dispatch surface.
bool ReadCameraLens(JSContext* c,JSValueConst options,CameraLens& lens) {
    auto projection=JS_GetPropertyStr(c,options,"projection");
    if(!JS_IsUndefined(projection)) {
        if(!JS_IsString(projection)){JS_FreeValue(c,projection);return false;}
        const auto name=String(c,projection);
        if(name=="orthographic")lens.projection=CameraProjectionKind::Orthographic;
        else if(name=="perspective")lens.projection=CameraProjectionKind::Perspective;
        else {JS_FreeValue(c,projection);return false;}
    }
    JS_FreeValue(c,projection);
    for(const auto& pair:{std::pair<const char*,float*>{"height",&lens.height},{"pixelsPerUnit",&lens.pixelsPerUnit}}) {
        const auto value=JS_GetPropertyStr(c,options,pair.first);
        const bool ok=JS_IsUndefined(value)||Number(c,options,pair.first,*pair.second);
        JS_FreeValue(c,value);if(!ok)return false;
    }
    auto pixel=JS_GetPropertyStr(c,options,"pixelPerfect");
    if(!JS_IsUndefined(pixel)) {
        if(!JS_IsBool(pixel)){JS_FreeValue(c,pixel);return false;}
        lens.pixelPerfect=JS_ToBool(c,pixel);
    }
    JS_FreeValue(c,pixel);
    for(const auto& pair:{std::pair<const char*,int*>{"referenceWidth",&lens.referenceWidth},{"referenceHeight",&lens.referenceHeight}}) {
        const auto value=JS_GetPropertyStr(c,options,pair.first);
        if(!JS_IsUndefined(value)) {
            double number=0;
            const bool ok=JS_IsNumber(value)&&JS_ToFloat64(c,&number,value)==0&&std::isfinite(number)&&number>=1&&number<=4096&&std::floor(number)==number;
            if(!ok){JS_FreeValue(c,value);return false;}
            *pair.second=int(number);
        }
        JS_FreeValue(c,value);
    }
    return ValidCameraLens(lens);
}
JSValue CameraLensValue(JSContext* c,const CameraLens& lens) {
    auto out=JS_NewObject(c);
    JS_SetPropertyStr(c,out,"projection",JS_NewString(c,lens.projection==CameraProjectionKind::Orthographic?"orthographic":"perspective"));
    JS_SetPropertyStr(c,out,"height",JS_NewFloat64(c,lens.height));
    JS_SetPropertyStr(c,out,"pixelPerfect",JS_NewBool(c,lens.pixelPerfect));
    JS_SetPropertyStr(c,out,"referenceWidth",JS_NewInt32(c,lens.referenceWidth));JS_SetPropertyStr(c,out,"referenceHeight",JS_NewInt32(c,lens.referenceHeight));
    JS_SetPropertyStr(c,out,"pixelsPerUnit",JS_NewFloat64(c,lens.pixelsPerUnit));
    return out;
}
