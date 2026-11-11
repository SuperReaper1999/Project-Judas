#pragma once
#include "StartupMovie.h"
#include "Renderer.h"
#include <chrono>
#include <functional>
#include <string>
class EngineHost;
// All input and GL operations stay on the context owner. No gameplay or clock
// advances while this display owns the window. Close remains responsive.
class StartupScreen {
public:
    explicit StartupScreen(EngineHost& host);
    ~StartupScreen();
    bool Present(const std::string& stage,bool force=false,const std::function<void()>& beforeSwap={});
    void Finish();
private:
    EngineHost& m_host;
    StartupMovie m_movie;
    TextureHandle m_texture;
    std::chrono::steady_clock::time_point m_begin,m_last;
    unsigned m_frame=~0u;
    bool m_finished=false,m_movieAvailable=false;
};
