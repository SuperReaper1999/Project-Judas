#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Localization.h"
class Renderer; class ResourceManager; class AssetDatabase; class InputSystem;
enum class UIKind { Canvas, Panel, Text, Image, Button, Slider, Toggle };
enum class UIFlow { Free, Horizontal, Vertical };
struct UIRect {glm::vec2 position{0},size{0};bool Contains(glm::vec2 p)const;};
struct UIElement {
    std::string id,parent,text,texture,font;
    std::string textKey;TextDirection direction=TextDirection::Auto;int textLogicalAlign=-1;bool mirrorRow=false;
    UIKind kind=UIKind::Panel; UIFlow flow=UIFlow::Free;
    bool visible=true,enabled=true,clip=false,wrap=false,fit=true;
    glm::vec2 anchorMin{0},anchorMax{0},offset{0},size{180,40},relativeSize{0},align{0},textAlign{0};
    glm::vec4 margin{0},padding{0},background{0,0,0,0},color{1};
    float spacing=8,fontSize=22,value=0,minimum=0,maximum=1;
};
struct UIDocument {glm::vec2 reference{1280,720};bool visible=true,enabled=true,modal=false;std::vector<UIElement> elements;
    bool Validate(std::string& error)const;
};
bool LoadUIDocument(const std::string& path,UIDocument& out,std::string& error);
bool SaveUIDocument(const std::string& path,const UIDocument& doc,std::string& error);
std::string SerializeUIDocument(const UIDocument& doc);
bool ParseUIDocument(const std::string& text,UIDocument& out,std::string& error);
bool ValidateUIAssets(const UIDocument& doc,const AssetDatabase& assets,std::string& error);
struct UIEvent {std::string document,element,type;float value=0;};
struct UILayout {UIRect rect,clip;bool visible=false,enabled=false;};
struct UIStats {unsigned elements=0,draws=0;double updateUs=0,renderUs=0;};
// World-owned document instances; monotonically increasing handles never revive
// after unload. Authored data is copied, never mutated by runtime edits.
class RuntimeUI {
public:
    explicit RuntimeUI(ResourceManager* resources=nullptr):m_resources(resources){}
    ~RuntimeUI();
    std::uint32_t Load(const std::string& asset,const std::string& name,std::uint64_t owner,std::string& error,std::uint64_t slot=0);
    std::uint32_t Add(const UIDocument& doc,const std::string& name,std::uint64_t owner,std::string& error,std::uint64_t slot=0);
    bool Unload(std::uint32_t handle);void Clear();void RemoveOwner(std::uint64_t owner);void RemoveSlotOwner(std::uint64_t owner,std::uint64_t slot);
    void SetLocalization(LocalizationSession* l){m_localization=l;}
    std::uint32_t Find(const std::string& name)const;
    UIDocument* Document(std::uint32_t handle);const UIDocument* Document(std::uint32_t handle)const;
    UIElement* Element(std::uint32_t handle,const std::string& id);
    const UILayout* LayoutOf(std::uint32_t handle,const std::string& id)const;
    void Layout(int width,int height);
    bool SetElementLayout(std::uint32_t handle,const std::string& id,const std::map<std::string,glm::vec2>& patch,std::string& error);
    void InvalidateLayout(){++m_layoutRevision;}
    void Input(InputSystem& input,glm::vec2 pointer,bool pointerAvailable,int width,int height);
    void Draw(Renderer& renderer,int width,int height);
    std::vector<UIEvent> TakeEvents();
    bool Paused()const;
    bool debugOverlayVisible=true; // Engine diagnostics remain independent of project controls.
    bool QuitRequested()const{return m_quit;}void RequestQuit(){m_quit=true;}
    bool OwnsInput()const;bool Empty()const{return m_documents.empty();}
    const UIStats& Stats()const{return m_stats;}
private:
    struct Instance {UIDocument doc;std::string name,focus,pressed;std::uint64_t owner=0,slot=0;std::map<std::string,UILayout> layout;std::vector<std::string> refs;};
    std::map<std::uint32_t,Instance> m_documents;std::uint32_t m_next=1;
    ResourceManager* m_resources=nullptr;std::vector<UIEvent> m_events;
    LocalizationSession* m_localization=nullptr;
    std::map<std::pair<uint32_t,std::string>,std::pair<uint64_t,std::string>> m_localizedText;
    uint64_t m_layoutRevision=1,m_computedRevision=0,m_localeRevision=0;int m_layoutWidth=0,m_layoutHeight=0;
    bool m_quit=false;UIStats m_stats;glm::vec2 m_lastPointer{-1};
    void Activate(Instance& doc,UIElement& e,const char* type="click");
};
