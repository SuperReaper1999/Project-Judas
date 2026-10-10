#include "RetargetPanel.h"
#include "RetargetDraftState.h"

#include "AnimationRetarget.h"
#include "EditorPanels.h"
#include "ModelCook.h"
#include "ModelImport.h"
#include "PlatformServices.h"
#include "PoseComposition.h"
#include "Project.h"
#include "Renderer.h"
#include "SceneFingerprint.h"
#include "imgui.h"
#include "../../third_party/nlohmann/json.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

namespace fs = std::filesystem;
using RetargetJson = nlohmann::json;

namespace {
struct RetargetInputs {
    MeshData source, target;
    std::string sourceIdentity, targetIdentity, sourcePath, targetRecipe, root;
    std::map<std::string, std::string> hashes;
    glm::vec3 low{0}, high{1};
    double sourceUnitMeters = 0, sourceSampleRate = 60;
    glm::quat sourceBasis{1, 0, 0, 0};
};

struct RetargetInputJob {
    std::atomic<bool> done{false}, cancel{false};
    JobHandle job;
    std::string root, error;
    std::shared_ptr<const RetargetInputs> inputs;
};

struct RetargetComparisonJob {
    std::atomic<bool> done{false}, cancel{false};
    JobHandle job;
    std::string root, draftIdentity, error;
    std::shared_ptr<const MeshData> target;
    RetargetBakeReport report;
    RetargetDraft draft;
};

std::string DraftIdentity(const RetargetDraft& draft) {
    return RetargetDraftIdentity(draft);
}

bool ProjectPath(const std::string& root, const std::string& input, fs::path& output, std::string& error,
                 const char* extension = nullptr, bool importsOnly = false) {
    try {
        if (input.empty()) throw std::runtime_error("Choose a project-relative path first");
        const auto project = fs::weakly_canonical(root);
        output = fs::weakly_canonical(project / fs::u8path(input));
        const auto relative = output.lexically_relative(project);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
            throw std::runtime_error("Authoring path must stay inside the open project");
        if (extension && output.extension() != extension) throw std::runtime_error(std::string("Expected ") + extension);
        if (importsOnly && (relative.begin()->string() != "Imports" || std::distance(relative.begin(), relative.end()) != 2))
            throw std::runtime_error("Profiles and recipes belong directly in this project's Imports folder");
        error.clear();
        return true;
    } catch (const std::exception& exception) { error = exception.what(); return false; }
}

RetargetJson ReadRecipe(const fs::path& path) {
    if (!fs::is_regular_file(path) || fs::file_size(path) > kRetargetProfileBytes)
        throw std::runtime_error("Recipe is missing or exceeds 4 MiB");
    std::ifstream file(path);
    auto recipe = RetargetJson::parse(file, [](int depth, RetargetJson::parse_event_t, const RetargetJson&) {
        if (depth >= 48) throw std::runtime_error("Recipe nesting exceeds authoring bound");
        return true;
    });
    if (recipe.at("format") != "JudasImport" || recipe.at("version") != 1)
        throw std::runtime_error("Expected current JudasImport recipe");
    return recipe;
}

bool TextInput(const char* label, std::string& value) {
    std::vector<char> text(std::max(size_t(4096), value.size() + 256), 0);
    std::copy(value.begin(), value.end(), text.begin());
    if (!ImGui::InputText(label, text.data(), text.size())) return false;
    value = text.data();
    return true;
}

// Same full escaped hierarchy identities as the ordinary skeleton pickers.
bool SkeletonPicker(const char* label, const Skeleton& skeleton, std::string& key, bool none = true) {
    bool changed = false;
    if (ImGui::BeginCombo(label, key.empty() ? "Choose full joint key" : key.c_str())) {
        if (none && ImGui::Selectable("None", key.empty())) { key.clear(); changed = true; }
        for (size_t index = 0; index < skeleton.names.size(); ++index) {
            const auto full = SkeletonJointKey(skeleton, int(index));
            if (ImGui::Selectable(full.c_str(), full == key)) { key = full; changed = true; }
        }
        ImGui::EndCombo();
    }
    if (!key.empty() && FindSkeletonJoint(skeleton, key) < 0)
        ImGui::TextColored({1, .42f, .25f, 1}, "Missing or ambiguous key: %s", key.c_str());
    return changed;
}

void QuaternionInput(const char* label, glm::quat& rotation) {
    glm::vec4 values(rotation.x, rotation.y, rotation.z, rotation.w);
    if (ImGui::InputFloat4(label, &values.x)) rotation = {values.w, values.x, values.y, values.z};
    ImGui::SameLine();
    ImGui::PushID(label);
    if (ImGui::SmallButton("Normalize") && glm::dot(rotation, rotation) > 1e-12f) rotation = glm::normalize(rotation);
    glm::vec3 degrees(0);
    if (glm::dot(rotation, rotation) > 1e-12f && std::isfinite(glm::length(rotation)))
        degrees = glm::degrees(glm::eulerAngles(glm::normalize(rotation)));
    if (ImGui::InputFloat3("Euler X/Y/Z degrees", &degrees.x)) rotation = glm::quat(glm::radians(degrees));
    ImGui::PopID();
}

void Bounds(const MeshData& mesh, glm::vec3& low, glm::vec3& high) {
    if (!mesh.skeletal) return;
    auto skin = ResolveSkinMatrices(mesh.skeletal->skeleton, mesh.skeletal->skeleton.rest);
    for (size_t index = 0; index < mesh.vertices.size(); ++index) {
        glm::vec3 point = mesh.vertices[index].position;
        if (index < mesh.skinVertices.size() && !skin.empty()) {
            glm::mat4 matrix(0);
            const auto& weights = mesh.skinVertices[index];
            for (int slot = 0; slot < 4; ++slot) {
                if (weights.weights[slot] && weights.joints[slot] < skin.size()) matrix += skin[weights.joints[slot]] * weights.weights[slot];
                if (weights.weights1[slot] && weights.joints1[slot] < skin.size()) matrix += skin[weights.joints1[slot]] * weights.weights1[slot];
            }
            point = glm::vec3(matrix * glm::vec4(point, 1));
        }
        low = glm::min(low, point); high = glm::max(high, point);
    }
    // Animation-only source files still have a useful skeleton preview.
    for (const auto& matrix : ResolveJointMatrices(mesh.skeletal->skeleton, mesh.skeletal->skeleton.rest)) {
        low = glm::min(low, glm::vec3(matrix[3])); high = glm::max(high, glm::vec3(matrix[3]));
    }
}
} // namespace

struct RetargetPanelState {
    std::string root, source, targetRecipe, profilePath = "Imports/retarget.judasretarget", message;
    ModelImportSettings sourceSettings;
    RetargetDraftState history;
    RetargetDraft comparisonDraft;
    std::string comparisonIdentity;
    std::string savedProfilePath, savedRecipeInputs;
    std::shared_ptr<RetargetInputJob> inputJob;
    std::shared_ptr<RetargetComparisonJob> comparisonJob;
    std::shared_ptr<const RetargetInputs> inputs;
    std::shared_ptr<const MeshData> comparison;
    std::shared_ptr<ModelCookTask> bake;
    std::string bakeProject;
    bool bakePublished = false, wasOpen = false, playing = false, showSkeleton = true, showAxes = true;
    bool referencePose = false, replaceOperation = false;
    float time = 0, yaw = .35f, previewSeparation = 2.5f;
    std::uintptr_t image = 0;
    MeshHandle sourceGpu, targetGpu;
    RenderTargetHandle target;
    std::weak_ptr<const RetargetInputs> uploadedInputs;
    bool gpuReset = false;
    int autotestPhase = 0; // 0 ordinary use; 1 input; 2 comparison; 3 rendered; -1 failed
};

namespace {
std::string OpenProjectRoot(const EditorPanelState& panels) {
    return panels.project && panels.project->IsLoaded() ? fs::weakly_canonical(panels.project->RootDir()).string() : std::string();
}

void CancelTasks(RetargetPanelState& state, JobSystem* jobs) {
    if (state.inputJob) { state.inputJob->cancel = true; if (jobs) jobs->Cancel(state.inputJob->job); }
    if (state.comparisonJob) { state.comparisonJob->cancel = true; if (jobs) jobs->Cancel(state.comparisonJob->job); }
    if (state.bake && !state.bakePublished) { state.bake->cancel = true; if (jobs) jobs->Cancel(state.bake->job); }
}

void SyncProject(EditorPanelState& panels) {
    if (!panels.retarget) panels.retarget = std::make_shared<RetargetPanelState>();
    auto& state = *panels.retarget;
    const auto root = OpenProjectRoot(panels);
    if (root == state.root) return;
    CancelTasks(state, panels.importJobs);
    state.root = root; state.source.clear(); state.targetRecipe.clear(); state.profilePath = "Imports/retarget.judasretarget";
    state.inputs.reset(); state.comparison.reset(); state.history.Reset();
    state.savedProfilePath.clear(); state.savedRecipeInputs.clear();
    state.time = 0; state.playing = false; state.gpuReset = true; state.image = 0;
    state.autotestPhase = 0;
    state.message = "Project changed: prior preview and uncommitted draft retired; last-good cooked assets preserved";
}

void RecordDraft(RetargetPanelState& state, const RetargetDraft& before) {
    if (state.history.Record(before)) state.playing = false;
}

bool InputsCurrent(const RetargetPanelState& state) {
    return state.inputs && state.inputs->sourcePath == state.source && state.inputs->targetRecipe == state.targetRecipe &&
        state.inputs->sourceUnitMeters == state.sourceSettings.sourceUnitMeters &&
        state.inputs->sourceSampleRate == state.history.current.sampleRate && state.inputs->sourceBasis == state.sourceSettings.basisRotation;
}

bool ProfileInputsMatch(const RetargetPanelState& state, std::string& error) {
    if (!InputsCurrent(state)) { error = "Load the selected source and target before validating"; return false; }
    const auto& inputs = *state.inputs;
    if (state.history.current.profile.sourceIdentity != inputs.sourceIdentity || state.history.current.profile.targetIdentity != inputs.targetIdentity) {
        error = "Profile identity differs from selected source/target; choose its matching inputs or create a new profile"; return false;
    }
    error.clear(); return true;
}

bool ValidateDraft(const RetargetPanelState& state, std::string& error) {
    if (!ProfileInputsMatch(state, error)) return false;
    const auto& inputs = *state.inputs;
    return ValidateRetargetProfile(inputs.source.skeletal->skeleton, inputs.target.skeletal->skeleton, state.history.current.profile, error);
}

std::string OperationInputIdentity(const RetargetPanelState& state) {
    std::ostringstream identity;
    identity.imbue(std::locale::classic()); identity << std::setprecision(17)
        << std::quoted(state.source) << std::quoted(state.targetRecipe) << std::quoted(state.profilePath)
        << state.sourceSettings.sourceUnitMeters << '/' << state.sourceSettings.basisRotation.x << '/'
        << state.sourceSettings.basisRotation.y << '/' << state.sourceSettings.basisRotation.z << '/'
        << state.sourceSettings.basisRotation.w;
    return identity.str();
}

void StartInputLoad(EditorPanelState& panels) {
    auto& state = *panels.retarget;
    if (!panels.importJobs) { state.message = "Import worker pool unavailable"; return; }
    fs::path source, recipe;
    if (!ProjectPath(state.root, state.source, source, state.message) ||
        !ProjectPath(state.root, state.targetRecipe, recipe, state.message, ".judasimport", true)) return;
    const auto normalizedSource = source.lexically_relative(fs::weakly_canonical(state.root)).generic_string();
    const auto normalizedRecipe = recipe.lexically_relative(fs::weakly_canonical(state.root)).generic_string();
    state.source = normalizedSource; state.targetRecipe = normalizedRecipe;
    auto task = std::make_shared<RetargetInputJob>(); task->root = state.root; state.inputJob = task;
    auto settings = state.sourceSettings;
    settings.sampleRate = state.history.current.sampleRate;
    task->job = panels.importJobs->Submit([task, source, recipe, normalizedSource, normalizedRecipe, settings](JobContext& context) {
        try {
            auto loaded = std::make_shared<RetargetInputs>(); loaded->root = task->root;
            loaded->sourceIdentity = normalizedSource; loaded->sourcePath = normalizedSource; loaded->targetRecipe = normalizedRecipe;
            loaded->sourceUnitMeters = settings.sourceUnitMeters; loaded->sourceSampleRate = settings.sampleRate;
            loaded->sourceBasis = settings.basisRotation;
            auto cancelled = [&] { return task->cancel.load() || context.CancelRequested(); };
            ModelImportSettings targetSettings, sourceSettings = settings;
            targetSettings.cancelled = cancelled;
            sourceSettings.cancelled = cancelled;
            std::string root, error; ModelImportReport report;
            for (const auto& path : {source, recipe}) {
                std::string hash; if (!SceneFingerprintSha256File(path.string(), hash, error)) throw std::runtime_error(error);
                loaded->hashes[path.string()] = hash;
            }
            if (cancelled()) throw std::runtime_error("Preview input load cancelled");
            if (!LoadModelRecipeTarget(recipe.string(), loaded->target, targetSettings, root, error)) throw std::runtime_error(error);
            if (fs::weakly_canonical(root).string() != task->root) throw std::runtime_error("Recipe belongs to a different project");
            if (!ImportMotionSource(source.string(), sourceSettings, loaded->source, report, error)) throw std::runtime_error(error);
            for (const auto& dependency : report.dependencies) {
                fs::path approved;
                if (!ProjectPath(task->root, dependency, approved, error)) throw std::runtime_error("Source dependency: " + error);
                std::string hash; if (!SceneFingerprintSha256File(approved.string(), hash, error, cancelled)) throw std::runtime_error(error);
                loaded->hashes[approved.string()] = hash;
            }
            if (!loaded->source.skeletal || !loaded->target.skeletal) throw std::runtime_error("Both inputs require skeletons");
            loaded->targetIdentity = ReadRecipe(recipe).at("assetId").get<std::string>();
            for (const auto& [path, expected] : loaded->hashes) {
                std::string hash; if (!SceneFingerprintSha256File(path, hash, error) || hash != expected)
                    throw std::runtime_error("Input changed during preview load; retry");
            }
            glm::vec3 low(std::numeric_limits<float>::infinity()), high(-std::numeric_limits<float>::infinity());
            Bounds(loaded->source, low, high); Bounds(loaded->target, low, high);
            loaded->low = low; loaded->high = high;
            if (cancelled()) throw std::runtime_error("Preview input load cancelled");
            task->inputs = loaded;
        } catch (const std::exception& exception) { task->error = exception.what(); }
        if (context.CancelRequested() || task->cancel) context.ReportCancelled();
        else if (!task->error.empty()) context.SetError(task->error);
        task->done.store(true, std::memory_order_release);
    }, JobPriority::Normal, "Retarget preview source / target decode");
    state.message = "Loading immutable CPU source and target inputs on import worker";
}

void StartComparison(EditorPanelState& panels) {
    auto& state = *panels.retarget;
    std::string error;
    if (!ValidateDraft(state, error)) { state.message = error; return; }
    if (!panels.importJobs) return;
    auto task = std::make_shared<RetargetComparisonJob>(); task->root = state.root;
    task->draftIdentity = DraftIdentity(state.history.current); task->draft = state.history.current; state.comparisonJob = task;
    const auto inputs = state.inputs; const auto draft = state.history.current;
    task->job = panels.importJobs->Submit([task, inputs, draft](JobContext& context) {
        try {
            const auto& source = *inputs->source.skeletal;
            auto clip = std::find_if(source.clips.begin(), source.clips.end(), [&](const auto& value) { return value.name == draft.take; });
            if (clip == source.clips.end()) throw std::runtime_error("Choose an existing source take");
            RetargetBakeSettings settings; settings.name = draft.outputName; settings.begin = draft.begin;
            settings.end = draft.end; settings.sampleRate = draft.sampleRate;
            settings.cancelled = [&] { return task->cancel.load() || context.CancelRequested(); };
            auto target = std::make_shared<MeshData>(inputs->target);
            auto skeletal = std::make_shared<SkeletalAsset>(*target->skeletal);
            AnimationClip transferred; std::string error;
            if (!BakeRetargetClip(source.skeleton, *clip, skeletal->skeleton, draft.profile, settings, transferred, error, &task->report))
                throw std::runtime_error(error);
            transferred.loop = draft.loop;
            ModelRootMotionSettings root; root.policy = draft.rootPolicy; root.node = draft.rootJoint;
            root.translation = {{draft.rootTranslation.x, draft.rootTranslation.y, draft.rootTranslation.z}};
            root.rotationAxis = draft.rootRotationAxis;
            if (!ApplyModelRootMotionPolicy(*skeletal, transferred, root, draft.sampleRate, error)) throw std::runtime_error(error);
            skeletal->clips = {std::move(transferred)}; target->skeletal = skeletal;
            if (settings.cancelled()) throw std::runtime_error("Retarget comparison cancelled");
            task->target = target;
        } catch (const std::exception& exception) { task->error = exception.what(); }
        if (context.CancelRequested() || task->cancel) context.ReportCancelled();
        else if (!task->error.empty()) context.SetError(task->error);
        task->done.store(true, std::memory_order_release);
    }, JobPriority::Normal, "Retarget chosen operation comparison");
    state.message = "Evaluating chosen operation on import worker; no runtime publication";
}

bool SaveOperation(RetargetPanelState& state) {
    std::string error;
    if (!ValidateDraft(state, error)) { state.message = error; return false; }
    if (state.history.ProfileDirty() || state.savedProfilePath != state.profilePath) {
        state.message = "Save the profile at the selected path separately before saving the recipe operation"; return false;
    }
    try {
        fs::path recipePath, profilePath;
        if (!ProjectPath(state.root, state.targetRecipe, recipePath, error, ".judasimport", true) ||
            !ProjectPath(state.root, state.profilePath, profilePath, error, ".judasretarget", true)) throw std::runtime_error(error);
        if (state.history.current.outputName.empty()) throw std::runtime_error("Choose an output clip name");
        auto recipe = ReadRecipe(recipePath);
        if (!recipe.contains("motions")) recipe["motions"] = RetargetJson::array();
        auto& motions = recipe["motions"];
        if (!motions.is_array() || motions.size() >= 1024) throw std::runtime_error("Motion recipe array is invalid or full");
        for (const auto& clip : state.inputs->target.skeletal->clips)
            if (clip.name == state.history.current.outputName) throw std::runtime_error("Output name conflicts with a native target clip; choose another name");
        for (const auto& clip : recipe.value("clips", RetargetJson::array()))
            if (clip.value("name", clip.value("sourceClip", std::string())) == state.history.current.outputName &&
                clip.value("sourceClip", std::string()) != state.history.current.outputName)
                throw std::runtime_error("Output name conflicts with existing clip selection");
        auto duplicate = motions.end();
        for (auto item = motions.begin(); item != motions.end(); ++item)
            if (item->value("name", std::string()) == state.history.current.outputName) {
                if (duplicate != motions.end()) throw std::runtime_error("Recipe already contains duplicate output names");
                duplicate = item;
            }
        if (duplicate != motions.end() && (!state.replaceOperation || !duplicate->contains("retargetProfile")))
            throw std::runtime_error("Output name exists: explicitly permit replacing its retarget operation, or choose another name");
        RetargetJson root = {{"policy", state.history.current.rootPolicy}};
        if (state.history.current.rootPolicy != "preserve") {
            root["node"] = state.history.current.rootJoint;
            root["translation"] = {state.history.current.rootTranslation.x, state.history.current.rootTranslation.y, state.history.current.rootTranslation.z};
            if (glm::dot(state.history.current.rootRotationAxis, state.history.current.rootRotationAxis) > 1e-12f)
                root["rotationAxis"] = {state.history.current.rootRotationAxis.x, state.history.current.rootRotationAxis.y, state.history.current.rootRotationAxis.z};
        }
        const auto q = state.sourceSettings.basisRotation;
        RetargetJson operation = {{"source", state.inputs->sourceIdentity}, {"take", state.history.current.take},
            {"name", state.history.current.outputName}, {"retargetProfile", profilePath.lexically_relative(state.root).generic_string()},
            {"sampleRate", state.history.current.sampleRate}, {"loop", state.history.current.loop}, {"rootMotion", root},
            {"settings", {{"unitMeters", state.sourceSettings.sourceUnitMeters}, {"basisRotation", {q.x, q.y, q.z, q.w}}}}};
        if (state.history.current.begin != 0 || state.history.current.end >= 0) {
            auto found = std::find_if(state.inputs->source.skeletal->clips.begin(), state.inputs->source.skeletal->clips.end(),
                [&](const auto& clip) { return clip.name == state.history.current.take; });
            if (found == state.inputs->source.skeletal->clips.end()) throw std::runtime_error("Source take is unavailable");
            operation["trim"] = {state.history.current.begin, state.history.current.end < 0 ? found->duration : state.history.current.end};
        }
        if (duplicate == motions.end()) motions.push_back(operation); else *duplicate = operation;
        // Explicit clip-selection recipes must retain their existing selections
        // and include this ordinary new clip exactly once.
        if (recipe.contains("clips")) {
            bool selected = false;
            const RetargetJson selection = {{"sourceClip", state.history.current.outputName}, {"name", state.history.current.outputName},
                                           {"loop", state.history.current.loop}, {"rootMotion", root}};
            for (auto& clip : recipe["clips"]) if (clip.value("sourceClip", std::string()) == state.history.current.outputName) {
                // This explicitly edited operation owns only its own selection.
                // Keep other clip selections; do not apply trim/root twice.
                clip = selection; selected = true;
            }
            if (!selected) recipe["clips"].push_back(selection);
        }
        const auto text = recipe.dump(2) + '\n';
        if (text.size() > kRetargetProfileBytes) throw std::runtime_error("Recipe exceeds 4 MiB");
        auto staged = CreateImportStagingFile(recipePath);
        try {
            std::ofstream file(staged, std::ios::binary); file.write(text.data(), std::streamsize(text.size())); file.close();
            if (!file) throw std::runtime_error("Could not write staged recipe");
            ReplaceStagedFile(fs::u8path(staged), recipePath);
        } catch (...) { std::error_code ignored; fs::remove(staged, ignored); throw; }
        state.history.savedOperation = DraftIdentity(state.history.current);
        state.savedRecipeInputs = OperationInputIdentity(state);
        state.message = "Recipe operation saved; cooked output remains unchanged until explicit Bake";
        return true;
    } catch (const std::exception& exception) { state.message = exception.what(); return false; }
}

void PollTasks(EditorPanelState& panels, EditorRequests& requests) {
    auto& state = *panels.retarget;
    // Queued jobs cancelled before execution never enter the worker lambda or
    // set its completion atomic. Retire them through the JobSystem state too.
    if (state.inputJob && state.inputJob->cancel && panels.importJobs && panels.importJobs->IsFinished(state.inputJob->job) &&
        !state.inputJob->done.load(std::memory_order_acquire)) {
        panels.importJobs->Forget(state.inputJob->job); state.inputJob.reset();
    }
    if (state.comparisonJob && state.comparisonJob->cancel && panels.importJobs && panels.importJobs->IsFinished(state.comparisonJob->job) &&
        !state.comparisonJob->done.load(std::memory_order_acquire)) {
        panels.importJobs->Forget(state.comparisonJob->job); state.comparisonJob.reset();
    }
    if (state.bake && state.bake->cancel && panels.importJobs && panels.importJobs->IsFinished(state.bake->job) &&
        !state.bake->done.load(std::memory_order_acquire)) {
        panels.importJobs->Forget(state.bake->job); state.bake.reset(); state.bakePublished = true;
    }
    if (state.inputJob && state.inputJob->done.load(std::memory_order_acquire) &&
        (!panels.importJobs || panels.importJobs->IsFinished(state.inputJob->job))) {
        auto task = state.inputJob;
        if (panels.importJobs) panels.importJobs->Forget(task->job);
        if (!task->cancel && task->root == state.root) {
            if (task->inputs) {
                state.inputs = task->inputs; state.comparison.reset(); state.gpuReset = true; state.time = 0;
                if (state.history.current.profile.sourceIdentity.empty()) {
                    const auto before = state.history.current;
                    state.history.current.profile.sourceIdentity = task->inputs->sourceIdentity;
                    state.history.current.profile.targetIdentity = task->inputs->targetIdentity;
                    state.history.current.profile.sourceSignature = SkeletonRetargetSignature(task->inputs->source.skeletal->skeleton);
                    state.history.current.profile.targetSignature = SkeletonRetargetSignature(task->inputs->target.skeletal->skeleton);
                    if (!task->inputs->source.skeletal->clips.empty()) state.history.current.take = task->inputs->source.skeletal->clips.front().name;
                    RecordDraft(state, before);
                }
                state.message = "Inputs ready. Select explicit correspondence; unmapped target joints retain reference/rest locals";
            } else { state.message = task->error; if (state.autotestPhase) state.autotestPhase = -1; }
        }
        state.inputJob.reset();
    }
    if (state.comparisonJob && state.comparisonJob->done.load(std::memory_order_acquire) &&
        (!panels.importJobs || panels.importJobs->IsFinished(state.comparisonJob->job))) {
        auto task = state.comparisonJob;
        if (panels.importJobs) panels.importJobs->Forget(task->job);
        if (!task->cancel && task->root == state.root) {
            if (task->target && task->draftIdentity == DraftIdentity(state.history.current)) {
                state.comparison = task->target; state.comparisonIdentity = task->draftIdentity; state.time = 0;
                state.comparisonDraft = task->draft;
                if (state.autotestPhase == 2) state.time = task->target->skeletal->clips.front().duration * .5f;
                state.message = "Comparison ready: " + std::to_string(task->report.samples) + " samples, " +
                    std::to_string(task->report.milliseconds) + " ms CPU. Preview uses ordinary sampled baked tracks";
            } else {
                state.message = task->error.empty() ? "Draft changed during comparison; refresh it" : task->error;
                if (state.autotestPhase) state.autotestPhase = -1;
            }
        }
        state.comparisonJob.reset();
    }
    if (state.autotestPhase == 1 && !state.inputJob && state.inputs) {
        const auto& clips = state.inputs->source.skeletal->clips;
        if (state.history.current.take.empty()) {
            auto selected = std::find_if(clips.begin(), clips.end(), [](const auto& clip) { return clip.name == "Wave"; });
            if (selected != clips.end()) state.history.current.take = selected->name;
            else if (!clips.empty()) state.history.current.take = clips.front().name;
        }
        state.autotestPhase = 2; StartComparison(panels);
        if (!state.comparisonJob) state.autotestPhase = -1;
    }
    if (!state.bake || !state.bake->done.load(std::memory_order_acquire)) return;
    auto& task = *state.bake;
    if (task.job.IsValid() && panels.importJobs && panels.importJobs->IsFinished(task.job)) {
        panels.importJobs->Forget(task.job); task.job = {};
    }
    if (state.bakePublished) return;
    if (task.cancel || state.bakeProject != state.root || !panels.showRetarget) {
        task.cancel = true; state.bakePublished = true; state.message = "Cancelled/stale bake retired; last-good cooked asset retained"; return;
    }
    if (!task.success) { state.bakePublished = true; state.message = "Bake failed; last-good cooked asset retained: " + task.error; return; }
    if (panels.mode != EditorMode::Edit) { state.message = "Bake ready; stop Play before publishing"; return; }
    std::string error;
    if (PublishModelImport(task, error)) {
        requests.rescanAssets = true; panels.browserSelection = task.assetId;
        if (panels.resources) panels.resources->Invalidate(task.assetId);
        panels.importAccepted = state.bake;
        state.message = task.unchanged ? "Bake unchanged: reused accepted output" : "Complete target model published with ordinary named clip; existing scene untouched";
    } else state.message = "Publication rejected; last-good output retained: " + error;
    state.bakePublished = true;
}

void ReferenceEditor(const char* title, const Skeleton& skeleton, std::vector<RetargetReferenceCorrection>& corrections) {
    if (!ImGui::TreeNode(title)) return;
    ImGui::TextWrapped("Static local post-rest rotation calibration only. This does not modify the bind skeleton, weights or authored animation keys.");
    if (ImGui::Button("Add reference correction") && corrections.size() < skeleton.names.size()) corrections.emplace_back();
    for (size_t index = 0; index < corrections.size(); ++index) {
        ImGui::PushID(int(index));
        SkeletonPicker("Full joint key", skeleton, corrections[index].joint);
        QuaternionInput("Local correction x/y/z/w", corrections[index].rotation);
        if (ImGui::SmallButton("Remove correction")) { corrections.erase(corrections.begin() + std::ptrdiff_t(index)); ImGui::PopID(); break; }
        ImGui::Separator(); ImGui::PopID();
    }
    ImGui::TreePop();
}
} // namespace

void OpenRetargetPanel(EditorPanelState& panels, const std::string& source, const std::string& recipe, const std::string& profile) {
    SyncProject(panels); auto& state = *panels.retarget;
    if (!source.empty()) state.source = panels.project ? panels.project->MakeRelative(source) : source;
    if (!recipe.empty()) state.targetRecipe = panels.project ? panels.project->MakeRelative(recipe) : recipe;
    if (!profile.empty()) state.profilePath = panels.project ? panels.project->MakeRelative(profile) : profile;
    panels.showRetarget = true;
}

bool BeginRetargetAutotest(EditorPanelState& panels, const std::string& profile, std::string& error) {
    SyncProject(panels); auto& state = *panels.retarget;
    try {
        if (state.root.empty()) throw std::runtime_error("Retarget autotest requires an open project");
        fs::path path;
        if (!ProjectPath(state.root, profile, path, error, ".judasretarget", true)) throw std::runtime_error(error);
        RetargetProfile loaded;
        if (!LoadRetargetProfile(path.string(), loaded, error)) throw std::runtime_error(error);
        std::vector<fs::path> matches; size_t count = 0;
        for (const auto& entry : fs::directory_iterator(fs::path(state.root) / "Imports")) {
            if (++count > 1024) throw std::runtime_error("Retarget autotest recipe selection exceeds 1024 entries");
            if (!entry.is_regular_file() || entry.path().extension() != ".judasimport") continue;
            if (ReadRecipe(entry.path()).at("assetId") == loaded.targetIdentity) matches.push_back(entry.path());
        }
        if (matches.size() != 1) throw std::runtime_error("Retarget profile must identify exactly one target import recipe");
        state.source = loaded.sourceIdentity; state.targetRecipe = matches.front().lexically_relative(state.root).generic_string();
        state.profilePath = path.lexically_relative(state.root).generic_string();
        state.history.Reset(); state.history.current.profile = loaded; state.sourceSettings = {};
        const auto recipe = ReadRecipe(matches.front());
        const auto motions = recipe.value("motions", RetargetJson::array());
        auto operation = motions.end();
        for (auto motion = motions.begin(); motion != motions.end(); ++motion) {
            if (motion->value("retargetProfile", std::string()) != state.profilePath || motion->value("source", std::string()) != state.source) continue;
            if (operation == motions.end() || motion->value("take", std::string()) == "Wave") operation = motion;
            if (motion->value("take", std::string()) == "Wave") break;
        }
        if (operation != motions.end()) {
            state.history.current.take = operation->at("take"); state.history.current.outputName = operation->at("name");
            state.history.current.sampleRate = operation->value("sampleRate", 60.0);
            state.history.current.loop = operation->value("loop", false);
            if (operation->contains("trim")) { state.history.current.begin = operation->at("trim").at(0); state.history.current.end = operation->at("trim").at(1); }
            const auto settings = operation->value("settings", RetargetJson::object());
            state.sourceSettings.sourceUnitMeters = settings.value("unitMeters", 0.0);
            if (settings.contains("basisRotation")) {
                const auto q = settings.at("basisRotation").get<std::vector<float>>();
                if (q.size() != 4) throw std::runtime_error("Source basis dimensions");
                state.sourceSettings.basisRotation = {q[3], q[0], q[1], q[2]};
            }
            const auto root = operation->value("rootMotion", RetargetJson::object());
            state.history.current.rootPolicy = root.value("policy", std::string("preserve")); state.history.current.rootJoint = root.value("node", std::string());
            const auto axes = root.value("translation", std::vector<bool>{true, false, true});
            if (axes.size() != 3) throw std::runtime_error("Root translation dimensions");
            state.history.current.rootTranslation = {axes[0], axes[1], axes[2]};
            if (root.contains("rotationAxis")) {
                const auto axis = root.at("rotationAxis").get<std::vector<float>>();
                if (axis.size() != 3) throw std::runtime_error("Root rotation axis dimensions");
                state.history.current.rootRotationAxis = {axis[0], axis[1], axis[2]};
            }
        }
        state.history.MarkProfileSaved(); state.savedProfilePath = state.profilePath; panels.showRetarget = true;
        state.autotestPhase = 1; StartInputLoad(panels);
        if (!state.inputJob) throw std::runtime_error(state.message);
        error.clear(); return true;
    } catch (const std::exception& exception) { error = exception.what(); state.message = error; state.autotestPhase = -1; return false; }
}

int RetargetAutotestStatus(const EditorPanelState& panels, std::string& error) {
    if (!panels.retarget) { error = "Retarget autotest was not initialized"; return -1; }
    const auto& state = *panels.retarget;
    if (state.autotestPhase < 0) { error = state.message; return -1; }
    if (state.autotestPhase == 3 && state.image) { error.clear(); return 1; }
    error.clear(); return 0;
}

void RetireRetargetProject(EditorPanelState& panels) {
    if (!panels.retarget) return;
    auto& state = *panels.retarget;
    CancelTasks(state, panels.importJobs);
    state.root.clear(); state.inputs.reset(); state.comparison.reset(); state.history.Reset();
    state.savedProfilePath.clear(); state.savedRecipeInputs.clear();
    state.image = 0; state.playing = false; state.gpuReset = true; state.autotestPhase = 0;
}

void DrawRetargetPanel(EditorPanelState& panels, EditorRequests& requests) {
    SyncProject(panels); auto& state = *panels.retarget;
    const bool justOpened = panels.showRetarget && !state.wasOpen;
    if (state.wasOpen && !panels.showRetarget) { CancelTasks(state, panels.importJobs); state.playing = false; }
    state.wasOpen = panels.showRetarget;
    PollTasks(panels, requests);
    if (!panels.showRetarget) return;
    ImGui::SetNextWindowSize({1080, 780}, ImGuiCond_FirstUseEver);
    if (justOpened || state.autotestPhase > 0) ImGui::SetNextWindowFocus();
    if (!ImGui::Begin("Animation retargeting | IMPORT > MAP > PREVIEW > BAKE", &panels.showRetarget)) { ImGui::End(); return; }
    if (state.root.empty()) { ImGui::TextWrapped("Open a project first. Profiles and recipes stay in its Imports folder."); ImGui::End(); return; }
    ImGui::TextWrapped("Authoring preview only: no scripts, game world or physics. Save profile, save recipe operation and publish cooked output are separate transactions.");
    ImGui::TextWrapped("%s", state.message.c_str());
    if (!ImGui::BeginTabBar("retargetWorkspace")) { ImGui::End(); return; }
    if (ImGui::BeginTabItem("Setup / map / bake")) {
    const bool edit = panels.mode == EditorMode::Edit;
    ImGui::BeginDisabled(!edit);
    TextInput("Source animation (project relative)", state.source);
    TextInput("Target import recipe", state.targetRecipe);
    ImGui::InputDouble("Source metres per unit (0 = metadata)", &state.sourceSettings.sourceUnitMeters);
    QuaternionInput("Source post-metadata basis x/y/z/w", state.sourceSettings.basisRotation);
    const bool loading = bool(state.inputJob), comparing = bool(state.comparisonJob), baking = state.bake && !state.bake->done.load();
    ImGui::BeginDisabled(loading || comparing || baking);
    if (ImGui::Button("Load source + target on worker")) StartInputLoad(panels);
    ImGui::EndDisabled();
    if (loading) { ImGui::SameLine(); if (ImGui::Button("Cancel loading")) state.inputJob->cancel = true; }
    TextInput("Reusable profile (.judasretarget)", state.profilePath);
    if (ImGui::Button("Load profile")) {
        fs::path path;
        if (ProjectPath(state.root, state.profilePath, path, state.message, ".judasretarget", true)) {
            RetargetProfile loaded; std::string error;
            if (LoadRetargetProfile(path.string(), loaded, error)) {
                const auto before = state.history.current; state.history.current.profile = loaded;
                state.profilePath = path.lexically_relative(state.root).generic_string(); state.savedProfilePath = state.profilePath;
                state.history.MarkProfileSaved();
                RecordDraft(state, before); state.message = "Profile loaded; validate it against the selected inputs before baking";
            } else state.message = error;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("New profile for selected rigs") && InputsCurrent(state)) {
        const auto before = state.history.current; state.history.current.profile = {};
        state.history.current.profile.sourceIdentity = state.inputs->sourceIdentity; state.history.current.profile.targetIdentity = state.inputs->targetIdentity;
        state.history.current.profile.sourceSignature = SkeletonRetargetSignature(state.inputs->source.skeletal->skeleton);
        state.history.current.profile.targetSignature = SkeletonRetargetSignature(state.inputs->target.skeletal->skeleton);
        RecordDraft(state, before); state.history.savedProfile.clear(); state.message = "New draft identity/signatures; mapping is explicit and empty";
    }
    ImGui::SameLine();
    if (ImGui::Button("Save profile")) {
        std::string error; fs::path path;
        if (ProfileInputsMatch(state, error) && ProjectPath(state.root, state.profilePath, path, error, ".judasretarget", true) &&
            SaveRetargetProfile(path.string(), state.history.current.profile, error)) {
            state.profilePath = path.lexically_relative(state.root).generic_string(); state.savedProfilePath = state.profilePath;
            state.history.MarkProfileSaved();
            state.message = "Profile draft saved separately; validate complete correspondence before baking. No cooked model or scene changed";
        } else state.message = error;
    }
    ImGui::BeginDisabled(state.history.undo.empty());
    if (ImGui::Button("Undo draft")) { state.history.Undo(); state.playing = false; }
    ImGui::EndDisabled(); ImGui::SameLine(); ImGui::BeginDisabled(state.history.redo.empty());
    if (ImGui::Button("Redo draft")) { state.history.Redo(); state.playing = false; }
    ImGui::EndDisabled(); ImGui::SameLine();
    if (ImGui::Button("Cancel draft edits")) {
        state.history.Cancel(); state.playing = false;
        state.message = "Draft restored to last saved/loaded profile; filesystem writes and published assets are not undone";
    }
    ImGui::TextDisabled("%s", state.history.ProfileDirty() || state.savedProfilePath != state.profilePath ? "Profile draft/path is unsaved" : "Profile matches saved state");
    if (InputsCurrent(state)) {
        const auto& source = state.inputs->source.skeletal->skeleton; const auto& target = state.inputs->target.skeletal->skeleton;
        ImGui::Text("Source: %zu hierarchy joints / target: %zu", source.names.size(), target.names.size());
        const auto before = state.history.current;
        if (ImGui::CollapsingHeader("Explicit correspondence / full stable hierarchy keys", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Add mapping row") && state.history.current.profile.mapping.size() < std::min(source.names.size(), target.names.size()))
                state.history.current.profile.mapping.emplace_back();
            for (size_t index = 0; index < state.history.current.profile.mapping.size(); ++index) {
                ImGui::PushID(int(index)); auto& mapping = state.history.current.profile.mapping[index];
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * .43f); SkeletonPicker("Source", source, mapping.source);
                ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * .68f); SkeletonPicker("Target", target, mapping.target);
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) { state.history.current.profile.mapping.erase(state.history.current.profile.mapping.begin() + std::ptrdiff_t(index)); ImGui::PopID(); break; }
                bool translates = std::find(state.history.current.profile.translationJoints.begin(), state.history.current.profile.translationJoints.end(), mapping.target) != state.history.current.profile.translationJoints.end();
                if (ImGui::Checkbox("Transfer selected local translation delta", &translates)) {
                    if (translates) state.history.current.profile.translationJoints.push_back(mapping.target);
                    else {
                        auto& joints = state.history.current.profile.translationJoints;
                        joints.erase(std::remove(joints.begin(), joints.end(), mapping.target), joints.end());
                    }
                }
                ImGui::PopID(); ImGui::Separator();
            }
            ImGui::TextWrapped("Unmapped source ancestors still contribute to observed model motion. Unmapped target helpers retain declared reference/rest locals. No automatic name guessing.");
        }
        if (ImGui::CollapsingHeader("Reference / alignment / proportions")) {
            QuaternionInput("Source-model to target-model alignment x/y/z/w", state.history.current.profile.modelAlignment);
            ImGui::InputFloat("Positive translation scale", &state.history.current.profile.translationScale);
            ReferenceEditor("Source local reference corrections", source, state.history.current.profile.sourceReference);
            ReferenceEditor("Target local reference corrections", target, state.history.current.profile.targetReference);
            ImGui::TextWrapped("Translations and scales otherwise keep target proportions. Reference calibration does not rewrite skin bind matrices.");
        }
        if (ImGui::CollapsingHeader("Chosen clip / root-motion recipe operation", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginCombo("Source take", state.history.current.take.c_str())) {
                for (const auto& clip : state.inputs->source.skeletal->clips) if (ImGui::Selectable(clip.name.c_str(), clip.name == state.history.current.take)) state.history.current.take = clip.name;
                ImGui::EndCombo();
            }
            TextInput("Ordinary target output clip name", state.history.current.outputName);
            ImGui::Checkbox("Output clip loop metadata", &state.history.current.loop);
            ImGui::InputFloat("Trim begin seconds", &state.history.current.begin); ImGui::InputFloat("Trim end seconds (-1 = duration)", &state.history.current.end);
            ImGui::InputDouble("Baked samples / second", &state.history.current.sampleRate);
            if (ImGui::BeginCombo("Motion policy", state.history.current.rootPolicy.c_str())) {
                for (const auto* policy : {"preserve", "inPlace", "extract"}) if (ImGui::Selectable(policy, state.history.current.rootPolicy == policy)) state.history.current.rootPolicy = policy;
                ImGui::EndCombo();
            }
            if (state.history.current.rootPolicy != "preserve") {
                SkeletonPicker("Declared target locomotion joint", target, state.history.current.rootJoint);
                ImGui::Checkbox("Remove model X locomotion", &state.history.current.rootTranslation.x); ImGui::SameLine();
                ImGui::Checkbox("Remove model Y locomotion", &state.history.current.rootTranslation.y); ImGui::SameLine();
                ImGui::Checkbox("Remove model Z locomotion", &state.history.current.rootTranslation.z);
                ImGui::InputFloat3("Model rotation axis (zero = keep rotation)", &state.history.current.rootRotationAxis.x);
            }
            ImGui::TextWrapped("Model axes are profile/import axes, not world gravity. Root policy removes selected locomotion exactly once; unrelated pelvis motion remains.");
        }
        RecordDraft(state, before);
        if (ImGui::Button("Validate profile")) {
            std::string error; state.message = ValidateDraft(state, error) ? "Profile valid for the complete selected source/target hierarchy" : error;
        }
        ImGui::SameLine(); ImGui::BeginDisabled(loading || comparing || baking);
        if (ImGui::Button("Refresh comparison (CPU only)")) StartComparison(panels);
        ImGui::EndDisabled();
        if (comparing) { ImGui::SameLine(); if (ImGui::Button("Cancel comparison")) state.comparisonJob->cancel = true; }
        ImGui::Checkbox("Explicitly replace existing retarget operation with this output name", &state.replaceOperation);
        ImGui::BeginDisabled(loading || comparing || baking);
        if (ImGui::Button("Save recipe operation")) SaveOperation(state);
        ImGui::SameLine();
        if (ImGui::Button("Bake / reimport target model")) {
            std::string validation;
            if (!ValidateDraft(state, validation)) state.message = validation;
            else if (state.history.OperationDirty() || state.history.ProfileDirty() || state.savedProfilePath != state.profilePath ||
                     state.savedRecipeInputs != OperationInputIdentity(state))
                state.message = "Save the profile and current recipe operation first";
            else if (panels.importJobs) {
                fs::path path; if (ProjectPath(state.root, state.targetRecipe, path, state.message, ".judasimport", true)) {
                    state.bake = QueueModelImport(*panels.importJobs, path.string()); state.bakeProject = state.root; state.bakePublished = false;
                    state.message = "Baking on normal import worker; last-good runtime model remains active";
                }
            }
        }
        ImGui::EndDisabled();
    } else if (state.inputs) ImGui::TextColored({1, .7f, .25f, 1}, "Input selection changed; reload before mapping or baking");
    if (baking) { ImGui::ProgressBar(float(state.bake->progress.load()) / 100); if (ImGui::Button("Cancel bake")) state.bake->cancel = true; }
    ImGui::EndDisabled();
    ImGui::EndTabItem();
    }
    const auto comparisonTabFlags = state.autotestPhase > 0 ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem("Comparison", nullptr, comparisonTabFlags)) {
    if (state.inputs) {
        const bool current = state.history.PreviewCurrent(state.comparisonIdentity);
        if (state.comparison && !current) ImGui::TextColored({1, .7f, .25f, 1}, "Comparison is from the previous draft. Refresh it before trusting the result.");
        if (!state.comparison) ImGui::TextWrapped("Source and target reference inspection. Complete the mapping and Refresh comparison to see transferred motion.");
        ImGui::Checkbox("Play comparison", &state.playing); ImGui::SameLine(); ImGui::Checkbox("Declared reference pose", &state.referencePose);
        float duration = 0;
        if (state.comparison) duration = state.comparison->skeletal->clips.front().duration;
        else for (const auto& clip : state.inputs->source.skeletal->clips) if (clip.name == state.history.current.take)
            duration = std::max(0.f, (state.history.current.end < 0 ? clip.duration : state.history.current.end) - state.history.current.begin);
        ImGui::SliderFloat("Scrub chosen operation seconds", &state.time, 0, duration);
        ImGui::Checkbox("Skeleton hierarchy", &state.showSkeleton); ImGui::SameLine(); ImGui::Checkbox("Joint reference axes", &state.showAxes);
        ImGui::SliderFloat("Preview orbit", &state.yaw, -3.14f, 3.14f); ImGui::SliderFloat("Display separation only (model m)", &state.previewSeparation, .5f, 10);
        ImGui::TextWrapped("LEFT source | RIGHT target. Shared time and model scale; placement offsets never enter baked motion. Green hierarchy, RGB local axes, orange extracted motion. White ruler = one metre.");
        if (state.image) {
            const auto width = std::max(200.f, ImGui::GetContentRegionAvail().x);
            ImGui::Image(ImTextureID(state.image), {width, width * 480 / 960}, {0, 1}, {1, 0});
        }
    } else ImGui::TextWrapped("Load source and target in Setup first. Decode and comparison jobs run on import workers.");
    ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    ImGui::End();
}

void ShutdownRetargetPreview(EditorPanelState& panels, Renderer& renderer) {
    if (!panels.retarget) return;
    auto& state = *panels.retarget; CancelTasks(state, panels.importJobs);
    if (state.sourceGpu.IsValid()) renderer.DestroyMesh(state.sourceGpu);
    if (state.targetGpu.IsValid()) renderer.DestroyMesh(state.targetGpu);
    if (state.target.IsValid()) renderer.DestroyRenderTarget(state.target);
    state.sourceGpu = {}; state.targetGpu = {}; state.target = {}; state.image = 0; state.uploadedInputs.reset();
}

void UpdateRetargetPreview(EditorPanelState& panels, Renderer& renderer, float deltaSeconds) {
    SyncProject(panels); auto& state = *panels.retarget;
    if (state.gpuReset || state.uploadedInputs.lock() != state.inputs) {
        if (state.sourceGpu.IsValid()) renderer.DestroyMesh(state.sourceGpu);
        if (state.targetGpu.IsValid()) renderer.DestroyMesh(state.targetGpu);
        state.sourceGpu = {}; state.targetGpu = {}; state.image = 0; state.uploadedInputs = state.inputs;
        if (state.inputs) {
            if (!state.inputs->source.vertices.empty()) state.sourceGpu = renderer.CreateMesh(state.inputs->source);
            if (!state.inputs->target.vertices.empty()) state.targetGpu = renderer.CreateMesh(state.inputs->target);
        }
        state.gpuReset = false;
    }
    if (!panels.showRetarget || panels.mode != EditorMode::Edit || !state.inputs) return;
    const auto& source = *state.inputs->source.skeletal;
    const auto& target = state.comparison ? *state.comparison->skeletal : *state.inputs->target.skeletal;
    const auto& chosen = state.comparison ? state.comparisonDraft : state.history.current;
    const auto sourceClip = std::find_if(source.clips.begin(), source.clips.end(), [&](const auto& clip) { return clip.name == chosen.take; });
    if (sourceClip == source.clips.end() || (state.comparison && target.clips.empty())) return;
    const auto duration = state.comparison ? target.clips.front().duration : std::max(0.f, (chosen.end < 0 ? sourceClip->duration : chosen.end) - chosen.begin);
    if (state.playing && !state.referencePose && duration > 0) state.time = std::fmod(state.time + std::max(0.f, deltaSeconds), duration);
    auto sourcePose = state.referencePose ? source.skeleton.rest : SampleClip(source.skeleton, *sourceClip, state.time + chosen.begin);
    SkeletalPose targetPose;
    if (state.referencePose && state.comparison) {
        auto reference = source.skeleton.rest;
        for (const auto& correction : chosen.profile.sourceReference) {
            const int joint = FindSkeletonJoint(source.skeleton, correction.joint);
            if (joint >= 0) reference.local[joint].rotation = glm::normalize(reference.local[joint].rotation * correction.rotation);
        }
        std::string error;
        sourcePose = reference;
        if (!RetargetPose(source.skeleton, state.inputs->target.skeletal->skeleton, chosen.profile, reference, targetPose, error)) return;
        if (targetPose.local.size() < target.skeleton.rest.local.size()) targetPose.local.push_back(target.skeleton.rest.local.back());
    } else if (state.comparison) targetPose = SampleClip(target.skeleton, target.clips.front(), state.time);
    else {
        targetPose = target.skeleton.rest;
        auto corrections = [](const Skeleton& skeleton, SkeletalPose& pose, const std::vector<RetargetReferenceCorrection>& reference) {
            for (const auto& correction : reference) {
                const int joint = FindSkeletonJoint(skeleton, correction.joint);
                if (joint >= 0 && std::isfinite(glm::length(correction.rotation)) && glm::dot(correction.rotation, correction.rotation) > 1e-12f)
                    pose.local[joint].rotation = glm::normalize(pose.local[joint].rotation * correction.rotation);
            }
        };
        corrections(target.skeleton, targetPose, chosen.profile.targetReference);
        if (state.referencePose) corrections(source.skeleton, sourcePose, chosen.profile.sourceReference);
    }
    const auto low = state.inputs->low, high = state.inputs->high;
    const float span = std::max(.5f, glm::length(high - low));
    const float halfSeparation = state.previewSeparation * .5f;
    const glm::vec3 center = (low + high) * .5f;
    const float radius = span + state.previewSeparation;
    const glm::vec3 eye = center + glm::vec3(std::sin(state.yaw) * radius, radius * .2f, std::cos(state.yaw) * radius);
    std::string error;
    if (!state.target.IsValid()) state.target = renderer.CreateRenderTarget(960, 480, error);
    if (!state.target.IsValid() || !renderer.BeginRenderTarget(state.target)) return;
    renderer.SetCamera(glm::lookAt(eye, center, glm::vec3(0, 1, 0)), glm::perspective(glm::radians(50.f), 2.f, .01f, std::max(100.f, radius * 10)));
    renderer.SetLighting(glm::normalize(glm::vec3(1, 2, 3)), {1.8f, 1.8f, 1.8f}, {.35f, .35f, .35f});
    renderer.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {.035f, .045f, .065f});
    renderer.SetDynamicLights({}); renderer.SetMaterialBindings({});
    const auto sourceSkin = ResolveSkinMatrices(source.skeleton, sourcePose), targetSkin = ResolveSkinMatrices(target.skeleton, targetPose);
    const glm::vec3 sourceOffset(-halfSeparation, 0, 0), targetOffset(halfSeparation, 0, 0);
    if (state.sourceGpu.IsValid()) renderer.DrawMesh(state.sourceGpu, sourceOffset, {1, 0, 0, 0}, {1, 1, 1}, {}, {1, 1, 1}, 1, &sourceSkin);
    if (state.targetGpu.IsValid()) renderer.DrawMesh(state.targetGpu, targetOffset, {1, 0, 0, 0}, {1, 1, 1}, {}, {1, 1, 1}, 1, &targetSkin);
    DebugLineList lines;
    auto hierarchy = [&](const Skeleton& skeleton, const SkeletalPose& pose, glm::vec3 offset) {
        const auto globals = ResolveJointMatrices(skeleton, pose);
        for (size_t joint = 0; joint < globals.size(); ++joint) {
            const auto position = glm::vec3(globals[joint][3]) + offset;
            if (state.showSkeleton && skeleton.parents[joint] >= 0)
                lines.Line(position, glm::vec3(globals[skeleton.parents[joint]][3]) + offset, {.2f, 1, .6f});
            if (state.showAxes) {
                for (int axis = 0; axis < 3; ++axis) {
                    glm::vec3 color(0); color[axis] = 1;
                    const auto direction = glm::vec3(globals[joint][axis]);
                    if (glm::dot(direction, direction) > 1e-12f) lines.Line(position, position + glm::normalize(direction) * .12f, color);
                }
            }
        }
    };
    hierarchy(source.skeleton, sourcePose, sourceOffset); hierarchy(target.skeleton, targetPose, targetOffset);
    lines.Line({low.x, low.y, low.z}, {low.x + 1, low.y, low.z}, {1, 1, 1});
    if (state.comparison && !target.clips.front().motion.empty()) {
        const auto& clip = target.clips.front();
        glm::vec3 previous(0);
        for (unsigned sample = 0; sample <= 60; ++sample) {
            const auto point = SampleRootMotion(clip, double(clip.duration) * sample / 60, false).translation + targetOffset;
            if (sample) lines.Line(previous, point, {1, .7f, .1f});
            previous = point;
        }
    }
    renderer.DrawDebugLines(lines.Lines(), false); renderer.EndRenderTarget();
    state.image = renderer.EditorImageToken(renderer.RenderTargetTexture(state.target));
    if (state.autotestPhase == 2 && state.comparison && state.image) {
        state.autotestPhase = 3;
        std::printf("[M74 editor retarget] READY source_joints=%zu target_joints=%zu comparison_keys=%zu\n",
                    source.skeleton.names.size(), target.skeleton.names.size(), target.clips.front().tracks.size());
    }
}
