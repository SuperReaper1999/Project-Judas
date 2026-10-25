#include "EditorPrimitiveAssets.h"
#include "EditorDocument.h"
#include "AssetDatabase.h"
#include "Project.h"
#include "CollisionAsset.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs=std::filesystem;
namespace {
std::string PrimitiveOBJ(bool capsule) {
    constexpr unsigned segments=16;
    constexpr double pi=3.14159265358979323846;
    std::vector<glm::dvec2> profile;
    if(capsule){
        profile.push_back({0,-1});
        for(unsigned i=1;i<=3;++i){const double a=-pi/2+i*pi/6;profile.push_back({.5*std::cos(a),-.5+.5*std::sin(a)});}
        profile.push_back({.5,.5});
        for(unsigned i=1;i<3;++i){const double a=i*pi/6;profile.push_back({.5*std::cos(a),.5+.5*std::sin(a)});}
        profile.push_back({0,1});
    }else profile={{0,-1},{.5,-1},{.5,1},{0,1}};
    std::ostringstream out;out<<std::setprecision(17)<<"# Judas editor primitive v1; metres, local Y axis\n";
    std::vector<std::vector<unsigned>> rings;unsigned next=1;
    for(const auto& p:profile){
        std::vector<unsigned> ring;
        const unsigned count=p.x==0?1:segments;
        for(unsigned j=0;j<count;++j){const double a=2*pi*j/segments;
            out<<"v "<<p.x*std::cos(a)<<' '<<p.y<<' '<<p.x*std::sin(a)<<'\n';ring.push_back(next++);}
        rings.push_back(std::move(ring));
    }
    out<<"s 1\n";
    for(size_t i=1;i<rings.size();++i){const auto& low=rings[i-1];const auto& high=rings[i];
        for(unsigned j=0;j<segments;++j){const auto k=(j+1)%segments;
            if(low.size()==1)out<<"f "<<low[0]<<' '<<high[j]<<' '<<high[k]<<'\n';
            else if(high.size()==1)out<<"f "<<low[j]<<' '<<high[0]<<' '<<low[k]<<'\n';
            else out<<"f "<<low[j]<<' '<<high[j]<<' '<<high[k]<<'\n'<<"f "<<low[j]<<' '<<high[k]<<' '<<low[k]<<'\n';
        }
    }
    return out.str();
}
}

SceneObjectId CreateEditorPrimitive(EditorDocument& doc,const Project& project,AssetDatabase& assets,
    const std::string& kind,glm::vec3 position,SceneObjectId parent,std::string& error){
    error.clear();
    try{
        if(kind!="capsule"&&kind!="cylinder")throw std::runtime_error("Unknown generated primitive.");
        if(!project.IsLoaded())throw std::runtime_error("Open a project before creating mesh primitives.");
        if(parent&&!doc.GetScene().Find(parent))throw std::runtime_error("The chosen parent no longer exists.");
        const auto root=fs::canonical(fs::u8path(project.AssetsDir()));
        if(root!=fs::canonical(fs::u8path(assets.AssetsDir())))throw std::runtime_error("Asset database belongs to another project.");
        auto directory=root;
        for(const auto* part:{"Generated","Primitives"}){
            directory/=part;if(fs::is_symlink(fs::symlink_status(directory)))throw std::runtime_error("Generated primitive folders must not be symbolic links.");
            fs::create_directories(directory);
        }
        const auto model=directory/(kind+"-v1.obj"), collision=directory/(kind+"-v1.judascollision");
        const auto bytes=PrimitiveOBJ(kind=="capsule");
        if(fs::exists(model)){
            std::ifstream existing(model,std::ios::binary);const std::string actual{std::istreambuf_iterator<char>(existing),{}};
            if(actual!=bytes)throw std::runtime_error("Generated primitive was edited; refusing to overwrite "+model.u8string());
        }else{
            // Exclusive publication preserves existing files, including dangling symlinks.
            const auto temporary=directory/(MintAssetId()+".obj");
            {std::ofstream file(temporary,std::ios::binary);file<<bytes;file.close();if(!file)throw std::runtime_error("Cannot write primitive mesh.");}
            std::error_code ec;fs::copy_file(temporary,model,fs::copy_options::none,ec);fs::remove(temporary);
            if(ec)throw std::runtime_error("Cannot publish primitive mesh: "+ec.message());
        }
        AssetRecord mesh,shape;
        if(const auto* found=assets.FindByRelativePath(fs::relative(model,project.RootDir()).generic_string()))mesh=*found;
        else if(!assets.Track(model.u8string(),mesh,error))return 0;
        CollisionAsset cooked;
        if(fs::exists(collision)){
            std::string reason;
            if(!LoadCollisionAsset(collision.u8string(),cooked,error)||!cooked.convex||
               cooked.sourceAsset!=mesh.id||CollisionAssetStale(cooked,model.u8string(),reason))
                throw std::runtime_error("Existing primitive collision is modified or stale; refusing to replace it. "+error+reason);
        }else{
            CollisionCookSettings settings;settings.convex=true;
            if(!CookCollisionFile(model.u8string(),mesh.id,settings,collision.u8string(),cooked,error))return 0;
        }
        if(const auto* found=assets.FindByRelativePath(fs::relative(collision,project.RootDir()).generic_string()))shape=*found;
        else if(!assets.Track(collision.u8string(),shape,error))return 0;
        doc.BeginEdit();auto& object=doc.GetScene().CreateObject(kind=="capsule"?"Capsule":"Cylinder");const auto id=object.id;
        object.parent=parent;object.transform.position=parent?glm::vec3(0):position;
        object.render=SceneRenderComponent{};object.render->shape=SceneShape::Mesh;object.render->meshAsset=mesh.id;
        object.body=SceneBodyComponent{};object.body->shape=SceneShape::ConvexHull;object.body->collisionAsset=shape.id;object.body->mass=5;
        doc.CommitEdit();if(!doc.ValidationError().empty()){error=doc.ValidationError();return 0;}
        doc.Select(id);return id;
    }catch(const std::exception& e){error=e.what();return 0;}
}
