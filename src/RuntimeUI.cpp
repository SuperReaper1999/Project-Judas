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
bool Control(UIKind k){return k==UIKind::Button||k==UIKind::Slider||k==UIKind::Toggle||k==UIKind::TextInput||k==UIKind::List||k==UIKind::Scroll;}
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
        if(int(e.kind)<0||int(e.kind)>9||int(e.flow)<0||int(e.flow)>2||e.text.size()>16384)return fail("invalid UI kind, flow or text size");
        for(auto v:{e.anchorMin,e.anchorMax,e.offset,e.size,e.relativeSize,e.align,e.textAlign})if(!Finite(v.x)||!Finite(v.y))return fail("nonfinite UI layout");
        if(!Finite(e.rowHeight)||e.rowHeight<12||e.rowHeight>512||e.maxBytes<1||e.maxBytes>16384||!ValidTextUTF8(e.placeholder,error)||!ValidTextUTF8(e.accessibleLabel,error)||!ValidTextUTF8(e.description,error)||e.placeholder.size()>4096||e.accessibleLabel.size()>512||e.description.size()>2048)return fail("invalid UI control settings");
        if(e.kind==UIKind::TextInput&&(e.text.size()>e.maxBytes||e.text.find_first_of("\r\n\0",0,3)!=std::string::npos||!e.textKey.empty()))return fail("text inputs require bounded single-line literal text");
        for(auto v:{e.margin,e.padding,e.background,e.color,e.focusColor})for(int c=0;c<4;++c)if(!Finite(v[c]))return fail("nonfinite UI colour/insets");
        if(glm::any(glm::lessThan(e.anchorMin,glm::vec2(0)))||glm::any(glm::greaterThan(e.anchorMax,glm::vec2(1)))||glm::any(glm::lessThan(e.anchorMax,e.anchorMin))||glm::any(glm::lessThan(e.size,glm::vec2(0)))||glm::any(glm::lessThan(e.relativeSize,glm::vec2(0)))||glm::any(glm::lessThan(e.align,glm::vec2(0)))||glm::any(glm::greaterThan(e.align,glm::vec2(1))))return fail("UI anchors, size or alignment outside range");
        if(!Finite(e.fontSize)||e.fontSize<=0||e.fontSize>512||!Finite(e.spacing)||e.spacing<0||!Finite(e.value)||!Finite(e.minimum)||!Finite(e.maximum)||e.maximum<=e.minimum||e.value<e.minimum||e.value>e.maximum)return fail("invalid UI numeric settings");
        if((!e.texture.empty()&&!IsValidAssetId(e.texture))||(!e.font.empty()&&!IsValidAssetId(e.font)))return fail("invalid UI asset ID");
    }error.clear();return true;
}
std::string SerializeUIDocument(const UIDocument& d){bool extended=std::any_of(d.elements.begin(),d.elements.end(),[](const auto& e){return int(e.kind)>6||!e.placeholder.empty()||!e.accessibleLabel.empty()||!e.description.empty()||e.readOnly||e.maxBytes!=4096||e.rowHeight!=40||e.focusColor!=glm::vec4(1,.78f,.28f,1);});bool modern=extended||std::any_of(d.elements.begin(),d.elements.end(),[](const auto& e){return !e.textKey.empty()||e.direction!=TextDirection::Auto||e.textLogicalAlign!=-1||e.mirrorRow;});std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(9)<<"JudasUI "<<(extended?3:modern?2:1)<<"\n"<<d.reference.x<<' '<<d.reference.y<<' '<<d.visible<<' '<<d.enabled<<' '<<d.modal<<' '<<d.elements.size()<<'\n';
    for(const auto& e:d.elements){s<<std::quoted(e.id)<<' '<<std::quoted(e.parent)<<' '<<int(e.kind)<<' '<<int(e.flow)<<' '<<e.visible<<' '<<e.enabled<<' '<<e.clip<<' '<<e.wrap<<' '<<e.fit<<' ';
        for(auto v:{e.anchorMin,e.anchorMax,e.offset,e.size,e.relativeSize,e.align,e.textAlign})s<<v.x<<' '<<v.y<<' ';
        for(auto v:{e.margin,e.padding,e.background,e.color})for(int c=0;c<4;++c)s<<v[c]<<' ';
        s<<e.spacing<<' '<<e.fontSize<<' '<<e.value<<' '<<e.minimum<<' '<<e.maximum<<' '<<std::quoted(e.text)<<' '<<std::quoted(e.texture)<<' '<<std::quoted(e.font);if(modern)s<<' '<<std::quoted(e.textKey)<<' '<<int(e.direction)<<' '<<e.textLogicalAlign<<' '<<e.mirrorRow;if(extended){s<<' '<<std::quoted(e.placeholder)<<' '<<std::quoted(e.accessibleLabel)<<' '<<std::quoted(e.description)<<' '<<e.readOnly<<' '<<e.maxBytes<<' '<<e.rowHeight;for(int c=0;c<4;++c)s<<' '<<e.focusColor[c];}s<<'\n';}return s.str();}
bool ParseUIDocument(const std::string& text,UIDocument& out,std::string& error){
    if(IsNamedDocument(text)){std::string legacy;if(!NamedToLegacy(text,"ui",legacy,error,false))return false;return ParseUIDocument(legacy,out,error);}
    if(text.size()>4*1024*1024){error="UI document too large";return false;}if(!ValidTextUTF8(text,error)){error="UI document UTF-8: "+error;return false;}
    std::istringstream s(text);s.imbue(std::locale::classic());std::string magic;int version;size_t count=0;UIDocument d;
    auto fail=[&](){error="malformed or unsupported UI document";return false;};
    if(!(s>>magic>>version)||magic!="JudasUI"||(version<1||version>3)||!(s>>d.reference.x>>d.reference.y>>d.visible>>d.enabled>>d.modal>>count)||count>2048)return fail();
    for(size_t i=0;i<count;++i){UIElement e;int kind,flow;if(!(s>>std::quoted(e.id)>>std::quoted(e.parent)>>kind>>flow>>e.visible>>e.enabled>>e.clip>>e.wrap>>e.fit))return fail();e.kind=UIKind(kind);e.flow=UIFlow(flow);
        for(auto* v:{&e.anchorMin,&e.anchorMax,&e.offset,&e.size,&e.relativeSize,&e.align,&e.textAlign})if(!(s>>v->x>>v->y))return fail();
        for(auto* v:{&e.margin,&e.padding,&e.background,&e.color})for(int c=0;c<4;++c)if(!(s>>(*v)[c]))return fail();
        if(!(s>>e.spacing>>e.fontSize>>e.value>>e.minimum>>e.maximum>>std::quoted(e.text)>>std::quoted(e.texture)>>std::quoted(e.font)))return fail();
        if(version>=2){int dir=0;if(!(s>>std::quoted(e.textKey)>>dir>>e.textLogicalAlign>>e.mirrorRow))return fail();e.direction=TextDirection(dir);}
        if(version>=3){if(!(s>>std::quoted(e.placeholder)>>std::quoted(e.accessibleLabel)>>std::quoted(e.description)>>e.readOnly>>e.maxBytes>>e.rowHeight))return fail();for(int c=0;c<4;++c)if(!(s>>e.focusColor[c]))return fail();}
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
    InvalidateLayout();auto id=m_next++;Instance instance;instance.doc=d;instance.name=name;instance.owner=owner;instance.slot=slot;m_documents.emplace(id,std::move(instance));return id;}
bool RuntimeUI::Unload(std::uint32_t h){if(m_textDocument==h){m_textDocument=0;m_textElement.clear();}InvalidateLayout();auto it=m_documents.find(h);if(it==m_documents.end())return false;const auto name=it->second.name;for(auto i=m_localizedText.begin();i!=m_localizedText.end();)if(i->first.first==h)i=m_localizedText.erase(i);else ++i;if(m_resources)for(const auto& ref:it->second.refs)m_resources->ReleaseRef(ref);m_documents.erase(it);m_events.erase(std::remove_if(m_events.begin(),m_events.end(),[&](const auto& e){return e.document==name;}),m_events.end());return true;}
void RuntimeUI::Clear(){while(!m_documents.empty())Unload(m_documents.begin()->first);m_events.clear();m_localizedText.clear();m_quit=false;}
void RuntimeUI::RemoveOwner(std::uint64_t id){std::vector<uint32_t> remove;for(auto& p:m_documents)if(p.second.owner==id)remove.push_back(p.first);for(auto h:remove)Unload(h);}
std::uint32_t RuntimeUI::Find(const std::string& n)const{for(const auto& p:m_documents)if(p.second.name==n)return p.first;return 0;}
UIDocument* RuntimeUI::Document(uint32_t h){InvalidateLayout();auto it=m_documents.find(h);return it==m_documents.end()?nullptr:&it->second.doc;}
const UIDocument* RuntimeUI::Document(uint32_t h)const{auto it=m_documents.find(h);return it==m_documents.end()?nullptr:&it->second.doc;}
UIElement* RuntimeUI::Element(uint32_t h,const std::string& id){auto* d=Document(h);return d?ById(*d,id):nullptr;}
const UILayout* RuntimeUI::LayoutOf(uint32_t h,const std::string& id)const{auto it=m_documents.find(h);if(it==m_documents.end())return nullptr;auto p=it->second.layout.find(id);return p==it->second.layout.end()?nullptr:&p->second;}
bool RuntimeUI::Paused(std::uint64_t player)const{for(auto& p:m_documents)if(p.second.player==player&&p.second.doc.visible&&p.second.doc.enabled&&p.second.doc.modal)return true;return false;}
bool RuntimeUI::OwnsInput()const{return Paused();}
void RuntimeUI::Layout(int w,int h){
    auto localeRevision=m_localization?m_localization->Revision():0;if(w==m_layoutWidth&&h==m_layoutHeight&&m_layoutRevision==m_computedRevision&&localeRevision==m_localeRevision)return;
    m_layoutWidth=w;m_layoutHeight=h;m_computedRevision=m_layoutRevision;m_localeRevision=localeRevision;
    JUDAS_PROFILE_SCOPE("Runtime UI layout");auto start=Clock::now();m_stats.elements=0;
    for(auto& p:m_documents){auto& in=p.second;in.layout.clear();for(auto& state:in.widgets)state.second.extent={0,0};auto& d=in.doc;glm::vec2 available=glm::vec2(w,h)*glm::vec2(in.viewport.z,in.viewport.w),offset=glm::vec2(w,h)*glm::vec2(in.viewport.x,in.viewport.y);float base=std::min(available.x/d.reference.x,available.y/d.reference.y),scale=base*in.scale;glm::vec2 origin=offset+(available-d.reference*base)*.5f;
        std::map<std::string,float> cursor;std::map<std::string,UIElement*> authored;for(auto& e:d.elements)authored[e.id]=&e;for(auto& e:d.elements){UILayout l;if(e.parent.empty()){l.rect={origin,d.reference*base};l.clip={offset,available};l.visible=d.visible&&e.visible;l.enabled=d.enabled&&e.enabled;}
            else {const auto& parent=in.layout.at(e.parent);auto* pe=authored.at(e.parent);UIRect content=parent.rect;content.position+=glm::vec2(pe->padding.x,pe->padding.y)*scale;content.size=glm::max(glm::vec2(0),content.size-glm::vec2(pe->padding.x+pe->padding.z,pe->padding.y+pe->padding.w)*scale);
                l.rect.size=glm::max(glm::vec2(0),(e.anchorMax-e.anchorMin+e.relativeSize)*content.size+e.size*scale-glm::vec2(e.margin.x+e.margin.z,e.margin.y+e.margin.w)*scale);
                l.rect.position=content.position+e.anchorMin*content.size+(e.offset+glm::vec2(e.margin.x,e.margin.y))*scale-e.align*l.rect.size;
                if(pe->flow!=UIFlow::Free){int axis=pe->flow==UIFlow::Horizontal?0:1;l.rect.position[axis]=content.position[axis]+cursor[e.parent]+(e.offset[axis]+e.margin[axis])*scale;if(e.visible)cursor[e.parent]+=l.rect.size[axis]+(pe->spacing+e.margin[axis]+e.margin[axis+2])*scale;}
                auto& parentState=in.widgets[e.parent];l.rect.position-=parentState.scroll*scale;l.clip=(pe->clip||pe->kind==UIKind::Scroll||pe->kind==UIKind::List)?Intersect(parent.clip,parent.rect):parent.clip;l.visible=parent.visible&&e.visible;l.enabled=parent.enabled&&e.enabled;}
            if(e.parent.size()){auto* pe=authored.at(e.parent);if(pe->flow==UIFlow::Horizontal&&pe->mirrorRow&&m_localization&&m_localization->RTL()){const auto& parent=in.layout.at(e.parent);float left=parent.rect.position.x+pe->padding.x*scale;float right=parent.rect.position.x+parent.rect.size.x-pe->padding.z*scale;l.rect.position.x=right-(l.rect.position.x-left)-l.rect.size.x;}}
            if(!e.parent.empty()&&e.visible){auto& parent=in.layout.at(e.parent);auto& state=in.widgets[e.parent];state.extent=glm::max(state.extent,(l.rect.position+l.rect.size-parent.rect.position)/scale+state.scroll);}
            in.layout[e.id]=l;++m_stats.elements;}
        for(auto& e:d.elements)if(e.kind==UIKind::Scroll||e.kind==UIKind::List){auto& state=in.widgets[e.id];SetScroll(p.first,e.id,state.scroll);}}
    if(m_computedRevision!=m_layoutRevision){Layout(w,h);return;}
    m_stats.updateUs=Us(start);
}
void RuntimeUI::Activate(Instance& in,UIElement& e,const char* type){if(e.kind==UIKind::List){auto& state=in.widgets[e.id];if(state.selected<0||state.selected>=int(state.items.size())||!state.items[state.selected].enabled)return;m_events.push_back({in.name,e.id,type,float(state.selected),in.player,state.items[state.selected].text,state.items[state.selected].id});return;}if(e.kind==UIKind::Toggle){e.value=e.value>.5f?0:1;type="change";}m_events.push_back({in.name,e.id,type,e.value,in.player});}
void RuntimeUI::Input(InputSystem& input,glm::vec2 pointer,bool available,int w,int h,std::uint64_t player){
    JUDAS_PROFILE_SCOPE("Runtime UI input");Layout(w,h);
    auto top=m_documents.end();for(auto it=m_documents.begin();it!=m_documents.end();++it)if(it->second.player==player&&it->second.doc.visible&&it->second.doc.enabled&&(top==m_documents.end()||it->second.doc.modal||!top->second.doc.modal))top=it;
    if(top==m_documents.end())return;
    auto handle=top->first;auto& in=top->second;
    if(in.inputRevision!=input.ResetRevision()){in.pressed.clear();in.dragging.clear();for(auto& pair:in.widgets)pair.second.composition.clear();in.inputRevision=input.ResetRevision();}
    std::vector<UIElement*> controls;UIElement *hit=nullptr,*scrollHit=nullptr;
    for(auto& e:in.doc.elements){auto& l=in.layout.at(e.id);if(l.visible&&l.enabled&&Control(e.kind)){controls.push_back(&e);if(available&&l.rect.Contains(pointer)&&l.clip.Contains(pointer)){hit=&e;if(e.kind==UIKind::Scroll||e.kind==UIKind::List)scrollHit=&e;}}}
    if(!in.focus.empty()&&std::none_of(controls.begin(),controls.end(),[&](auto* e){return e->id==in.focus;})){if(m_textDocument==handle){m_textDocument=0;m_textElement.clear();}in.focus.clear();}
    if(available&&pointer!=in.lastPointer&&hit&&hit->kind!=UIKind::TextInput&&!(ById(in.doc,in.focus)&&ById(in.doc,in.focus)->kind==UIKind::TextInput)&&in.pressed.empty())Focus(handle,hit->id);
    in.lastPointer=pointer;const bool modal=in.doc.modal;bool editing=false,submitted=false;
    auto* current=ById(in.doc,in.focus);if(current&&current->kind==UIKind::TextInput){editing=true;for(auto& event:input.TextEvents())if(event.player==player&&(event.token==0||event.token==m_textSerial)&&(event.command=="submit"||event.command=="tab"))submitted=true;Edit(in,*current,input,player);}
    auto moveFocus=[&](int direction){if(controls.empty())return;auto it=std::find_if(controls.begin(),controls.end(),[&](auto* e){return e->id==in.focus;});int index=it==controls.end()?(direction>0?-1:0):int(it-controls.begin());Focus(handle,controls[(index+direction+int(controls.size()))%controls.size()]->id);};
    if((modal||editing)&&!controls.empty()){
        if(in.focus.empty())Focus(handle,controls.front()->id);
        int direction=input.Action("ui_down").pressed?1:input.Action("ui_up").pressed?-1:0;
        current=ById(in.doc,in.focus);
        if(current&&current->kind==UIKind::List&&direction){auto& state=in.widgets[current->id];int index=state.selected;for(size_t n=0;n<state.items.size();++n){index+=direction;if(index<0||index>=int(state.items.size()))break;if(state.items[index].enabled){Select(handle,current->id,index);Activate(in,*current,"change");break;}}}
        else if(current&&current->kind==UIKind::Scroll&&direction)SetScroll(handle,current->id,Scroll(handle,current->id)+glm::vec2(0,direction*current->rowHeight));
        else if(direction)moveFocus(direction);
        bool nativeTab=std::any_of(input.TextEvents().begin(),input.TextEvents().end(),[&](const auto& event){return event.player==player&&(event.token==0||event.token==m_textSerial)&&event.command=="tab";});if(input.Action("ui_next").pressed&&!editing&&!nativeTab)moveFocus(1);
        for(auto& event:input.TextEvents())if(event.player==player&&(event.token==0||event.token==m_textSerial)&&event.command=="tab"&&(editing||input.Action("ui_next").pressed))moveFocus(event.shift?-1:1);
        current=ById(in.doc,in.focus);
        if(current&&current->kind!=UIKind::Slider&&input.Action("ui_activate").pressed&&!submitted){if(current->kind==UIKind::TextInput){m_textDocument=handle;m_textElement=current->id;}else Activate(in,*current);}
        if(current&&current->kind==UIKind::Slider){float change=(input.Action("ui_right").pressed?1.f:0)-(input.Action("ui_left").pressed?1.f:0);if(change){current->value=std::clamp(current->value+change*(current->maximum-current->minimum)*.05f,current->minimum,current->maximum);Activate(in,*current,"change");}}
        if(input.Action("pause").pressed&&!editing)m_events.push_back({in.name,"","back",0,in.player});
    }
    auto click=input.Action("ui_click");
    if(!click.held&&!click.pressed&&!click.released){in.pressed.clear();in.dragging.clear();}
    float scale=std::min(w*in.viewport.z/in.doc.reference.x,h*in.viewport.w/in.doc.reference.y)*in.scale;
    if(scrollHit){auto delta=input.Axis("ui_scroll");if(delta)SetScroll(handle,scrollHit->id,Scroll(handle,scrollHit->id)-glm::vec2(0,delta*scrollHit->rowHeight*3));}
    if(available&&click.pressed){in.pressed=hit?hit->id:"";if(hit)Focus(handle,hit->id);else{in.focus.clear();if(m_textDocument==handle)m_textDocument=0;}
        in.dragged=false;in.dragStart=pointer;in.dragging=scrollHit&&(!hit||(hit->kind!=UIKind::TextInput&&hit->kind!=UIKind::Slider))?scrollHit->id:"";if(scrollHit)in.dragScroll=Scroll(handle,scrollHit->id);
        if(hit&&hit->kind==UIKind::TextInput){auto& state=in.widgets[hit->id];auto& rect=in.layout.at(hit->id).rect;float x=pointer.x-rect.position.x+state.textPan;size_t nearest=0;float best=1e30f;for(size_t n=0;n<state.carets.size();++n)if(std::abs(state.carets[n]-x)<best){best=std::abs(state.carets[n]-x);nearest=n;}state.selection={nearest,nearest};state.composition.clear();}
    }
    if(available&&click.held&&!in.pressed.empty()){
        auto* e=ById(in.doc,in.pressed);if(e&&e->kind==UIKind::Slider){auto& rect=in.layout.at(e->id).rect;float t=std::clamp((pointer.x-rect.position.x)/std::max(rect.size.x,1.f),0.f,1.f);float value=e->minimum+t*(e->maximum-e->minimum);if(value!=e->value){e->value=value;Activate(in,*e,"change");}}
        if(!in.dragging.empty()&&glm::length(pointer-in.dragStart)>6){in.dragged=true;SetScroll(handle,in.dragging,in.dragScroll-(pointer-in.dragStart)/std::max(.01f,scale));}
        if(e&&e->kind==UIKind::TextInput){auto& state=in.widgets[e->id];float x=pointer.x-in.layout.at(e->id).rect.position.x+state.textPan;float best=1e30f;for(size_t n=0;n<state.carets.size();++n)if(std::abs(state.carets[n]-x)<best){best=std::abs(state.carets[n]-x);state.selection.caret=n;}}
    }
    if(click.released){if(hit&&hit->id==in.pressed&&!in.dragged&&hit->kind!=UIKind::Slider&&hit->kind!=UIKind::TextInput&&hit->kind!=UIKind::Scroll){if(hit->kind==UIKind::List){auto& state=in.widgets[hit->id];int index=int(((pointer.y-in.layout.at(hit->id).rect.position.y)/scale+state.scroll.y)/hit->rowHeight);if(index>=0&&index<int(state.items.size())&&state.items[index].enabled){Select(handle,hit->id,index);Activate(in,*hit,"change");}}else Activate(in,*hit);}in.pressed.clear();in.dragging.clear();}
    if(modal||hit||!in.pressed.empty())input.ConsumeBindings({"ui_up","ui_down","ui_left","ui_right","ui_activate","ui_click","ui_next","ui_scroll","pause"});
    if(editing){std::vector<std::string> keys;for(auto& entry:input.Map().entries)for(auto& binding:entry.bindings)if(binding.control.rfind("key:",0)==0){keys.push_back(entry.name);break;}input.ConsumeBindings(keys);}
    input.ConsumeText();
}
std::vector<UIEvent> RuntimeUI::TakeEvents(){auto events=std::move(m_events);m_events.clear();return events;}
void RuntimeUI::Draw(Renderer& r,int w,int h){
    JUDAS_PROFILE_SCOPE("Runtime UI drawing");auto start=Clock::now();Layout(w,h);m_stats.draws=0;m_stats.listRows=0;unsigned before=r.UIDrawCalls();
    for(auto& p:m_documents){auto& in=p.second;
        if(m_resources){for(auto it=in.refs.begin();it!=in.refs.end();)if(std::none_of(in.doc.elements.begin(),in.doc.elements.end(),[&](const auto& e){return e.texture==*it||e.font==*it;})){m_resources->ReleaseRef(*it);it=in.refs.erase(it);}else ++it;}
        float scale=std::min(w*in.viewport.z/in.doc.reference.x,h*in.viewport.w/in.doc.reference.y)*in.scale;for(auto& e:in.doc.elements){const auto& l=in.layout.at(e.id);if(!l.visible||Intersect(l.rect,l.clip).size.x<=0||Intersect(l.rect,l.clip).size.y<=0)continue;r.SetUIClip(l.clip.position,l.clip.size);
            auto bg=e.background;if(!l.enabled)bg.a*=.5f;if(in.focus==e.id&&Control(e.kind))bg=glm::vec4(glm::min(glm::vec3(bg)+glm::vec3(.15f),glm::vec3(1)),bg.a);
            if(bg.a>0){r.DrawUIRect(l.rect.position,l.rect.size,bg);++m_stats.draws;}
            if(e.kind==UIKind::Slider){auto size=l.rect.size;size.x*= (e.value-e.minimum)/(e.maximum-e.minimum);r.DrawUIRect(l.rect.position,size,e.color);++m_stats.draws;}
            if(e.kind==UIKind::Toggle&&e.value>.5f){r.DrawUIRect(l.rect.position+glm::vec2(5)*scale,glm::vec2(20)*scale,e.color);++m_stats.draws;}
            if(e.kind==UIKind::Image&&!e.texture.empty()&&m_resources){if(std::find(in.refs.begin(),in.refs.end(),e.texture)==in.refs.end()){m_resources->AddRef(e.texture);in.refs.push_back(e.texture);}m_resources->RequestTexture(e.texture);auto t=m_resources->TryGetTexture(e.texture);if(t.IsValid()){r.DrawUIImage(l.rect.position,l.rect.size,t,e.color,e.fit);++m_stats.draws;}}
            std::string text=e.text;if(!e.textKey.empty()&&m_localization){m_localization->Refresh();auto& cached=m_localizedText[{p.first,e.id}];std::string identity=e.textKey+":"+std::to_string(m_localization->Revision());if(cached.second.empty()||cached.first!=m_localization->Revision()||cached.second.substr(0,identity.size())!=identity){std::string error;auto value=m_localization->Format(e.textKey,{},error);cached={m_localization->Revision(),identity+'\0'+value};}auto nul=cached.second.find('\0');text=cached.second.substr(nul+1);}
            if(!text.empty()||e.kind==UIKind::TextInput||e.kind==UIKind::List){
                std::vector<std::shared_ptr<const TextFont>> fonts;
                if(!e.font.empty()&&m_resources){if(std::find(in.refs.begin(),in.refs.end(),e.font)==in.refs.end()){m_resources->AddRef(e.font);in.refs.push_back(e.font);}m_resources->RequestFont(e.font);if(auto f=m_resources->TryGetFont(e.font))fonts.push_back(f);}
                if(fonts.empty())if(auto f=r.DefaultTextFont())fonts.push_back(f);
                if(m_localization){auto fallback=m_localization->Fonts();fonts.insert(fonts.end(),fallback.begin(),fallback.end());}r.SelectTextFonts(std::move(fonts));
                TextOptions options;options.pixels=std::clamp(e.fontSize*scale,1.f,512.f);options.width=l.rect.size.x;options.wrap=e.wrap;options.direction=e.direction;options.locale=m_localization?m_localization->Locale():"en";
                if(options.direction==TextDirection::Auto){auto* parent=ById(in.doc,e.parent);while(parent){if(parent->direction!=TextDirection::Auto){options.direction=parent->direction;break;}parent=ById(in.doc,parent->parent);}}
                options.alignment=e.textLogicalAlign>=0?TextAlignment(e.textLogicalAlign):e.textAlign.x>=.75f?TextAlignment::Right:e.textAlign.x>=.25f?TextAlignment::Centre:TextAlignment::Left;
                if(e.kind==UIKind::List){
                    auto& state=in.widgets[e.id];float row=std::max(.001f,e.rowHeight*scale);auto visible=Intersect(l.clip,l.rect);size_t first=std::min(state.items.size(),size_t(std::max(0.f,std::floor((visible.position.y-l.rect.position.y+state.scroll.y*scale)/row))));size_t last=std::min(state.items.size(),size_t(std::max(0.f,std::ceil((visible.position.y+visible.size.y-l.rect.position.y+state.scroll.y*scale)/row))));
                    r.SetUIClip(Intersect(l.clip,l.rect).position,Intersect(l.clip,l.rect).size);
                    options.wrap=false;options.width=std::max(0.f,l.rect.size.x-16*scale);
                    for(size_t n=first;n<last;++n){glm::vec2 pos=l.rect.position+glm::vec2(8*scale,n*row-state.scroll.y*scale);if(int(n)==state.selected)r.DrawUIRect({l.rect.position.x,pos.y},{l.rect.size.x,row},glm::vec4(glm::vec3(e.focusColor)*.25f,1));auto label=r.LayoutText(state.items[n].text,options);r.DrawTextLayout(*label,pos+glm::vec2(0,std::max(0.f,(row-label->height)*.5f)),state.items[n].enabled?e.color:glm::vec4(glm::vec3(e.color),e.color.a*.4f));++m_stats.listRows;}
                }else if(e.kind==UIKind::TextInput){
                    options.wrap=false;options.width=0;options.alignment=TextAlignment::Left;
                    auto layout=r.LayoutText(text,options);auto& state=in.widgets[e.id];auto boundaries=TextGraphemes(text);std::vector<uint32_t> utf16;uint32_t units=0;size_t byte=0;
                    for(auto stop:boundaries){while(byte<stop){unsigned char c=static_cast<unsigned char>(text[byte]);size_t len=c<128?1:c<224?2:c<240?3:4;units+=len==4?2:1;byte+=len;}utf16.push_back(units);}
                    state.carets.assign(boundaries.size(),0);std::map<uint32_t,std::pair<float,float>> clusters;std::map<uint32_t,bool> rtl;
                    for(auto& glyph:layout->glyphs){auto found=clusters.find(glyph.cluster);float a=glyph.x,b=glyph.x+glyph.advance;if(found==clusters.end())clusters[glyph.cluster]={a,b};else{found->second.first=std::min(found->second.first,a);found->second.second=std::max(found->second.second,b);}rtl[glyph.cluster]=glyph.rtl;}
                    for(size_t n=0;n<utf16.size();++n){auto it=clusters.upper_bound(utf16[n]);if(it==clusters.begin())continue;--it;auto next=std::next(it);uint32_t end=next==clusters.end()?units:next->first;auto lo=std::lower_bound(utf16.begin(),utf16.end(),it->first),hi=std::lower_bound(utf16.begin(),utf16.end(),end);float fraction=hi==lo?0:float(std::ptrdiff_t(n)-(lo-utf16.begin()))/float(hi-lo);fraction=std::clamp(fraction,0.f,1.f);if(rtl[it->first])fraction=1-fraction;state.carets[n]=it->second.first+(it->second.second-it->second.first)*fraction;}
                    size_t caret=std::min(state.selection.caret,state.carets.size()-1);float x=state.carets[caret];float inner=std::max(1.f,l.rect.size.x-16*scale);if(x-state.textPan>inner)state.textPan=x-inner;if(x-state.textPan<0)state.textPan=x;state.textPan=std::max(0.f,state.textPan);
                    auto clip=Intersect(l.clip,l.rect);r.SetUIClip(clip.position,clip.size);glm::vec2 pos=l.rect.position+glm::vec2(8*scale-state.textPan,std::max(0.f,(l.rect.size.y-layout->height)*.5f));
                    if(in.focus==e.id){auto a=std::min(state.selection.anchor,caret),b=std::max(state.selection.anchor,caret);b=std::min(b,state.carets.size()-1);for(size_t n=a;n<b;++n)r.DrawUIRect(pos+glm::vec2(std::min(state.carets[n],state.carets[n+1]),0),{std::abs(state.carets[n+1]-state.carets[n]),std::max(options.pixels,layout->height)},glm::vec4(.25f,.48f,.75f,.65f));}
                    if(text.empty()&&state.composition.empty()){auto placeholder=r.LayoutText(e.placeholder,options);r.DrawTextLayout(*placeholder,pos,glm::vec4(glm::vec3(e.color),e.color.a*.55f));}else r.DrawTextLayout(*layout,pos,e.color);
                    if(in.focus==e.id){float cx=state.carets[caret];if(!state.composition.empty()){auto composition=r.LayoutText(state.composition,options);r.DrawUIRect(pos+glm::vec2(cx,0),{composition->width,std::max(options.pixels,composition->height)},e.background);r.DrawTextLayout(*composition,pos+glm::vec2(cx,0),e.color);r.DrawUIRect(pos+glm::vec2(cx,std::max(options.pixels,composition->height)),{composition->width,2*scale},e.focusColor);}r.DrawUIRect(pos+glm::vec2(cx,0),{std::max(1.f,scale),std::max(options.pixels,layout->height)},e.focusColor);}
                }else{auto layout=r.LayoutText(text,options);glm::vec2 position=l.rect.position;position.y+=e.textAlign.y*std::max(0.f,l.rect.size.y-layout->height);r.DrawTextLayout(*layout,position,e.color);}
            }
            if(in.focus==e.id&&Control(e.kind)){auto clip=Intersect(l.clip,l.rect);r.SetUIClip(clip.position,clip.size);float t=std::max(2.f,scale*2);auto pos=l.rect.position,size=l.rect.size;for(auto rect:std::vector<UIRect>{{pos,{size.x,t}},{pos+glm::vec2(0,size.y-t),{size.x,t}},{pos,{t,size.y}},{pos+glm::vec2(size.x-t,0),{t,size.y}}})r.DrawUIRect(rect.position,rect.size,e.focusColor);}
            if(e.kind==UIKind::Scroll||e.kind==UIKind::List){auto& state=in.widgets[e.id];float total=e.kind==UIKind::List?state.items.size()*e.rowHeight*scale:state.extent.y*scale;if(total>l.rect.size.y){float height=std::max(12.f,l.rect.size.y*l.rect.size.y/total),travel=l.rect.size.y-height;float y=travel*state.scroll.y*scale/std::max(1.f,total-l.rect.size.y);r.DrawUIRect(l.rect.position+glm::vec2(l.rect.size.x-5*scale,y),{4*scale,height},e.focusColor);}}
        }}r.ClearUIClip();std::string error;r.SelectUIFont("",error);m_stats.draws=r.UIDrawCalls()-before;m_stats.renderUs=Us(start);
}

void RuntimeUI::RemoveSlotOwner(std::uint64_t owner,std::uint64_t slot){std::vector<uint32_t> remove;for(auto& p:m_documents)if(p.second.owner==owner&&p.second.slot==slot)remove.push_back(p.first);for(auto h:remove)Unload(h);}

bool RuntimeUI::SetElementLayout(uint32_t h,const std::string& id,const std::map<std::string,glm::vec2>& patch,std::string& error){auto* e=Element(h,id);if(!e){error="stale UI document/element";return false;}auto candidate=*e;for(auto& [key,v]:patch){if(key=="offset")candidate.offset=v;else if(key=="size")candidate.size=v;else if(key=="anchorMin")candidate.anchorMin=v;else if(key=="anchorMax")candidate.anchorMax=v;else if(key=="relativeSize")candidate.relativeSize=v;else if(key=="align")candidate.align=v;else{error="unknown layout property "+key;return false;}}UIDocument proof;UIElement root;root.id="__layout_root";root.kind=UIKind::Canvas;auto test=candidate;test.id="__layout_element";test.parent=root.id;test.kind=UIKind::Panel;proof.elements={root,test};if(!proof.Validate(error))return false;*e=candidate;InvalidateLayout();return true;}

bool RuntimeUI::SetPlayer(uint32_t h,uint64_t player){auto it=m_documents.find(h);if(it==m_documents.end())return false;it->second.player=player;it->second.focus.clear();it->second.pressed.clear();it->second.viewport={0,0,1,1};InvalidateLayout();return true;}
uint64_t RuntimeUI::Player(uint32_t h)const{auto it=m_documents.find(h);return it==m_documents.end()?0:it->second.player;}
void RuntimeUI::PlayerViewport(uint64_t player,glm::vec4 rect){for(auto& pair:m_documents)if(pair.second.player==player&&pair.second.viewport!=rect){pair.second.viewport=rect;InvalidateLayout();}}
void RuntimeUI::RemovePlayer(uint64_t player){std::vector<uint32_t> remove;for(auto& pair:m_documents)if(pair.second.player==player)remove.push_back(pair.first);for(auto handle:remove)Unload(handle);}
