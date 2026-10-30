// Public shape descriptors are snapshots. Geometry mutation remains an authored
// operation; native shape handles and shared immutable storage never escape JS.
bool ReadPlanarValues(JSContext* c, JSValueConst value,
                      std::shared_ptr<const PlanarCollider>& out) {
    auto kindValue=JS_GetPropertyStr(c,value,"kind");
    const auto kind=String(c,kindValue);JS_FreeValue(c,kindValue);
    auto scalar=[&](const char* key,double& n){auto v=JS_GetPropertyStr(c,value,key);const bool ok=JS_IsNumber(v)&&JS_ToFloat64(c,&n,v)==0&&std::isfinite(n);JS_FreeValue(c,v);return ok;};
    try {
        if(kind=="circle") {
            double radius=0;if(!KnownOptions(c,value,{"kind","radius"})||!scalar("radius",radius))return false;
            PlanarCollider point{PlanarKind::Circle,{{0,0}},radius,false};std::string error;
            if(!ValidatePlanar(point,error,true))return false;
            out=std::make_shared<const PlanarCollider>(point);
        } else if(kind=="box") {
            auto h=JS_GetPropertyStr(c,value,"halfExtents");float x=0,y=0;
            const bool ok=KnownOptions(c,value,{"kind","halfExtents"})&&KnownOptions(c,h,{"x","y"})&&Number(c,h,"x",x)&&Number(c,h,"y",y);JS_FreeValue(c,h);
            if(!ok)return false;
            out=PlanarBox({x,y});
        } else if(kind=="capsule") {
            double radius=0,height=0;
            if(!KnownOptions(c,value,{"kind","radius","halfHeight"})||!scalar("radius",radius)||!scalar("halfHeight",height))return false;
            out=PlanarCapsule(radius,height);
        } else if(kind=="convex") {
            if(!KnownOptions(c,value,{"kind","vertices"}))return false;
            auto vertices=JS_GetPropertyStr(c,value,"vertices"),length=JS_GetPropertyStr(c,vertices,"length");uint32_t count=0;
            const bool ok=JS_IsArray(vertices)&&JS_ToUint32(c,&count,length)==0&&count>=3&&count<=64;JS_FreeValue(c,length);
            if(!ok){JS_FreeValue(c,vertices);return false;}
            std::vector<glm::dvec2> points;
            for(uint32_t i=0;i<count;++i){auto p=JS_GetPropertyUint32(c,vertices,i);float x=0,y=0;
                const bool valid=KnownOptions(c,p,{"x","y"})&&Number(c,p,"x",x)&&Number(c,p,"y",y);JS_FreeValue(c,p);
                if(!valid){JS_FreeValue(c,vertices);return false;}points.push_back({x,y});}
            JS_FreeValue(c,vertices);out=PlanarConvex(std::move(points));
        } else return false;
        return true;
    } catch(const std::exception&) {return false;}
}
JSValue PlanarValue(JSContext* c,const PlanarCollider& shape) {
    auto out=JS_NewObject(c);
    const char* kinds[]={"circle","box","capsule","convex","chain"};
    JS_SetPropertyStr(c,out,"kind",JS_NewString(c,kinds[int(shape.kind)]));
    JS_SetPropertyStr(c,out,"radius",JS_NewFloat64(c,shape.radius));
    JS_SetPropertyStr(c,out,"lockRotation",JS_NewBool(c,shape.lockRotation));
    JS_SetPropertyStr(c,out,"oneWay",JS_NewBool(c,shape.oneWay));
    auto vertices=JS_NewArray(c);uint32_t index=0;
    for(auto p:shape.vertices){auto v=JS_NewObject(c);JS_SetPropertyStr(c,v,"x",JS_NewFloat64(c,p.x));JS_SetPropertyStr(c,v,"y",JS_NewFloat64(c,p.y));JS_SetPropertyUint32(c,vertices,index++,v);}
    JS_SetPropertyStr(c,out,"vertices",vertices);
    return out;
}
