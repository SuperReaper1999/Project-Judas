#pragma once
#include "Project.h"
#include <vector>

// Editor-owned snapshots. Runtime Project instances carry no undo history.
class ProjectSettingsHistory {
public:
    void Observe(Project& project) {
        if(m_path!=project.ProjectFile()){
            m_path=project.ProjectFile();m_last=project.Settings();m_undo.clear();m_redo.clear();return;
        }
        if(Project::SerializeToString(m_last)==Project::SerializeToString(project.Settings()))return;
        m_undo.push_back(m_last);
        if(m_undo.size()>128)m_undo.erase(m_undo.begin());
        m_redo.clear();m_last=project.Settings();
    }
    bool CanUndo()const{return !m_undo.empty();}
    bool CanRedo()const{return !m_redo.empty();}
    void Undo(Project& project){
        Observe(project);if(m_undo.empty())return;
        m_redo.push_back(project.Settings());project.Settings()=std::move(m_undo.back());m_undo.pop_back();m_last=project.Settings();
    }
    void Redo(Project& project){
        if(m_redo.empty())return;
        m_undo.push_back(project.Settings());project.Settings()=std::move(m_redo.back());m_redo.pop_back();m_last=project.Settings();
    }
private:
    std::string m_path;
    ProjectSettings m_last;
    std::vector<ProjectSettings> m_undo,m_redo;
};
