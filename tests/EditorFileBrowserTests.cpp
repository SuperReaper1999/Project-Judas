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
    fs::create_directories(root/"models"/"nested");std::ofstream(root/"level.judas")<<"fixture";std::ofstream(root/"note.txt")<<"fixture";
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
    auto frame=[&](){ImGui::NewFrame();auto value=browser.Draw();ImGui::Render();return value;};
    frame();frame(); // Allow first-appearance modal placement to settle.
    // Find the actual child instead of depending on ImGui's generated ID suffix.
    ImGuiWindow* files=nullptr;
    for(auto* window:ImGui::GetCurrentContext()->Windows)
        if(window->ParentWindow==dialog&&(window->Flags&ImGuiWindowFlags_ChildWindow))files=window;
    check(files!=nullptr,"actual folder listing is available for pointer navigation");
    const ImVec2 firstRow={files->Pos.x+35,files->Pos.y+io.Fonts->Fonts[0]->FontSize};
    io.AddMousePosEvent(firstRow.x,firstRow.y);frame();
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    check(browser.Active()&&!ImGui::GetCurrentContext()->OpenPopupStack.empty(),"single-click selects folder without dismissing popup");
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    ImGui::ActivateItemByID(dialog->GetID("Choose folder"));
    auto navigated=frame();if(!navigated)navigated=frame();
    check(navigated&&fs::equivalent(fs::u8path(navigated->path),root/"models"),"double-click enters folder in the same picker before confirmation");
    browser.Open(EditorFileBrowser::Target::NewProject,EditorFileBrowser::Mode::Directory,(root/"models").string());frame();frame();
    for(auto* window:ImGui::GetCurrentContext()->Windows)
        if(window->ParentWindow==dialog&&(window->Flags&ImGuiWindowFlags_ChildWindow))files=window;
    ImGui::ActivateItemByID(files->GetID("[folder] nested"));frame();frame();
    ImGui::ActivateItemByID(dialog->GetID("Open folder"));frame();frame();
    // Parent distinguishes actual navigation from merely choosing a child's path.
    ImGui::ActivateItemByID(dialog->GetID("Parent"));frame();frame();
    ImGui::ActivateItemByID(dialog->GetID("Choose folder"));
    std::optional<EditorFileBrowser::Selection> selected;
    for(int i=0;i<2;++i){ImGui::NewFrame();if(auto value=browser.Draw())selected=value;ImGui::Render();}
    check(selected&&selected->target==EditorFileBrowser::Target::NewProject&&fs::equivalent(fs::u8path(selected->path),root/"models")&&!browser.Active(),"Open folder enters nested directory, Parent returns, and Choose confirms");
    ImGui::DestroyContext();fs::remove_all(root);
    std::cout<<"Editor file browser: "<<checks<<" checks, 0 failures\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
