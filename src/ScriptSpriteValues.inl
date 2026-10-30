bool ReadSpriteValues(JSContext* c,JSValueConst value,SpriteSettings& sprite) {
    if(!KnownOptions(c,value,{"size","pivot","rect","tint","flipX","flipY","nearest","pixelSnap","alphaPick","ySort","sortingLayer","order"}))return false;
    auto vector=[&](const char* key,float* data,std::initializer_list<const char*> names) {
        auto v=JS_GetPropertyStr(c,value,key);
        if(JS_IsUndefined(v)){JS_FreeValue(c,v);return true;}
        bool ok=KnownOptions(c,v,names);size_t i=0;
        for(const auto name:names)ok=Number(c,v,name,data[i++])&&ok;
        JS_FreeValue(c,v);return ok;
    };
    if(!vector("size",&sprite.size.x,{"x","y"})||!vector("pivot",&sprite.pivot.x,{"x","y"})||
       !vector("rect",&sprite.rect.x,{"x","y","width","height"})||!vector("tint",&sprite.tint.x,{"x","y","z","w"}))return false;
    for(auto pair:{std::pair<const char*,bool*>{"flipX",&sprite.flipX},{"flipY",&sprite.flipY},{"nearest",&sprite.nearest},
                  {"pixelSnap",&sprite.pixelSnap},{"alphaPick",&sprite.alphaPick},{"ySort",&sprite.ySort}}) {
        auto v=JS_GetPropertyStr(c,value,pair.first);
        if(!JS_IsUndefined(v)){if(!JS_IsBool(v)){JS_FreeValue(c,v);return false;}*pair.second=JS_ToBool(c,v);}
        JS_FreeValue(c,v);
    }
    for(auto pair:{std::pair<const char*,int*>{"sortingLayer",&sprite.sortingLayer},{"order",&sprite.order}}) {
        auto v=JS_GetPropertyStr(c,value,pair.first);
        if(!JS_IsUndefined(v)){double number=0;bool ok=JS_IsNumber(v)&&JS_ToFloat64(c,&number,v)==0&&std::isfinite(number)&&number>=-32768&&number<=32767&&std::floor(number)==number;
            if(!ok){JS_FreeValue(c,v);return false;}*pair.second=int(number);}
        JS_FreeValue(c,v);
    }
    return ValidSprite(sprite);
}
JSValue SpriteValue(JSContext* c,const SpriteSettings& sprite) {
    auto out=JS_NewObject(c);
    auto vector=[&](const char* key,const float* data,std::initializer_list<const char*> names){auto value=JS_NewObject(c);size_t i=0;for(const auto name:names)JS_SetPropertyStr(c,value,name,JS_NewFloat64(c,data[i++]));JS_SetPropertyStr(c,out,key,value);};
    vector("size",&sprite.size.x,{"x","y"});vector("pivot",&sprite.pivot.x,{"x","y"});
    vector("rect",&sprite.rect.x,{"x","y","width","height"});vector("tint",&sprite.tint.x,{"x","y","z","w"});
    for(auto pair:{std::pair<const char*,bool>{"flipX",sprite.flipX},{"flipY",sprite.flipY},{"nearest",sprite.nearest},
                  {"pixelSnap",sprite.pixelSnap},{"alphaPick",sprite.alphaPick},{"ySort",sprite.ySort}})JS_SetPropertyStr(c,out,pair.first,JS_NewBool(c,pair.second));
    JS_SetPropertyStr(c,out,"sortingLayer",JS_NewInt32(c,sprite.sortingLayer));JS_SetPropertyStr(c,out,"order",JS_NewInt32(c,sprite.order));
    return out;
}
