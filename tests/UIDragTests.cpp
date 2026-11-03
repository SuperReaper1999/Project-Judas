#include "RuntimeUI.h"
#include "InputSystem.h"
#include "NamedAuthoring.h"
#include <algorithm>
#include <iostream>

namespace {
int checks=0,failures=0;
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
UIDocument Menu(){
    UIDocument d;d.modal=true;
    UIElement root;root.id="canvas";root.kind=UIKind::Canvas;
    UIElement source;source.id="wood";source.parent="canvas";source.kind=UIKind::Image;source.size={80,80};source.offset={20,20};source.dragType="inventory";source.text="Wood";
    UIElement target;target.id="bag";target.parent="canvas";target.kind=UIKind::Button;target.offset={160,20};target.size={80,80};target.dropType="inventory";
    UIElement wrong=target;wrong.id="equipment";wrong.offset={300,20};wrong.dropType="armour";
    d.elements={root,source,target,wrong};return d;
}
bool Has(const std::vector<UIEvent>& events,const char* type,const char* element=""){
    return std::any_of(events.begin(),events.end(),[&](const auto& e){return e.type==type&&(!*element||e.element==element);});
}
}
int main(){
    std::string error;auto d=Menu();Check(d.Validate(error),"opt-in image and destination validate");
    const auto serialized=SerializeUIDocument(d);Check(serialized.rfind("JudasUI 4",0)==0,"drag fields select version four");
    UIDocument restored;Check(ParseUIDocument(serialized,restored,error)&&restored.elements[1].dragType=="inventory"&&restored.elements[2].dropType=="inventory","authored categories survive exact round trip");
    auto legacy=d;for(auto& e:legacy.elements){e.dragType.clear();e.dropType.clear();}Check(SerializeUIDocument(legacy).rfind("JudasUI 1",0)==0,"default documents retain legacy encoding");
    std::string named,legacyText;Check(LegacyToNamed(serialized,"ui",named,error)&&NamedToLegacy(named,"ui",legacyText,error)&&legacyText==serialized,"named authoring retains transfer categories");
    auto bad=d;bad.elements[1].dragType=std::string("bad\0type",8);Check(!bad.Validate(error),"embedded null category rejected");
    auto invalidSource=d;invalidSource.elements[1].kind=UIKind::Slider;Check(!invalidSource.Validate(error),"item drag cannot hijack slider editing or list-row identity");
    RuntimeUI names;Check(!names.Add(d,std::string("bad\0name",8),0,error)&&!names.Add(d,std::string(129,'x'),0,error),"transfer document identities reject NUL and unbounded names");
    auto unicode=names.Add(d,u8"Inventaire café",0,error);Check(unicode&&names.Name(unicode)==u8"Inventaire café","bounded Unicode document identities remain supported");
    RuntimeUI ui;auto h=ui.Add(d,"inventory",0,error);ui.Layout(1280,720);InputSystem input;
    auto frame=[&](glm::vec2 point,float button){input.BeginFrame();input.SetPhysical("mouse:Left",button);ui.Input(input,point,true,1280,720);};
    frame({40,40},1);frame({65,45},1);auto events=ui.TakeEvents();Check(Has(events,"dragStart","wood")&&ui.Drag(h).has_value(),"actual pointer threshold starts typed drag");
    frame({180,40},1);frame({180,40},0);events=ui.TakeEvents();
    Check(Has(events,"drop","bag")&&Has(events,"dragEnd","wood")&&!Has(events,"click"),"release requests transfer once without a duplicate click");
    auto drop=std::find_if(events.begin(),events.end(),[](const auto& e){return e.type=="drop";});
    Check(drop!=events.end()&&drop->sourceDocument=="inventory"&&drop->sourceElement=="wood"&&drop->targetElement=="bag"&&drop->dragType=="inventory","drop records stable source, target and category");
    Check(!ui.Drag(h),"completed drag releases capture");
    for(auto point:{glm::vec2(330,40),glm::vec2(900,600)}){
        frame({40,40},1);frame({65,45},1);frame(point,0);events=ui.TakeEvents();Check(!Has(events,"drop")&&Has(events,"dragCancel"),"wrong category or outside release cancels without transfer");
    }
    frame({40,40},1);frame({40,40},0);events=ui.TakeEvents();Check(Has(events,"click","wood")&&!Has(events,"drop"),"short pointer tap retains ordinary click");
    frame({40,40},1);frame({65,45},1);input.Reset();ui.Input(input,{180,40},true,1280,720);events=ui.TakeEvents();Check(!ui.Drag(h)&&Has(events,"dragCancel")&&!Has(events,"drop"),"focus/input reset cancels captured item");
    ui.Element(h,"bag")->enabled=false;frame({40,40},1);frame({65,45},1);frame({180,40},0);events=ui.TakeEvents();Check(!Has(events,"drop"),"disabled target never accepts");ui.Element(h,"bag")->enabled=true;
    frame({40,40},1);frame({65,45},1);ui.Element(h,"wood")->visible=false;frame({180,40},0);events=ui.TakeEvents();Check(Has(events,"dragCancel")&&!Has(events,"drop"),"hidden source invalidates transaction");ui.Element(h,"wood")->visible=true;
    input.Reset();ui.Focus(h,"wood");input.SetPhysical("key:Return",1);ui.Input(input,{-1,-1},false,1280,720);Check(ui.Drag(h)&&ui.Drag(h)->keyboard,"focus Activate picks up without pointer");
    input.BeginFrame();input.SetPhysical("key:Return",0);input.SetPhysical("key:Down",1);ui.Input(input,{-1,-1},false,1280,720);
    input.BeginFrame();input.SetPhysical("key:Down",0);input.SetPhysical("key:Return",1);ui.Input(input,{-1,-1},false,1280,720);events=ui.TakeEvents();Check(Has(events,"drop","bag")&&!ui.Drag(h),"keyboard focus and Activate complete transfer");
    input.Reset();ui.Focus(h,"wood");Check(ui.BeginDrag(h,"wood"),"scripted pickup uses same validation");input.SetPhysical("key:Escape",1);ui.Input(input,{-1,-1},false,1280,720);events=ui.TakeEvents();Check(Has(events,"dragCancel")&&!Has(events,"back")&&!Has(events,"drop"),"Back cancels instead of both cancelling and closing menu");
    input.Reset();ui.BeginDrag(h,"wood");auto snapshot=ui.CaptureState(h);Check(snapshot.find("dragType")==std::string::npos&&snapshot.find("keyboard")==std::string::npos,"transient drag is not a saved transaction");Check(ui.RestoreState(h,snapshot,error)&&!ui.Drag(h),"successful state restore cancels transient capture");
    Check(ui.SetPreviewCamera(h,"wood",55)&&ui.PreviewCamera(h,"wood")==55,"image stores borrowed camera identity");Check(!ui.SetPreviewCamera(h,"bag",55),"non-image preview binding rejected");Check(ui.SetPreviewCamera(h,"wood",0)&&ui.PreviewCamera(h,"wood")==0,"clear releases borrowed binding");
    ui.BeginDrag(h,"wood");ui.TakeEvents();ui.Unload(h);Check(!ui.Drag(h)&&ui.TakeEvents().empty()&&!ui.SetPreviewCamera(h,"wood",55),"unload invalidates drag, preview and queued source events");
    auto a=ui.Add(d,"first",0,error),b=ui.Add(d,"second",0,error);ui.SetPlayer(a,1);ui.SetPlayer(b,2);ui.Layout(1280,720);Check(ui.BeginDrag(a,"wood")&&!ui.Drag(b),"players cannot observe another player's drag");ui.RemovePlayer(1);Check(!ui.Document(a)&&ui.Document(b),"player departure cleans only owned transaction");
    ui.Clear();auto nonmodal=Menu();nonmodal.modal=false;
    auto left=ui.Add(nonmodal,"left",0,error);auto rightDoc=nonmodal;rightDoc.elements={rightDoc.elements[0],rightDoc.elements[2]};rightDoc.elements[1].offset={500,20};auto right=ui.Add(rightDoc,"right",0,error);ui.Layout(1280,720);input.Reset();
    frame({40,40},1);frame({65,45},1);frame({520,40},0);events=ui.TakeEvents();
    Check(Has(events,"drop","bag")&&std::any_of(events.begin(),events.end(),[](const auto& e){return e.type=="drop"&&e.sourceDocument=="left"&&e.targetDocument=="right";}),"actual nonmodal pointer capture transfers between documents");
    ui.Clear();auto clipped=Menu();UIElement parent;parent.id="clip";parent.parent="canvas";parent.clip=true;parent.size={50,50};clipped.elements.insert(clipped.elements.begin()+1,parent);clipped.elements[2].parent="clip";clipped.elements[2].offset={80,20};auto clip=ui.Add(clipped,"clipped",0,error);ui.Layout(1280,720);Check(!ui.BeginDrag(clip,"wood"),"fully clipped source cannot be picked up programmatically");
    ui.Clear();auto scaled=ui.Add(Menu(),"scaled",0,error);
    for(auto size:{glm::ivec2(800,600),glm::ivec2(1920,1080)})for(float scale:{.75f,1.f,1.5f}){
        ui.SetScale(scaled,scale);ui.Layout(size.x,size.y);input.Reset();auto point=ui.LayoutOf(scaled,"wood")->rect.position+glm::vec2(10);auto target=ui.LayoutOf(scaled,"bag")->rect.position+glm::vec2(10);
        input.SetPhysical("mouse:Left",1);ui.Input(input,point,true,size.x,size.y);input.BeginFrame();ui.Input(input,point+glm::vec2(30),true,size.x,size.y);input.BeginFrame();input.SetPhysical("mouse:Left",0);ui.Input(input,target,true,size.x,size.y);events=ui.TakeEvents();Check(Has(events,"drop","bag"),"drag hit geometry agrees across reference resolutions and UI scales");
    }
    ui.Clear();auto offscreen=Menu();offscreen.elements[2].offset={1800,20};auto off=ui.Add(offscreen,"offscreen",0,error);ui.Layout(1280,720);input.Reset();ui.BeginDrag(off,"wood");ui.Focus(off,"bag");input.SetPhysical("key:Return",1);ui.Input(input,{-1,-1},false,1280,720);events=ui.TakeEvents();Check(!Has(events,"drop")&&Has(events,"dragCancel"),"keyboard cannot drop into a fully clipped target");
    ui.Clear();auto modal=ui.Add(Menu(),"source-modal",0,error);ui.Layout(1280,720);ui.BeginDrag(modal,"wood");auto blocker=ui.Add(Menu(),"new-modal",0,error);input.Reset();ui.Input(input,{180,40},true,1280,720);events=ui.TakeEvents();Check(!ui.Drag(modal)&&!Has(events,"drop"),"new modal invalidates an earlier drag instead of accepting through it");(void)blocker;
    (void)left;(void)right;
    std::cout<<"SUMMARY "<<checks<<" checks "<<failures<<" failures\n";return failures?1:0;
}
