#pragma once

#include <vector>

class UIMenuScreen;

// Milestone 13: the entire "screen open/close/back stack" concept this
// milestone's required navigation flow (gameplay -> pause -> nested screen
// -> back -> resume -> gameplay) needs — see docs/ARCHITECTURE.md,
// "Milestone 13." Deliberately NOT an owning container: every UIMenuScreen
// this engine has (currently two — see src/PauseMenu.h) is a long-lived
// member of whoever composes the menu, constructed once; UIStack only ever
// holds non-owning pointers into screens that already exist, the same
// "handle, not ownership" shape PhysicsWorld::BodyHandle/MeshHandle already
// establish elsewhere in this engine.
class UIStack {
public:
    void Push(UIMenuScreen& screen) { m_screens.push_back(&screen); }

    // Pops the top screen, if any. A no-op on an empty stack.
    void Pop() {
        if (!m_screens.empty()) m_screens.pop_back();
    }

    // Empties the stack entirely — used by "Resume" (closes the whole menu
    // in one action, regardless of how deep navigation went) and by
    // "Quit"'s own cleanup, rather than requiring N pops to unwind however
    // deep the player happened to navigate.
    void Clear() { m_screens.clear(); }

    bool IsOpen() const { return !m_screens.empty(); }

    UIMenuScreen* Top() const { return m_screens.empty() ? nullptr : m_screens.back(); }

private:
    std::vector<UIMenuScreen*> m_screens;
};
