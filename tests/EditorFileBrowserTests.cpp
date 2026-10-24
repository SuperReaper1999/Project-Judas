#include "editor/FileBrowser.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
int main(){try{
    unsigned checks=0;auto check=[&](bool okay,const char* label){++checks;if(!okay)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';};
    const auto root=fs::temp_directory_path()/("judas-picker-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"models");std::ofstream(root/"level.judas")<<"fixture";std::ofstream(root/"note.txt")<<"fixture";
    std::string error,path;auto entries=EditorFileBrowser::List(root,".judas",error);
    check(error.empty()&&entries.size()==2&&entries.front().directory,"picker lists directories first and filters files");
    check(!EditorFileBrowser::List(root/"absent","",error).size()&&!error.empty(),"unavailable directory gives actionable error");
    check(EditorFileBrowser::Select(root,"level.judas",EditorFileBrowser::Mode::OpenFile,".judas",path,error),"select existing scene");
    check(!EditorFileBrowser::Select(root,"note.txt",EditorFileBrowser::Mode::OpenFile,".judas",path,error),"wrong file type rejected");
    check(!EditorFileBrowser::Select(root,"missing.judas",EditorFileBrowser::Mode::OpenFile,".judas",path,error),"missing open-file rejected");
    check(EditorFileBrowser::Select(root,"new.judas",EditorFileBrowser::Mode::SaveFile,".judas",path,error)&&!fs::exists(root/"new.judas"),"save-path choice never creates or overwrites files");
    check(EditorFileBrowser::Select(root,"New Game",EditorFileBrowser::Mode::Directory,"",path,error)&&!fs::exists(root/"New Game"),"new project/export folder choice is non-mutating");
    check(!EditorFileBrowser::Select(root,"note.txt",EditorFileBrowser::Mode::Directory,"",path,error),"file cannot masquerade as project directory");
    check(!EditorFileBrowser::Select(root,"missing/inner.judas",EditorFileBrowser::Mode::SaveFile,".judas",path,error),"save cannot target missing parent");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1000,800};io.DeltaTime=1.f/60;
    unsigned char* data;int w,h;io.Fonts->GetTexDataAsRGBA32(&data,&w,&h);
    EditorFileBrowser browser;browser.Open(EditorFileBrowser::Target::NewProject,EditorFileBrowser::Mode::Directory,root.string());
    ImGui::NewFrame();browser.Draw();ImGui::Render();check(browser.Active(),"real ImGui picker opens without creating any files");
    auto* dialog=ImGui::FindWindowByName("Choose a path");check(dialog!=nullptr,"actual path-picker dialog is available");
    ImGui::ActivateItemByID(dialog->GetID("Choose folder"));
    std::optional<EditorFileBrowser::Selection> selected;
    for(int i=0;i<2;++i){ImGui::NewFrame();if(auto value=browser.Draw())selected=value;ImGui::Render();}
    check(selected&&selected->target==EditorFileBrowser::Target::NewProject&&fs::equivalent(fs::u8path(selected->path),root)&&!browser.Active(),"actual ImGui choice returns the selected folder and closes the picker");
    ImGui::DestroyContext();fs::remove_all(root);
    std::cout<<"Editor file browser: "<<checks<<" checks, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
