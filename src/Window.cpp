#include "Window.h"
#include "AppIcon.h"

#include <cstdio>

bool Window::Init(const char* title, int width, int height, bool visible) {
#ifdef _WIN32
    SDL_SetMainReady();
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    m_sdlInitialized = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
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

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    m_width = width;
    m_height = height;
    return true;
}

Window::~Window() {
    Shutdown();
}

void Window::Shutdown() {
    if(m_controller){SDL_GameControllerClose(m_controller);m_controller=nullptr;}
    m_controllerId=-1;m_controllerInputReady=false;
    m_input.Reset();
    if (m_glContext) {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
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
void Window::PollEvents(){
    m_input.BeginFrame();m_consumed.clear();
    // Select one controller before events, but do not overwrite the previous
    // pair with its final poll: each delivered axis event must remain observable.
    if(!m_testInputMode)RefreshController(false);
    for(const auto& [control,value]:m_testPhysical){m_input.SetPhysical(control,value);}
    m_testPhysical.clear();
    if(m_keyboardClaimed)m_input.ClearDevice("key:");
    if(m_mouseClaimed)m_input.ClearDevice("mouse:");
    SDL_Event event;
    while(SDL_PollEvent(&event)){
        // Harness input is injected into InputSystem; real devices must not mix in.
        if(m_testInputMode&&event.type!=SDL_QUIT&&event.type!=SDL_WINDOWEVENT)continue;
        if(m_eventHook)m_eventHook(event);
        if(event.type==SDL_QUIT)m_shouldClose=true;
        else if(event.type==SDL_WINDOWEVENT){
            if(event.window.event==SDL_WINDOWEVENT_CLOSE)m_shouldClose=true;
            if(event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){m_width=event.window.data1;m_height=event.window.data2;}
            if(event.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&!m_testInputMode){m_inputFocused=false;m_controllerInputReady=false;m_input.Reset();ClearPendingRequests();}
            if(event.window.event==SDL_WINDOWEVENT_FOCUS_GAINED){
                m_inputFocused=true;m_controllerInputReady=false;
                // SDL/compositor capture can disappear independently of the
                // gameplay request. Restore that request, preserving paused UI.
                SetMouseCaptured(m_mouseCaptured);
            }
        }else if(event.type==SDL_CONTROLLERDEVICEREMOVED&&event.cdevice.which==m_controllerId){
            RefreshController(false);
        }else if(event.type==SDL_CONTROLLERAXISMOTION&&event.caxis.which==m_controllerId&&m_controllerInputReady&&!m_keyboardClaimed&&m_inputFocused){
            if(const auto* axis=PadAxis(event.caxis.axis))m_input.SetPhysical(std::string("stick:")+axis,NormalizedPadAxis(event.caxis.axis,event.caxis.value));
        }else if((event.type==SDL_KEYDOWN||event.type==SDL_KEYUP)&&!m_keyboardClaimed){
            m_input.SetPhysical(std::string("key:")+SDL_GetScancodeName(event.key.keysym.scancode),event.type==SDL_KEYDOWN?1:0);
        }else if((event.type==SDL_MOUSEBUTTONDOWN||event.type==SDL_MOUSEBUTTONUP)&&!m_mouseClaimed){
            if(const char* name=MouseButton(event.button.button))m_input.SetPhysical(std::string("mouse:")+name,event.type==SDL_MOUSEBUTTONDOWN?1:0);
            if(event.type==SDL_MOUSEBUTTONDOWN){m_uiClickX=event.button.x;m_uiClickY=event.button.y;}
        }else if(event.type==SDL_MOUSEMOTION&&!m_mouseClaimed&&m_mouseCaptured){m_input.AddDelta("mouse:dx",float(event.motion.xrel));m_input.AddDelta("mouse:dy",float(event.motion.yrel));}
        else if(event.type==SDL_MOUSEWHEEL&&!m_mouseClaimed){const float sign=event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-1.f:1.f;m_input.AddDelta("mouse:wheelX",sign*event.wheel.x);m_input.AddDelta("mouse:wheelY",sign*event.wheel.y);}
    }
    // Restore held physical keys after UI ownership ends without stale edges.
    if(!m_testInputMode&&!m_keyboardClaimed&&m_inputFocused){const auto* keys=SDL_GetKeyboardState(nullptr);for(int i=0;i<SDL_NUM_SCANCODES;++i)if(keys[i])m_input.SetPhysical(std::string("key:")+SDL_GetScancodeName(static_cast<SDL_Scancode>(i)),1);}
    if(!m_testInputMode)RefreshController();
    // Requested capture and the backend's actual grab are separate state.
    // A compositor/backend release need not produce another FOCUS_GAINED
    // event. Reconcile while this window owns focus; never grab it back from
    // another application, and leave an uncaptured menu alone.
    if(m_mouseCaptured && (SDL_GetWindowFlags(m_window)&SDL_WINDOW_INPUT_FOCUS) &&
       (!SDL_GetRelativeMouseMode() || !SDL_GetWindowGrab(m_window)))
        SetMouseCaptured(true);
}
bool Window::ConsumeNamedAction(const std::string& name){return m_input.Action(name).pressed&&m_consumed.insert(name).second;}

void Window::SwapBuffers() {
    SDL_GL_SwapWindow(m_window);
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
