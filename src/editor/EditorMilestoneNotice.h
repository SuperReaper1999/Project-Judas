#pragma once

#include <atomic>
#include <future>
#include <string>
#include "BuildUpdate.h"

// Editor-only notification. Never updates the installation or local repository.
class EditorMilestoneNotice {
public:
    ~EditorMilestoneNotice();
    void Check(bool manual = false);
    void Draw(std::string& status);
private:
    using Result = BuildUpdateResult;
    static Result Query(const std::atomic<bool>& cancelled);
    std::atomic<bool> m_cancelled{false};
    std::future<Result> m_pending;
    Result m_result;
    bool m_manual = false;
    bool m_visible = false;
};
