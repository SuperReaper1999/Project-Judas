#include "BasicWater.h"
#include "AuthoringJSON.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
bool Finite(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool Inside(const BasicWaterInstance& w, glm::vec3 point) {
    const auto p = glm::inverse(w.rotation) * (point - w.position);
    const auto h = w.settings.halfExtents;
    return std::abs(p.x) <= h.x && p.y <= 0 && p.y >= -h.y && std::abs(p.z) <= h.z;
}
}
bool ValidateBasicWater(const BasicWaterSettings& s, std::string& error) {
    if (!Finite(s.halfExtents) || glm::any(glm::lessThan(s.halfExtents, glm::vec3(.01f))) ||
        glm::any(glm::greaterThan(s.halfExtents, glm::vec3(100000))) ||
        !Finite(s.color) || glm::any(glm::lessThan(s.color, glm::vec3(0))) ||
        glm::any(glm::greaterThan(s.color, glm::vec3(1))) || !Finite(s.flow) ||
        glm::length(s.flow) > 100 || !std::isfinite(s.waveAmplitude) ||
        s.waveAmplitude < 0 || s.waveAmplitude > std::min(5.f, s.halfExtents.y) ||
        !std::isfinite(s.wavelength) || s.wavelength < .1f || s.wavelength > 10000 ||
        !std::isfinite(s.waveSpeed) || std::abs(s.waveSpeed) > 20 ||
        !std::isfinite(s.opacity) || s.opacity < 0 || s.opacity > 1 ||
        !std::isfinite(s.density) || s.density <= 0 || s.density > 10000) {
        error = "basic water: invalid dimensions, waves, colour, flow or density";
        return false;
    }
    error.clear(); return true;
}
std::string EncodeBasicWater(const BasicWaterSettings& s) {
    return AuthoringJSON::J{{"version",1},{"enabled",s.enabled},
        {"halfExtents",{s.halfExtents.x,s.halfExtents.y,s.halfExtents.z}},
        {"color",{s.color.x,s.color.y,s.color.z}}, {"flow",{s.flow.x,s.flow.y,s.flow.z}},
        {"waveAmplitude",s.waveAmplitude},{"wavelength",s.wavelength},
        {"waveSpeed",s.waveSpeed},{"opacity",s.opacity},{"density",s.density}}.dump();
}
bool DecodeBasicWater(const std::string& text, BasicWaterSettings& out, std::string& error) {
    try {
        if (text.size()>4096) throw std::runtime_error("basic water exceeds text budget");
        auto j=AuthoringJSON::J::parse(text); BasicWaterSettings s;
        const auto schema=AuthoringJSON::J::parse(EncodeBasicWater(s));
        if (!j.is_object()) throw std::runtime_error("basic water requires an object");
        for (auto it=j.begin();it!=j.end();++it)
            if (!schema.contains(it.key())) throw std::runtime_error("unknown basic water key: "+it.key());
        if (!j.contains("version") || j["version"]!=1) throw std::runtime_error("unsupported basic water version");
        if (j.contains("enabled")) {
            if (!j["enabled"].is_boolean()) throw std::runtime_error("basic water enabled requires boolean");
            s.enabled=j["enabled"].get<bool>();
        }
        auto vector=[&](const char* key,glm::vec3& value) {
            if (!j.contains(key)) return;
            const auto& a=j[key]; if (!a.is_array() || a.size()!=3) throw std::runtime_error("basic water requires three-vector");
            for (unsigned i=0;i<3;++i) { if (!a[i].is_number()) throw std::runtime_error("basic water vector requires numbers"); value[i]=a[i].get<float>(); }
        };
        vector("halfExtents",s.halfExtents);vector("color",s.color);vector("flow",s.flow);
        for (auto item : {std::pair<const char*,float*>{"waveAmplitude",&s.waveAmplitude},
            {"wavelength",&s.wavelength},{"waveSpeed",&s.waveSpeed},{"opacity",&s.opacity},{"density",&s.density}})
            if (j.contains(item.first)) { if (!j[item.first].is_number()) throw std::runtime_error("basic water parameter requires number"); *item.second=j[item.first].get<float>(); }
        if (!ValidateBasicWater(s,error)) return false;
        out=s;error.clear();return true;
    } catch (const std::exception& e) {error=e.what();return false;}
}
BasicWaterSample SampleBasicWater(const std::vector<BasicWaterInstance>& waters,
                                 glm::vec3 center, glm::vec3 up, float halfHeight, float radius) {
    BasicWaterSample result;
    if (!Finite(center) || !Finite(up) || glm::length(up)<1e-6f ||
        !std::isfinite(halfHeight) || halfHeight<0 || !std::isfinite(radius) || radius<=0) return result;
    up=glm::normalize(up);
    // Bounded centre-line quadrature of the capsule envelope, independent of
    // world Y. This is swimming occupancy, not a displaced-volume calculation.
    constexpr unsigned samples=17;
    const float extent=halfHeight+radius;
    for (const auto& w:waters) if (w.settings.enabled) {
        unsigned wet=0;
        for (unsigned i=0;i<samples;++i)
            wet+=Inside(w,center+up*(extent*(2*(float(i)+.5f)/samples-1)));
        const float fraction=float(wet)/samples;
        // Overlapping prescribed water does not double buoyancy or currents.
        if (fraction>result.immersion) result={fraction,w.settings.density,w.rotation*w.settings.flow};
    }
    return result;
}
std::vector<glm::vec2> BasicWaterOpticalPaths(const std::vector<BasicWaterInstance>& waters,
    const glm::mat4& view,const glm::mat4& projection,unsigned columns,unsigned rows) {
    if (!columns || !rows || columns>256 || rows>256) return {};
    std::vector<glm::vec2> result(size_t(columns)*rows,glm::vec2(0));
    const glm::mat4 inverse=glm::inverse(projection*view);
    for (unsigned y=0;y<rows;++y) for (unsigned x=0;x<columns;++x) {
        glm::vec2 uv((x+.5f)/columns,(y+.5f)/rows);
        auto a=inverse*glm::vec4(uv*2.f-1.f,-1,1),b=inverse*glm::vec4(uv*2.f-1.f,1,1);
        if (std::abs(a.w)<1e-12f || std::abs(b.w)<1e-12f) continue;
        const glm::vec3 start=glm::vec3(a)/a.w,end=glm::vec3(b)/b.w;
        const float distance=glm::length(end-start);if (!(distance>0)) continue;
        const auto direction=(end-start)/distance;
        float first=distance,last=0;
        for (const auto& w:waters) if (w.settings.enabled) {
            const auto inv=glm::inverse(w.rotation);const auto p=inv*(start-w.position),d=inv*direction;
            const auto h=w.settings.halfExtents;const glm::vec3 lo(-h.x,-h.y,-h.z),hi(h.x,0,h.z);
            float enter=0,exit=distance;
            for (unsigned axis=0;axis<3;++axis) {
                if (std::abs(d[axis])<1e-8f) { if(p[axis]<lo[axis]||p[axis]>hi[axis]) {exit=-1;break;} }
                else {float u=(lo[axis]-p[axis])/d[axis],v=(hi[axis]-p[axis])/d[axis];
                    enter=std::max(enter,std::min(u,v));exit=std::min(exit,std::max(u,v));}
            }
            if (exit>enter && enter<first) {first=enter;last=exit;}
        }
        if(last>first)result[size_t(y)*columns+x]={first,last};
    }
    return result;
}
