#include "RuntimeUI.h"
#include "InputSystem.h"
#include "AuthoringJSON.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <atomic>
using nlohmann::json;
namespace {
std::atomic<uint64_t> textSerial{1};
const UIElement* FindElement(const UIDocument& d,const std::string& id){for(auto& e:d.elements)if(e.id==id)return &e;return nullptr;}
bool Focusable(UIKind k){return int(k)>=int(UIKind::Button);}
bool Available(const UIDocument& d,const UIElement* e){if(!d.visible||!d.enabled)return false;while(e){if(!e->visible||!e->enabled)return false;e=FindElement(d,e->parent);}return true;}
}
bool RuntimeUI::Focus(uint32_t h,const std::string& id){
 auto it=m_documents.find(h);if(it==m_documents.end())return false;auto& in=it->second;auto* e=FindElement(in.doc,id);
 if(!id.empty()&&(!e||!Focusable(e->kind)||!Available(in.doc,e)))return false;
 if(in.focus!=id){in.widgets[in.focus].composition.clear();in.focus=id;m_events.push_back({in.name,id,"focus",0,in.player});}
 if(e&&e->kind==UIKind::TextInput){if(m_textDocument!=h||m_textElement!=id)m_textSerial=textSerial++;m_textDocument=h;m_textElement=id;}else if(m_textDocument==h){m_textDocument=0;m_textElement.clear();}
 // Keep keyboard navigation visible through every scroll ancestor.
 if(e&&m_layoutWidth>0){Layout(m_layoutWidth,m_layoutHeight);auto target=in.layout.at(id).rect;auto* parent=FindElement(in.doc,e->parent);float scale=std::min(m_layoutWidth*in.viewport.z/in.doc.reference.x,m_layoutHeight*in.viewport.w/in.doc.reference.y)*in.scale;
 while(parent){if(parent->kind==UIKind::Scroll){auto rect=in.layout.at(parent->id).rect;auto offset=Scroll(h,parent->id);if(target.position.y<rect.position.y)offset.y+=(target.position.y-rect.position.y)/scale;else if(target.position.y+target.size.y>rect.position.y+rect.size.y)offset.y+=(target.position.y+target.size.y-rect.position.y-rect.size.y)/scale;SetScroll(h,parent->id,offset);}parent=FindElement(in.doc,parent->parent);}}
 return true;
}
std::string RuntimeUI::Focused(uint32_t h)const{auto i=m_documents.find(h);return i==m_documents.end()?"":i->second.focus;}
bool RuntimeUI::SetScale(uint32_t h,float value){auto i=m_documents.find(h);if(i==m_documents.end()||!std::isfinite(value)||value<.75f||value>2)return false;i->second.scale=value;InvalidateLayout();return true;}
float RuntimeUI::Scale(uint32_t h)const{auto i=m_documents.find(h);return i==m_documents.end()?1:i->second.scale;}
glm::vec2 RuntimeUI::Scroll(uint32_t h,const std::string& id)const{auto i=m_documents.find(h);if(i==m_documents.end())return {};auto w=i->second.widgets.find(id);return w==i->second.widgets.end()?glm::vec2(0):w->second.scroll;}
bool RuntimeUI::SetScroll(uint32_t h,const std::string& id,glm::vec2 value){auto i=m_documents.find(h);if(i==m_documents.end()||!std::isfinite(value.x)||!std::isfinite(value.y))return false;auto* e=FindElement(i->second.doc,id);if(!e||(e->kind!=UIKind::Scroll&&e->kind!=UIKind::List))return false;
 auto& in=i->second;auto& state=in.widgets[id];glm::vec2 visible=e->size;auto l=in.layout.find(id);if(l!=in.layout.end()){float scale=std::min(m_layoutWidth*in.viewport.z/in.doc.reference.x,m_layoutHeight*in.viewport.w/in.doc.reference.y)*in.scale;visible=l->second.rect.size/std::max(.001f,scale);}
 auto extent=state.extent;if(e->kind==UIKind::List)extent={visible.x,e->rowHeight*state.items.size()};value=glm::clamp(value,glm::vec2(0),glm::max(glm::vec2(0),extent-visible));if(value!=state.scroll){state.scroll=value;InvalidateLayout();}return true;
}
bool RuntimeUI::SetItems(uint32_t h,const std::string& id,const std::vector<UIListItem>& items,std::string& error){
 auto i=m_documents.find(h);auto* e=i==m_documents.end()?nullptr:FindElement(i->second.doc,id);if(!e||e->kind!=UIKind::List){error="stale/non-list UI element";return false;}if(items.size()>10000){error="UI list exceeds 10000 items";return false;}
 size_t bytes=0;std::set<std::string> ids;for(auto& item:items){bytes+=item.text.size()+item.id.size();if(item.id.empty()||item.id.size()>128||item.text.size()>512||bytes>2*1024*1024||!ids.insert(item.id).second||!ValidTextUTF8(item.text,error)||!ValidTextUTF8(item.id,error)||item.id.find('\0')!=std::string::npos||item.text.find_first_of("\r\n\0",0,3)!=std::string::npos){error="invalid/duplicate/bounded UI list item";return false;}}
 auto& state=i->second.widgets[id];std::string selected=state.selected>=0&&state.selected<int(state.items.size())?state.items[state.selected].id:"";state.items=items;state.selected=-1;for(size_t n=0;n<items.size();++n)if(items[n].id==selected&&items[n].enabled)state.selected=int(n);SetScroll(h,id,state.scroll);InvalidateLayout();error.clear();return true;
}
int RuntimeUI::Selected(uint32_t h,const std::string& id)const{auto i=m_documents.find(h);if(i==m_documents.end())return -1;auto w=i->second.widgets.find(id);return w==i->second.widgets.end()?-1:w->second.selected;}
bool RuntimeUI::Select(uint32_t h,const std::string& id,int index){auto i=m_documents.find(h);if(i==m_documents.end())return false;auto* e=FindElement(i->second.doc,id);if(!e||e->kind!=UIKind::List)return false;auto& state=i->second.widgets[id];if(index< -1||index>=int(state.items.size())||(index>=0&&!state.items[index].enabled))return false;state.selected=index;if(index>=0){float visible=e->size.y;auto layout=i->second.layout.find(id);if(layout!=i->second.layout.end()){float scale=std::min(m_layoutWidth*i->second.viewport.z/i->second.doc.reference.x,m_layoutHeight*i->second.viewport.w/i->second.doc.reference.y)*i->second.scale;visible=layout->second.rect.size.y/std::max(.001f,scale);}
 auto offset=state.scroll;float top=index*e->rowHeight;if(top<offset.y)offset.y=top;else if(top+e->rowHeight>offset.y+visible)offset.y=top+e->rowHeight-visible;SetScroll(h,id,offset);}return true;}
TextSelection RuntimeUI::Selection(uint32_t h,const std::string& id)const{auto i=m_documents.find(h);if(i==m_documents.end())return {};auto w=i->second.widgets.find(id);return w==i->second.widgets.end()?TextSelection{}:w->second.selection;}
bool RuntimeUI::SelectText(uint32_t h,const std::string& id,TextSelection selection){auto i=m_documents.find(h);if(i==m_documents.end())return false;auto* e=FindElement(i->second.doc,id);if(!e||e->kind!=UIKind::TextInput)return false;auto n=TextGraphemes(e->text).size()-1;if(selection.anchor>n||selection.caret>n)return false;i->second.widgets[id].selection=selection;return true;}
std::optional<UITextTarget> RuntimeUI::TextTarget()const{auto i=m_documents.find(m_textDocument);if(i==m_documents.end()||i->second.focus!=m_textElement)return {};auto* e=FindElement(i->second.doc,m_textElement);if(!e||!Available(i->second.doc,e))return {};auto l=i->second.layout.find(m_textElement);if(l==i->second.layout.end())return {};auto rect=l->second.rect;auto state=i->second.widgets.find(m_textElement);if(state!=i->second.widgets.end()&&!state->second.carets.empty()){auto& w=state->second;rect.position.x+=8+ w.carets[std::min(w.selection.caret,w.carets.size()-1)]-w.textPan;rect.size.x=2;}return UITextTarget{i->second.player,rect,m_textSerial};}
void RuntimeUI::Edit(Instance& in,UIElement& e,InputSystem& input,uint64_t player){
 auto& state=in.widgets[e.id];auto original=e.text;auto bounds=TextGraphemes(e.text);state.selection.anchor=std::min(state.selection.anchor,bounds.size()-1);state.selection.caret=std::min(state.selection.caret,bounds.size()-1);
 auto remember=[&]{if(state.undo.size()>=32)state.undo.erase(state.undo.begin());state.undo.emplace_back(e.text,state.selection);state.redo.clear();};
 auto replace=[&](const std::string& value){if(e.readOnly)return;auto text=e.text;auto selection=state.selection;if(EditText(text,selection,value,e.maxBytes)){remember();e.text=std::move(text);state.selection=selection;state.composition.clear();}};
 for(const auto& event:input.TextEvents())if(event.player==player&&(event.token==0||event.token==m_textSerial)){
  auto cmd=event.command;auto n=TextGraphemes(e.text).size()-1;
  if(cmd=="composition"&&!e.readOnly){state.composition=event.text;state.compositionStart=event.start;state.compositionLength=event.length;}
  else if(cmd=="insert")replace(event.text);
  else if(cmd=="all")state.selection={0,n};
  else if(cmd=="left"||cmd=="right"||cmd=="home"||cmd=="end"){
   size_t next=state.selection.caret;if(cmd=="home")next=0;else if(cmd=="end")next=n;else if(!event.shift&&state.selection.anchor!=next)next=cmd=="left"?std::min(next,state.selection.anchor):std::max(next,state.selection.anchor);else if(cmd=="left")next=next?next-1:0;else next=std::min(n,next+1);state.selection.caret=next;if(!event.shift)state.selection.anchor=next;state.composition.clear();
  }else if(cmd=="backspace"||cmd=="delete"){
   if(state.selection.anchor==state.selection.caret){if(cmd=="backspace"&&state.selection.anchor)state.selection.anchor--;else if(cmd=="delete"&&state.selection.anchor<n)state.selection.anchor++;}replace("");
  }else if(cmd=="copy"||cmd=="cut"){
   auto b=TextGraphemes(e.text);auto a=std::min(state.selection.anchor,state.selection.caret),z=std::max(state.selection.anchor,state.selection.caret);if(input.textPlatform&&input.textPlatform->clipboardWrite&&a!=z)input.textPlatform->clipboardWrite(e.text.substr(b[a],b[z]-b[a]));if(cmd=="cut")replace("");
  }else if(cmd=="paste"){if(input.textPlatform&&input.textPlatform->clipboardRead)replace(input.textPlatform->clipboardRead());}
  else if((cmd=="undo"||cmd=="redo")&&!e.readOnly){auto& from=cmd=="undo"&&!event.shift?state.undo:state.redo;auto& to=cmd=="undo"&&!event.shift?state.redo:state.undo;if(!from.empty()){if(to.size()>=32)to.erase(to.begin());to.emplace_back(e.text,state.selection);e.text=from.back().first;state.selection=from.back().second;from.pop_back();state.composition.clear();}}
  else if(cmd=="submit"){m_events.push_back({in.name,e.id,"submit",0,in.player,e.text});state.composition.clear();}
  else if(cmd=="cancel")state.composition.clear();
 }
 if(original!=e.text){m_events.push_back({in.name,e.id,"change",0,in.player,e.text});InvalidateLayout();}
}
std::string RuntimeUI::CaptureState(uint32_t h)const{
 auto it=m_documents.find(h);if(it==m_documents.end())return "null";const auto& in=it->second;json values=json::array();for(auto& e:in.doc.elements){json value={{"id",e.id},{"kind",int(e.kind)},{"text",e.text},{"value",e.value},{"visible",e.visible},{"enabled",e.enabled}};auto state=in.widgets.find(e.id);if(state!=in.widgets.end()){auto& w=state->second;value["scroll"]={w.scroll.x,w.scroll.y};value["selection"]={w.selection.anchor,w.selection.caret};value["selectedKey"]=w.selected>=0&&w.selected<int(w.items.size())?w.items[w.selected].id:"";}values.push_back(value);}return json{{"version",1},{"scale",in.scale},{"focus",Available(in.doc,FindElement(in.doc,in.focus))?in.focus:""},{"elements",values}}.dump();
}
bool RuntimeUI::RestoreState(uint32_t h,const std::string& source,std::string& error){
 auto it=m_documents.find(h);if(it==m_documents.end()){error="stale UI document";return false;}auto backup=it->second;auto textDoc=m_textDocument;auto textElement=m_textElement;auto events=m_events;
 try{if(source.size()>4*1024*1024)throw std::runtime_error("UI state too large");auto j=AuthoringJSON::parse(source);if(j.at("version")!=1||!j.at("elements").is_array()||j["elements"].size()!=backup.doc.elements.size()||!SetScale(h,j.at("scale").get<float>()))throw std::runtime_error("UI state schema/scale mismatch");std::set<std::string> ids;
 for(auto& value:j["elements"]){auto id=value.at("id").get<std::string>();auto* e=Element(h,id);if(!e||int(e->kind)!=AuthoringJSON::typed<int>(value.at("kind"))||!ids.insert(id).second)throw std::runtime_error("UI state element mismatch");e->text=value.at("text").get<std::string>();e->value=value.at("value").get<float>();e->visible=value.at("visible").get<bool>();e->enabled=value.at("enabled").get<bool>();auto& state=it->second.widgets[id];state.composition.clear();state.undo.clear();state.redo.clear();
 if(e->kind==UIKind::List&&value.contains("selectedKey")){auto key=value.at("selectedKey").get<std::string>();int selected=-1;for(size_t n=0;n<state.items.size();++n)if(state.items[n].id==key&&state.items[n].enabled)selected=int(n);if(!Select(h,id,selected))throw std::runtime_error("invalid saved list selection");}
 if(e->kind==UIKind::TextInput&&value.contains("selection")){auto a=value["selection"];if(a.size()!=2||!SelectText(h,id,{AuthoringJSON::typed<size_t>(a[0]),AuthoringJSON::typed<size_t>(a[1])}))throw std::runtime_error("invalid saved selection");}
 if(value.contains("scroll")&&(e->kind==UIKind::Scroll||e->kind==UIKind::List)){auto a=value["scroll"];if(a.size()!=2||!std::isfinite(a[0].get<float>())||!std::isfinite(a[1].get<float>()))throw std::runtime_error("invalid saved scroll");state.scroll={a[0].get<float>(),a[1].get<float>()};}
 }
 if(!it->second.doc.Validate(error)||!Focus(h,j.at("focus").get<std::string>()))throw std::runtime_error(error.empty()?"invalid saved focus":error);
 if(m_textDocument==h)m_textSerial=textSerial++;
 InvalidateLayout();if(m_layoutWidth)Layout(m_layoutWidth,m_layoutHeight);m_events=std::move(events);error.clear();return true;
 }catch(const std::exception& e){it->second=std::move(backup);m_textDocument=textDoc;m_textElement=textElement;m_events=std::move(events);InvalidateLayout();error=e.what();return false;}
}
std::string RuntimeUI::Accessibility(uint32_t h)const{
 auto i=m_documents.find(h);if(i==m_documents.end())return "null";json rows=json::array();const char* roles[]={"group","group","text","image","button","slider","toggle","textbox","scroll","list"};for(auto& e:i->second.doc.elements)if(Available(i->second.doc,&e)){json row={{"id",e.id},{"role",roles[int(e.kind)]},{"label",e.accessibleLabel.empty()?e.text:e.accessibleLabel},{"description",e.description},{"focused",i->second.focus==e.id},{"value",e.value}};if(e.kind==UIKind::TextInput)row["text"]=e.text;if(e.kind==UIKind::List)row["selectedIndex"]=Selected(h,e.id);rows.push_back(row);}return rows.dump();
}
