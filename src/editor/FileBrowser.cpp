#include "FileBrowser.h"
#include "imgui.h"
#include <algorithm>
#include <cstring>

namespace fs=std::filesystem;
namespace {
std::string Text(const fs::path& path) { return path.u8string(); }
void Buffer(const std::string& text,char* buffer,size_t size) {
    const auto n=std::min(size-1,text.size());std::memcpy(buffer,text.data(),n);buffer[n]=0;
}
}
std::vector<EditorFileBrowser::Entry> EditorFileBrowser::List(
    const fs::path& directory,const std::string& extension,std::string& error) {
    error.clear();std::vector<Entry> entries;std::error_code ec;
    fs::directory_iterator it(directory,ec),end;
    if(ec){error=ec.message();return entries;}
    size_t examined=0;
    for(;it!=end;it.increment(ec)) {
        if(++examined>1024){error="Listing limited to 1024 directory entries. Refine the folder or enter a file name.";break;}
        if(ec){error=ec.message();break;}
        const bool folder=it->is_directory(ec);if(ec){ec.clear();continue;}
        if(folder||extension.empty()||it->path().extension()==fs::u8path(extension))
            entries.push_back({Text(it->path().filename()),folder});
        if(entries.size()>=1024){error="Showing the first 1024 entries. Open a more specific folder.";break;}
    }
    std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){
        return a.directory!=b.directory?a.directory>b.directory:a.name<b.name;
    });
    return entries;
}
bool EditorFileBrowser::Select(const fs::path& directory,const std::string& name,Mode mode,
    const std::string& extension,std::string& selected,std::string& error) {
    error.clear();std::error_code ec;
    auto path=name.empty()?directory:directory/fs::u8path(name);
    path=fs::absolute(path,ec).lexically_normal();
    if(ec){error=ec.message();return false;}
    if(mode==Mode::Directory) {
        if(fs::exists(path,ec)&&!fs::is_directory(path,ec)){error="Choose a directory.";return false;}
        if(!fs::is_directory(path.parent_path(),ec)){error="The parent directory does not exist.";return false;}
    } else {
        if(name.empty()){error="Choose a file name.";return false;}
        if(!extension.empty()&&path.extension()!=fs::u8path(extension)){
            error="Choose a "+extension+" file.";return false;
        }
        if(mode==Mode::OpenFile&&!fs::is_regular_file(path,ec)){
            error="The selected file does not exist.";return false;
        }
        if(mode==Mode::SaveFile&&(!fs::is_directory(path.parent_path(),ec)||fs::is_directory(path,ec))){
            error="Choose a file in an existing directory.";return false;
        }
    }
    selected=Text(path);return true;
}
void EditorFileBrowser::Open(Target target,Mode mode,const std::string& path,std::string extension) {
    m_target=target;m_mode=mode;m_extension=std::move(extension);m_name.clear();m_error.clear();
    std::error_code ec;auto start=fs::absolute(fs::u8path(path.empty()?".":path),ec);
    if(ec)start=fs::current_path(ec);
    if(fs::is_directory(start,ec))m_directory=start;
    else {m_directory=start.parent_path();m_name=Text(start.filename());}
    while(!m_directory.empty()&&!fs::is_directory(m_directory,ec))m_directory=m_directory.parent_path();
    if(m_directory.empty())m_directory=fs::current_path(ec);
    m_location=Text(m_directory);m_refresh=true;m_open=true;m_active=true;
}
std::optional<EditorFileBrowser::Selection> EditorFileBrowser::Draw() {
    if(m_open){ImGui::OpenPopup("Choose a path");m_open=false;}
    ImGui::SetNextWindowSize({700,510},ImGuiCond_Appearing);
    if(!ImGui::BeginPopupModal("Choose a path",nullptr,ImGuiWindowFlags_NoSavedSettings))return {};
    std::optional<Selection> result;
    auto navigate=[&](const fs::path& next){
        std::error_code ec;
        auto absolute=fs::absolute(next,ec).lexically_normal();
        if(ec||!fs::is_directory(absolute,ec)){m_error="Directory unavailable.";return;}
        m_directory=absolute;m_location=Text(m_directory);m_refresh=true;m_name.clear();m_error.clear();
    };
    char location[4096];Buffer(m_location,location,sizeof(location));
    ImGui::SetNextItemWidth(-70);
    const bool go=ImGui::InputText("##location",location,sizeof(location),ImGuiInputTextFlags_EnterReturnsTrue);
    m_location=location;ImGui::SameLine();
    if(ImGui::Button("Go")||go)navigate(fs::u8path(m_location));
    if(ImGui::Button("Parent")&&m_directory.has_parent_path()){
        navigate(m_directory.parent_path());
    }
    ImGui::SameLine();ImGui::TextDisabled("%s",m_extension.empty()?"All files":m_extension.c_str());
    ImGui::SameLine();if(ImGui::Button("Refresh"))m_refresh=true;
    if(m_refresh){m_entries=List(m_directory,m_extension,m_listingError);m_refresh=false;}
    ImGui::TextDisabled("Double-click a folder to open it; Choose confirms your selection.");
    ImGui::BeginChild("Files",{0,-110},true);
    for(const auto& entry:m_entries){
        const auto label=(entry.directory?"[folder] ":"         ")+entry.name;
        if(ImGui::Selectable(label.c_str(),m_name==entry.name,
            ImGuiSelectableFlags_AllowDoubleClick|ImGuiSelectableFlags_NoAutoClosePopups)){
            m_name=entry.name;
            if(entry.directory&&ImGui::IsMouseDoubleClicked(0)){
                navigate(m_directory/fs::u8path(entry.name));
                break;
            }
        }
    }
    ImGui::EndChild();
    char name[512];Buffer(m_name,name,sizeof(name));
    ImGui::SetNextItemWidth(-1);if(ImGui::InputTextWithHint("##name",m_mode==Mode::Directory?"Optional new folder name":"File name",name,sizeof(name)))m_name=name;
    const auto message=m_error.empty()?m_listingError:m_error;
    if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
    if(std::any_of(m_entries.begin(),m_entries.end(),[&](const Entry& e){return e.directory&&e.name==m_name;})){
        if(ImGui::Button("Open folder"))navigate(m_directory/fs::u8path(m_name));
        ImGui::SameLine();
    }
    if(ImGui::Button(m_mode==Mode::Directory?"Choose folder":"Choose file")){
        std::string path;if(Select(m_directory,m_name,m_mode,m_extension,path,m_error)){
            m_active=false;result=Selection{m_target,path};ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();if(ImGui::Button("Cancel")||ImGui::IsKeyPressed(ImGuiKey_Escape)){m_active=false;result=Selection{m_target,{}};ImGui::CloseCurrentPopup();}
    ImGui::EndPopup();return result;
}
