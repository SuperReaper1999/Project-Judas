#pragma once
#include "PhysicsEventDebugger.h"
#include <array>
class Window;class Renderer;
// Runtime-only diagnostic UI. Never calls world presentation/gameplay scripts
// or pumps resources during an in-flight stop. No second physics thread/world.
class RuntimePhysicsDebugView {
public:
    RuntimePhysicsDebugView(Window&,Renderer&);
    PhysicsEventDebugger::Command Inspect(const PhysicsEventDebugger::Snapshot&);
    bool PollPauseKey();
    void ServiceStatus(std::string text,bool busy){m_status=std::move(text);m_busy=busy;}
private:
    Window& m_window;Renderer& m_renderer;
    std::array<unsigned char,512> m_keys{};bool m_mouse=false,m_pauseKey=false,m_busy=false;
    unsigned m_focus=0;
    int m_inspectDx=0,m_inspectDy=0;
    int m_previousMouseX=0,m_previousMouseY=0;bool m_rightMouse=false;
    float m_yaw=.8f,m_pitch=.35f,m_distance=14;
    std::string m_status;
    PhysicsEventDebugger::Command Paint(const PhysicsEventDebugger::Snapshot&);
};
