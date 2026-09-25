#include "SceneSerialization.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "Scene.h"

namespace {

// --- Writing ---------------------------------------------------------------

// The shortest decimal that parses back to exactly the same float, so a
// file reads "9.81" rather than "9.81000042" while still round-tripping
// bit-exactly (9 significant digits always suffice for a float, 17 for a
// double).
std::string F(float v) {
    char buffer[64];
    for (int precision = 1; precision <= 9; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(v));
        // Prefer plain notation ("20", not "2e+01") while a short form exists.
        if (std::strtof(buffer, nullptr) == v && (precision == 9 || std::strchr(buffer, 'e') == nullptr)) break;
    }
    return buffer;
}
std::string D(double v) {
    char buffer[64];
    for (int precision = 1; precision <= 17; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, v);
        if (std::strtod(buffer, nullptr) == v && (precision == 17 || std::strchr(buffer, 'e') == nullptr)) break;
    }
    return buffer;
}
std::string V(const glm::vec3& v) { return F(v.x) + " " + F(v.y) + " " + F(v.z); }
std::string DV(const glm::dvec3& v) { return D(v.x) + " " + D(v.y) + " " + D(v.z); }
std::string Q(const glm::quat& q) { return F(q.w) + " " + F(q.x) + " " + F(q.y) + " " + F(q.z); }
std::string Quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

const char* ShapeName(SceneShape s) {
    switch (s) {
        case SceneShape::Box: return "box";
        case SceneShape::Sphere: return "sphere";
        case SceneShape::Compound: return "compound";
        case SceneShape::Mesh: return "mesh";
        case SceneShape::Terrain: return "terrain";
    }
    return "box";
}

class Writer {
public:
    void Line(const std::string& key, const std::string& value) {
        m_out += "  " + key + " " + value + "\n";
    }
    void Raw(const std::string& s) { m_out += s; }
    std::string Take() { return std::move(m_out); }

private:
    std::string m_out;
};

void WriteObject(Writer& w, const SceneObject& o) {
    w.Raw("object " + std::to_string(o.id) + " " + Quote(o.name) + "\n");
    w.Line("position", V(o.transform.position));
    w.Line("rotation", Q(o.transform.rotation));
    w.Line("scale", V(o.transform.scale));
    if (o.render) {
        const SceneRenderComponent& r = *o.render;
        w.Line("render", ShapeName(r.shape));
        w.Line("render.half-extents", V(r.halfExtents));
        w.Line("render.radius", F(r.radius));
        w.Line("render.color", V(r.color));
        w.Line("render.alpha", F(r.alpha));
        w.Line("render.secondary-color", V(r.secondaryColor));
        w.Line("render.secondary-alpha", F(r.secondaryAlpha));
        w.Line("render.mesh", Quote(r.meshPath));
        w.Line("render.texture", Quote(r.texturePath));
    }
    if (o.body) {
        const SceneBodyComponent& b = *o.body;
        w.Line("body", std::string(b.motion == SceneBodyMotion::Static ? "static" : "dynamic") +
                           " " + ShapeName(b.shape));
        w.Line("body.half-extents", V(b.halfExtents));
        w.Line("body.radius", F(b.radius));
        w.Line("body.terrain", Quote(b.terrainSurface));
        w.Line("body.mass", F(b.mass));
        w.Line("body.friction", F(b.friction));
        w.Line("body.restitution", F(b.restitution));
        w.Line("body.initial-velocity", V(b.initialLinearVelocity));
        w.Line("body.pickable", b.pickable ? "true" : "false");
        w.Line("body.compound-count", std::to_string(b.compoundBoxes.size()));
        for (const CompoundBox& box : b.compoundBoxes) {
            w.Line("body.compound-box", V(box.localCenter) + " " + V(box.halfExtents));
        }
    }
    if (o.gravity) {
        const SceneGravityComponent& g = *o.gravity;
        w.Line("gravity", std::string(g.kind == SceneGravityKind::Radial ? "radial" : "uniform") +
                              " " + F(g.magnitude));
        w.Line("gravity.region", g.regionShape == SceneRegionShape::Sphere
                                     ? "sphere " + F(g.regionRadius)
                                     : "box " + V(g.regionHalfExtents));
    }
    if (o.light) {
        const SceneLightComponent& l = *o.light;
        w.Line("light", l.kind == SceneLightKind::Point ? "point" : "spot");
        w.Line("light.color", V(l.color));
        w.Line("light.range", F(l.range));
        w.Line("light.cone", F(l.innerConeDegrees) + " " + F(l.outerConeDegrees));
    }
    if (o.door) {
        const SceneDoorComponent& d = *o.door;
        w.Line("door", "");
        w.Line("door.hinge-axis", V(d.localHingeAxis));
        w.Line("door.open-angle", F(d.openAngleDegrees));
        w.Line("door.angular-speed", F(d.angularSpeedDegreesPerSecond));
    }
    if (o.lightSwitch) {
        const SceneLightSwitchComponent& s = *o.lightSwitch;
        w.Line("light-switch", "");
        w.Line("light-switch.hinge-axis", V(s.localHingeAxis));
        w.Line("light-switch.toggle-angle", F(s.toggleAngleDegrees));
        w.Line("light-switch.angular-speed", F(s.angularSpeedDegreesPerSecond));
        w.Line("light-switch.lamp-offset", V(s.lampLocalOffset));
        w.Line("light-switch.lamp-color", V(s.lampColor));
        w.Line("light-switch.lamp-range", F(s.lampRange));
    }
    if (o.vehicle) {
        const SceneVehicleComponent& v = *o.vehicle;
        w.Line("vehicle", v.gravity == SceneVehicleGravity::Local ? "local" : "celestial");
        w.Line("vehicle.headlight", v.headlight ? "true" : "false");
        w.Line("vehicle.navigation-lights", v.navigationLights ? "true" : "false");
        w.Line("vehicle.drag-coefficient", F(v.dragCoefficient));
        w.Line("vehicle.initial-pilot-attached", v.initialPilotAttached ? "true" : "false");
    }
    if (o.celestial) {
        const SceneCelestialComponent& c = *o.celestial;
        w.Line("celestial", "");
        w.Line("celestial.gravitational-parameter", F(c.gravitationalParameter));
        w.Line("celestial.operator-thrust", F(c.operatorThrustForce));
    }
    if (o.atmosphere) {
        const SceneAtmosphereComponent& a = *o.atmosphere;
        w.Line("atmosphere", "");
        w.Line("atmosphere.reference-radius", F(a.referenceRadius));
        w.Line("atmosphere.top-radius", F(a.topRadius));
        w.Line("atmosphere.reference-density", F(a.referenceDensity));
        w.Line("atmosphere.polytropic-exponent", F(a.polytropicExponent));
        w.Line("atmosphere.oxidizer-fraction", F(a.oxidizerMassFraction));
        w.Line("atmosphere.reference-temperature", F(a.referenceTemperatureKelvin));
    }
    if (o.combustible) {
        const SceneCombustibleComponent& c = *o.combustible;
        w.Line("combustible", "");
        w.Line("combustible.heat-capacity", F(c.heatCapacityJPerK));
        w.Line("combustible.fuel-mass", F(c.initialFuelMassKg));
        w.Line("combustible.ignition-temperature", F(c.ignitionTemperatureK));
        w.Line("combustible.max-fuel-rate", F(c.maximumFuelRateKgPerSecond));
        w.Line("combustible.radiative-area", F(c.radiativeAreaSquareMeters));
        w.Line("combustible.retained-heat", F(c.retainedCombustionHeatFraction));
    }
    if (o.fluidVolume) {
        const SceneFluidVolumeComponent& f = *o.fluidVolume;
        w.Line("fluid-volume", "");
        w.Line("fluid-volume.spacing", F(f.spacing));
        w.Line("fluid-volume.count", std::to_string(f.countX) + " " + std::to_string(f.countY) +
                                         " " + std::to_string(f.countZ));
        w.Line("fluid-volume.emitter", f.emitter ? "true" : "false");
        w.Line("fluid-volume.emitter-offset", V(f.emitterLocalOffset));
        w.Line("fluid-volume.max-particles", std::to_string(f.maxParticles));
    }
    if (o.playerStart) {
        const ScenePlayerStartComponent& p = *o.playerStart;
        w.Line("player-start", p.view == ScenePlayerView::ThirdPerson ? "third-person" : "first-person");
        w.Line("player-start.yaw", F(p.yawDegrees));
    }
    w.Raw("end\n");
}

// --- Reading ---------------------------------------------------------------

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
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '#') {
            t.text += line[i++];
        }
        out.push_back(t);
    }
    return true;
}

class Reader {
public:
    Reader(const std::string& text, std::string& error) : m_error(error) {
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) m_lines.push_back(line);
    }

    bool Fail(const std::string& message) {
        m_error = "scene line " + std::to_string(m_lineNumber) + ": " + message;
        return false;
    }

    // Reads the next non-empty, non-comment line's tokens. Returns false at
    // end of input (with no error set).
    bool Next(std::vector<Token>& tokens) {
        while (m_index < m_lines.size()) {
            m_lineNumber = m_index + 1;
            const std::string& line = m_lines[m_index++];
            std::string tokenError;
            if (!Tokenize(line, tokens, tokenError)) { Fail(tokenError); m_failed = true; return false; }
            if (!tokens.empty()) return true;
        }
        return false;
    }
    bool Failed() const { return m_failed; }
    void MarkFailed() { m_failed = true; }

private:
    std::vector<std::string> m_lines;
    std::size_t m_index = 0;
    std::size_t m_lineNumber = 0;
    bool m_failed = false;
    std::string& m_error;
};

bool ParseFloat(const Token& t, float& out) {
    if (t.quoted || t.text.empty()) return false;
    char* end = nullptr;
    const double value = std::strtod(t.text.c_str(), &end);
    if (end == nullptr || *end != '\0' || !std::isfinite(value)) return false;
    out = static_cast<float>(value);
    return std::isfinite(out);
}
bool ParseDouble(const Token& t, double& out) {
    if (t.quoted || t.text.empty()) return false;
    char* end = nullptr;
    out = std::strtod(t.text.c_str(), &end);
    return end != nullptr && *end == '\0' && std::isfinite(out);
}
bool ParseInt(const Token& t, long long& out) {
    if (t.quoted || t.text.empty()) return false;
    char* end = nullptr;
    out = std::strtoll(t.text.c_str(), &end, 10);
    return end != nullptr && *end == '\0';
}
bool ParseBool(const Token& t, bool& out) {
    if (t.quoted) return false;
    if (t.text == "true") { out = true; return true; }
    if (t.text == "false") { out = false; return true; }
    return false;
}
bool ParseShape(const Token& t, SceneShape& out) {
    if (t.quoted) return false;
    if (t.text == "box") out = SceneShape::Box;
    else if (t.text == "sphere") out = SceneShape::Sphere;
    else if (t.text == "compound") out = SceneShape::Compound;
    else if (t.text == "mesh") out = SceneShape::Mesh;
    else if (t.text == "terrain") out = SceneShape::Terrain;
    else return false;
    return true;
}

// One object block being assembled: collects `key -> tokens` and tracks
// which keys were seen so required fields can be checked after `end`.
struct Block {
    std::map<std::string, std::vector<Token>> values;
    std::vector<std::vector<Token>> compoundBoxes;
    std::set<std::string> seen;
};

class ObjectParser {
public:
    ObjectParser(Reader& reader, const Block& block) : m_reader(reader), m_block(block) {}

    bool Has(const std::string& key) const { return m_block.values.count(key) != 0; }

    bool Vec3(const std::string& key, glm::vec3& out) {
        const std::vector<Token>* t = Require(key, 3);
        if (!t) return false;
        return ParseFloat((*t)[0], out.x) && ParseFloat((*t)[1], out.y) && ParseFloat((*t)[2], out.z)
                   ? Consume(key)
                   : m_reader.Fail("key '" + key + "' expects three finite numbers");
    }
    bool DVec3(const std::string& key, glm::dvec3& out) {
        const std::vector<Token>* t = Require(key, 3);
        if (!t) return false;
        return ParseDouble((*t)[0], out.x) && ParseDouble((*t)[1], out.y) && ParseDouble((*t)[2], out.z)
                   ? Consume(key)
                   : m_reader.Fail("key '" + key + "' expects three finite numbers");
    }
    bool Quat(const std::string& key, glm::quat& out) {
        const std::vector<Token>* t = Require(key, 4);
        if (!t) return false;
        float w, x, y, z;
        if (!ParseFloat((*t)[0], w) || !ParseFloat((*t)[1], x) || !ParseFloat((*t)[2], y) ||
            !ParseFloat((*t)[3], z)) {
            return m_reader.Fail("key '" + key + "' expects four finite numbers (w x y z)");
        }
        out = glm::quat(w, x, y, z);
        return Consume(key);
    }
    bool Float(const std::string& key, float& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        return ParseFloat((*t)[0], out) ? Consume(key)
                                        : m_reader.Fail("key '" + key + "' expects a finite number");
    }
    bool Int(const std::string& key, int& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        long long v = 0;
        if (!ParseInt((*t)[0], v) || v < -2147483647LL || v > 2147483647LL) {
            return m_reader.Fail("key '" + key + "' expects an integer");
        }
        out = static_cast<int>(v);
        return Consume(key);
    }
    bool Bool(const std::string& key, bool& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        return ParseBool((*t)[0], out) ? Consume(key)
                                       : m_reader.Fail("key '" + key + "' expects true or false");
    }
    bool String(const std::string& key, std::string& out) {
        const std::vector<Token>* t = Require(key, 1);
        if (!t) return false;
        if (!(*t)[0].quoted) return m_reader.Fail("key '" + key + "' expects a quoted string");
        out = (*t)[0].text;
        return Consume(key);
    }
    // Header keys with a small enumerated word list; returns the tokens.
    const std::vector<Token>* Header(const std::string& key, std::size_t minCount, std::size_t maxCount) {
        const auto it = m_block.values.find(key);
        if (it == m_block.values.end()) { m_reader.Fail("missing '" + key + "'"); return nullptr; }
        if (it->second.size() < minCount || it->second.size() > maxCount) {
            m_reader.Fail("key '" + key + "' has the wrong number of values");
            return nullptr;
        }
        Consume(key);
        return &it->second;
    }

    // After parsing: every key present must have been consumed.
    bool CheckNoUnknown() {
        for (const auto& [key, tokens] : m_block.values) {
            if (!m_consumed.count(key)) return m_reader.Fail("unknown or misplaced key '" + key + "'");
        }
        return true;
    }
    const std::vector<std::vector<Token>>& CompoundBoxes() const { return m_block.compoundBoxes; }

private:
    const std::vector<Token>* Require(const std::string& key, std::size_t count) {
        const auto it = m_block.values.find(key);
        if (it == m_block.values.end()) { m_reader.Fail("missing required key '" + key + "'"); return nullptr; }
        if (it->second.size() != count) {
            m_reader.Fail("key '" + key + "' has the wrong number of values");
            return nullptr;
        }
        return &it->second;
    }
    bool Consume(const std::string& key) { m_consumed.insert(key); return true; }

    Reader& m_reader;
    const Block& m_block;
    std::set<std::string> m_consumed;
};

bool ReadBlock(Reader& reader, Block& block) {
    std::vector<Token> tokens;
    while (reader.Next(tokens)) {
        if (tokens[0].text == "end" && !tokens[0].quoted) return true;
        if (tokens[0].quoted) return reader.Fail("expected a key");
        const std::string key = tokens[0].text;
        std::vector<Token> values(tokens.begin() + 1, tokens.end());
        if (key == "body.compound-box") {
            block.compoundBoxes.push_back(values);
            block.seen.insert(key);
            continue;
        }
        if (block.values.count(key)) return reader.Fail("duplicate key '" + key + "'");
        block.values[key] = values;
        block.seen.insert(key);
    }
    if (reader.Failed()) return false;
    return reader.Fail("unexpected end of file inside a block (missing 'end')");
}

bool ParseSettings(Reader& reader, const Block& block, Scene& scene) {
    ObjectParser p(reader, block);
    SceneSettings& s = scene.Settings();
    if (!p.String("name", s.name)) return false;
    if (!p.DVec3("world-origin", s.worldOrigin)) return false;
    if (!p.Vec3("sun-direction", s.sunDirection)) return false;
    if (!p.Vec3("sun-color", s.sunColor)) return false;
    if (!p.Vec3("ambient", s.ambientColor)) return false;
    if (!p.Float("fluid-scale", s.fluidScale)) return false;
    int nextId = 0;
    if (!p.Int("next-id", nextId)) return false;
    if (nextId < 1) return reader.Fail("next-id must be at least 1");
    scene.SetNextId(static_cast<SceneObjectId>(nextId));
    return p.CheckNoUnknown();
}

bool ParseObject(Reader& reader, const std::vector<Token>& header, const Block& block,
                 SceneObject& o) {
    if (header.size() != 3 || header[1].quoted || !header[2].quoted) {
        return reader.Fail("object header must be: object <id> \"<name>\"");
    }
    long long id = 0;
    if (!ParseInt(header[1], id) || id <= 0) return reader.Fail("object id must be a positive integer");
    o.id = static_cast<SceneObjectId>(id);
    o.name = header[2].text;

    ObjectParser p(reader, block);
    if (!p.Vec3("position", o.transform.position)) return false;
    if (!p.Quat("rotation", o.transform.rotation)) return false;
    if (!p.Vec3("scale", o.transform.scale)) return false;

    if (p.Has("render")) {
        SceneRenderComponent r;
        const std::vector<Token>* h = p.Header("render", 1, 1);
        if (!h) return false;
        if (!ParseShape((*h)[0], r.shape)) return reader.Fail("render shape must be box, sphere, compound, mesh or terrain");
        if (!p.Vec3("render.half-extents", r.halfExtents)) return false;
        if (!p.Float("render.radius", r.radius)) return false;
        if (!p.Vec3("render.color", r.color)) return false;
        if (!p.Float("render.alpha", r.alpha)) return false;
        if (!p.Vec3("render.secondary-color", r.secondaryColor)) return false;
        if (!p.Float("render.secondary-alpha", r.secondaryAlpha)) return false;
        if (!p.String("render.mesh", r.meshPath)) return false;
        if (!p.String("render.texture", r.texturePath)) return false;
        o.render = r;
    }
    if (p.Has("body")) {
        SceneBodyComponent b;
        const std::vector<Token>* h = p.Header("body", 2, 2);
        if (!h) return false;
        if ((*h)[0].text == "static") b.motion = SceneBodyMotion::Static;
        else if ((*h)[0].text == "dynamic") b.motion = SceneBodyMotion::Dynamic;
        else return reader.Fail("body motion must be static or dynamic");
        if (!ParseShape((*h)[1], b.shape) || b.shape == SceneShape::Mesh) {
            return reader.Fail("body shape must be box, sphere, compound or terrain");
        }
        if (!p.Vec3("body.half-extents", b.halfExtents)) return false;
        if (!p.Float("body.radius", b.radius)) return false;
        if (!p.String("body.terrain", b.terrainSurface)) return false;
        if (!p.Float("body.mass", b.mass)) return false;
        if (!p.Float("body.friction", b.friction)) return false;
        if (!p.Float("body.restitution", b.restitution)) return false;
        if (!p.Vec3("body.initial-velocity", b.initialLinearVelocity)) return false;
        if (!p.Bool("body.pickable", b.pickable)) return false;
        int compoundCount = 0;
        if (!p.Int("body.compound-count", compoundCount)) return false;
        if (compoundCount < 0 || static_cast<std::size_t>(compoundCount) != p.CompoundBoxes().size()) {
            return reader.Fail("body.compound-count does not match the body.compound-box lines");
        }
        for (const std::vector<Token>& t : p.CompoundBoxes()) {
            CompoundBox box;
            if (t.size() != 6 || !ParseFloat(t[0], box.localCenter.x) || !ParseFloat(t[1], box.localCenter.y) ||
                !ParseFloat(t[2], box.localCenter.z) || !ParseFloat(t[3], box.halfExtents.x) ||
                !ParseFloat(t[4], box.halfExtents.y) || !ParseFloat(t[5], box.halfExtents.z)) {
                return reader.Fail("body.compound-box expects six finite numbers");
            }
            b.compoundBoxes.push_back(box);
        }
        if (b.shape == SceneShape::Compound && b.compoundBoxes.empty()) {
            return reader.Fail("a compound body needs at least one body.compound-box");
        }
        if (b.shape == SceneShape::Terrain && b.terrainSurface.empty()) {
            return reader.Fail("a terrain body needs a body.terrain surface identifier");
        }
        if (b.motion == SceneBodyMotion::Dynamic && !(b.mass > 0.0f)) {
            return reader.Fail("a dynamic body needs a positive body.mass");
        }
        if (b.motion == SceneBodyMotion::Dynamic && b.shape == SceneShape::Terrain) {
            return reader.Fail("terrain bodies must be static");
        }
        o.body = b;
    } else if (p.Has("body.compound-count") || !p.CompoundBoxes().empty()) {
        return reader.Fail("body.* keys require a 'body' header");
    }
    if (p.Has("gravity")) {
        SceneGravityComponent g;
        const std::vector<Token>* h = p.Header("gravity", 2, 2);
        if (!h) return false;
        if ((*h)[0].text == "radial") g.kind = SceneGravityKind::Radial;
        else if ((*h)[0].text == "uniform") g.kind = SceneGravityKind::Uniform;
        else return reader.Fail("gravity kind must be radial or uniform");
        if (!ParseFloat((*h)[1], g.magnitude)) return reader.Fail("gravity magnitude must be a finite number");
        const std::vector<Token>* region = p.Header("gravity.region", 2, 4);
        if (!region) return false;
        if ((*region)[0].text == "sphere" && region->size() == 2) {
            g.regionShape = SceneRegionShape::Sphere;
            if (!ParseFloat((*region)[1], g.regionRadius)) return reader.Fail("gravity.region sphere radius must be a finite number");
        } else if ((*region)[0].text == "box" && region->size() == 4) {
            g.regionShape = SceneRegionShape::Box;
            if (!ParseFloat((*region)[1], g.regionHalfExtents.x) || !ParseFloat((*region)[2], g.regionHalfExtents.y) ||
                !ParseFloat((*region)[3], g.regionHalfExtents.z)) {
                return reader.Fail("gravity.region box expects three finite numbers");
            }
        } else {
            return reader.Fail("gravity.region must be 'sphere <r>' or 'box <x> <y> <z>'");
        }
        o.gravity = g;
    }
    if (p.Has("light")) {
        SceneLightComponent l;
        const std::vector<Token>* h = p.Header("light", 1, 1);
        if (!h) return false;
        if ((*h)[0].text == "point") l.kind = SceneLightKind::Point;
        else if ((*h)[0].text == "spot") l.kind = SceneLightKind::Spot;
        else return reader.Fail("light kind must be point or spot");
        if (!p.Vec3("light.color", l.color)) return false;
        if (!p.Float("light.range", l.range)) return false;
        const std::vector<Token>* cone = p.Header("light.cone", 2, 2);
        if (!cone) return false;
        if (!ParseFloat((*cone)[0], l.innerConeDegrees) || !ParseFloat((*cone)[1], l.outerConeDegrees)) {
            return reader.Fail("light.cone expects two finite numbers");
        }
        o.light = l;
    }
    if (p.Has("door")) {
        SceneDoorComponent d;
        if (!p.Header("door", 0, 0)) return false;
        if (!p.Vec3("door.hinge-axis", d.localHingeAxis)) return false;
        if (!p.Float("door.open-angle", d.openAngleDegrees)) return false;
        if (!p.Float("door.angular-speed", d.angularSpeedDegreesPerSecond)) return false;
        if (!o.render || o.render->shape != SceneShape::Box) {
            return reader.Fail("a door needs a box render component for its panel");
        }
        o.door = d;
    }
    if (p.Has("light-switch")) {
        SceneLightSwitchComponent s;
        if (!p.Header("light-switch", 0, 0)) return false;
        if (!p.Vec3("light-switch.hinge-axis", s.localHingeAxis)) return false;
        if (!p.Float("light-switch.toggle-angle", s.toggleAngleDegrees)) return false;
        if (!p.Float("light-switch.angular-speed", s.angularSpeedDegreesPerSecond)) return false;
        if (!p.Vec3("light-switch.lamp-offset", s.lampLocalOffset)) return false;
        if (!p.Vec3("light-switch.lamp-color", s.lampColor)) return false;
        if (!p.Float("light-switch.lamp-range", s.lampRange)) return false;
        if (!o.render || o.render->shape != SceneShape::Box) {
            return reader.Fail("a light switch needs a box render component for its lever");
        }
        o.lightSwitch = s;
    }
    if (p.Has("vehicle")) {
        SceneVehicleComponent v;
        const std::vector<Token>* h = p.Header("vehicle", 1, 1);
        if (!h) return false;
        if ((*h)[0].text == "local") v.gravity = SceneVehicleGravity::Local;
        else if ((*h)[0].text == "celestial") v.gravity = SceneVehicleGravity::Celestial;
        else return reader.Fail("vehicle gravity must be local or celestial");
        if (!p.Bool("vehicle.headlight", v.headlight)) return false;
        if (!p.Bool("vehicle.navigation-lights", v.navigationLights)) return false;
        if (!p.Float("vehicle.drag-coefficient", v.dragCoefficient)) return false;
        if (!p.Bool("vehicle.initial-pilot-attached", v.initialPilotAttached)) return false;
        if (!o.body || o.body->motion != SceneBodyMotion::Dynamic || o.body->shape != SceneShape::Box) {
            return reader.Fail("a vehicle needs a dynamic box body");
        }
        o.vehicle = v;
    }
    if (p.Has("celestial")) {
        SceneCelestialComponent c;
        if (!p.Header("celestial", 0, 0)) return false;
        if (!p.Float("celestial.gravitational-parameter", c.gravitationalParameter)) return false;
        if (!p.Float("celestial.operator-thrust", c.operatorThrustForce)) return false;
        if (!o.body) return reader.Fail("a celestial component needs a body");
        o.celestial = c;
    }
    if (p.Has("atmosphere")) {
        SceneAtmosphereComponent a;
        if (!p.Header("atmosphere", 0, 0)) return false;
        if (!p.Float("atmosphere.reference-radius", a.referenceRadius)) return false;
        if (!p.Float("atmosphere.top-radius", a.topRadius)) return false;
        if (!p.Float("atmosphere.reference-density", a.referenceDensity)) return false;
        if (!p.Float("atmosphere.polytropic-exponent", a.polytropicExponent)) return false;
        if (!p.Float("atmosphere.oxidizer-fraction", a.oxidizerMassFraction)) return false;
        if (!p.Float("atmosphere.reference-temperature", a.referenceTemperatureKelvin)) return false;
        if (!o.celestial || !(o.celestial->gravitationalParameter > 0.0f)) {
            return reader.Fail("an atmosphere needs a celestial component with a positive gravitational parameter");
        }
        o.atmosphere = a;
    }
    if (p.Has("combustible")) {
        SceneCombustibleComponent c;
        if (!p.Header("combustible", 0, 0)) return false;
        if (!p.Float("combustible.heat-capacity", c.heatCapacityJPerK)) return false;
        if (!p.Float("combustible.fuel-mass", c.initialFuelMassKg)) return false;
        if (!p.Float("combustible.ignition-temperature", c.ignitionTemperatureK)) return false;
        if (!p.Float("combustible.max-fuel-rate", c.maximumFuelRateKgPerSecond)) return false;
        if (!p.Float("combustible.radiative-area", c.radiativeAreaSquareMeters)) return false;
        if (!p.Float("combustible.retained-heat", c.retainedCombustionHeatFraction)) return false;
        if (!o.body || o.body->motion != SceneBodyMotion::Dynamic) {
            return reader.Fail("a combustible component needs a dynamic body");
        }
        o.combustible = c;
    }
    if (p.Has("fluid-volume")) {
        SceneFluidVolumeComponent f;
        if (!p.Header("fluid-volume", 0, 0)) return false;
        if (!p.Float("fluid-volume.spacing", f.spacing)) return false;
        const std::vector<Token>* count = p.Header("fluid-volume.count", 3, 3);
        if (!count) return false;
        long long cx = 0, cy = 0, cz = 0;
        if (!ParseInt((*count)[0], cx) || !ParseInt((*count)[1], cy) || !ParseInt((*count)[2], cz) ||
            cx < 0 || cy < 0 || cz < 0 || cx > 1000 || cy > 1000 || cz > 1000) {
            return reader.Fail("fluid-volume.count expects three non-negative integers");
        }
        f.countX = static_cast<int>(cx);
        f.countY = static_cast<int>(cy);
        f.countZ = static_cast<int>(cz);
        if (!p.Bool("fluid-volume.emitter", f.emitter)) return false;
        if (!p.Vec3("fluid-volume.emitter-offset", f.emitterLocalOffset)) return false;
        if (!p.Int("fluid-volume.max-particles", f.maxParticles)) return false;
        if (!(f.spacing > 0.0f)) return reader.Fail("fluid-volume.spacing must be positive");
        o.fluidVolume = f;
    }
    if (p.Has("player-start")) {
        ScenePlayerStartComponent ps;
        const std::vector<Token>* h = p.Header("player-start", 1, 1);
        if (!h) return false;
        if ((*h)[0].text == "third-person") ps.view = ScenePlayerView::ThirdPerson;
        else if ((*h)[0].text == "first-person") ps.view = ScenePlayerView::FirstPerson;
        else return reader.Fail("player-start view must be third-person or first-person");
        if (!p.Float("player-start.yaw", ps.yawDegrees)) return false;
        o.playerStart = ps;
    }
    if (o.render && (o.render->shape == SceneShape::Compound || o.render->shape == SceneShape::Terrain)) {
        if (!o.body || o.body->shape != o.render->shape) {
            return reader.Fail("a compound/terrain render component needs a body of the same shape");
        }
    }
    return p.CheckNoUnknown();
}

}  // namespace

bool SaveSceneToString(const Scene& scene, std::string& outText) {
    Writer w;
    w.Raw("JudasScene " + std::to_string(kSceneFormatVersion) + "\n");
    w.Raw("settings\n");
    const SceneSettings& s = scene.Settings();
    w.Line("name", Quote(s.name));
    w.Line("world-origin", DV(s.worldOrigin));
    w.Line("sun-direction", V(s.sunDirection));
    w.Line("sun-color", V(s.sunColor));
    w.Line("ambient", V(s.ambientColor));
    w.Line("fluid-scale", F(s.fluidScale));
    w.Line("next-id", std::to_string(scene.NextId()));
    w.Raw("end\n");
    for (const SceneObject& o : scene.Objects()) {
        w.Raw("\n");
        WriteObject(w, o);
    }
    outText = w.Take();
    return true;
}

bool SaveSceneToFile(const Scene& scene, const std::string& path, std::string& outError) {
    std::string text;
    SaveSceneToString(scene, text);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        outError = "could not open scene file for writing: " + path;
        return false;
    }
    file << text;
    if (!file) {
        outError = "failed writing scene file: " + path;
        return false;
    }
    return true;
}

bool LoadSceneFromString(const std::string& text, Scene& outScene, std::string& outError) {
    outError.clear();
    Reader reader(text, outError);
    Scene scene;
    std::vector<Token> tokens;

    if (!reader.Next(tokens)) {
        if (!reader.Failed()) outError = "scene file is empty";
        return false;
    }
    if (tokens.size() != 2 || tokens[0].text != "JudasScene" || tokens[0].quoted) {
        return reader.Fail("expected 'JudasScene <version>' on the first line");
    }
    long long version = 0;
    if (!ParseInt(tokens[1], version)) return reader.Fail("scene version must be an integer");
    if (version != kSceneFormatVersion) {
        return reader.Fail("unsupported scene format version " + std::to_string(version) +
                           " (this build reads version " + std::to_string(kSceneFormatVersion) + ")");
    }

    bool settingsSeen = false;
    bool playerStartSeen = false;
    while (reader.Next(tokens)) {
        if (tokens[0].quoted) return reader.Fail("expected 'settings' or 'object'");
        if (tokens[0].text == "settings") {
            if (settingsSeen) return reader.Fail("duplicate settings block");
            if (tokens.size() != 1) return reader.Fail("settings takes no values");
            Block block;
            if (!ReadBlock(reader, block)) return false;
            if (!ParseSettings(reader, block, scene)) return false;
            settingsSeen = true;
            continue;
        }
        if (tokens[0].text == "object") {
            if (!settingsSeen) return reader.Fail("settings block must precede objects");
            const std::vector<Token> header = tokens;
            Block block;
            if (!ReadBlock(reader, block)) return false;
            SceneObject object;
            if (!ParseObject(reader, header, block, object)) return false;
            if (object.playerStart) {
                if (playerStartSeen) return reader.Fail("more than one object has a player-start");
                playerStartSeen = true;
            }
            if (!scene.InsertObject(object)) {
                return reader.Fail("duplicate object id " + std::to_string(object.id));
            }
            continue;
        }
        return reader.Fail("unknown top-level directive '" + tokens[0].text + "'");
    }
    if (reader.Failed()) return false;
    if (!settingsSeen) {
        outError = "scene file has no settings block";
        return false;
    }
    outScene = std::move(scene);
    return true;
}

bool LoadSceneFromFile(const std::string& path, Scene& outScene, std::string& outError) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        outError = "could not open scene file: " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    if (!LoadSceneFromString(buffer.str(), outScene, outError)) {
        outError = path + ": " + outError;
        return false;
    }
    return true;
}
