#include "WorldState.h"
#include <map>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

#include "RuntimeWorld.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"

namespace {
// A physical state closer than this to the baseline definition is "not
// moved". A body resting on static geometry never sits exactly at its
// authored pose: the contact solver's steady state leaves it a few
// millimetres into the surface with a few millimetres per second of
// corrected velocity (measured 1.4 mm / 2.7 mm/s for a 5 kg crate). That
// settling is not a persistent change and must not produce a delta; a
// real move, tip or throw is orders of magnitude larger.
constexpr float kMovedPositionTolerance = 1.0e-2f;   // 1 cm
constexpr float kMovedVelocityTolerance = 2.0e-2f;   // 2 cm/s
constexpr float kMovedRotationTolerance = 1.0e-3f;   // ~2.5 degrees

std::string F(float v) {
    char buffer[64];
    for (int precision = 1; precision <= 9; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(v));
        if (std::strtof(buffer, nullptr) == v && (precision == 9 || std::strchr(buffer, 'e') == nullptr)) break;
    }
    return buffer;
}
std::string V(const glm::vec3& v) { return F(v.x) + " " + F(v.y) + " " + F(v.z); }
std::string Q(const glm::quat& q) { return F(q.w) + " " + F(q.x) + " " + F(q.y) + " " + F(q.z); }
std::string Quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

struct Token {
    std::string text;
    bool quoted = false;
};

bool Tokenize(const std::string& line, std::vector<Token>& out, std::string& error) {
    out.clear();
    std::size_t i = 0;
    while (i < line.size()) {
        const char c = line[i];
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        if (c == '#') break;
        if (c == '"') {
            Token t;
            t.quoted = true;
            ++i;
            bool closed = false;
            while (i < line.size()) {
                if (line[i] == '\\' && i + 1 < line.size()) { t.text += line[i + 1]; i += 2; continue; }
                if (line[i] == '"') { closed = true; ++i; break; }
                t.text += line[i++];
            }
            if (!closed) { error = "unterminated string"; return false; }
            out.push_back(t);
            continue;
        }
        Token t;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '#') t.text += line[i++];
        out.push_back(t);
    }
    return true;
}

bool ParseFloat(const Token& t, float& out) {
    if (t.quoted || t.text.empty()) return false;
    char* end = nullptr;
    const double v = std::strtod(t.text.c_str(), &end);
    if (!end || *end != '\0' || !std::isfinite(v)) return false;
    out = static_cast<float>(v);
    return std::isfinite(out);
}
bool ParseU64(const Token& t, unsigned long long& out) {
    if (t.quoted || t.text.empty()) return false;
    const auto result = std::from_chars(t.text.data(), t.text.data() + t.text.size(), out);
    return result.ec == std::errc{} && result.ptr == t.text.data() + t.text.size();
}
bool ParseVec3(const std::vector<Token>& t, std::size_t start, glm::vec3& out) {
    return t.size() >= start + 3 && ParseFloat(t[start], out.x) && ParseFloat(t[start + 1], out.y) &&
           ParseFloat(t[start + 2], out.z);
}

void WriteState(std::string& out, const EntityPhysicalState& s) {
    out += "  position " + V(s.position) + "\n";
    out += "  rotation " + Q(s.rotation) + "\n";
    out += "  linear-velocity " + V(s.linearVelocity) + "\n";
    out += "  angular-velocity " + V(s.angularVelocity) + "\n";
}

bool StateDiffers(const EntityPhysicalState& a, const EntityPhysicalState& b) {
    if (glm::length(a.position - b.position) > kMovedPositionTolerance) return true;
    if (glm::length(a.linearVelocity - b.linearVelocity) > kMovedVelocityTolerance) return true;
    if (glm::length(a.angularVelocity - b.angularVelocity) > kMovedVelocityTolerance) return true;
    const glm::quat qa = glm::normalize(a.rotation), qb = glm::normalize(b.rotation);
    const float dot = std::abs(glm::dot(qa, qb));
    return 1.0f - dot > kMovedRotationTolerance;
}

std::string Fail(std::size_t line, const std::string& message) {
    return "world state line " + std::to_string(line) + ": " + message;
}
bool ValidCompatibility(const WorldStateCompatibility& c, std::string& error) {
    if (c.formatVersion != kWorldStateFormatVersion || c.fingerprintVersion != kSceneFingerprintVersion ||
        c.algorithm != "sha256") {
        error = "unsupported world-state compatibility version or fingerprint algorithm";
        return false;
    }
    if (c.baselineFingerprint.size() != 64 || !std::all_of(c.baselineFingerprint.begin(), c.baselineFingerprint.end(),
        [](char x) { return (x >= '0' && x <= '9') || (x >= 'a' && x <= 'f'); })) {
        error = "missing or invalid authored-baseline SHA-256 fingerprint; unverifiable saves cannot be applied";
        return false;
    }
    return true;
}

bool ValidPhysicalState(const EntityPhysicalState& state) {
    const auto finite = [](const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    const float norm = glm::dot(state.rotation, state.rotation);
    return finite(state.position) && finite(state.linearVelocity) && finite(state.angularVelocity) &&
           std::isfinite(norm) && norm > 0.0f;
}

// Shared by file parsing, saving and the in-memory Apply API. These checks
// require no world mutations or resource requests.
bool ValidateStructure(const WorldState& state, std::string& error) {
    if (!ValidCompatibility(state.compatibility, error)) return false;
    if (state.baselineName.find_first_of("\r\n") != std::string::npos ||
        state.baselineName.find('\0') != std::string::npos ||
        state.nextRuntimeId < kRuntimeEntityIdBase ||
        state.nextRuntimeId > static_cast<EntityId>(std::numeric_limits<std::int64_t>::max())) {
        error = "invalid baseline name or next-runtime-id";
        return false;
    }
    std::set<std::pair<EntityId,uint64_t>> slots;
    if(state.scripts.size()>4096){error="too many script state records";return false;}
    for(const auto& r:state.scripts){if(!r.entity||!r.slot||!slots.insert({r.entity,r.slot}).second||!ScriptSystem::ValidateJson(r.json,error)){
        if(error.empty())error="invalid/duplicate script state reference";
        return false;}}
    std::set<EntityId> seen;
    for (const auto& change : state.entities) {
        if (change.id == 0 || !seen.insert(change.id).second || (change.created && change.destroyed)) {
            error = "invalid or duplicate entity record " + std::to_string(change.id);
            return false;
        }
        if (change.created) {
            if (change.id < kRuntimeEntityIdBase || change.id >= state.nextRuntimeId || change.definition.id != change.id) {
                error = "invalid created entity id, definition id or next-runtime-id";
                return false;
            }
            if (!RuntimeWorld::ValidateEntityDefinition(change.definition, error)) return false;
        } else if (change.id >= kRuntimeEntityIdBase) {
            error = "moved/destroyed records must refer to authored entities; runtime entities require a created record";
            return false;
        }
        if (!change.destroyed && !ValidPhysicalState(change.state)) {
            error = "entity " + std::to_string(change.id) + " has non-finite state or a degenerate rotation";
            return false;
        }
    }
    std::set<std::pair<bool, SceneObjectId>> interactables;
    for (const auto& change : state.interactables) {
        if (change.id == 0 || change.id >= kRuntimeEntityIdBase || !interactables.insert({change.isDoor, change.id}).second) {
            error = "invalid or duplicate interactable reference " + std::to_string(change.id);
            return false;
        }
    }
    return true;
}
}  // namespace

bool CanCaptureLegacyWorldState(const RuntimeWorld& world,std::string& error){
    for(const auto& definition:world.ScriptObjects()){
        if(!definition.body||world.IsTransientEntity(definition.id))continue;
        const auto handle=world.RuntimeBody(definition.id);if(!handle.IsValid())continue;
        const auto current=world.Physics().GetMotionType(handle);const auto* record=world.FindEntity(definition.id);
        const auto baseline=record&&record->definition.body?record->definition.body->motion:definition.body->motion;
        const auto expected=baseline==SceneBodyMotion::Static?BodyMotionType::Static:baseline==SceneBodyMotion::Dynamic?BodyMotionType::Dynamic:BodyMotionType::Kinematic;
        if(current==BodyMotionType::Kinematic||current!=expected){
            error="entity "+std::to_string(definition.id)+": legacy world-state deltas cannot preserve body authority or kinematic commands; use modern save slots";return false;
        }
    }
    return true;
}

WorldState CaptureWorldState(const RuntimeWorld& world) {
    WorldState out;
    out.baselineName = world.Settings().name;
    out.compatibility.baselineFingerprint = world.BaselineFingerprint();
    out.nextRuntimeId = world.NextRuntimeEntityId();
    if(world.Scripts())out.scripts=world.Scripts()->Capture();
    auto persistent=world.Entities();
    persistent.insert(persistent.end(),world.AdditionalEntities().begin(),world.AdditionalEntities().end());
    for (const EntityRecord& e : persistent) {
        if(e.transient)continue; // articulation bodies belong to their runtime owner

        WorldStateEntityChange change;
        change.id = e.id;
        if (e.lifecycle == EntityLifecycle::Destroyed) {
            // A created-then-destroyed entity is absent from the baseline too.
            // Preserve its consumed ID through nextRuntimeId, not a dangling tombstone.
            if (!e.authored) continue;
            change.destroyed = true;
            out.entities.push_back(change);
            continue;
        }
        EntityPhysicalState state;
        world.GetEntityState(e.id, state);
        if (!e.authored) {
            change.created = true;
            change.definition = e.definition;
            change.state = state;
            out.entities.push_back(change);
            continue;
        }
        EntityPhysicalState authored;
        authored.position = e.definition.transform.position;
        authored.rotation = glm::normalize(e.definition.transform.rotation);
        authored.linearVelocity = e.definition.body ? e.definition.body->initialLinearVelocity : glm::vec3(0.0f);
        authored.angularVelocity = e.definition.body ? e.definition.body->initialAngularVelocity : glm::vec3(0.0f);
        if (StateDiffers(state, authored)) {
            change.state = state;
            out.entities.push_back(change);
        }
    }
    std::sort(out.entities.begin(), out.entities.end(),
              [](const WorldStateEntityChange& a, const WorldStateEntityChange& b) { return a.id < b.id; });
    RuntimeWorld& mutableWorld = const_cast<RuntimeWorld&>(world);  // FindDoor/FindLightSwitch are non-const accessors
    for (const SceneObjectId id : world.DoorIds()) {
        const Door* door = mutableWorld.FindDoor(id);
        if (door && door->IsOpen()) out.interactables.push_back({id, true, true});
    }
    for (const SceneObjectId id : world.LightSwitchIds()) {
        const LightSwitch* lightSwitch = mutableWorld.FindLightSwitch(id);
        if (lightSwitch && lightSwitch->IsLampOn()) out.interactables.push_back({id, false, true});
    }
    return out;
}

bool ApplyWorldState(RuntimeWorld& world, const WorldState& state, std::string& outError) {
    if(world.IsComposed()){outError="Disk saves are unsupported for composed worlds (M59)";return false;}
    outError.clear();
    if (!ValidateStructure(state, outError)) return false;
    if (!world.IsBuilt() || world.BaselineFingerprint().empty() || state.compatibility.baselineFingerprint != world.BaselineFingerprint()) {
        outError = "authored baseline fingerprint mismatch for '" + state.baselineName +
                   "': saved deltas require exactly compatible authored scene content";
        return false;
    }
    if (state.nextRuntimeId < world.NextRuntimeEntityId()) {
        outError = "next-runtime-id would reuse an already consumed runtime identity";
        return false;
    }
    // Preflight the same capability checks used by the mutators themselves.
    // No asset demand, handles, identity counters or world state change here.
    for (const auto& change : state.entities) {
        if (change.created) {
            if (!world.ValidateEntityCreation(change.definition, outError)) return false;
        } else {
            const EntityRecord* existing = world.FindEntity(change.id);
            if (!existing || !existing->authored) {
                outError = "world state refers to unknown authored entity " + std::to_string(change.id);
                return false;
            }
            if (change.destroyed) {
                if (!world.ValidateEntityDestruction(change.id, outError)) return false;
            } else if (existing->lifecycle == EntityLifecycle::Destroyed) {
                outError = "cannot move destroyed entity " + std::to_string(change.id);
                return false;
            }
        }
    }
    // Created hierarchy references are checked as a complete batch before any
    // mutation. Forward references are legal; dangling parents and cycles are not.
    std::map<EntityId,EntityId> createdParents;
    for(const auto& change:state.entities)if(change.created)createdParents[change.id]=change.definition.parent;
    for(const auto& pair:createdParents){
        std::set<EntityId> path;auto id=pair.first;
        while(id&&createdParents.count(id)){
            if(!path.insert(id).second){outError="cyclic created-entity hierarchy";return false;}
            id=createdParents.at(id);
        }
        if(id&&!world.FindEntity(id)){outError="created entity references an unknown parent";return false;}
    }
    for (const auto& change : state.interactables) {
        const bool known = change.isDoor ? world.FindDoor(change.id) != nullptr : world.FindLightSwitch(change.id) != nullptr;
        if (!known) {
            outError = "world state refers to an unknown interactable " + std::to_string(change.id);
            return false;
        }
    }

    // Complete script-reference/state preflight before any live mutation or JS evaluation.
    for(const auto& r:state.scripts){const SceneObject* definition=world.RuntimeDefinition(r.entity);
        for(const auto& change:state.entities)if(change.id==r.entity){if(change.destroyed){outError="script state references destroyed entity";return false;}if(change.created)definition=&change.definition;}
        if(!definition||std::none_of(definition->scripts.begin(),definition->scripts.end(),[&](const auto& slot){return slot.id==r.slot&&slot.enabled;})){
            outError="unknown or disabled script slot";return false;}
    }
    // Created constraints are checked with the complete prospective entity set before mutation.
    auto jointBody=[&](EntityId id)->const SceneObject* {
        for(const auto& change:state.entities)if(change.id==id){if(change.destroyed)return nullptr;if(change.created)return &change.definition;}
        return world.RuntimeDefinition(id);
    };
    for(const auto& change:state.entities)if(change.created&&change.definition.joint){
        const auto& j=*change.definition.joint;auto a=jointBody(j.bodyA),b=j.bodyB?jointBody(j.bodyB):nullptr;
        if(!a||!a->body||(j.bodyB&&(!b||!b->body))||(a->body->motion==SceneBodyMotion::Static&&(!b||b->body->motion==SceneBodyMotion::Static))){outError="created joint requires available bodies and a dynamic participant";return false;}
    }
    // --- Apply.
    for (const WorldStateEntityChange& change : state.entities) {
        std::string error;
        if (change.destroyed) {
            if (!world.DestroyEntity(change.id, &error)) { outError = error; return false; }
        } else if (change.created) {
            if (world.CreateEntity(change.definition, &change.state, &error) == kInvalidSceneObjectId) {
                outError = "created entity " + std::to_string(change.id) + ": " + error;
                return false;
            }
        } else {
            if (!world.SetEntityState(change.id, change.state)) {
                outError = "prevalidated entity became unavailable"; return false;
            }
        }
    }
    for (const WorldStateInteractableChange& change : state.interactables) {
        if (change.isDoor) world.FindDoor(change.id)->SetOpen(change.on);
        else world.FindLightSwitch(change.id)->SetLampOn(change.on);
    }
    if(!world.RestoreScriptState(state.scripts,outError))return false;
    world.SetNextRuntimeEntityId(state.nextRuntimeId);
    return true;
}

bool SaveWorldStateToString(const WorldState& state, std::string& outText) {
    std::string error;
    if (!ValidateStructure(state, error)) return false;
    std::string out = "JudasWorldState " + std::to_string(kWorldStateFormatVersion) + "\n";
    out += "compatibility " + std::to_string(state.compatibility.fingerprintVersion) + " " +
           state.compatibility.algorithm + " " + state.compatibility.baselineFingerprint + "\n";
    out += "baseline " + Quote(state.baselineName) + "\n";
    out += "next-runtime-id " + std::to_string(state.nextRuntimeId) + "\n";
    std::vector<WorldStateEntityChange> entities = state.entities;
    std::sort(entities.begin(), entities.end(),
              [](const WorldStateEntityChange& a, const WorldStateEntityChange& b) { return a.id < b.id; });
    for (const WorldStateEntityChange& change : entities) {
        out += "\n";
        if (change.destroyed) {
            out += "entity " + std::to_string(change.id) + " destroyed\n";
        } else if (change.created) {
            out += "entity " + std::to_string(change.id) + " created\n";
            WriteState(out, change.state);
            WriteSceneObjectBlock(change.definition, out);
            out += "end\n";
        } else {
            out += "entity " + std::to_string(change.id) + " moved\n";
            WriteState(out, change.state);
            out += "end\n";
        }
    }
    std::vector<WorldStateInteractableChange> interactables = state.interactables;
    std::sort(interactables.begin(), interactables.end(),
              [](const WorldStateInteractableChange& a, const WorldStateInteractableChange& b) {
                  return a.isDoor != b.isDoor ? a.isDoor : a.id < b.id;
              });
    for (const WorldStateInteractableChange& change : interactables) {
        out += std::string(change.isDoor ? "door " : "light-switch ") + std::to_string(change.id) + " " +
               (change.on ? "true" : "false") + "\n";
    }
    // Versioned optional extension preserves script-free version-2 saves.
    if(!state.scripts.empty()){out+="script-state-schema 1\n";for(const auto& r:state.scripts)
        out+="script-state "+std::to_string(r.entity)+" "+std::to_string(r.slot)+" "+Quote(r.json)+"\n";}
    outText = out;
    return true;
}

bool SaveWorldStateToFile(const WorldState& state, const std::string& path, std::string& outError) {
    std::string text;
    outError.clear();
    if (!ValidateStructure(state, outError) || !SaveWorldStateToString(state, text)) return false;
    // Never turn an unreadable, legacy or incompatible save into a new save
    // implicitly. The operator can archive/delete it explicitly if desired.
    std::error_code existsError;
    const bool exists = std::filesystem::exists(path, existsError);
    if (existsError) { outError = "cannot inspect world state file: " + existsError.message(); return false; }
    if (exists) {
        WorldState previous;
        if (!LoadWorldStateFromFile(path, previous, outError)) return false;
        if (previous.compatibility.baselineFingerprint != state.compatibility.baselineFingerprint) {
            outError = "refusing to overwrite an incompatible authored-baseline save: " + path;
            return false;
        }
    }
    // The default location is saves/<scene>.judasstate; create the folder.
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        outError = "could not open world state file for writing: " + path;
        return false;
    }
    file << text;
    if (!file) {
        outError = "failed writing world state file: " + path;
        return false;
    }
    return true;
}

bool LoadWorldStateFromString(const std::string& text, WorldState& outState, std::string& outError) {
    outError.clear();
    if (text.find('\0') != std::string::npos) { outError = "world state contains a NUL byte"; return false; }
    std::vector<std::string> lines;
    {
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) lines.push_back(line);
    }
    WorldState state;
    std::size_t i = 0;
    std::vector<Token> tokens;
    const auto next = [&](std::size_t& lineNumber) -> bool {
        while (i < lines.size()) {
            lineNumber = i + 1;
            std::string error;
            if (!Tokenize(lines[i], tokens, error)) { outError = Fail(lineNumber, error); return false; }
            ++i;
            if (!tokens.empty()) return true;
        }
        return false;
    };
    std::size_t lineNumber = 0;
    if (!next(lineNumber)) {
        if (outError.empty()) outError = "world state file is empty";
        return false;
    }
    unsigned long long version = 0;
    if (tokens.size() != 2 || tokens[0].quoted || tokens[0].text != "JudasWorldState" || !ParseU64(tokens[1], version)) {
        outError = Fail(lineNumber, "expected 'JudasWorldState <version>' on the first line");
        return false;
    }
    if (version != static_cast<unsigned long long>(kWorldStateFormatVersion)) {
        outError = Fail(lineNumber, "unsupported world state version " + std::to_string(version) +
                        (version == 1 ? "; legacy saves have no verifiable baseline fingerprint; preserve/archive the file explicitly" : ""));
        return false;
    }
    bool baselineSeen = false, nextIdSeen = false, compatibilitySeen = false, scriptsSeen=false;
    std::set<EntityId> seen;
    while (next(lineNumber)) {
        const std::string& key = tokens[0].text;
        if (tokens[0].quoted) { outError = Fail(lineNumber, "expected a directive"); return false; }
        if (key == "compatibility") {
            unsigned long long schema = 0;
            if (compatibilitySeen || tokens.size() != 4 || !ParseU64(tokens[1], schema) ||
                schema != kSceneFingerprintVersion || tokens[2].quoted || tokens[3].quoted) {
                outError = Fail(lineNumber, "expected one supported compatibility <schema> sha256 <fingerprint> record");
                return false;
            }
            state.compatibility.fingerprintVersion = static_cast<int>(schema);
            state.compatibility.algorithm = tokens[2].text;
            state.compatibility.baselineFingerprint = tokens[3].text;
            compatibilitySeen = true;
        } else if (key == "baseline") {
            if (baselineSeen || tokens.size() != 2 || !tokens[1].quoted) { outError = Fail(lineNumber, "baseline expects one quoted name"); return false; }
            state.baselineName = tokens[1].text;
            baselineSeen = true;
        } else if (key == "next-runtime-id") {
            unsigned long long v = 0;
            if (nextIdSeen || tokens.size() != 2 || !ParseU64(tokens[1], v) || v < kRuntimeEntityIdBase) {
                outError = Fail(lineNumber, "next-runtime-id must be an integer in the runtime range");
                return false;
            }
            state.nextRuntimeId = v;
            nextIdSeen = true;
        } else if (key == "entity") {
            unsigned long long id = 0;
            if (tokens.size() != 3 || tokens[2].quoted || !ParseU64(tokens[1], id) || id == 0) {
                outError = Fail(lineNumber, "entity expects '<id> moved|destroyed|created'");
                return false;
            }
            if (!seen.insert(id).second) { outError = Fail(lineNumber, "entity " + std::to_string(id) + " listed twice"); return false; }
            WorldStateEntityChange change;
            change.id = id;
            const std::string& kind = tokens[2].text;
            if (kind == "destroyed") {
                change.destroyed = true;
            } else if (kind == "moved" || kind == "created") {
                change.created = kind == "created";
                bool seenPosition = false, seenRotation = false, seenLinear = false, seenAngular = false, seenObject = false;
                bool closed = false;
                while (next(lineNumber)) {
                    const std::string& field = tokens[0].text;
                    if (tokens[0].quoted) { outError = Fail(lineNumber, "expected an entity directive"); return false; }
                    if (field == "end" && tokens.size() == 1) { closed = true; break; }
                    if (field == "position" && !seenPosition && ParseVec3(tokens, 1, change.state.position) && tokens.size() == 4) { seenPosition = true; continue; }
                    if (field == "rotation" && !seenRotation && tokens.size() == 5) {
                        float w, x, y, z;
                        if (ParseFloat(tokens[1], w) && ParseFloat(tokens[2], x) && ParseFloat(tokens[3], y) && ParseFloat(tokens[4], z)) {
                            change.state.rotation = glm::quat(w, x, y, z);
                            seenRotation = true;
                            continue;
                        }
                    }
                    if (field == "linear-velocity" && !seenLinear && ParseVec3(tokens, 1, change.state.linearVelocity) && tokens.size() == 4) { seenLinear = true; continue; }
                    if (field == "angular-velocity" && !seenAngular && ParseVec3(tokens, 1, change.state.angularVelocity) && tokens.size() == 4) { seenAngular = true; continue; }
                    if (field == "object" && change.created && !seenObject) {
                        // The definition block starts on the line just consumed.
                        std::size_t blockIndex = i - 1;
                        std::string error;
                        if (!ParseSceneObjectBlock(lines, blockIndex, change.definition, error)) {
                            outError = Fail(lineNumber, "created entity definition: " + error);
                            return false;
                        }
                        seenObject = true;
                        i = blockIndex;
                        continue;
                    }
                    outError = Fail(lineNumber, "unexpected '" + field + "' in an entity block");
                    return false;
                }
                if (!closed) { outError = Fail(lineNumber, "entity block is missing its 'end'"); return false; }
                if (!seenPosition || !seenRotation || !seenLinear || !seenAngular) {
                    outError = Fail(lineNumber, "entity " + std::to_string(id) + " is missing a physical state field");
                    return false;
                }
                if (change.created && change.definition.id != id) {
                    outError = Fail(lineNumber, "created entity " + std::to_string(id) + " needs an object block with the same id");
                    return false;
                }
            } else {
                outError = Fail(lineNumber, "entity change must be moved, destroyed or created");
                return false;
            }
            state.entities.push_back(change);
        } else if(key=="script-state-schema"){
            if(scriptsSeen||tokens.size()!=2||tokens[1].text!="1"||tokens[1].quoted){outError=Fail(lineNumber,"unsupported/duplicate script state schema");return false;}scriptsSeen=true;
        } else if(key=="script-state"){
            unsigned long long id=0,slot=0;if(!scriptsSeen||tokens.size()!=4||!ParseU64(tokens[1],id)||!ParseU64(tokens[2],slot)||!tokens[3].quoted){outError=Fail(lineNumber,"invalid script-state record");return false;}
            state.scripts.push_back({id,slot,tokens[3].text});
        } else if (key == "door" || key == "light-switch") {
            unsigned long long id = 0;
            if (tokens.size() != 3 || tokens[2].quoted || !ParseU64(tokens[1], id) || (tokens[2].text != "true" && tokens[2].text != "false")) {
                outError = Fail(lineNumber, key + " expects '<id> true|false'");
                return false;
            }
            state.interactables.push_back({id, key == "door", tokens[2].text == "true"});
        } else {
            outError = Fail(lineNumber, "unknown directive '" + key + "'");
            return false;
        }
    }
    if (!outError.empty()) return false;
    if (!baselineSeen || !nextIdSeen || !compatibilitySeen) {
        outError = "world state file is missing its compatibility, baseline or next-runtime-id line";
        return false;
    }
    if (!ValidateStructure(state, outError)) return false;
    outState = std::move(state);
    return true;
}

bool LoadWorldStateFromFile(const std::string& path, WorldState& outState, std::string& outError) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outError = "could not open world state file: " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (file.bad() || buffer.fail()) { outError = "could not read world state file: " + path; return false; }
    if (!LoadWorldStateFromString(buffer.str(), outState, outError)) {
        outError = path + ": " + outError;
        return false;
    }
    return true;
}

std::string DefaultWorldStatePath(const std::string& scenePath) {
    std::string stem = scenePath;
    const std::size_t slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const std::size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
    return stem.empty() ? std::string() : "saves/" + stem + ".judasstate";
}

bool ApplyWorldStateFileIfPresent(RuntimeWorld& world, const std::string& path, bool& outApplied, std::string& outError) {
    outApplied = false;
    outError.clear();
    if (path.empty()) return true;
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) { outError = "cannot inspect world state file: " + error.message(); return false; }
    if (!exists) return true;  // Only an actually absent file is 'no saved state'.
    WorldState state;
    if (!LoadWorldStateFromFile(path, state, outError)) return false;
    if (!ApplyWorldState(world, state, outError)) return false;
    outApplied = true;
    return true;
}
