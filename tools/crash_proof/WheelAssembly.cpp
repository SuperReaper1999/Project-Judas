#include "WheelAssembly.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include "../../third_party/nlohmann/json.hpp"

bool ParseCrashPartRoles(const std::string& text, CrashPartRoles& result, std::string& error) {
    try {
        const auto document = nlohmann::json::parse(text);
        if (!document.is_object() || document.value("schema", 0) != 1 ||
            !document.contains("parts") || !document.at("parts").is_object() ||
            document.at("parts").empty())
            throw std::runtime_error("part roles require schema 1 and a nonempty parts object");
        CrashPartRoles next;
        for (const auto& entry : document.at("parts").items()) {
            if (entry.key().empty() || !entry.value().is_string())
                throw std::runtime_error("part roles require nonempty identities and string roles");
            const auto role = entry.value().get<std::string>();
            if (role != "body" && role != "wheel" && role != "caliper")
                throw std::runtime_error("unknown part role: " + role);
            next.emplace(entry.key(), role == "body" ? CrashPartRole::Body :
                         role == "wheel" ? CrashPartRole::Wheel : CrashPartRole::Caliper);
        }
        result = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

CrashPartRole ResolveCrashPartRole(const std::string& part, const CrashPartRoles& roles) {
    if (!roles.empty()) {
        const auto found = roles.find(part);
        if (found == roles.end()) throw std::runtime_error("missing authored part role: " + part);
        return found->second;
    }
    auto name = part;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    const bool caliper = name.find("calliperexotic") != std::string::npos ||
                         name.find("calliperbadge") != std::string::npos ||
                         name.find("caliper_body") != std::string::npos ||
                         name.find("_caliper_") != std::string::npos;
    if (caliper) return CrashPartRole::Caliper;
    const bool rotating = name.find("wheel") != std::string::npos ||
                          name.find("tyre") != std::string::npos ||
                          name.find("tire") != std::string::npos ||
                          name.find("/rim") != std::string::npos ||
                          name.find("_rim") != std::string::npos ||
                          name.find("spoke") != std::string::npos ||
                          name.find("_brake_") != std::string::npos ||
                          name.find("_brakedisc_") != std::string::npos;
    return rotating ? CrashPartRole::Wheel : CrashPartRole::Body;
}
CrashWheelMount PrepareCrashWheel(MeshData& rotating, MeshData& brakes) {
    using V=glm::dvec3; using M=glm::dmat3;
    if(rotating.indices.empty())throw std::runtime_error("wheel has no rotating geometry");
    double area=0; V first(0); M second(0);
    for(size_t t=0;t<rotating.indices.size();t+=3){
        V v[3]; for(int k=0;k<3;++k)v[k]=rotating.vertices.at(rotating.indices[t+k]).position;
        double a=glm::length(glm::cross(v[1]-v[0],v[2]-v[0]))*.5;
        V sum=v[0]+v[1]+v[2]; M d(0);for(auto p:v)d+=glm::outerProduct(p,p);
        area+=a; first+=a*sum/3.;second+=a*(d+glm::outerProduct(sum,sum))/12.;
    }
    if(area<1e-10)throw std::runtime_error("degenerate wheel surface");
    V mean=first/area;M covariance=second/area-glm::outerProduct(mean,mean),vectors(1);
    // Symmetric Jacobi eigensolve, bounded independently of mesh size.
    for(unsigned step=0;step<32;++step){
        unsigned p=0,q=1;double largest=std::abs(covariance[p][q]);
        for(unsigned i=0;i<3;++i)for(unsigned j=i+1;j<3;++j)
            if(std::abs(covariance[i][j])>largest){p=i;q=j;largest=std::abs(covariance[i][j]);}
        if(largest<1e-15)break;
        double angle=.5*std::atan2(2*covariance[p][q],covariance[q][q]-covariance[p][p]);
        M r(1); r[p][p]=r[q][q]=std::cos(angle);r[q][p]=std::sin(angle);r[p][q]=-std::sin(angle);
        covariance=glm::transpose(r)*covariance*r;vectors=vectors*r;
    }
    unsigned axis=0;for(unsigned i=1;i<3;++i)if(covariance[i][i]<covariance[axis][axis])axis=i;
    V x=glm::normalize(vectors[axis]);if(x.x<0)x=-x;
    if(std::abs(x.x)<.8)throw std::runtime_error("wheel axle does not match vehicle lateral axis; explicit authoring required");
    V y=glm::normalize(V(0,1,0)-x*glm::dot(x,V(0,1,0)));V z=glm::cross(x,y);M basis(x,y,z);
    V lo(1e20),hi(-1e20);
    for(const auto& v:rotating.vertices){auto p=glm::transpose(basis)*(V(v.position)-mean);lo=glm::min(lo,p);hi=glm::max(hi,p);}
    V center=mean+basis*((lo+hi)*.5);
    CrashWheelMount result;result.center=glm::vec3(center);result.axleFrame=glm::quat_cast(glm::mat3(basis));
    result.radius=float(std::max(hi.y-lo.y,hi.z-lo.z)*.5);result.width=float(hi.x-lo.x);
    if(result.width>=result.radius*1.8f||result.radius<=0)throw std::runtime_error("wheel fit is not a plausible cylinder");
    for(auto* mesh:{&rotating,&brakes})for(auto& v:mesh->vertices){
        v.position=glm::vec3(glm::transpose(basis)*(V(v.position)-center));
        v.normal=glm::vec3(glm::transpose(basis)*V(v.normal));
        v.tangent=glm::vec4(glm::vec3(glm::transpose(basis)*V(glm::vec3(v.tangent))),v.tangent.w);
    }
    return result;
}
