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
// (measured with the M67 several-hundred-object workshop). A copy per committed edit
// preserves complete revisions, and covers creation, deletion, reordering,
// component add/remove and settings changes with one mechanism — no
// per-property command classes to keep in sync with the data model.
class EditorDocument {
public:
    Scene& GetScene() { return m_scene; }
    const Scene& GetScene() const { return m_scene; }

    const std::string& Path() const { return m_path; }
    bool IsDirty() const { return m_dirty; }
    uint64_t Generation()const{return m_generation;}
    const std::string& ValidationError()const{return m_validationError;}
    SceneObjectId Selected() const { return m_selected; }
    void Select(SceneObjectId id,bool toggle=false);
    const std::vector<SceneObjectId>& Selection() const {return m_selection;}
    bool IsSelected(SceneObjectId id)const;
    void PruneSelection();
    bool BatchProperties(const std::map<std::string,std::string>&,std::string& error);
    bool BatchTransform(glm::vec3 translation,glm::quat rotation,glm::vec3 scale,std::string& error,bool individual=true,bool world=false);
    bool ReparentSelection(SceneObjectId parent,std::string& error,bool preserveWorld=true);
    std::vector<SceneObjectId> SelectionRoots() const;
    std::vector<SceneObjectId> Search(const std::string& query) const;
    void SelectRange(SceneObjectId id,const std::vector<SceneObjectId>& visible,bool additive=false);
    bool DeleteSelection(std::string& error);
    bool ApplySource(const std::string& text,std::string& error);
    bool ExternalChanged() const;
    const std::string& LoadedSource() const{return m_loadedSource;}
    bool AlignSelection(unsigned axis,bool distribute,std::string& error);
    bool SnapSelection(float translation,float rotationDegrees,float scale,std::string& error);
    bool RenameSelection(const std::string& prefix,std::string& error);
    bool SurfaceSnap(const class RuntimeWorld&,glm::vec3 direction,float distance,bool orientToNormal,std::string& error);
    bool SetWorldTransforms(const std::map<SceneObjectId,SceneTransform>&,std::string& error);
    std::vector<std::string> SelectionReferences()const;
    bool SaveNamed(std::string& error);
    const Scene& EditBaseline()const{return m_pendingSnapshot;}
    bool GroupSelection(std::string& error);
    bool DuplicateSelection(std::string& error);
    void CopyComponent(const std::string& prefix);
    bool PasteComponent(std::string& error);
    SceneObject* SelectedObject() { return m_scene.Find(m_selected); }

    void NewScene();
    bool Load(const std::string& path, std::string& outError);
    bool Save(std::string& outError);            // to Path()
    bool SaveAs(const std::string& path, std::string& outError);

    // An edit is a snapshot taken before the change and confirmed after
    // it. Cancellation restores the pre-action document.
    void BeginEdit();
    void CommitEdit(bool capturePrefab = true);
    void CancelEdit();
    bool EditInProgress() const { return m_editInProgress; }

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }
    void Undo();
    void Redo();

private:
    void CommitEditImpl(bool capturePrefab,bool alreadyValidated);
    bool CommitCandidate(Scene&&,std::string& error,bool capturePrefab=true,bool alreadyValidated=false);
    mutable uint64_t m_searchGeneration=~uint64_t(0);mutable std::string m_searchQuery;mutable std::vector<SceneObjectId> m_searchResults;
    Scene m_scene;
    std::string m_path,m_loadedSource,m_validationError;
    uint64_t m_generation=0;
    bool m_dirty = false;
    std::vector<SceneObjectId> m_selection;
    std::map<std::string,std::string> m_componentClipboard;
    std::string m_clipboardPrefix;
    SceneObjectId m_selected = kInvalidSceneObjectId;
    std::vector<Scene> m_undo;
    std::vector<Scene> m_redo;
    Scene m_pendingSnapshot;
    bool m_editInProgress = false;
};
