#include "Window.h"
#include "InputDeviceHub.h"
#include "AppIcon.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "BrowserSurface.h"
#include <emscripten/html5_webgl.h>
EM_ASYNC_JS(void, AwaitBrowserFrame, (), {
    // RAF paces the compositor, but does not bound work queued to WebGL's
    // renderer process. Keep only one submitted GPU frame outstanding. Waiting
    // asynchronously leaves the page and input proxies responsive; it does not
    // change the fixed-step simulation policy or discard rendered frames.
    const gl=GLctx;
    const fence=gl.fenceSync(gl.SYNC_GPU_COMMANDS_COMPLETE,0);
    if(!fence)throw new Error('Cannot synchronize browser GPU frame');
    try {
        gl.flush();
        await new Promise((resolve,reject) => {
            let complete=false,frame=0,timer=0;
            const cancel=()=>{cancelAnimationFrame(frame);clearTimeout(timer);};
            const schedule=()=>{
                // A hidden tab can stop RAF before the engine observes it.
                // Retain the lifecycle wake while waiting for GPU completion.
                timer=setTimeout(poll,250);frame=requestAnimationFrame(poll);
            };
            const poll=()=>{
                if(complete)return;
                cancel();
                const state=gl.clientWaitSync(fence,0,0);
                if(state===gl.ALREADY_SIGNALED||state===gl.CONDITION_SATISFIED){
                    complete=true;resolve();
                }else if(state===gl.WAIT_FAILED||gl.isContextLost()){
                    complete=true;reject(new Error('Browser GPU frame failed'));
                }else schedule();
            };
            schedule();
        });
    } finally {
        gl.deleteSync(fence);
    }
});
#endif

#include <cstdio>
#include <cmath>

bool Window::Init(const char* title, int width, int height, bool visible) {
#ifdef _WIN32
    SDL_SetMainReady();
#endif
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE,"0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS,"0");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    m_sdlInitialized = true;

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
#ifdef __ANDROID__
    windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
    windowFlags |= visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN;

    m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                                 height, windowFlags);
    if (!m_window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    std::string iconError;
    if (!ApplyAppIcon(m_window, "", iconError))
        std::fprintf(stderr, "App icon: %s\n", iconError.c_str());

#ifdef __EMSCRIPTEN__
    // SDL2's EGL functions proxy to the UI thread. That thread no longer owns
    // the transferred canvas. Create WebGL through the supported worker API;
    // SDL continues owning window events and input, not a forged SDL GL handle.
    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.majorVersion=2;attributes.minorVersion=0;
    attributes.alpha=EM_FALSE;attributes.antialias=EM_FALSE;
    attributes.proxyContextToMainThread=EMSCRIPTEN_WEBGL_CONTEXT_PROXY_DISALLOW;
    m_browserGLContext=emscripten_webgl_create_context("#canvas",&attributes);
    if(m_browserGLContext<=0||emscripten_webgl_make_context_current(m_browserGLContext)!=EMSCRIPTEN_RESULT_SUCCESS) {
        std::fprintf(stderr,"Cannot create a WebGL2 context on the game worker\n");return false;
    }
#else
    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);
#endif

    m_input.textPlatform=std::make_shared<TextInputPlatform>();
    m_input.textPlatform->clipboardRead=[](){char* p=SDL_GetClipboardText();std::string result=p?p:"";SDL_free(p);return result;};
    m_input.textPlatform->clipboardWrite=[](const std::string& s){SDL_SetClipboardText(s.c_str());};
    SDL_StopTextInput();
    SDL_GetWindowSize(m_window,&m_width,&m_height);
    return true;
}

bool Window::Resize(int width, int height, std::string& error) {
#ifdef __EMSCRIPTEN__
    (void)width; (void)height;
    error = "Browser RESIZE is page-owned; resize the browser viewport or use the page Fullscreen button";
    return false;
#elif defined(__ANDROID__)
    (void)width; (void)height;
    error = "Desktop window resize is unavailable on Android; the native surface owns its size";
    return false;
#else
    if (!m_window || width < 1 || height < 1 || width > 8192 || height > 8192) {
        error = "Window resize requires an initialized window and dimensions 1..8192";
        return false;
    }
    const char* driver = SDL_GetCurrentVideoDriver();
    if (driver && std::string(driver) == "offscreen") {
        error = "SDL offscreen cannot resize its GL backing surface; use a desktop video driver or Xvfb, or set the startup window size";
        return false;
    }
    SDL_SetWindowSize(m_window, width, height);
    // Publish native configure events to the GL drawable as well as SDL's
    // logical size. Pumping queues device events but does not route them into
    // gameplay; deterministic harness input still bypasses PollEvents.
    SDL_PumpEvents();
    // Complete the surface's presentation boundary before another draw/readback.
    // Mesa GLX can report the resized X window while retaining the previous
    // backbuffer until a swap. This also matters for hidden, capture-only runs
    // which deliberately do not present every harness frame.
    SDL_GL_SwapWindow(m_window);
    SDL_GetWindowSize(m_window, &m_width, &m_height);
    if (m_width != width || m_height != height) {
        error = "Window manager did not grant the requested size";
        return false;
    }
    return true;
#endif
}

Window::~Window() {
    Shutdown();
}

void Window::Shutdown() {
    if(m_controller){SDL_GameControllerClose(m_controller);m_controller=nullptr;}
    m_controllerId=-1;m_controllerInputReady=false;
    m_input.Reset();
#ifdef __EMSCRIPTEN__
    if(m_browserGLContext) {
        emscripten_webgl_make_context_current(0);
        emscripten_webgl_destroy_context(m_browserGLContext);
        m_browserGLContext=0;
    }
#endif
    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    for(auto& p:m_localControllers)SDL_GameControllerClose(p.second.first);
    m_localControllers.clear();
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlInitialized) {
        SDL_Quit();
        m_sdlInitialized = false;
    }
}

void Window::SetEventHook(std::function<void(const SDL_Event&)> hook) {
    m_eventHook = std::move(hook);
}

bool Window::ConsumeSpawnEntityRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("spawn");
    m_spawnEntityRequested = false;
    return requested;
}

bool Window::ConsumeDestroyEntityRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("destroy");
    m_destroyEntityRequested = false;
    return requested;
}

bool Window::ConsumeSaveWorldStateRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("save_state");
    m_saveWorldStateRequested = false;
    return requested;
}

bool Window::ConsumeDeleteWorldStateRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("delete_state");
    m_deleteWorldStateRequested = false;
    return requested;
}

void Window::ClearPendingRequests() {
    m_input.DiscardPending();m_consumed.clear();
    m_spawnEntityRequested = m_destroyEntityRequested = m_saveWorldStateRequested = m_deleteWorldStateRequested = false;
    m_resetRequested = m_jumpRequested = m_controlToggleRequested = m_torchToggleRequested = false;
    m_interactRequested = m_viewToggleRequested = m_throwRequested = m_sasToggleRequested = false;
    m_uiBackRequested = m_uiUpRequested = m_uiDownRequested = m_uiActivateRequested = m_uiClickRequested = false;
}

void Window::SetInputClaimed(bool keyboard, bool mouse) {
    if(m_keyboardClaimed!=keyboard){m_controllerInputReady=false;m_input.ClearDevice("stick:");}
    if(m_keyboardClaimed!=keyboard||m_mouseClaimed!=mouse)if(auto hub=m_input.localDevices.lock())hub->Reset();
    m_keyboardClaimed = keyboard;
    m_mouseClaimed = mouse;
}

namespace {
const char* PadButton(int b){static const char* names[]={"South","East","West","North","Back","Guide","Start","LeftStick","RightStick","LeftShoulder","RightShoulder","DpadUp","DpadDown","DpadLeft","DpadRight"};return b>=0&&b<15?names[b]:nullptr;}
const char* PadAxis(int a){static const char* names[]={"LeftX","LeftY","RightX","RightY","LeftTrigger","RightTrigger"};return a>=0&&a<6?names[a]:nullptr;}
const char* MouseButton(int b){switch(b){case 1:return "Left";case 2:return "Middle";case 3:return "Right";case 4:return "X1";case 5:return "X2";default:return nullptr;}}
float NormalizedPadAxis(int axis,int value){return axis>=4?std::max(0,value)/32767.f:value/(value<0?32768.f:32767.f);}
}
void Window::RefreshController(bool poll){
    if(m_controller&&!SDL_GameControllerGetAttached(m_controller)){SDL_GameControllerClose(m_controller);m_controller=nullptr;m_controllerId=-1;m_controllerInputReady=false;m_input.ClearDevice("pad:");m_input.ClearDevice("stick:");}
    if(!m_controller){for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){m_controller=SDL_GameControllerOpen(i);if(m_controller){m_controllerId=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(m_controller));m_controllerInputReady=false;m_input.ClearDevice("stick:");break;}}}
    if(m_controller&&poll){
        const bool available=!m_keyboardClaimed&&m_inputFocused;
        for(int b=0;b<15;++b)m_input.SetPhysical(std::string("pad:")+PadButton(b),(m_keyboardClaimed||!m_inputFocused)?0:SDL_GameControllerGetButton(m_controller,static_cast<SDL_GameControllerButton>(b)));
        auto axis=[&](int a){return available?NormalizedPadAxis(a,SDL_GameControllerGetAxis(m_controller,static_cast<SDL_GameControllerAxis>(a))):0.f;};
        m_input.SetStick("left",axis(0),axis(1),!m_controllerInputReady||!available);
        m_input.SetStick("right",axis(2),axis(3),!m_controllerInputReady||!available);
        for(int a=4;a<6;++a)m_input.SetPhysical(std::string("stick:")+PadAxis(a),axis(a));
        m_controllerInputReady=available;
    }
}
void Window::RefreshLocalControllers(bool poll){
 auto hub=m_input.localDevices.lock();
 // A new world may publish its hub before an intervening device poll. Native
 // controller handles are reusable, but their previous world tokens are not.
 if(hub!=m_localHub.lock()){for(auto& p:m_localControllers)SDL_GameControllerClose(p.second.first);m_localControllers.clear();m_localHub=hub;}
 if(!hub){for(auto& p:m_localControllers)SDL_GameControllerClose(p.second.first);m_localControllers.clear();return;}
 for(auto it=m_localControllers.begin();it!=m_localControllers.end();)if(!SDL_GameControllerGetAttached(it->second.first)){hub->Disconnect(it->second.second);SDL_GameControllerClose(it->second.first);it=m_localControllers.erase(it);}else ++it;
 for(int i=0;i<SDL_NumJoysticks()&&m_localControllers.size()<16;++i)if(SDL_IsGameController(i)){
  const auto instance=SDL_JoystickGetDeviceInstanceID(i);if(m_localControllers.count(instance))continue;
  auto* controller=SDL_GameControllerOpen(i);if(!controller)continue;const char* name=SDL_GameControllerName(controller);
  auto token=hub->Connect("gamepad",name?name:"Gamepad");if(token.empty()){SDL_GameControllerClose(controller);break;}
  m_localControllers[instance]={controller,token};
 }
 if(!poll)return;
 const bool available=!m_keyboardClaimed&&m_inputFocused&&m_applicationActive;
 for(auto& entry:m_localControllers){auto* pad=entry.second.first;const auto& id=entry.second.second;
  for(int b=0;b<15;++b)hub->Set(id,std::string("pad:")+PadButton(b),available?SDL_GameControllerGetButton(pad,SDL_GameControllerButton(b)):0);
  for(int a=0;a<6;++a)hub->Set(id,std::string("stick:")+PadAxis(a),available?NormalizedPadAxis(a,SDL_GameControllerGetAxis(pad,SDL_GameControllerAxis(a))):0);
 }
}
void Window::SetTextInput(bool enabled,std::uint64_t player,int x,int y,int w,int h,std::uint64_t token){
 if(enabled&&m_textActive&&(m_textPlayer!=player||m_textToken!=token)&&!m_hostTextInput)SDL_StopTextInput();
 enabled=enabled&&m_inputFocused&&m_applicationActive&&!m_keyboardClaimed&&!m_hostTextInput;
 if(enabled){SDL_Rect rect{x,y,std::max(1,w),std::max(1,h)};SDL_SetTextInputRect(&rect);}
 m_textActive=enabled;m_textPlayer=enabled?player:0;m_textToken=enabled?token:0;
 UpdateNativeTextInput();
}
void Window::SetHostTextInput(bool enabled){
 m_hostTextInput=enabled;
 if(enabled){m_textActive=false;m_textPlayer=0;m_textToken=0;}
 UpdateNativeTextInput();
}
void Window::UpdateNativeTextInput(){
 const bool enabled=(m_hostTextInput||m_textActive)&&m_inputFocused&&m_applicationActive;
 if(enabled!=(SDL_IsTextInputActive()==SDL_TRUE)){
  if(enabled)SDL_StartTextInput();else SDL_StopTextInput();
 }
}
#ifdef __EMSCRIPTEN__
void Window::UpdateBrowserSurface() {
    double values[3]{};int flags[4]{};
    // DOM lives on the UI thread; SDL/WebGL resizing stays on this game worker.
    // One synchronous observation keeps canvas focus, visibility and sizing coherent.
    MAIN_THREAD_EM_ASM({
        const canvas=Module.canvas;const r=canvas.getBoundingClientRect();
        HEAPF64[$0>>3]=r.width;HEAPF64[($0>>3)+1]=r.height;
        HEAPF64[($0>>3)+2]=window.devicePixelRatio||1;
        HEAP32[$1>>2]=!document.hidden&&r.width>0&&r.height>0?1:0;
        HEAP32[($1>>2)+1]=document.hasFocus()&&
            (document.activeElement===canvas||document.pointerLockElement===canvas)?1:0;
        HEAP32[($1>>2)+2]=document.pointerLockElement===canvas?1:0;
        HEAP32[($1>>2)+3]=Module.judasVisibilityEpoch||0;
    },values,flags);
    const auto size=ChooseBrowserSurface(values[0],values[1],values[2]);
    if(size.width>0&&size.height>0) {
        int w=0,h=0;SDL_GetWindowSize(m_window,&w,&h);
        if(w!=size.width||h!=size.height)SDL_SetWindowSize(m_window,size.width,size.height);
        emscripten_get_canvas_element_size("#canvas",&m_width,&m_height);
        m_browserMouseScaleX=float(values[0]/std::max(1,m_width));
        m_browserMouseScaleY=float(values[1]/std::max(1,m_height));
    }
    // A capture request is not a browser-granted lock. Discard page motion and
    // the acquisition/release boundary, including queued pre-lock mouse events.
    const bool locked=flags[2]!=0;
    m_browserAcceptRelative=locked&&m_browserPointerLocked&&
        size.width==m_browserPublishedWidth&&size.height==m_browserPublishedHeight;
    if(!locked||locked!=m_browserPointerLocked) {
        m_input.SetPhysical("mouse:dx",0);m_input.SetPhysical("mouse:dy",0);
        if(auto hub=m_input.localDevices.lock()) {
            hub->Set("mouse","mouse:dx",0);hub->Set("mouse","mouse:dy",0);
        }
    }
    m_browserPointerLocked=locked;
    // Latch transitions on the UI thread: a browser may freeze this worker
    // for the entire hidden interval, so polling only the current flag loses it.
    if(flags[3]!=m_browserVisibilityEpoch) {
        m_browserVisibilityEpoch=flags[3];m_lifecycleClockReset=true;
    }
    const bool active=flags[0]!=0,focused=active&&flags[1]!=0;
    if(active!=m_applicationActive) {m_applicationActive=active;m_lifecycleClockReset=true;}
    if(focused!=m_inputFocused || !active) {
        m_inputFocused=focused;m_controllerInputReady=false;
        if(!focused)SetTextInput(false);
        m_input.Reset();if(auto hub=m_input.localDevices.lock())hub->Reset();ClearPendingRequests();
    }
    // SDL's document keyboard listener can also see page controls. They must not
    // feed either the legacy or per-player input route while the canvas is blurred.
    if(!focused) {m_input.Reset();if(auto hub=m_input.localDevices.lock())hub->Reset();ClearPendingRequests();}
}
void Window::PublishBrowserSurface() {
    // SDL may deliver queued CSS/logical size events after our resize request.
    // Publish the storage owned by this worker, never those stale dimensions.
    emscripten_get_canvas_element_size("#canvas",&m_width,&m_height);
    if(m_browserPublishedWidth!=m_width||m_browserPublishedHeight!=m_height||
       m_browserPublishedActive!=m_applicationActive||m_browserPublishedFocus!=m_inputFocused) {
        m_browserPublishedWidth=m_width;m_browserPublishedHeight=m_height;
        m_browserPublishedActive=m_applicationActive;m_browserPublishedFocus=m_inputFocused;
        MAIN_THREAD_EM_ASM({Module.judasSurface=({width:$0,height:$1,active:!!$2,focused:!!$3,
            generation:(Module.judasSurface?.generation||0)+1});},m_width,m_height,m_applicationActive,m_inputFocused);
    }
}
#endif

void Window::PollEvents(){
    m_input.BeginFrame();m_consumed.clear();
    auto hub=m_input.localDevices.lock();if(hub)hub->BeginFrame();
#ifdef __EMSCRIPTEN__
    // Observe DOM ownership before consuming queued input: resetting focus after
    // the event loop would discard the first legitimate click/key on return.
    UpdateBrowserSurface();
#endif
    if(!m_testInputMode)RefreshLocalControllers(false);
    if(hub)for(auto& [device,control,value]:m_testDevices)hub->Set(device,control,value);
    m_testDevices.clear();
    // Select one controller before events, but do not overwrite the previous
    // pair with its final poll: each delivered axis event must remain observable.
    if(!m_testInputMode)RefreshController(false);
    for(const auto& [control,value]:m_testPhysical){ApplyTestPhysical(control,value);}
    m_testPhysical.clear();
    if(m_keyboardClaimed)m_input.ClearDevice("key:");
    if(m_mouseClaimed){m_input.ClearDevice("mouse:");m_input.ClearDevice("touch:");if(m_testInputMode){auto p=m_input.Pointer();m_input.SetPointer(p.x,p.y,false);}}
    if(m_keyboardClaimed)SetTextInput(false);
    if(hub)hub->textPlatform=m_input.textPlatform;
    auto textEvent=[&](TextInputEvent value){value.player=m_textPlayer;value.token=m_textToken;m_input.QueueText(value);if(hub&&hub->textEvents.size()<128)hub->textEvents.push_back(std::move(value));};
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        // Harness input is injected into InputSystem; real devices must not mix in.
        if(m_testInputMode&&event.type!=SDL_QUIT&&event.type!=SDL_WINDOWEVENT&&
           !(event.type>=SDL_APP_TERMINATING&&event.type<=SDL_APP_DIDENTERFOREGROUND)&&event.type!=SDL_RENDER_DEVICE_RESET)continue;
        if(m_eventHook)m_eventHook(event);
        float mouseDX=0,mouseDY=0;
        if(event.type==SDL_MOUSEMOTION) {
            mouseDX=float(event.motion.xrel);mouseDY=float(event.motion.yrel);
#ifdef __EMSCRIPTEN__
            mouseDX=m_browserAcceptRelative?mouseDX*m_browserMouseScaleX:0;
            mouseDY=m_browserAcceptRelative?mouseDY*m_browserMouseScaleY:0;
#endif
        }
        if(hub){
         if(event.type==SDL_KEYDOWN||event.type==SDL_KEYUP){if(!m_keyboardClaimed&&m_inputFocused&&m_applicationActive)hub->Set("keyboard",std::string("key:")+SDL_GetScancodeName(event.key.keysym.scancode),event.type==SDL_KEYDOWN?1:0);}
         else if(event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP){if(!m_mouseClaimed&&m_inputFocused&&m_applicationActive&&event.button.which!=SDL_TOUCH_MOUSEID)if(auto* name=MouseButton(event.button.button)){hub->Set("mouse",std::string("mouse:")+name,event.type==SDL_MOUSEBUTTONDOWN?1:0);const auto* d=hub->Find("mouse");auto left=d->values.find("mouse:Left");hub->Pointer("mouse",{float(event.button.x)/std::max(1,m_width),float(event.button.y)/std::max(1,m_height),left!=d->values.end()&&left->second>.5f});}}
         else if(event.type==SDL_MOUSEMOTION&&!m_mouseClaimed&&m_inputFocused&&m_applicationActive&&event.motion.which!=SDL_TOUCH_MOUSEID){auto* d=hub->Find("mouse");auto pointer=d->pointer;pointer.x=float(event.motion.x)/std::max(1,m_width);pointer.y=float(event.motion.y)/std::max(1,m_height);hub->Pointer("mouse",pointer);if(m_mouseCaptured)for(auto p:{std::pair<const char*,float>{"mouse:dx",mouseDX},{"mouse:dy",mouseDY}}){auto old=d->values.find(p.first);hub->Set("mouse",p.first,p.second+(old==d->values.end()?0:old->second));}}
         else if(event.type==SDL_MOUSEWHEEL&&!m_mouseClaimed&&m_inputFocused&&m_applicationActive&&event.wheel.which!=SDL_TOUCH_MOUSEID){float sign=event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1:1;for(auto p:{std::pair<const char*,float>{"mouse:wheelX",sign*event.wheel.x},{"mouse:wheelY",sign*event.wheel.y}}){auto* d=hub->Find("mouse");auto old=d->values.find(p.first);hub->Set("mouse",p.first,p.second+(old==d->values.end()?0:old->second));}}
         else if(event.type==SDL_CONTROLLERAXISMOTION){auto pad=m_localControllers.find(event.caxis.which);if(pad!=m_localControllers.end()&&!m_keyboardClaimed&&m_inputFocused)if(auto* axis=PadAxis(event.caxis.axis))hub->Set(pad->second.second,std::string("stick:")+axis,NormalizedPadAxis(event.caxis.axis,event.caxis.value));}
         else if(event.type==SDL_CONTROLLERBUTTONDOWN||event.type==SDL_CONTROLLERBUTTONUP){auto pad=m_localControllers.find(event.cbutton.which);if(pad!=m_localControllers.end()&&!m_keyboardClaimed&&m_inputFocused)if(auto* button=PadButton(event.cbutton.button))hub->Set(pad->second.second,std::string("pad:")+button,event.type==SDL_CONTROLLERBUTTONDOWN?1:0);}
        }
        if(!m_textActive&&!m_keyboardClaimed&&m_inputFocused&&event.type==SDL_KEYDOWN&&event.key.keysym.sym==SDLK_TAB){
            TextInputEvent tab{"tab","",(event.key.keysym.mod&KMOD_SHIFT)!=0};m_input.QueueText(tab);
            if(hub&&hub->textEvents.size()<128){tab.player=~uint64_t(0);hub->textEvents.push_back(tab);}
        }
        if(m_textActive&&!m_keyboardClaimed&&m_inputFocused){
            if(event.type==SDL_TEXTINPUT)textEvent({"insert",event.text.text});
            else if(event.type==SDL_TEXTEDITING)textEvent({"composition",event.edit.text,false,false,event.edit.start,event.edit.length});
#if SDL_VERSION_ATLEAST(2,0,22)
            else if(event.type==SDL_TEXTEDITING_EXT){textEvent({"composition",event.editExt.text?event.editExt.text:"",false,false,event.editExt.start,event.editExt.length});SDL_free(event.editExt.text);event.editExt.text=nullptr;}
#endif
            else if(event.type==SDL_KEYDOWN){std::string cmd;auto key=event.key.keysym.sym;bool ctrl=(event.key.keysym.mod&KMOD_CTRL)!=0;
                if(key==SDLK_LEFT)cmd="left";else if(key==SDLK_RIGHT)cmd="right";else if(key==SDLK_HOME)cmd="home";else if(key==SDLK_END)cmd="end";else if(key==SDLK_BACKSPACE)cmd="backspace";else if(key==SDLK_DELETE)cmd="delete";else if(key==SDLK_RETURN||key==SDLK_KP_ENTER)cmd="submit";else if(key==SDLK_ESCAPE)cmd="cancel";else if(key==SDLK_TAB)cmd="tab";
                else if(ctrl){if(key==SDLK_a)cmd="all";else if(key==SDLK_c)cmd="copy";else if(key==SDLK_x)cmd="cut";else if(key==SDLK_v)cmd="paste";else if(key==SDLK_z)cmd="undo";else if(key==SDLK_y)cmd="redo";}
                if(!cmd.empty())textEvent({cmd,"",(event.key.keysym.mod&KMOD_SHIFT)!=0,ctrl});
            }
        }
#if SDL_VERSION_ATLEAST(2,0,22)
        if(event.type==SDL_TEXTEDITING_EXT&&event.editExt.text){SDL_free(event.editExt.text);event.editExt.text=nullptr;}
#endif
        if(event.type==SDL_QUIT)m_shouldClose=true;
        else if(event.type==SDL_APP_WILLENTERBACKGROUND){m_applicationActive=false;SetTextInput(false);m_lifecycleClockReset=true;m_input.Reset();if(hub)hub->Reset();ClearPendingRequests();}
        else if(event.type==SDL_APP_DIDENTERFOREGROUND){m_applicationActive=true;m_lifecycleClockReset=true;m_input.Reset();if(hub)hub->Reset();ClearPendingRequests();}
        else if(event.type==SDL_RENDER_DEVICE_RESET){
            // Context loss cannot leave stale native GPU handles in a live world.
            // Android normally preserves EGL across pause; a lost context exits
            // cleanly so a subsequent launch rebuilds resources from content.
            std::fprintf(stderr,"Graphics context lost; restart the application\n");m_shouldClose=true;
        }
        else if(event.type==SDL_FINGERDOWN||event.type==SDL_FINGERMOTION||event.type==SDL_FINGERUP){
            if((!m_mouseClaimed||event.type==SDL_FINGERUP)&&m_applicationActive){const auto previous=m_input.TouchId(event.tfinger.touchId,event.tfinger.fingerId);m_input.SetTouch(event.tfinger.touchId,event.tfinger.fingerId,event.tfinger.x,event.tfinger.y,event.tfinger.pressure,event.type!=SDL_FINGERUP);if(hub&&m_inputFocused){const auto current=m_input.TouchId(event.tfinger.touchId,event.tfinger.fingerId);hub->RecordTouch({current?current:previous,event.tfinger.x,event.tfinger.y,event.tfinger.pressure},event.type!=SDL_FINGERUP);}}
        }
        else if(event.type==SDL_WINDOWEVENT){
            if(event.window.event==SDL_WINDOWEVENT_CLOSE)m_shouldClose=true;
#ifndef __EMSCRIPTEN__
            if(event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){m_width=event.window.data1;m_height=event.window.data2;}
            if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&!m_testInputMode){m_inputFocused=false;SetTextInput(false);m_controllerInputReady=false;m_input.Reset();if(hub)hub->Reset();ClearPendingRequests();}
            if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED){
                m_inputFocused=true;m_controllerInputReady=false;
                // SDL/compositor capture can disappear independently of the
                // gameplay request. Restore that request, preserving paused UI.
                SetMouseCaptured(m_mouseCaptured);
            }
#endif
        }else if(event.type==SDL_CONTROLLERDEVICEREMOVED&&event.cdevice.which==m_controllerId){
            RefreshController(false);
        }else if(event.type==SDL_CONTROLLERAXISMOTION&&event.caxis.which==m_controllerId&&m_controllerInputReady&&!m_keyboardClaimed&&m_inputFocused){
            if(const auto* axis=PadAxis(event.caxis.axis))m_input.SetPhysical(std::string("stick:")+axis,NormalizedPadAxis(event.caxis.axis,event.caxis.value));
        }else if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&!m_keyboardClaimed){
            m_input.SetPhysical(std::string("key:")+SDL_GetScancodeName(event.key.keysym.scancode),event.type==SDL_KEYDOWN?1:0);
        }else if((event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&!m_mouseClaimed){
            if(const char* name=MouseButton(event.button.button))m_input.SetPhysical(std::string("mouse:")+name,event.type==SDL_MOUSEBUTTONDOWN?1:0);
            if(event.type==SDL_MOUSEBUTTONDOWN){m_uiClickX=event.button.x;m_uiClickY=event.button.y;}
        }else if(event.type==SDL_MOUSEMOTION&&!m_mouseClaimed&&m_mouseCaptured){
            m_input.AddDelta("mouse:dx",mouseDX);m_input.AddDelta("mouse:dy",mouseDY);
        }
        else if(event.type==SDL_MOUSEWHEEL&&!m_mouseClaimed){const float sign=event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1.f:1.f;m_input.AddDelta("mouse:wheelX",sign*event.wheel.x);m_input.AddDelta("mouse:wheelY",sign*event.wheel.y);}
    }
#ifdef __EMSCRIPTEN__
    PublishBrowserSurface();
    if(!m_inputFocused||!m_applicationActive)m_input.Reset();
#endif
    if(!m_testInputMode){int x=0,y=0;const auto buttons=SDL_GetMouseState(&x,&y);
        m_input.SetPointer(float(x)/std::max(1,m_width),float(y)/std::max(1,m_height),!m_mouseClaimed&&m_inputFocused&&m_applicationActive&&(buttons&SDL_BUTTON_LMASK));}
    // Restore held physical keys after UI ownership ends without stale edges.
    if(!m_testInputMode&&!m_keyboardClaimed&&m_inputFocused){const auto* keys=SDL_GetKeyboardState(nullptr);for(int i=0;i<SDL_NUM_SCANCODES;++i)if(keys[i])m_input.SetPhysical(std::string("key:")+SDL_GetScancodeName(static_cast<SDL_Scancode>(i)),1);}
    if(!m_testInputMode)RefreshController();
    if(hub){
      // Touch-generated mouse events remain on the legacy UI route only.
      // They must never masquerade as another player's physical mouse.
      hub->Touches(m_inputFocused&&m_applicationActive?m_input.Touches():std::vector<InputTouch>{});
      if(!m_testInputMode&&!m_keyboardClaimed&&m_inputFocused&&m_applicationActive){const auto* keys=SDL_GetKeyboardState(nullptr);for(int i=0;i<SDL_NUM_SCANCODES;++i)if(keys[i])hub->Set("keyboard",std::string("key:")+SDL_GetScancodeName(SDL_Scancode(i)),1);}
      if(!m_testInputMode)RefreshLocalControllers(true);
    }
    // Requested capture and the backend's actual grab are separate state.
    // A compositor/backend release need not produce another FOCUS_GAINED
    // event. Reconcile while this window owns focus; never grab it back from
    // another application, and leave an uncaptured menu alone.
    if(m_mouseCaptured && m_inputFocused && m_applicationActive && (SDL_GetWindowFlags(m_window)&SDL_WINDOW_INPUT_FOCUS) &&
       (!SDL_GetRelativeMouseMode() || !SDL_GetWindowGrab(m_window)))
        SetMouseCaptured(true);
}
bool Window::ConsumeNamedAction(const std::string& name){return m_input.Action(name).pressed&&m_consumed.insert(name).second;}

void Window::SwapBuffers() {
#ifdef __EMSCRIPTEN__
    // Presentation is implicit when the worker yields. Prefer RAF, allowing a
    // lifecycle wake if the browser stops delivering frames in a hidden tab.
    // Simulation retains its independent fixed-step policy.
    AwaitBrowserFrame();
#else
    SDL_GL_SwapWindow(m_window);
#endif
}

bool Window::IsActionActive(Action action) const {
    if (m_testInputMode) {
        return m_testActionState[static_cast<int>(action)];
    }

    static const char* names[]={"move_forward","move_backward","strafe_left","strafe_right","move_up","move_down","pitch_up","pitch_down","yaw_left","yaw_right","roll_left","roll_right","prograde","retrograde","radial","add_water","igniter"};
    const int index=static_cast<int>(action);return index>=0&&index<static_cast<int>(Action::Count)&&m_input.Action(names[index]).held;
}

void Window::GetMouseDelta(int& deltaX, int& deltaY) const {
    if (m_testInputMode) {
        deltaX = m_testMouseDeltaX;
        deltaY = m_testMouseDeltaY;
        m_testMouseDeltaX = 0;
        m_testMouseDeltaY = 0;
        return;
    }

    deltaX=m_mouseCaptured?static_cast<int>(m_input.Axis("look_x")):0;
    deltaY=m_mouseCaptured?static_cast<int>(m_input.Axis("look_y")):0;
}

bool Window::ConsumeResetRequest() {
    if (m_testInputMode) {
        const bool requested = m_testResetRequested;
        m_testResetRequested = false;
        return requested;
    }
    const bool requested = ConsumeNamedAction("reset");
    m_resetRequested = false;
    return requested;
}

bool Window::ConsumeJumpRequest() {
    if (m_testInputMode) {
        const bool requested = m_testJumpRequested;
        m_testJumpRequested = false;
        return requested;
    }
    const bool requested = ConsumeNamedAction("jump");
    m_jumpRequested = false;
    return requested;
}

bool Window::ConsumeControlToggleRequest() {
    if (m_testInputMode) {
        const bool requested = m_testControlToggleRequested;
        m_testControlToggleRequested = false;
        return requested;
    }
    const bool requested = ConsumeNamedAction("control_toggle");
    m_controlToggleRequested = false;
    return requested;
}

bool Window::ConsumeTorchToggleRequest() {
    if (m_testInputMode) {
        const bool requested = m_testTorchToggleRequested;
        m_testTorchToggleRequested = false;
        return requested;
    }
    const bool requested = ConsumeNamedAction("torch_toggle");
    m_torchToggleRequested = false;
    return requested;
}

void Window::RequestTestTorchToggle() {
    m_testTorchToggleRequested = true;
}

bool Window::ConsumeInteractRequest() {
    if (m_testInputMode) {
        const bool requested = m_testInteractRequested;
        m_testInteractRequested = false;
        return requested;
    }
    const bool requested = ConsumeNamedAction("interact");
    m_interactRequested = false;
    return requested;
}

bool Window::ConsumeViewToggleRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("view_toggle");
    m_viewToggleRequested = false;
    return requested;
}

bool Window::ConsumeThrowRequest() {
    const bool requested = ConsumeNamedAction("throw");
    m_throwRequested = false;
    return requested;
}

bool Window::ConsumeSasToggleRequest() {
    if (m_testInputMode) {
        const bool requested = m_testSasToggleRequested;
        m_testSasToggleRequested = false;
        return requested;
    }
    const bool requested = ConsumeNamedAction("sas_toggle");
    m_sasToggleRequested = false;
    return requested;
}

void Window::RequestTestSasToggle() {
    m_testSasToggleRequested = true;
}

void Window::RequestTestInteract() {
    m_testInteractRequested = true;
}

void Window::SetTestInputMode(bool enabled) {
    if(m_testInputMode!=enabled)m_input.Reset();
    m_testInputMode = enabled;
}

void Window::BeginTestFrame() { m_input.BeginFrame();m_consumed.clear(); }

void Window::SetTestPointer(int x,int y) {
    if(!m_testInputMode)return;
    m_uiClickX=x;m_uiClickY=y;
    m_input.SetPointer(float(x)/std::max(1,m_width),float(y)/std::max(1,m_height),m_input.Pointer().down);
}

void Window::ApplyTestPhysical(const std::string& control,float value) {
    if(!m_testInputMode||!std::isfinite(value))return;
    m_input.SetPhysical(control,value);
    if(control=="mouse:Left"){
        const auto p=m_input.Pointer();m_input.SetPointer(p.x,p.y,value>.5f);
    }
}

void Window::SetTestTouch(std::int64_t finger,float x,float y,float pressure,bool down) {
    if(m_testInputMode)m_input.SetTouch(0,finger,x,y,pressure,down);
}


void Window::SetTestActionState(Action action, bool active) {
    m_testActionState[static_cast<int>(action)] = active;
}

void Window::QueueTestMouseDelta(int deltaX, int deltaY) {
    m_testMouseDeltaX += deltaX;
    m_testMouseDeltaY += deltaY;
}

void Window::RequestTestJump() {
    m_testJumpRequested = true;
}

void Window::RequestTestReset() {
    m_testResetRequested = true;
}

void Window::RequestTestControlToggle() {
    m_testControlToggleRequested = true;
}

bool Window::ConsumeUIBackRequest() {
    if (m_testInputMode) return false;  // no JUDAS_TEST_SCRIPT scripting for UI in M13 — see Window.h
    const bool requested = ConsumeNamedAction("pause");
    m_uiBackRequested = false;
    return requested;
}

bool Window::ConsumeUINavigateUpRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("ui_up");
    m_uiUpRequested = false;
    return requested;
}

bool Window::ConsumeUINavigateDownRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("ui_down");
    m_uiDownRequested = false;
    return requested;
}

bool Window::ConsumeUIActivateRequest() {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("ui_activate");
    m_uiActivateRequested = false;
    return requested;
}

bool Window::ConsumeUIClickRequest(int& outX, int& outY) {
    if (m_testInputMode) return false;
    const bool requested = ConsumeNamedAction("ui_click");
    outX = m_uiClickX;
    outY = m_uiClickY;
    m_uiClickRequested = false;
    return requested;
}

void Window::GetMousePosition(int& outX, int& outY) const {
    if(m_testInputMode){outX=m_uiClickX;outY=m_uiClickY;return;}
    if (m_testInputMode) {
        outX = 0;
        outY = 0;
        return;
    }
    SDL_GetMouseState(&outX, &outY);
}

void Window::SetMouseCaptured(bool captured) {
#ifdef __EMSCRIPTEN__
    if(captured!=m_mouseCaptured)MAIN_THREAD_EM_ASM({Module.judasPointerCaptureRequested=!!$0;},captured);
#endif
    m_mouseCaptured = captured;
    SDL_SetWindowGrab(m_window,captured ? SDL_TRUE : SDL_FALSE);
    if(SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE)!=0 &&
       (SDL_GetWindowFlags(m_window)&SDL_WINDOW_SHOWN))
        std::fprintf(stderr,"Mouse capture failed: %s\n",SDL_GetError());
}

float Window::InputAxis(const std::string& name)const{
    if(m_testInputMode){
        auto pair=[&](Action positive,Action negative){return float(IsActionActive(positive))-float(IsActionActive(negative));};
        if(name=="move_y")return pair(Action::MoveForward,Action::MoveBackward);
        if(name=="move_x")return pair(Action::StrafeRight,Action::StrafeLeft);
        if(name=="move_z")return pair(Action::MoveUp,Action::MoveDown);
        if(name=="pitch")return pair(Action::PitchUp,Action::PitchDown);
        if(name=="yaw")return pair(Action::YawLeft,Action::YawRight);
        if(name=="roll")return pair(Action::RollRight,Action::RollLeft);
        return 0;
    }
    return m_input.Axis(name);
}

void Window::GetLookDelta(float& x,float& y)const{
    if(m_testInputMode){int ix=0,iy=0;GetMouseDelta(ix,iy);x=float(ix);y=float(iy);return;}
    x=m_mouseCaptured?m_input.Axis("look_x"):0;
    y=m_mouseCaptured?m_input.Axis("look_y"):0;
}

void Window::QueueTestPhysical(std::string control,float value){if(m_testInputMode)m_testPhysical.emplace_back(std::move(control),value);}
