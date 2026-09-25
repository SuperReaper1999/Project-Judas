#include "EditorDocument.h"

#include "SceneSerialization.h"

namespace {
constexpr std::size_t kMaxHistory = 200;
}

void EditorDocument::NewScene() {
    m_scene.Clear();
    m_scene.Settings().name = "Untitled scene";
    m_path.clear();
    m_dirty = false;
    m_selected = kInvalidSceneObjectId;
    m_undo.clear();
    m_redo.clear();
    m_editInProgress = false;
}

bool EditorDocument::Load(const std::string& path, std::string& outError) {
    Scene loaded;
    if (!LoadSceneFromFile(path, loaded, outError)) return false;
    m_scene = std::move(loaded);
    m_path = path;
    m_dirty = false;
    m_selected = kInvalidSceneObjectId;
    m_undo.clear();
    m_redo.clear();
    m_editInProgress = false;
    return true;
}

bool EditorDocument::Save(std::string& outError) {
    if (m_path.empty()) {
        outError = "the scene has no file path yet; use Save As";
        return false;
    }
    if (!SaveSceneToFile(m_scene, m_path, outError)) return false;
    m_dirty = false;
    return true;
}

bool EditorDocument::SaveAs(const std::string& path, std::string& outError) {
    if (!SaveSceneToFile(m_scene, path, outError)) return false;
    m_path = path;
    m_dirty = false;
    return true;
}

void EditorDocument::BeginEdit() {
    if (m_editInProgress) return;
    m_pendingSnapshot = m_scene;
    m_editInProgress = true;
}

void EditorDocument::CommitEdit() {
    if (!m_editInProgress) return;
    m_editInProgress = false;
    if (ScenesEqual(m_pendingSnapshot, m_scene)) return;
    m_undo.push_back(std::move(m_pendingSnapshot));
    if (m_undo.size() > kMaxHistory) m_undo.erase(m_undo.begin());
    m_redo.clear();
    m_dirty = true;
}

void EditorDocument::CancelEdit() {
    m_editInProgress = false;
}

void EditorDocument::Undo() {
    if (m_undo.empty()) return;
    m_redo.push_back(m_scene);
    m_scene = std::move(m_undo.back());
    m_undo.pop_back();
    m_dirty = true;
    if (!m_scene.Find(m_selected)) m_selected = kInvalidSceneObjectId;
}

void EditorDocument::Redo() {
    if (m_redo.empty()) return;
    m_undo.push_back(m_scene);
    m_scene = std::move(m_redo.back());
    m_redo.pop_back();
    m_dirty = true;
    if (!m_scene.Find(m_selected)) m_selected = kInvalidSceneObjectId;
}
