#include "PerformanceProfiler.h"
#include "RuntimeUI.h"
#include "NamedAuthoring.h"
#include "AssetDatabase.h"
#include "InputSystem.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
namespace {
using Clock=std::chrono::steady_clock;
double Us(Clock::time_point t){return std::chrono::duration<double,std::micro>(Clock::now()-t).count();}
UIRect Intersect(UIRect a,UIRect b){auto p=glm::max(a.position,b.position);return {p,glm::max(glm::vec2(0),glm::min(a.position+a.size,b.position+b.size)-p)};}
bool Control(UIKind k){return k==UIKind::Button||k==UIKind::Slider||k==UIKind::Toggle;}
UIElement* ById(UIDocument& d,const std::string& id){for(auto& e:d.elements)if(e.id==id)return &e;return nullptr;}
bool Finite(float x){return std::isfinite(x);}
}
bool UIRect::Contains(glm::vec2 p)const{return size.x>0&&size.y>0&&p.x>=position.x&&p.y>=position.y&&p.x<position.x+size.x&&p.y<position.y+size.y;}
bool UIDocument::Validate(std::string& error)const{
    auto fail=[&](const char* s){error=s;return false;};
    if(!Finite(reference.x)||!Finite(reference.y)||reference.x<=0||reference.y<=0||elements.empty()||elements.size()>2048)return fail("invalid UI reference resolution or element count");
    std::set<std::string> ids;
    for(size_t i=0;i<elements.size();++i){const auto& e=elements[i];
        if(e.id.empty()||e.id.size()>128||e.id.find_first_of("\r\n")!=std::string::npos||ids.count(e.id))return fail("invalid/duplicate UI element ID");
        if(i==0){if(!e.parent.empty()||e.kind!=UIKind::Canvas)return fail("UI needs one root canvas");}
        else if(!ids.count(e.parent)||e.kind==UIKind::Canvas)return fail("UI parents must precede children, with exactly one canvas");
        ids.insert(e.id);
        if(!ValidTextUTF8(e.id,error)||!ValidTextUTF8(e.parent,error)||!ValidTextUTF8(e.text,error)||!ValidTextUTF8(e.textKey,error)||e.textKey.find_first_of("\r\n\0",0,3)!=std::string::npos||e.textKey.size()>128||int(e.direction)<0||int(e.direction)>2||e.textLogicalAlign<-1||e.textLogicalAlign>4)return fail("invalid UTF-8/text direction/alignment");
        if(int(e.kind)<0||int(e.kind)>6||int(e.flow)<0||int(e.flow)>2||e.text.size()>16384)return fail("invalid UI kind, flow or text size");
        for(auto v:{e.anchorMin,e.anchorMax,e.offset,e.size,e.relativeSize,e.align,e.textAlign})if(!Finite(v.x)||!Finite(v.y))return fail("nonfinite UI layout");
        for(auto v:{e.margin,e.padding,e.background,e.color})for(int c=0;c<4;++c)if(!Finite(v[c]))return fail("nonfinite UI colour/insets");
        if(glm::any(glm::lessThan(e.anchorMin,glm::vec2(0)))||glm::any(glm::greaterThan(e.anchorMax,glm::vec2(1)))||glm::any(glm::lessThan(e.anchorMax,e.anchorMin))||glm::any(glm::lessThan(e.size,glm::vec2(0)))||glm::any(glm::lessThan(e.relativeSize,glm::vec2(0)))||glm::any(glm::lessThan(e.align,glm::vec2(0)))||glm::any(glm::greaterThan(e.align,glm::vec2(1))))return fail("UI anchors, size or alignment outside range");
        if(!Finite(e.fontSize)||e.fontSize<=0||e.fontSize>512||!Finite(e.spacing)||e.spacing<0||!Finite(e.value)||!Finite(e.minimum)||!Finite(e.maximum)||e.maximum<=e.minimum||e.value<e.minimum||e.value>e.maximum)return fail("invalid UI numeric settings");
        if((!e.texture.empty()&&!IsValidAssetId(e.texture))||(!e.font.empty()&&!IsValidAssetId(e.font)))return fail("invalid UI asset ID");
    }error.clear();return true;
}
std::string SerializeUIDocument(const UIDocument& d){bool modern=std::any_of(d.elements.begin(),d.elements.end(),[](const auto& e){return !e.textKey.empty()||e.direction!=TextDirection::Auto||e.textLogicalAlign!=-1||e.mirrorRow;});std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(9)<<"JudasUI "<<(modern?2:1)<<"\n"<<d.reference.x<<' '<<d.reference.y<<' '<<d.visible<<' '<<d.enabled<<' '<<d.modal<<' '<<d.elements.size()<<'\n';
    for(const auto& e:d.elements){s<<std::quoted(e.id)<<' '<<std::quoted(e.parent)<<' '<<int(e.kind)<<' '<<int(e.flow)<<' '<<e.visible<<' '<<e.enabled<<' '<<e.clip<<' '<<e.wrap<<' '<<e.fit<<' ';
        for(auto v:{e.anchorMin,e.anchorMax,e.offset,e.size,e.relativeSize,e.align,e.textAlign})s<<v.x<<' '<<v.y<<' ';
        for(auto v:{e.margin,e.padding,e.background,e.color})for(int c=0;c<4;++c)s<<v[c]<<' ';
        s<<e.spacing<<' '<<e.fontSize<<' '<<e.value<<' '<<e.minimum<<' '<<e.maximum<<' '<<std::quoted(e.text)<<' '<<std::quoted(e.texture)<<' '<<std::quoted(e.font);if(modern)s<<' '<<std::quoted(e.textKey)<<' '<<int(e.direction)<<' '<<e.textLogicalAlign<<' '<<e.mirrorRow;s<<'\n';}return s.str();}
bool ParseUIDocument(const std::string& text,UIDocument& out,std::string& error){
    if(IsNamedDocument(text)){std::string legacy;if(!NamedToLegacy(text,"ui",legacy,error,false))return false;return ParseUIDocument(legacy,out,error);}
    if(text.size()>4*1024*1024){error="UI document too large";return false;}if(!ValidTextUTF8(text,error)){error="UI document UTF-8: "+error;return false;}
    std::istringstream s(text);s.imbue(std::locale::classic());std::string magic;int version;size_t count=0;UIDocument d;
    auto fail=[&](){error="malformed or unsupported UI document";return false;};
    if(!(s>>magic>>version)||magic!="JudasUI"||(version!=1&&version!=2)||!(s>>d.reference.x>>d.reference.y>>d.visible>>d.enabled>>d.modal>>count)||count>2048)return fail();
    for(size_t i=0;i<count;++i){UIElement e;int kind,flow;if(!(s>>std::quoted(e.id)>>std::quoted(e.parent)>>kind>>flow>>e.visible>>e.enabled>>e.clip>>e.wrap>>e.fit))return fail();e.kind=UIKind(kind);e.flow=UIFlow(flow);
        for(auto* v:{&e.anchorMin,&e.anchorMax,&e.offset,&e.size,&e.relativeSize,&e.align,&e.textAlign})if(!(s>>v->x>>v->y))return fail();
        for(auto* v:{&e.margin,&e.padding,&e.background,&e.color})for(int c=0;c<4;++c)if(!(s>>(*v)[c]))return fail();
        if(!(s>>e.spacing>>e.fontSize>>e.value>>e.minimum>>e.maximum>>std::quoted(e.text)>>std::quoted(e.texture)>>std::quoted(e.font)))return fail();
        if(version==2){int dir=0;if(!(s>>std::quoted(e.textKey)>>dir>>e.textLogicalAlign>>e.mirrorRow))return fail();e.direction=TextDirection(dir);}
        d.elements.push_back(e);}
    s>>std::ws;if(!s.eof()||!d.Validate(error))return false;out=std::move(d);return true;
}
bool LoadUIDocument(const std::string& path,UIDocument& out,std::string& error){std::ifstream f(path);if(!f){error="cannot read UI document: "+path;return false;}std::ostringstream s;s<<f.rdbuf();return ParseUIDocument(s.str(),out,error);}
bool SaveUIDocument(const std::string& path,const UIDocument& d,std::string& error){if(!d.Validate(error))return false;return WriteAuthoredDocument(path,SerializeUIDocument(d),"ui",error);}

bool ValidateUIAssets(const UIDocument& d,const AssetDatabase& a,std::string& error){for(const auto& e:d.elements)for(auto p:{std::make_pair(e.texture,AssetType::Texture),std::make_pair(e.font,AssetType::Font)})if(!p.first.empty()){auto* r=a.Find(p.first);if(!r||r->missing||r->type!=p.second){error="missing/wrong-type UI asset "+p.first;return false;}}return true;}
RuntimeUI::~RuntimeUI(){Clear();}
std::uint32_t RuntimeUI::Load(const std::string& asset,const std::string& name,std::uint64_t owner,std::string& error,std::uint64_t slot){auto* a=m_resources?m_resources->Assets():nullptr;auto* r=a?a->Find(asset):nullptr;UIDocument d;
    if(!r||r->missing||r->type!=AssetType::UI){error="missing UI document asset "+asset;return 0;}if(!LoadUIDocument(r->path,d,error)||!ValidateUIAssets(d,*a,error))return 0;return Add(d,name,owner,error,slot);}
std::uint32_t RuntimeUI::Add(const UIDocument& d,const std::string& name,std::uint64_t owner,std::string& error,std::uint64_t slot){if(name.empty()||Find(name)||!d.Validate(error)){if(error.empty())error="duplicate/empty UI document name";return 0;}if(!m_next){error="UI handle space exhausted";return 0;}
    InvalidateLayout();auto id=m_next++;m_documents.emplace(id,Instance{d,name,{},{},owner,slot,{},{}});return id;}
bool RuntimeUI::Unload(std::uint32_t h){InvalidateLayout();auto it=m_documents.find(h);if(it==m_documents.end())return false;const auto name=it->second.name;for(auto i=m_localizedText.begin();i!=m_localizedText.end();)if(i->first.first==h)i=m_localizedText.erase(i);else ++i;if(m_resources)for(const auto& ref:it->second.refs)m_resources->ReleaseRef(ref);m_documents.erase(it);m_events.erase(std::remove_if(m_events.begin(),m_events.end(),[&](const auto& e){return e.document==name;}),m_events.end());return true;}
void RuntimeUI::Clear(){while(!m_documents.empty())Unload(m_documents.begin()->first);m_events.clear();m_localizedText.clear();m_quit=false;}
void RuntimeUI::RemoveOwner(std::uint64_t id){std::vector<uint32_t> remove;for(auto& p:m_documents)if(p.second.owner==id)remove.push_back(p.first);for(auto h:remove)Unload(h);}
std::uint32_t RuntimeUI::Find(const std::string& n)const{for(const auto& p:m_documents)if(p.second.name==n)return p.first;return 0;}
UIDocument* RuntimeUI::Document(uint32_t h){InvalidateLayout();auto it=m_documents.find(h);return it==m_documents.end()?nullptr:&it->second.doc;}
const UIDocument* RuntimeUI::Document(uint32_t h)const{auto it=m_documents.find(h);return it==m_documents.end()?nullptr:&it->second.doc;}
UIElement* RuntimeUI::Element(uint32_t h,const std::string& id){auto* d=Document(h);return d?ById(*d,id):nullptr;}
const UILayout* RuntimeUI::LayoutOf(uint32_t h,const std::string& id)const{auto it=m_documents.find(h);if(it==m_documents.end())return nullptr;auto p=it->second.layout.find(id);return p==it->second.layout.end()?nullptr:&p->second;}
bool RuntimeUI::Paused()const{for(auto& p:m_documents)if(p.second.doc.visible&&p.second.doc.enabled&&p.second.doc.modal)return true;return false;}
bool RuntimeUI::OwnsInput()const{return Paused();}
void RuntimeUI::Layout(int w,int h){
    auto localeRevision=m_localization?m_localization->Revision():0;if(w==m_layoutWidth&&h==m_layoutHeight&&m_layoutRevision==m_computedRevision&&localeRevision==m_localeRevision)return;
    m_layoutWidth=w;m_layoutHeight=h;m_computedRevision=m_layoutRevision;m_localeRevision=localeRevision;
    JUDAS_PROFILE_SCOPE("Runtime UI layout");auto start=Clock::now();m_stats.elements=0;
    for(auto& p:m_documents){auto& in=p.second;in.layout.clear();auto& d=in.doc;float scale=std::min(w/d.reference.x,h/d.reference.y);glm::vec2 origin=(glm::vec2(w,h)-d.reference*scale)*.5f;
        std::map<std::string,float> cursor;for(auto& e:d.elements){UILayout l;if(e.parent.empty()){l.rect={origin,d.reference*scale};l.clip={{0,0},{w,h}};l.visible=d.visible&&e.visible;l.enabled=d.enabled&&e.enabled;}
            else {const auto& parent=in.layout.at(e.parent);auto* pe=ById(d,e.parent);UIRect content=parent.rect;content.position+=glm::vec2(pe->padding.x,pe->padding.y)*scale;content.size=glm::max(glm::vec2(0),content.size-glm::vec2(pe->padding.x+pe->padding.z,pe->padding.y+pe->padding.w)*scale);
                l.rect.size=glm::max(glm::vec2(0),(e.anchorMax-e.anchorMin+e.relativeSize)*content.size+e.size*scale-glm::vec2(e.margin.x+e.margin.z,e.margin.y+e.margin.w)*scale);
                l.rect.position=content.position+e.anchorMin*content.size+(e.offset+glm::vec2(e.margin.x,e.margin.y))*scale-e.align*l.rect.size;
                if(pe->flow!=UIFlow::Free){int axis=pe->flow==UIFlow::Horizontal?0:1;l.rect.position[axis]=content.position[axis]+cursor[e.parent]+(e.offset[axis]+e.margin[axis])*scale;if(e.visible)cursor[e.parent]+=l.rect.size[axis]+(pe->spacing+e.margin[axis]+e.margin[axis+2])*scale;}
                l.clip=pe->clip?Intersect(parent.clip,parent.rect):parent.clip;l.visible=parent.visible&&e.visible;l.enabled=parent.enabled&&e.enabled;}
            if(e.parent.size()){auto* pe=ById(d,e.parent);if(pe->flow==UIFlow::Horizontal&&pe->mirrorRow&&m_localization&&m_localization->RTL()){const auto& parent=in.layout.at(e.parent);float left=parent.rect.position.x+pe->padding.x*scale;float right=parent.rect.position.x+parent.rect.size.x-pe->padding.z*scale;l.rect.position.x=right-(l.rect.position.x-left)-l.rect.size.x;}}
            in.layout[e.id]=l;++m_stats.elements;}}
    m_stats.updateUs=Us(start);
}
void RuntimeUI::Activate(Instance& in,UIElement& e,const char* type){if(e.kind==UIKind::Toggle){e.value=e.value>.5f?0:1;type="change";}m_events.push_back({in.name,e.id,type,e.value});}
void RuntimeUI::Input(InputSystem& input,glm::vec2 pointer,bool available,int w,int h){
    JUDAS_PROFILE_SCOPE("Runtime UI input");Layout(w,h);
    auto top=m_documents.end();for(auto it=m_documents.begin();it!=m_documents.end();++it)if(it->second.doc.visible&&it->second.doc.enabled&&(top==m_documents.end()||it->second.doc.modal||!top->second.doc.modal))top=it;
    if(top==m_documents.end())return;
    // A nonmodal HUD never steals navigation unless a pointer hits a control.
    auto& in=top->second;std::vector<UIElement*> controls;UIElement* hit=nullptr;
    for(auto& e:in.doc.elements){auto& l=in.layout.at(e.id);if(l.visible&&l.enabled&&Control(e.kind)){controls.push_back(&e);if(available&&l.rect.Contains(pointer)&&l.clip.Contains(pointer))hit=&e;}}
    auto focus=[&](const std::string& id){if(in.focus!=id){in.focus=id;m_events.push_back({in.name,id,"focus",0});}};
    if(!in.focus.empty()&&std::none_of(controls.begin(),controls.end(),[&](auto* e){return e->id==in.focus;}))in.focus.clear();
    if(available&&pointer!=m_lastPointer&&hit)focus(hit->id);
    m_lastPointer=pointer;
    const bool modal=in.doc.modal;
    if(modal&&!controls.empty()){
        if(in.focus.empty())focus(controls.front()->id);
        int direction=input.Action("ui_down").pressed?1:input.Action("ui_up").pressed?-1:0;
        if(direction){auto it=std::find_if(controls.begin(),controls.end(),[&](auto* e){return e->id==in.focus;});int index=int(it-controls.begin());focus(controls[(index+direction+int(controls.size()))%controls.size()]->id);}
        auto* e=ById(in.doc,in.focus);
        if(e&&e->kind!=UIKind::Slider&&input.Action("ui_activate").pressed)Activate(in,*e);
        if(e&&e->kind==UIKind::Slider){float change=(input.Action("ui_right").pressed?1.f:0)-(input.Action("ui_left").pressed?1.f:0);if(change){e->value=std::clamp(e->value+change*(e->maximum-e->minimum)*.05f,e->minimum,e->maximum);Activate(in,*e,"change");}}
        if(input.Action("pause").pressed)m_events.push_back({in.name,"","back",0});
    }
    auto click=input.Action("ui_click");
    if(available&&click.pressed)in.pressed=hit?hit->id:"";
    if(available&&click.held&&!in.pressed.empty()){auto* e=ById(in.doc,in.pressed);if(e&&e->kind==UIKind::Slider&&in.layout.at(e->id).visible&&in.layout.at(e->id).enabled){auto& r=in.layout.at(e->id).rect;float t=std::clamp((pointer.x-r.position.x)/std::max(r.size.x,1.f),0.f,1.f);float value=e->minimum+t*(e->maximum-e->minimum);if(value!=e->value){e->value=value;Activate(in,*e,"change");}}}
    if(click.released){if(hit&&hit->id==in.pressed&&hit->kind!=UIKind::Slider)Activate(in,*hit);in.pressed.clear();}
    if(modal||hit||!in.pressed.empty())input.ConsumeBindings({"ui_up","ui_down","ui_left","ui_right","ui_activate","ui_click","pause"});
}
std::vector<UIEvent> RuntimeUI::TakeEvents(){auto events=std::move(m_events);m_events.clear();return events;}
void RuntimeUI::Draw(Renderer& r,int w,int h){
    JUDAS_PROFILE_SCOPE("Runtime UI drawing");auto start=Clock::now();Layout(w,h);m_stats.draws=0;unsigned before=r.UIDrawCalls();
    for(auto& p:m_documents){auto& in=p.second;
        if(m_resources){for(auto it=in.refs.begin();it!=in.refs.end();)if(std::none_of(in.doc.elements.begin(),in.doc.elements.end(),[&](const auto& e){return e.texture==*it||e.font==*it;})){m_resources->ReleaseRef(*it);it=in.refs.erase(it);}else ++it;}
        float scale=std::min(w/in.doc.reference.x,h/in.doc.reference.y);for(auto& e:in.doc.elements){const auto& l=in.layout.at(e.id);if(!l.visible||l.clip.size.x<=0||l.clip.size.y<=0)continue;r.SetUIClip(l.clip.position,l.clip.size);
            auto bg=e.background;if(!l.enabled)bg.a*=.5f;if(in.focus==e.id&&Control(e.kind))bg=glm::vec4(glm::min(glm::vec3(bg)+glm::vec3(.15f),glm::vec3(1)),bg.a);
            if(bg.a>0){r.DrawUIRect(l.rect.position,l.rect.size,bg);++m_stats.draws;}
            if(e.kind==UIKind::Slider){auto size=l.rect.size;size.x*= (e.value-e.minimum)/(e.maximum-e.minimum);r.DrawUIRect(l.rect.position,size,e.color);++m_stats.draws;}
            if(e.kind==UIKind::Toggle&&e.value>.5f){r.DrawUIRect(l.rect.position+glm::vec2(5)*scale,glm::vec2(20)*scale,e.color);++m_stats.draws;}
            if(e.kind==UIKind::Image&&!e.texture.empty()&&m_resources){if(std::find(in.refs.begin(),in.refs.end(),e.texture)==in.refs.end()){m_resources->AddRef(e.texture);in.refs.push_back(e.texture);}m_resources->RequestTexture(e.texture);auto t=m_resources->TryGetTexture(e.texture);if(t.IsValid()){r.DrawUIImage(l.rect.position,l.rect.size,t,e.color,e.fit);++m_stats.draws;}}
            std::string text=e.text;if(!e.textKey.empty()&&m_localization){m_localization->Refresh();auto& cached=m_localizedText[{p.first,e.id}];std::string identity=e.textKey+":"+std::to_string(m_localization->Revision());if(cached.second.empty()||cached.first!=m_localization->Revision()||cached.second.substr(0,identity.size())!=identity){std::string error;auto value=m_localization->Format(e.textKey,{},error);cached={m_localization->Revision(),identity+'\0'+value};}auto nul=cached.second.find('\0');text=cached.second.substr(nul+1);}
            if(!text.empty()){
                std::vector<std::shared_ptr<const TextFont>> fonts;
                if(!e.font.empty()&&m_resources){if(std::find(in.refs.begin(),in.refs.end(),e.font)==in.refs.end()){m_resources->AddRef(e.font);in.refs.push_back(e.font);}m_resources->RequestFont(e.font);if(auto f=m_resources->TryGetFont(e.font))fonts.push_back(f);}
                if(fonts.empty())if(auto f=r.DefaultTextFont())fonts.push_back(f);
                if(m_localization){auto fallback=m_localization->Fonts();fonts.insert(fonts.end(),fallback.begin(),fallback.end());}r.SelectTextFonts(std::move(fonts));
                TextOptions options;options.pixels=e.fontSize*scale;options.width=l.rect.size.x;options.wrap=e.wrap;options.direction=e.direction;options.locale=m_localization?m_localization->Locale():"en";
                if(options.direction==TextDirection::Auto){auto* parent=ById(in.doc,e.parent);while(parent){if(parent->direction!=TextDirection::Auto){options.direction=parent->direction;break;}parent=ById(in.doc,parent->parent);}}
                options.alignment=e.textLogicalAlign>=0?TextAlignment(e.textLogicalAlign):e.textAlign.x>=.75f?TextAlignment::Right:e.textAlign.x>=.25f?TextAlignment::Centre:TextAlignment::Left;
                auto layout=r.LayoutText(text,options);glm::vec2 position=l.rect.position;position.y+=e.textAlign.y*std::max(0.f,l.rect.size.y-layout->height);r.DrawTextLayout(*layout,position,e.color);
            }
        }}r.ClearUIClip();std::string error;r.SelectUIFont("",error);m_stats.draws=r.UIDrawCalls()-before;m_stats.renderUs=Us(start);
}

void RuntimeUI::RemoveSlotOwner(std::uint64_t owner,std::uint64_t slot){std::vector<uint32_t> remove;for(auto& p:m_documents)if(p.second.owner==owner&&p.second.slot==slot)remove.push_back(p.first);for(auto h:remove)Unload(h);}

bool RuntimeUI::SetElementLayout(uint32_t h,const std::string& id,const std::map<std::string,glm::vec2>& patch,std::string& error){auto* e=Element(h,id);if(!e){error="stale UI document/element";return false;}auto candidate=*e;for(auto& [key,v]:patch){if(key=="offset")candidate.offset=v;else if(key=="size")candidate.size=v;else if(key=="anchorMin")candidate.anchorMin=v;else if(key=="anchorMax")candidate.anchorMax=v;else if(key=="relativeSize")candidate.relativeSize=v;else if(key=="align")candidate.align=v;else{error="unknown layout property "+key;return false;}}UIDocument proof;UIElement root;root.id="__layout_root";root.kind=UIKind::Canvas;auto test=candidate;test.id="__layout_element";test.parent=root.id;test.kind=UIKind::Panel;proof.elements={root,test};if(!proof.Validate(error))return false;*e=candidate;InvalidateLayout();return true;}
