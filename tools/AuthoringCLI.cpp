#include "AuthoringCLI.h"
#include "ContentReferences.h"
#include "NamedAuthoring.h"
#include "AuthoringDocument.h"
#include "WorldBuilder.h"
#include "Project.h"
#include "SceneSerialization.h"
#include "Scene.h"
#include "RuntimeUI.h"
#include "ScriptSystem.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <set>
using J=nlohmann::ordered_json;

namespace {
std::string read(const std::string& p){std::ifstream f(p,std::ios::binary);
if(!f)throw std::runtime_error("cannot read "+p);
std::ostringstream s;
s<<f.rdbuf();
return s.str();
}
std::string named(const std::string& text,const std::string& kind){
    std::string result,error,legacy=text;

    if(kind=="recipe"){WorldRecipe r;
if(!ParseWorldRecipe(text,r,error))throw std::runtime_error(error);
return SerializeWorldRecipe(r);
}
    if(IsNamedDocument(text)&&!NamedToLegacy(text,kind,legacy,error))throw std::runtime_error(error);

    if(!LegacyToNamed(legacy,kind,result,error))throw std::runtime_error(error);

    return result;

}
bool valid(const std::string& text,const std::string& kind,std::string& error){if(kind=="recipe"){WorldRecipe r;
return ParseWorldRecipe(text,r,error);
}return ValidateAuthoredDocument(text,kind,error);
}
}
int RunAuthoringCLI(int argc,char** argv){
    if(argc<2)return -1;

    std::string command=argv[1];

    if(command!="--validate"&&command!="--inspect"&&command!="--convert"&&command!="--edit"&&command!="--patch"&&command!="--diff"&&command!="--create"&&command!="--references"&&command!="--dependencies")return -1;

    try{
        if(argc<4)throw std::runtime_error("--validate|--inspect kind file; --convert kind input output named|legacy; --edit kind input output /pointer JSONvalue; --patch kind input output patch.json; --diff kind left right; --create kind output; --references|--dependencies kind file. Publication requires --overwrite for existing destinations; --dry-run never writes.");

        std::string kind=argv[2],error;
bool overwrite=false,dry=false;

        for(int i=4;i<argc;++i){overwrite|=std::string(argv[i])=="--overwrite";
dry|=std::string(argv[i])=="--dry-run";
}
        if(command=="--validate"){if(!valid(read(argv[3]),kind,error))throw std::runtime_error(std::string(argv[3])+": "+error);
std::cout<<"valid "<<kind<<" "<<argv[3]<<'\n';
return 0;
}
        if(command=="--inspect"){std::cout<<named(read(argv[3]),kind);
return 0;
}
        if(command=="--diff"){if(argc!=5)throw std::runtime_error("--diff kind left right");
auto a=J::parse(named(read(argv[3]),kind)),b=J::parse(named(read(argv[4]),kind));
std::cout<<J::diff(a,b).dump(2)<<'\n';
return 0;
}
        if(command=="--dependencies"){
            std::set<AssetId> ids;if(!DirectDocumentAssets(kind,read(argv[3]),ids,error))throw std::runtime_error(error);
            for(const auto& id:ids)std::cout<<id<<'\n';
            std::cout<<"Typed direct asset references; dynamic script IDs use M38 all-registered asset policy. Use --build-project for complete export validation.\n";return 0;

        }
        if(command=="--references"){
            if(kind!="scene"&&kind!="prefab")throw std::runtime_error("references requires scene or prefab");

            Scene scene;
if(!LoadSceneFromFile(argv[3],scene,error))throw std::runtime_error(error);

            auto ref=[](SceneObjectId from,SceneObjectId to,const char* field){if(to)std::cout<<from<<" / "<<field<<" -> "<<to<<'\n';
};

            for(auto& o:scene.Objects()){ref(o.id,o.parent,"parent");
if(o.socket)ref(o.id,o.socket->target,"socket");
if(o.render)ref(o.id,o.render->textureCamera,"textureCamera");
if(o.joint){ref(o.id,o.joint->bodyA,"bodyA");
ref(o.id,o.joint->bodyB,"bodyB");
}if(o.liquidConnection){ref(o.id,o.liquidConnection->source,"source");
ref(o.id,o.liquidConnection->destination,"destination");
}if(o.deformable)for(auto& a:o.deformable->attachments)ref(o.id,a.target,"attachment");
for(auto& slot:o.scripts)for(auto target:ScriptSystem::PropertyEntities(slot.properties))ref(o.id,target,"script");
}return 0;

        }
        std::string destination=command=="--create"?argv[3]:(argc>4?argv[4]:"");
        if(destination.empty())throw std::runtime_error("missing destination");
        const bool existed=std::filesystem::exists(destination);
        const auto expected=existed?read(destination):"";
        std::string text,inputPath,inputBytes;

        if(command=="--create"){
            destination=argv[3];
std::string legacy;

            if(kind=="recipe")text=SerializeWorldRecipe(WorldRecipe{});

            else {
                if(kind=="scene"||kind=="prefab"){Scene s;
s.Settings().name="New document";
if(kind=="prefab")s.CreateObject("Root");
SaveSceneToString(s,legacy);
}
                else if(kind=="project"){ProjectSettings p;
p.name="New project";
p.legacyGameplay=false;
legacy=Project::SerializeToString(p);
}
                else if(kind=="ui"){UIDocument d;
UIElement e;
e.id="canvas";
e.kind=UIKind::Canvas;
d.elements.push_back(e);
legacy=SerializeUIDocument(d);
}
                else if(kind=="input")legacy=InputMap::Defaults().Serialize();

                else if(kind=="world")legacy="JudasWorld 1\nbudget 2 16 2 33554432 67108864 4194304 67108864\n";

                else throw std::runtime_error("unknown document template "+kind);

                text=named(legacy,kind);

            }
        }else{
            inputPath=argv[3];
inputBytes=read(inputPath);

            if(command=="--convert"){
                if(argc<6)throw std::runtime_error("--convert kind input output named|legacy");

                destination=argv[4];

                if(std::string(argv[5])=="named")text=named(inputBytes,kind);

                else if(std::string(argv[5])=="legacy"){text=inputBytes;
if(IsNamedDocument(text)&&!NamedToLegacy(inputBytes,kind,text,error))throw std::runtime_error(error);
}
                else throw std::runtime_error("format must be named or legacy");

            }else{
                if(argc<(command=="--patch"?6:7))throw std::runtime_error("--edit kind input output /pointer JSONvalue; --patch kind input output patch.json");

                destination=argv[4];
auto j=J::parse(named(inputBytes,kind));

                if(command=="--patch"){auto patch=J::parse(read(argv[5]));
if(!patch.is_array()||patch.size()>256)throw std::runtime_error("patch requires at most 256 operations");
for(auto& op:patch){std::string action=op.at("op");
if(action!="add"&&action!="replace"&&action!="remove"&&action!="test")throw std::runtime_error("supported patch operations: add/replace/remove/test");
}j=j.patch(patch);
}
                else {auto pointer=J::json_pointer(argv[5]);
if(!j.contains(pointer))throw std::runtime_error("unknown field path "+std::string(argv[5]));
j[pointer]=J::parse(argv[6]);
}
                text=j.dump(2)+"\n";

            }
        }

        if(!valid(text,kind,error))throw std::runtime_error(destination+": "+error);

        if(dry){std::cout<<"validated dry run; no publication\n";
return 0;
}
        if(existed&&!overwrite)throw std::runtime_error("destination exists; use --overwrite after review");

        if(!inputPath.empty()&&read(inputPath)!=inputBytes)throw std::runtime_error("input changed externally during validation");

        if(!PublishAuthoringOutputs(std::filesystem::path(destination).parent_path().string()+"/.authoring-recovery",{{destination,text,expected,existed}},error))throw std::runtime_error(error);

        std::cout<<"wrote "<<destination<<'\n';
return 0;

    }catch(const std::exception& e){std::cerr<<"authoring: "<<e.what()<<'\n';
return 1;
}
}
