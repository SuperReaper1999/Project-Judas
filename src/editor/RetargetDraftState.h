#pragma once

#include "AnimationRetarget.h"
#include <iomanip>
#include <locale>
#include <sstream>

// Small authoring-only transaction state shared by the panel and focused
// checks. Undo changes this draft; it never reverses filesystem publication.
struct RetargetDraft {
    RetargetProfile profile;
    std::string take, outputName = "Retargeted", rootPolicy = "preserve", rootJoint;
    float begin = 0, end = -1;
    double sampleRate = 60;
    bool loop = false;
    glm::bvec3 rootTranslation{true, false, true};
    glm::vec3 rootRotationAxis{0};
};

// Dirty/history identity must accept incomplete and invalid editor values.
// Publication's strict serializer is deliberately NOT called here: a cleared
// joint row, zero quaternion or temporary negative scale is an editable draft,
// not permission to crash the UI. Save/Validate still use the strict service.
inline std::string RetargetProfileDraftIdentity(const RetargetProfile& profile) {
    std::ostringstream out;
    out.imbue(std::locale::classic()); out << std::hexfloat;
    out << "draft-profile/" << profile.version << '/' << std::quoted(profile.sourceIdentity)
        << std::quoted(profile.targetIdentity) << std::quoted(profile.sourceSignature) << std::quoted(profile.targetSignature);
    const auto quaternion = [&](glm::quat q) { out << '/' << q.x << '/' << q.y << '/' << q.z << '/' << q.w; };
    quaternion(profile.modelAlignment); out << '/' << profile.translationScale << "/mapping/" << profile.mapping.size();
    for (const auto& mapping : profile.mapping) out << '/' << std::quoted(mapping.source) << std::quoted(mapping.target);
    out << "/translation/" << profile.translationJoints.size();
    for (const auto& joint : profile.translationJoints) out << '/' << std::quoted(joint);
    const auto references = [&](const auto& corrections) {
        out << '/' << corrections.size();
        for (const auto& correction : corrections) { out << '/' << std::quoted(correction.joint); quaternion(correction.rotation); }
    };
    out << "/source-reference"; references(profile.sourceReference);
    out << "/target-reference"; references(profile.targetReference);
    return out.str();
}

inline std::string RetargetDraftIdentity(const RetargetDraft& draft) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << RetargetProfileDraftIdentity(draft.profile) << std::setprecision(17)
        << std::quoted(draft.take) << std::quoted(draft.outputName) << draft.begin << '/' << draft.end << '/'
        << draft.sampleRate << '/' << draft.loop << '/' << std::quoted(draft.rootPolicy) << std::quoted(draft.rootJoint)
        << draft.rootTranslation.x << draft.rootTranslation.y << draft.rootTranslation.z << '/'
        << draft.rootRotationAxis.x << '/' << draft.rootRotationAxis.y << '/' << draft.rootRotationAxis.z;
    return out.str();
}

struct RetargetDraftState {
    RetargetDraft current, saved;
    std::vector<RetargetDraft> undo, redo;
    std::string savedProfile, savedOperation;

    bool Record(const RetargetDraft& previous) {
        if (RetargetDraftIdentity(previous) == RetargetDraftIdentity(current)) return false;
        undo.push_back(previous); redo.clear();
        if (undo.size() > 32) undo.erase(undo.begin());
        return true;
    }
    bool Undo() {
        if (undo.empty()) return false;
        redo.push_back(current); current = undo.back(); undo.pop_back(); return true;
    }
    bool Redo() {
        if (redo.empty()) return false;
        undo.push_back(current); current = redo.back(); redo.pop_back(); return true;
    }
    void MarkProfileSaved() { savedProfile = RetargetProfileDraftIdentity(current.profile); saved = current; }
    bool Cancel() { const auto previous = current; current = saved; return Record(previous); }
    bool ProfileDirty() const { return savedProfile != RetargetProfileDraftIdentity(current.profile); }
    bool OperationDirty() const { return savedOperation != RetargetDraftIdentity(current); }
    bool PreviewCurrent(const std::string& identity) const { return identity == RetargetDraftIdentity(current); }
    void Reset() { current = {}; saved = {}; undo.clear(); redo.clear(); savedProfile.clear(); savedOperation.clear(); }
};
