#pragma once

#include <string>
#include <vector>

#include "Scene.h"

// Milestone 28: the editor's view of one authored Scene — the file it
// came from, whether it has unsaved changes, the current selection, and a
// snapshot-based undo/redo history. Every edit the panels make goes
// through BeginEdit/CommitEdit so history and the dirty flag stay exact.
//
// Undo stores whole-Scene snapshots. Scenes are small authored documents
// (tens of objects, a few hundred numbers), so a copy per committed edit
// is cheap, trivially correct, and covers creation, deletion, reordering,
// component add/remove and settings changes with one mechanism — no
// per-property command classes to keep in sync with the data model.
class EditorDocument {
public:
    Scene& GetScene() { return m_scene; }
    const Scene& GetScene() const { return m_scene; }

    const std::string& Path() const { return m_path; }
    bool IsDirty() const { return m_dirty; }
    SceneObjectId Selected() const { return m_selected; }
    void Select(SceneObjectId id) { m_selected = id; }
    SceneObject* SelectedObject() { return m_scene.Find(m_selected); }

    void NewScene();
    bool Load(const std::string& path, std::string& outError);
    bool Save(std::string& outError);            // to Path()
    bool SaveAs(const std::string& path, std::string& outError);

    // An edit is a snapshot taken before the change and confirmed after
    // it. CancelEdit discards the snapshot (a widget activated but its
    // value never changed).
    void BeginEdit();
    void CommitEdit();
    void CancelEdit();
    bool EditInProgress() const { return m_editInProgress; }

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    void Undo();
    void Redo();

private:
    Scene m_scene;
    std::string m_path;
    bool m_dirty = false;
    SceneObjectId m_selected = kInvalidSceneObjectId;
    std::vector<Scene> m_undo;
    std::vector<Scene> m_redo;
    Scene m_pendingSnapshot;
    bool m_editInProgress = false;
};
