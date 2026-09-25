#include "WorldState.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "RuntimeWorld.h"
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
    char* end = nullptr;
    out = std::strtoull(t.text.c_str(), &end, 10);
    return end && *end == '\0';
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
}  // namespace

WorldState CaptureWorldState(const RuntimeWorld& world) {
    WorldState out;
    out.baselineName = world.Settings().name;
    out.nextRuntimeId = world.NextRuntimeEntityId();
    for (const EntityRecord& e : world.Entities()) {
        WorldStateEntityChange change;
        change.id = e.id;
        if (e.lifecycle == EntityLifecycle::Destroyed) {
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
    // --- Validate everything first; touch nothing until it all checks out.
    std::set<EntityId> seen;
    for (const WorldStateEntityChange& change : state.entities) {
        if (!seen.insert(change.id).second) {
            outError = "entity " + std::to_string(change.id) + " appears twice in the world state";
            return false;
        }
        const EntityRecord* existing = world.FindEntity(change.id);
        if (change.created) {
            if (change.id < kRuntimeEntityIdBase) {
                outError = "created entity " + std::to_string(change.id) + " is not in the runtime id range";
                return false;
            }
            if (existing) {
                outError = "created entity " + std::to_string(change.id) + " already exists in the world";
                return false;
            }
            if (change.definition.id != change.id) {
                outError = "created entity " + std::to_string(change.id) + " has a mismatched definition id";
                return false;
            }
            if (!change.definition.body || change.definition.body->motion != SceneBodyMotion::Dynamic) {
                outError = "created entity " + std::to_string(change.id) + " needs a dynamic body";
                return false;
            }
        } else if (!existing) {
            outError = "world state refers to unknown entity " + std::to_string(change.id);
            return false;
        }
    }
    for (const WorldStateInteractableChange& change : state.interactables) {
        const bool known = change.isDoor ? world.FindDoor(change.id) != nullptr
                                         : world.FindLightSwitch(change.id) != nullptr;
        if (!known) {
            outError = std::string("world state refers to an unknown ") + (change.isDoor ? "door " : "light switch ") +
                       std::to_string(change.id);
            return false;
        }
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
            world.SetEntityState(change.id, change.state);
        }
    }
    for (const WorldStateInteractableChange& change : state.interactables) {
        if (change.isDoor) world.FindDoor(change.id)->SetOpen(change.on);
        else world.FindLightSwitch(change.id)->SetLampOn(change.on);
    }
    world.SetNextRuntimeEntityId(state.nextRuntimeId);
    return true;
}

bool SaveWorldStateToString(const WorldState& state, std::string& outText) {
    std::string out = "JudasWorldState " + std::to_string(kWorldStateFormatVersion) + "\n";
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
    outText = out;
    return true;
}

bool SaveWorldStateToFile(const WorldState& state, const std::string& path, std::string& outError) {
    std::string text;
    SaveWorldStateToString(state, text);
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
    if (tokens.size() != 2 || tokens[0].text != "JudasWorldState" || !ParseU64(tokens[1], version)) {
        outError = Fail(lineNumber, "expected 'JudasWorldState <version>' on the first line");
        return false;
    }
    if (version != static_cast<unsigned long long>(kWorldStateFormatVersion)) {
        outError = Fail(lineNumber, "unsupported world state version " + std::to_string(version));
        return false;
    }
    bool baselineSeen = false, nextIdSeen = false;
    std::set<EntityId> seen;
    while (next(lineNumber)) {
        const std::string& key = tokens[0].text;
        if (tokens[0].quoted) { outError = Fail(lineNumber, "expected a directive"); return false; }
        if (key == "baseline") {
            if (tokens.size() != 2 || !tokens[1].quoted) { outError = Fail(lineNumber, "baseline expects a quoted name"); return false; }
            state.baselineName = tokens[1].text;
            baselineSeen = true;
        } else if (key == "next-runtime-id") {
            unsigned long long v = 0;
            if (tokens.size() != 2 || !ParseU64(tokens[1], v) || v < kRuntimeEntityIdBase) {
                outError = Fail(lineNumber, "next-runtime-id must be an integer in the runtime range");
                return false;
            }
            state.nextRuntimeId = v;
            nextIdSeen = true;
        } else if (key == "entity") {
            unsigned long long id = 0;
            if (tokens.size() != 3 || !ParseU64(tokens[1], id) || id == 0) {
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
                bool seenPosition = false, seenRotation = false, seenLinear = false, seenAngular = false;
                bool closed = false;
                while (next(lineNumber)) {
                    const std::string& field = tokens[0].text;
                    if (field == "end") { closed = true; break; }
                    if (field == "position" && ParseVec3(tokens, 1, change.state.position) && tokens.size() == 4) { seenPosition = true; continue; }
                    if (field == "rotation" && tokens.size() == 5) {
                        float w, x, y, z;
                        if (ParseFloat(tokens[1], w) && ParseFloat(tokens[2], x) && ParseFloat(tokens[3], y) && ParseFloat(tokens[4], z)) {
                            change.state.rotation = glm::quat(w, x, y, z);
                            seenRotation = true;
                            continue;
                        }
                    }
                    if (field == "linear-velocity" && ParseVec3(tokens, 1, change.state.linearVelocity) && tokens.size() == 4) { seenLinear = true; continue; }
                    if (field == "angular-velocity" && ParseVec3(tokens, 1, change.state.angularVelocity) && tokens.size() == 4) { seenAngular = true; continue; }
                    if (field == "object" && change.created) {
                        // The definition block starts on the line just consumed.
                        std::size_t blockIndex = i - 1;
                        std::string error;
                        if (!ParseSceneObjectBlock(lines, blockIndex, change.definition, error)) {
                            outError = Fail(lineNumber, "created entity definition: " + error);
                            return false;
                        }
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
        } else if (key == "door" || key == "light-switch") {
            unsigned long long id = 0;
            if (tokens.size() != 3 || !ParseU64(tokens[1], id) || (tokens[2].text != "true" && tokens[2].text != "false")) {
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
    if (!baselineSeen || !nextIdSeen) {
        outError = "world state file is missing its baseline or next-runtime-id line";
        return false;
    }
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
    if (path.empty()) return true;
    std::ifstream probe(path, std::ios::binary);
    if (!probe) return true;  // no saved state: the pristine baseline
    probe.close();
    WorldState state;
    if (!LoadWorldStateFromFile(path, state, outError)) return false;
    if (!ApplyWorldState(world, state, outError)) return false;
    outApplied = true;
    return true;
}
