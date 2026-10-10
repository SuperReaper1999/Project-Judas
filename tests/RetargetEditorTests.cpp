#include "editor/RetargetDraftState.h"
#include <cstdio>
#include <filesystem>
#include <limits>

namespace {
int checks = 0, failures = 0;
void Check(bool condition, const char* text) {
    ++checks; failures += !condition; std::printf("%s %s\n", condition ? "PASS" : "FAIL", text);
}
RetargetProfile Profile() {
    Skeleton skeleton;
    skeleton.names = {"Body", "Arm"}; skeleton.parents = {-1, 0}; skeleton.order = {0, 1}; skeleton.rest.local.resize(2);
    skeleton.rest.local[1].translation = {1, 0, 0};
    RetargetProfile profile;
    profile.sourceIdentity = "Sources/source.glb"; profile.targetIdentity = "0123456789abcdef0123456789abcdef";
    profile.sourceSignature = profile.targetSignature = SkeletonRetargetSignature(skeleton);
    profile.mapping = {{"Body", "Body"}, {"Body/Arm", "Body/Arm"}};
    return profile;
}
}

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    const fs::path directory = argc > 1 ? argv[1] : ".cache/m74/editor-state-tests";
    fs::create_directories(directory);
    RetargetDraftState state; state.current.profile = Profile(); state.current.take = "Wave";
    Check(state.ProfileDirty() && state.OperationDirty(), "new draft requires separate profile and recipe saves");
    auto before = state.current;
    Check(!state.Record(before) && state.undo.empty(), "unchanged frames do not manufacture undo entries");
    state.MarkProfileSaved(); state.savedOperation = RetargetDraftIdentity(state.current);
    const auto preview = RetargetDraftIdentity(state.current);
    Check(!state.ProfileDirty() && !state.OperationDirty() && state.PreviewCurrent(preview), "saved draft and comparison initially current");
    before = state.current; state.current.profile.translationScale = 1.25f;
    Check(state.Record(before) && state.ProfileDirty() && state.OperationDirty() && !state.PreviewCurrent(preview),
          "calibration edit invalidates profile, operation and preview independently of scene state");
    Check(state.Undo() && state.current.profile.translationScale == 1 && !state.ProfileDirty() && state.PreviewCurrent(preview),
          "normal draft undo restores saved calibration and preview identity");
    Check(state.Redo() && state.current.profile.translationScale == 1.25f && !state.PreviewCurrent(preview),
          "normal draft redo restores edited calibration");
    before = state.current; state.current.outputName = "Borrowed wave"; state.Record(before);
    Check(state.Cancel() && state.current.profile.translationScale == 1 && state.current.outputName == "Retargeted" && !state.ProfileDirty(),
          "cancel restores saved draft without rolling back filesystem writes");
    Check(state.Undo() && state.current.outputName == "Borrowed wave" && state.current.profile.translationScale == 1.25f,
          "cancel is itself reversible in local authoring history");
    state.Cancel();
    before = state.current; state.current.take = "Turn"; state.current.rootPolicy = "extract"; state.current.rootJoint = "Body"; state.Record(before);
    Check(!state.ProfileDirty() && state.OperationDirty() && !state.PreviewCurrent(preview),
          "per-clip/root recipe edits do not corrupt reusable profile dirty state");
    state.MarkProfileSaved();
    const auto path = directory / "Reusable profile with spaces.judasretarget";
    std::string error; RetargetProfile loaded;
    Check(SaveRetargetProfile(path.string(), state.current.profile, error) && LoadRetargetProfile(path.string(), loaded, error),
          "panel's shared atomic profile save and reopen service succeeds");
    RetargetDraftState reopened; reopened.current.profile = loaded; reopened.MarkProfileSaved();
    Check(!reopened.ProfileDirty() && RetargetProfileDraftIdentity(loaded) == state.savedProfile,
          "reopened profile retains exact correspondence/calibration and clean profile state");
    Check(reopened.OperationDirty(), "loading a profile does not pretend a chosen recipe operation was saved");
    auto broken = loaded; broken.translationScale = -1;
    Check(!SaveRetargetProfile(path.string(), broken, error) && LoadRetargetProfile(path.string(), loaded, error) &&
          RetargetProfileDraftIdentity(loaded) == state.savedProfile, "invalid profile save retains last-good filesystem content");
    const auto currentPreview = RetargetDraftIdentity(state.current);
    state.Reset();
    Check(state.current.profile.mapping.empty() && state.undo.empty() && state.redo.empty() &&
          state.savedProfile.empty() && state.savedOperation.empty() && !state.PreviewCurrent(currentPreview),
          "project replacement retires draft, history and stale comparison identity");
    Check(!RetargetDraftIdentity(state.current).empty() && state.ProfileDirty(), "empty initial/reset draft identity never invokes strict publication serialization");
    before = state.current; state.current.profile = Profile(); state.current.profile.mapping.push_back({"", ""});
    state.current.profile.translationScale = -1; state.current.profile.modelAlignment = {0, 0, 0, 0};
    Check(state.Record(before) && state.ProfileDirty() && !state.PreviewCurrent(currentPreview),
          "incomplete row, negative scale and zero quaternion remain safe editable dirty states");
    before = state.current; state.current.profile.translationScale = std::numeric_limits<float>::quiet_NaN();
    state.current.sampleRate = std::numeric_limits<double>::infinity();
    Check(state.Record(before) && !RetargetDraftIdentity(state.current).empty(),
          "nonfinite temporary draft values do not throw while tracking changes");
    Check(state.Undo() && state.current.profile.translationScale == -1 && state.current.sampleRate == 60,
          "invalid temporary draft edits are still normally undoable");
    state.Reset();
    for (unsigned index = 0; index < 100; ++index) {
        before = state.current; state.current.outputName = "Motion" + std::to_string(index); state.Record(before);
    }
    Check(state.undo.size() == 32, "draft undo history remains bounded to 32 changes");
    state.Undo(); before = state.current; state.current.outputName = "New branch"; state.Record(before);
    Check(state.redo.empty(), "new draft edit after undo discards retired redo branch");
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
