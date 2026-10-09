#include "SceneFingerprint.h"
#include "ScriptSystem.h"

#include <array>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <string>

#include "Scene.h"
#include "Prefab.h"
#include "AssetDatabase.h"
#include "SceneSerialization.h"

namespace {

constexpr std::array<std::uint32_t, 64> kSha256Constants{{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
}};

std::uint32_t RotateRight(std::uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32 - bits));
}

class CanonicalWriter {
public:
    void Context(const std::string& context) { m_context = context; }
    void Fail(const std::string& message) {
        if (m_error.empty()) m_error = "authored scene fingerprint: " + m_context + ": " + message;
    }
    void U32(std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) m_bytes.push_back(static_cast<char>((value >> shift) & 0xffu));
    }
    void U64(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) m_bytes.push_back(static_cast<char>((value >> shift) & 0xffu));
    }
    void Integer(int value) {
        static_assert(sizeof(int) == 4, "canonical authored integers require 32 bits");
        U32(static_cast<std::uint32_t>(value));
    }
    void Boolean(bool value) { m_bytes.push_back(value ? '\1' : '\0'); }
    void Text(const std::string& value) { U64(value.size()); m_bytes.append(value); }
    void Number(float value) {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                      "fingerprints require IEEE-754 binary32");
        if (!std::isfinite(value)) Fail("non-finite number");
        if (value == 0.0f) value = 0.0f;
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        U32(bits);
    }
    void Number(double value) {
        static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
                      "fingerprints require IEEE-754 binary64");
        if (!std::isfinite(value)) Fail("non-finite number");
        if (value == 0.0) value = 0.0;
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        U64(bits);
    }
    void Vector(const glm::vec3& value) { Number(value.x); Number(value.y); Number(value.z); }
    void Vector(const glm::dvec3& value) { Number(value.x); Number(value.y); Number(value.z); }
    void Quaternion(const glm::quat& value) {
        Number(value.w); Number(value.x); Number(value.y); Number(value.z);
    }
    template <typename T> void Enum(T value, int last, const char* name) {
        const int tag = static_cast<int>(value);
        if (tag < 0 || tag > last) Fail(std::string("invalid ") + name);
        U32(static_cast<std::uint32_t>(tag));
    }
    const std::string& Bytes() const { return m_bytes; }
    const std::string& Error() const { return m_error; }

private:
    std::string m_bytes;
    std::string m_context;
    std::string m_error;
};

void WriteObject(CanonicalWriter& w, const SceneObject& o) {
    const std::string object = "object " + std::to_string(o.id);
    w.Context(object + " transform");
    w.U64(o.id); w.Text(o.name);
    w.U64(o.parent); w.Text(o.prefabAsset); w.U64(o.prefabRoot); w.U64(o.prefabSource);
    w.U64(o.prefabIds.size());for(const auto& pair:o.prefabIds){w.U64(pair.first);w.U64(pair.second);}
    w.Text(EncodePrefabOverrides(o.prefabOverrides));
    w.Vector(o.transform.position); w.Quaternion(o.transform.rotation); w.Vector(o.transform.scale);
    w.Boolean(o.render.has_value());
    if (o.render) {
        w.Context(object + " render");
        const auto& r = *o.render;
        w.Enum(r.shape, 4, "render shape"); w.Vector(r.halfExtents); w.Number(r.radius);
        w.Vector(r.color); w.Number(r.alpha); w.Vector(r.secondaryColor); w.Number(r.secondaryAlpha);
        w.Text(r.meshAsset); w.Text(r.textureAsset); w.U64(r.textureCamera);
    }
    w.Boolean(o.audioEmitter.has_value());
    if(o.audioEmitter){const auto& a=*o.audioEmitter;w.Context(object+" audio emitter");w.Text(a.asset);w.Boolean(a.enabled);w.Boolean(a.playOnStart);w.Boolean(a.loop);w.Boolean(a.spatial);
        w.Number(a.volume);w.Number(a.pitch);w.Number(a.referenceDistance);w.Number(a.maximumDistance);w.Number(a.rolloff);w.Enum(a.attenuation,2,"audio attenuation");
        if(a.loading!=AudioLoading::Buffered||a.streamPageFrames!=4096||!a.group.empty()||a.doppler!=0||a.send!=0||a.occlusion||a.bypass||a.occlusionMask!=kAllCategories||a.occludedGain!=.25f||a.occludedCutoff!=1200){w.Text("AudioAcoustics.1");w.Enum(a.loading,1,"audio loading");w.U64(a.streamPageFrames);w.Text(a.group);w.Number(a.doppler);w.Number(a.send);w.Boolean(a.occlusion);w.Boolean(a.bypass);w.U64(a.occlusionMask);w.Number(a.occludedGain);w.Number(a.occludedCutoff);}
        if(!ValidAudioSettings(a)||(!a.asset.empty()&&!IsValidAssetId(a.asset)))w.Fail("invalid audio settings or asset reference");
    }
    if(o.audioZone){const auto& z=*o.audioZone;w.Text("AudioZone.1");w.Text(z.asset);w.Boolean(z.enabled);w.Enum(z.shape,1,"audio zone shape");w.Vector(z.halfExtents);w.Number(z.radius);w.Number(z.blendDistance);w.Number(z.amount);w.U64(static_cast<std::uint64_t>(z.priority));}
    w.Boolean(o.audioListener.has_value());
    if(o.audioListener){w.Boolean(o.audioListener->enabled);w.Boolean(o.audioListener->followActiveView);}
    w.Boolean(o.renderCamera.has_value());
    if (o.renderCamera) {
        const auto& c = *o.renderCamera;
        w.Boolean(c.enabled); w.U64(c.width); w.U64(c.height); w.U64(c.updateEveryFrames);
        w.Number(c.verticalFovDegrees); w.Number(c.nearPlane); w.Number(c.farPlane);
        if (c.width < 1 || c.width > 4096 || c.height < 1 || c.height > 4096 ||
            c.updateEveryFrames < 1 || !(c.verticalFovDegrees > 0 && c.verticalFovDegrees < 179) ||
            !(c.nearPlane > 0 && c.farPlane > c.nearPlane)) w.Fail("invalid render camera");
    }
    w.Boolean(o.body.has_value());
    if (o.body) {
        w.Context(object + " body");
        const auto& b = *o.body;
        w.Enum(b.motion, 2, "body motion"); w.Enum(b.shape, 6, "body shape");
        if(b.motion!=SceneBodyMotion::Static&&(b.shape==SceneShape::Terrain||b.shape==SceneShape::TriangleMesh))w.Fail("terrain and concave triangle surfaces must remain static");
        w.Vector(b.halfExtents); w.Number(b.radius); w.Text(b.terrainSurface);
        w.Number(b.mass); w.Number(b.friction); w.Number(b.restitution); w.Vector(b.initialLinearVelocity);
        w.Boolean(b.pickable); w.Boolean(b.managed); w.U64(b.compoundBoxes.size());
        for (const auto& box : b.compoundBoxes) { w.Vector(box.localCenter); w.Vector(box.halfExtents); }
        w.U64(b.fluidCavities.size());
        for (const auto& cavity : b.fluidCavities) {
            w.Vector(cavity.localCenter); w.Vector(cavity.halfExtents);
            if (!(cavity.halfExtents.x > 0.0f) || !(cavity.halfExtents.y > 0.0f) ||
                !(cavity.halfExtents.z > 0.0f)) w.Fail("fluid cavity half extents must be positive");
        }
    }
    w.Boolean(o.gravity.has_value());
    if (o.gravity) {
        w.Context(object + " gravity");
        const auto& g = *o.gravity;
        w.Enum(g.kind, 1, "gravity kind"); w.Number(g.magnitude); w.Enum(g.regionShape, 1, "gravity region");
        if (g.regionShape == SceneRegionShape::Sphere) w.Number(g.regionRadius);
        else w.Vector(g.regionHalfExtents);
    }
    w.Boolean(o.light.has_value());
    if (o.light) {
        w.Context(object + " light");
        const auto& l = *o.light;
        w.Enum(l.kind, 1, "light kind"); w.Vector(l.color); w.Number(l.range);
        w.Number(l.innerConeDegrees); w.Number(l.outerConeDegrees);
    }
    w.Boolean(o.door.has_value());
    if (o.door) {
        w.Context(object + " door");
        const auto& d = *o.door;
        w.Vector(d.localHingeAxis); w.Number(d.openAngleDegrees); w.Number(d.angularSpeedDegreesPerSecond);
    }
    w.Boolean(o.lightSwitch.has_value());
    if (o.lightSwitch) {
        w.Context(object + " light switch");
        const auto& s = *o.lightSwitch;
        w.Vector(s.localHingeAxis); w.Number(s.toggleAngleDegrees); w.Number(s.angularSpeedDegreesPerSecond);
        w.Vector(s.lampLocalOffset); w.Vector(s.lampColor); w.Number(s.lampRange);
    }
    w.Boolean(o.vehicle.has_value());
    if (o.vehicle) {
        w.Context(object + " vehicle");
        const auto& v = *o.vehicle;
        w.Enum(v.gravity, 1, "vehicle gravity"); w.Boolean(v.headlight); w.Boolean(v.navigationLights);
        w.Number(v.dragCoefficient); w.Boolean(v.initialPilotAttached);
    }
    w.Boolean(o.celestial.has_value());
    if (o.celestial) {
        w.Context(object + " celestial");
        w.Number(o.celestial->gravitationalParameter); w.Number(o.celestial->operatorThrustForce);
    }
    w.Boolean(o.atmosphere.has_value());
    if (o.atmosphere) {
        w.Context(object + " atmosphere");
        const auto& a = *o.atmosphere;
        w.Number(a.referenceRadius); w.Number(a.topRadius); w.Number(a.referenceDensity);
        w.Number(a.polytropicExponent); w.Number(a.oxidizerMassFraction); w.Number(a.referenceTemperatureKelvin);
    }
    w.Boolean(o.combustible.has_value());
    if (o.combustible) {
        w.Context(object + " combustible");
        const auto& c = *o.combustible;
        w.Number(c.heatCapacityJPerK); w.Number(c.initialFuelMassKg); w.Number(c.ignitionTemperatureK);
        w.Number(c.maximumFuelRateKgPerSecond); w.Number(c.radiativeAreaSquareMeters);
        w.Number(c.retainedCombustionHeatFraction);
    }
    w.Boolean(o.fluidVolume.has_value());
    if (o.fluidVolume) {
        w.Context(object + " fluid volume");
        const auto& f = *o.fluidVolume;
        w.Number(f.spacing); w.Integer(f.countX); w.Integer(f.countY); w.Integer(f.countZ);
        w.Boolean(f.emitter); w.Vector(f.emitterLocalOffset); w.Integer(f.maxParticles);
    }
    w.Boolean(o.playerStart.has_value());
    if (o.playerStart) {
        w.Context(object + " player start");
        const auto& p = *o.playerStart;
        w.Enum(p.view, 1, "player view"); w.Number(p.yawDegrees);
        w.Number(p.density); w.Number(p.fluidDrag); w.Number(p.swimAcceleration);
        if (!(p.density > 0.0f) || p.fluidDrag < 0.0f || p.swimAcceleration < 0.0f)
            w.Fail("player density must be positive; fluid drag and swim acceleration must be non-negative");
    }
}

}  // namespace

namespace {
// The same SHA-256 block primitive serves canonical strings and bounded file reads.
struct FingerprintDigest {
    std::array<std::uint32_t, 8> hash{{
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    }};
    std::array<unsigned char, 64> pending{};
    size_t used = 0;
    uint64_t length = 0;
    void Compress(const unsigned char* block) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            for (std::size_t byte = 0; byte < 4; ++byte)
                words[i] = (words[i] << 8) | static_cast<unsigned char>(block[4 * i + byte]);
        }
        for (std::size_t i = 16; i < 64; ++i) {
            const auto s0 = RotateRight(words[i - 15], 7) ^ RotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const auto s1 = RotateRight(words[i - 2], 17) ^ RotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }
        auto a = hash[0], b = hash[1], c = hash[2], d = hash[3];
        auto e = hash[4], f = hash[5], g = hash[6], h = hash[7];
        for (std::size_t i = 0; i < 64; ++i) {
            const auto s1 = RotateRight(e, 6) ^ RotateRight(e, 11) ^ RotateRight(e, 25);
            const auto choice = (e & f) ^ (~e & g);
            const auto t1 = h + s1 + choice + kSha256Constants[i] + words[i];
            const auto s0 = RotateRight(a, 2) ^ RotateRight(a, 13) ^ RotateRight(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto t2 = s0 + majority;
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        hash[0] += a; hash[1] += b; hash[2] += c; hash[3] += d;
        hash[4] += e; hash[5] += f; hash[6] += g; hash[7] += h;
    }
    void Append(const char* data, size_t count) {
        length += count;
        while (count) {
            const size_t amount = std::min(count, pending.size() - used);
            std::memcpy(pending.data() + used, data, amount);
            used += amount; data += amount; count -= amount;
            if (used == pending.size()) { Compress(pending.data()); used = 0; }
        }
    }
    std::string Finish() {
        const uint64_t bits = length * 8u;
        pending[used++] = 0x80;
        if (used > 56) { std::fill(pending.begin()+used, pending.end(), 0); Compress(pending.data()); used = 0; }
        std::fill(pending.begin()+used, pending.begin()+56, 0);
        for (int i=0; i<8; ++i) pending[56+i] = static_cast<unsigned char>(bits >> (56-8*i));
        Compress(pending.data());
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (const auto word : hash)
        for (int shift = 28; shift >= 0; shift -= 4) result.push_back(hex[(word >> shift) & 0xfu]);
    return result;
    }
};
} // namespace
std::string SceneFingerprintSha256(std::string_view bytes) {
    FingerprintDigest digest; digest.Append(bytes.data(), bytes.size()); return digest.Finish();
}
bool SceneFingerprintSha256File(const std::string& path, std::string& result,
                               std::string& error, const std::function<bool()>& cancelled) {
    std::ifstream file(path, std::ios::binary);
    if (!file) { error = "Cannot hash asset: " + path; return false; }
    FingerprintDigest digest;
    std::array<char, 65536> page{};
    while (file) {
        if (cancelled && cancelled()) { error = "Asset hash cancelled"; return false; }
        file.read(page.data(), page.size());
        digest.Append(page.data(), static_cast<size_t>(file.gcount()));
    }
    if (!file.eof()) { error = "Cannot finish hashing asset: " + path; return false; }
    result = digest.Finish(); error.clear(); return true;
}

bool ComputeSceneFingerprint(const Scene& scene, std::string& outFingerprint,
                             std::string& outError) {
    outError.clear();
    CanonicalWriter w;
    w.Text("Judas.AuthoredSceneFingerprint");
    w.U32(kSceneFingerprintVersion);
    w.U32(kSceneFormatVersion);
    w.Context("settings");
    const auto& s = scene.Settings();
    w.Text(s.name); w.Vector(s.worldOrigin); w.Vector(s.sunDirection);
    w.Vector(s.sunColor); w.Vector(s.ambientColor); w.Number(s.fluidScale);
    w.Number(s.fluidUpdateRateHz); w.Number(s.fluidHydrostaticDragRate);
    if (!(s.fluidUpdateRateHz > 0.0f) || s.fluidHydrostaticDragRate < 0.0f)
        w.Fail("fluid update rate must be positive and hydrostatic drag non-negative");
    w.Enum(s.fidelityPolicy, 1, "fidelity policy");
    if (s.fidelityPolicy == SceneFidelityPolicy::Distance) {
        w.Number(s.fidelityFullRadius); w.Number(s.fidelityCoarseRadius);
    }
    w.U64(scene.NextId()); w.U64(scene.Objects().size());
    if (scene.NextId() == 0) w.Fail("NextId must be positive");
    std::set<SceneObjectId> ids;
    for (const auto& o : scene.Objects()) {
        w.Context("object " + std::to_string(o.id));
        if (o.id == 0 || !ids.insert(o.id).second) w.Fail("invalid or duplicate stable ID");
        if (o.id >= scene.NextId()) w.Fail("stable ID must precede NextId");
        WriteObject(w, o);
    }
    size_t hiddenCount=0;for(const auto& o:scene.Objects())hiddenCount+=o.render&&!o.render->hiddenParts.empty();if(hiddenCount){w.Text("Judas.ModelParts.1");w.U64(hiddenCount);for(const auto& o:scene.Objects())if(o.render&&!o.render->hiddenParts.empty()){w.U64(o.id);w.U64(o.render->hiddenParts.size());for(auto& key:o.render->hiddenParts)w.Text(key);}}
    size_t materialCount=0;for(const auto& o:scene.Objects())materialCount+=o.render&&!o.render->materials.empty();
    if(s.backgroundColor!=glm::vec3(.08f,.09f,.11f)){w.Text("Judas.Background.1");w.Vector(s.backgroundColor);}
    if(materialCount||s.linearRendering||!s.environmentAsset.empty()){w.Text("Judas.Materials.1");w.Boolean(s.linearRendering);w.Number(s.exposure);w.Text(s.environmentAsset);w.Number(s.environmentIntensity);w.Quaternion(s.environmentRotation);w.Boolean(s.environmentBackground);w.U64(materialCount);for(const auto& o:scene.Objects())if(o.render&&!o.render->materials.empty()){w.U64(o.id);w.Text(EncodeMaterialSlots(o.render->materials));}}
    // Versioned optional M72 records preserve schema-5 bytes for every scene
    // whose visibility, sunlight and material bindings retain their defaults.
    if(!s.sunEnabled||s.sunIntensity!=1){w.Text("Judas.SunControls.1");w.Boolean(s.sunEnabled);w.Number(s.sunIntensity);if(s.sunIntensity<0||s.sunIntensity>10000)w.Fail("sun intensity must be 0..10000");}
    if(!s.appearanceResetState.empty()){auto baseline=s;std::string error;if(s.appearanceResetState.size()>65536||!DecodeAppearanceState(s.appearanceResetState,baseline,error))w.Fail("invalid appearance reset baseline: "+error);w.Text("Judas.AppearanceReset.1");w.Text(s.appearanceResetState);}
    if(!materialCount&&!s.linearRendering&&s.environmentAsset.empty()&&(s.exposure!=1||s.environmentIntensity!=1||s.environmentRotation!=glm::quat(1,0,0,0)||s.environmentBackground)) {
        std::string error;if(!ValidateAppearance(s,error))w.Fail(error);
        w.Text("Judas.AppearanceControls.1");w.Number(s.exposure);w.Number(s.environmentIntensity);w.Quaternion(s.environmentRotation);w.Boolean(s.environmentBackground);
    }
    auto extended=[](const MaterialOverride& v){return v.alpha||v.alphaCutoff||v.normalStrength||v.occlusionStrength||v.doubleSided||std::any_of(v.textures.begin(),v.textures.end(),[](const auto& texture){return texture.has_value();});};
    auto altered=[&](const SceneObject& o){return !o.renderVisible||(o.render&&(!o.render->visible||!MaterialOverrideEmpty(o.render->instanceOverrides)||!o.render->partMaterials.empty()||!o.render->runtimeMaterials.empty()||std::any_of(o.render->materials.begin(),o.render->materials.end(),[&](const auto& slot){return slot.useSource||extended(slot.overrides);})));};
    size_t renderControlCount=std::count_if(scene.Objects().begin(),scene.Objects().end(),altered);
    if(renderControlCount) {
        w.Text("Judas.RenderControls.1");w.U64(renderControlCount);
        auto overrideRecord=[&](const MaterialOverride& value){std::string error;if(!ValidateMaterial(ApplyMaterialOverride(MaterialDefinition{},value),error))w.Fail(error);w.Text(EncodeMaterialOverrides(value));};
        auto bindings=[&](const std::map<std::string,MaterialSlot>& values){if(values.size()>64)w.Fail("render binding limit is 64");w.U64(values.size());for(const auto& [key,slot]:values){if(key.empty()||key.size()>1024||(!slot.asset.empty()&&!IsValidAssetId(slot.asset)))w.Fail("invalid render binding identity/material asset");w.Text(key);w.Text(slot.asset);w.Boolean(slot.useSource);overrideRecord(slot.overrides);}};
        for(const auto& o:scene.Objects())if(altered(o)) {
            w.U64(o.id);w.Boolean(o.renderVisible);w.Boolean(bool(o.render));if(!o.render)continue;
            const auto& r=*o.render;w.Boolean(r.visible);overrideRecord(r.instanceOverrides);bindings(r.partMaterials);bindings(r.runtimeMaterials);
            bool extra=std::any_of(r.materials.begin(),r.materials.end(),[&](const auto& slot){return slot.useSource||extended(slot.overrides);});w.Boolean(extra);
            if(extra){w.U64(r.materials.size());for(const auto& slot:r.materials){w.Boolean(slot.useSource);overrideRecord(slot.overrides);}}
        }
    }
    // Optional tagged extension: old scenes retain identical schema-5 bytes.
    // No pre-M37 baseline could contain this component; new configurations are
    // covered completely without invalidating unrelated existing saves.
    size_t emitterCount=0;for(const auto& o:scene.Objects())if(o.particleEmitter)++emitterCount;
    if(emitterCount){w.Text("Judas.VisualParticleEmitters.1");w.U64(emitterCount);
        for(const auto& o:scene.Objects())if(o.particleEmitter){const auto& e=*o.particleEmitter;w.U64(o.id);w.Context("particle emitter");
            if(!ValidParticleSettings(e))w.Fail("invalid particle emitter settings");
            w.Boolean(e.enabled);w.Boolean(e.loop);w.Boolean(e.localSpace);w.Boolean(e.useGravity);
            w.Number(e.rate);w.Number(e.lifetime);w.Number(e.size);w.Number(e.endSize);w.U32(e.burst);w.U32(e.maxParticles);
            w.Vector(e.spread);w.Vector(e.velocity);w.Vector(e.velocityVariation);w.Vector(e.acceleration);
            w.Vector(glm::vec3(e.color));w.Number(e.color.a);w.Vector(glm::vec3(e.endColor));w.Number(e.endColor.a);
            w.Text(e.textureAsset);w.U32(e.seed);
        }
    }
    size_t uiCount=0;for(const auto& o:scene.Objects())if(o.ui)++uiCount;
    if(uiCount){w.Text("Judas.RuntimeUI.1");w.U64(uiCount);for(const auto& o:scene.Objects())if(o.ui){if(!IsValidAssetId(o.ui->asset)||o.ui->name.empty())w.Fail("invalid UI asset/name");w.U64(o.id);w.Text(o.ui->asset);w.Text(o.ui->name);w.Boolean(o.ui->enabled);}}
    size_t scriptCount=0;for(const auto& o:scene.Objects())scriptCount+=o.scripts.size();
    if(scriptCount){w.Text("Judas.ScriptComponents.1");w.U64(scriptCount);
        for(const auto& o:scene.Objects()){std::set<uint64_t> slots;
            if(o.scripts.size()>64)w.Fail("too many script slots");
            for(const auto& slot:o.scripts){std::string error;
                if(!slot.id||!slots.insert(slot.id).second||!IsValidAssetId(slot.asset)||!ScriptSystem::ValidateJson(slot.properties,error))w.Fail("invalid script slot/properties: "+error);
                w.U64(o.id);w.U64(slot.id);w.Text(slot.asset);w.Boolean(slot.enabled);w.Text(slot.properties);}}
    }
    bool sensors=false;for(const auto& o:scene.Objects())if(o.body&&(o.body->sensor||!o.body->enabled))sensors=true;
    if(sensors){w.Text("Judas.BodySensor.1");for(const auto& o:scene.Objects())if(o.body){w.U64(o.id);w.Boolean(o.body->sensor);w.Boolean(o.body->enabled);}}
    bool classified=s.mainCameraRenderMask!=kAllCategories;
    for(const auto& o:scene.Objects()) classified|=o.tags||o.renderLayer||
        (o.body&&(o.body->collisionLayer||o.body->collisionMask!=kAllCategories))||
        (o.door&&(o.door->collisionLayer||o.door->collisionMask!=kAllCategories))||
        (o.playerStart&&(o.playerStart->collisionLayer||o.playerStart->collisionMask!=kAllCategories))||
        (o.renderCamera&&o.renderCamera->renderMask!=kAllCategories);
    if(classified){w.Text("Judas.Classification.1");w.U64(s.mainCameraRenderMask);
        for(const auto& o:scene.Objects()){
            w.U64(o.id);w.U64(o.tags);w.U32(o.renderLayer);
            if(o.renderLayer>=64)w.Fail("invalid render layer");
            w.Boolean(bool(o.body));if(o.body){w.U32(o.body->collisionLayer);w.U64(o.body->collisionMask);if(o.body->collisionLayer>=64)w.Fail("invalid collision layer");}
            w.Boolean(bool(o.door));if(o.door){w.U32(o.door->collisionLayer);w.U64(o.door->collisionMask);if(o.door->collisionLayer>=64)w.Fail("invalid door layer");}
            w.Boolean(bool(o.playerStart));if(o.playerStart){w.U32(o.playerStart->collisionLayer);w.U64(o.playerStart->collisionMask);if(o.playerStart->collisionLayer>=64)w.Fail("invalid player layer");}
            w.Boolean(bool(o.renderCamera));if(o.renderCamera)w.U64(o.renderCamera->renderMask);
        }
    }
    size_t deformables=0;for(const auto& o:scene.Objects())deformables+=o.deformable.has_value();
    if(deformables){w.Text("Judas.Deformable.1");w.U64(deformables);for(const auto& o:scene.Objects())if(o.deformable){w.U64(o.id);auto properties=DeformableProperties(o);w.U64(properties.size());for(auto [k,v]:properties){w.Text(k);w.Text(v);}}}
    size_t liquid=0;for(const auto& o:scene.Objects())liquid+=!LiquidProperties(o).empty();
    if(liquid){w.Text("Judas.ConservedLiquid.1");w.U64(liquid);for(const auto& o:scene.Objects()){auto properties=LiquidProperties(o);if(properties.empty())continue;std::string error;if(!ValidateLiquidComponents(o,error))w.Fail(error);w.U64(o.id);w.U64(properties.size());for(auto [k,v]:properties){w.Text(k);w.Text(v);}}}
    size_t navigation=0;for(const auto& o:scene.Objects())navigation+=!NavigationProperties(o).empty();
    if(navigation){w.Text("Judas.Navigation.1");w.U64(navigation);for(const auto& o:scene.Objects()){auto properties=NavigationProperties(o);if(properties.empty())continue;w.U64(o.id);w.U64(properties.size());for(auto [k,v]:properties){w.Text(k);w.Text(v);}}}
    size_t selectedGravity=0;for(const auto& o:scene.Objects())selectedGravity+=o.gravitySelection.has_value();
    if(selectedGravity){w.Text("Judas.GravitySelection.1");w.U64(selectedGravity);
        for(const auto& o:scene.Objects())if(o.gravitySelection){std::string error;
            if(!ValidGravitySelection(*o.gravitySelection,error))w.Fail(error);
            w.U64(o.id);w.U32(unsigned(o.gravitySelection->mode));w.U64(o.gravitySelection->source);w.Vector(o.gravitySelection->acceleration);}}
    size_t motors=0;for(const auto& o:scene.Objects())motors+=o.characterMotor.has_value();
    if(motors){w.Text("Judas.CharacterMotor.1");w.U64(motors);for(const auto& o:scene.Objects())if(o.characterMotor){const auto& m=*o.characterMotor;
        std::string error;if(!ValidCharacterMotor(m,error))w.Fail(error);if(o.body||(o.ragdoll&&!o.ragdoll->physicalAnimation))w.Fail("character motor cannot also own a root body/legacy ragdoll; an explicit physical-animation authority policy is required");
        if(o.transform.scale!=glm::vec3(1))w.Fail("character motor dimensions are in simulation metres; entity scale must be one");
        w.U64(o.id);
        w.Boolean(m.enabled);
        w.Number(m.radius);
        w.Number(m.halfHeight);
        w.Vector(m.offset);
        w.Number(m.stepHeight);
        w.Number(m.supportDistance);
        w.Number(m.skin);
        w.Number(m.maxSlopeDegrees);
        w.Number(m.gravityScale);
        w.Number(m.reorientationDegreesPerSecond);
        w.Number(m.interactionMass);
        w.Number(m.maxPushImpulse);
        w.U32(m.collisionLayer);w.U64(m.collisionMask);w.U64(m.requiredTags);w.U64(m.excludedTags);
    }}
    size_t animations=0;for(const auto& o:scene.Objects())if(o.animation)++animations;
    if(animations){w.Text("Judas.SkeletalPlayback.1");w.U64(animations);for(const auto& o:scene.Objects())if(o.animation){const auto& a=*o.animation;
        if(!o.render||o.render->shape!=SceneShape::Mesh||!std::isfinite(a.speed)||!std::isfinite(a.time)||a.time<0)w.Fail("invalid animation component");
        w.U64(o.id);w.Boolean(a.enabled);w.Boolean(a.playOnStart);w.Boolean(a.loop);w.Text(a.clip);w.Number(a.speed);w.Number(a.time);
    }}
    for(const auto& o:scene.Objects()){
        if(o.socket){const auto& k=*o.socket;w.Text("Judas.Socket.1");w.U64(o.id);w.U64(k.target);w.Text(k.joint);w.Boolean(k.enabled);w.Vector(k.offset.position);w.Quaternion(k.offset.rotation);w.Vector(k.offset.scale);}
        if(o.animation&&!o.animation->limbs.empty()){w.Text("Judas.LimbIK.1");w.U64(o.id);w.U64(o.animation->limbs.size());for(const auto& k:o.animation->limbs){w.Text(k.id);w.Text(k.root);w.Text(k.middle);w.Text(k.end);w.Vector(k.target);w.Vector(k.pole);w.Number(k.weight);w.Boolean(k.enabled);w.Integer(k.order);}}
    }
    // Optional extensions preserve canonical schema-5 fingerprints for older content.
    for(const auto& o:scene.Objects()){
        if(o.animation&&o.animation->fullBodyIK){w.Text("Judas.FullBodyIK.1");w.U64(o.id);w.Text(SerializeFullBodyIKSettings(*o.animation->fullBodyIK));}
        if(o.ragdoll&&o.ragdoll->physicalAnimation){w.Text("Judas.PhysicalAnimation.1");w.U64(o.id);w.Text(SerializePhysicalAnimationSettings(*o.ragdoll->physicalAnimation));}
        if(o.ragdoll&&o.ragdoll->receiveContactEvents){w.Text("Judas.RagdollContactEvents.1");w.U64(o.id);w.Boolean(true);}
    }
    bool layered=false;for(const auto& o:scene.Objects())layered|=o.animation&&!o.animation->layers.empty();
    size_t ragdolls=0;for(const auto& o:scene.Objects())ragdolls+=o.ragdoll.has_value();
    if(ragdolls){w.Text("Judas.Ragdoll.1");w.U64(ragdolls);for(const auto& o:scene.Objects())if(o.ragdoll){const auto& r=*o.ragdoll;std::string error;if(!ValidRagdollDefinition(r,error))w.Fail(error);if(!o.animation||!o.render||o.body)w.Fail("ragdoll needs an animated renderable without root collider");
        w.U64(o.id);w.Boolean(r.enabled);w.Boolean(r.playOnStart);w.Boolean(r.selfCollision);w.U64(r.bones.size());
        for(const auto& b:r.bones){w.Text(b.joint);w.Text(b.parent);w.Enum(b.shape,1,"ragdoll shape");w.Vector(b.offset);w.Quaternion(b.orientation);w.Vector(b.halfExtents);w.Number(b.radius);w.Number(b.mass);w.Number(b.friction);w.Number(b.restitution);w.U32(b.collisionLayer);w.U64(b.collisionMask);w.Boolean(b.suppressParentCollision);w.Boolean(b.autoAnchors);const auto& c=b.constraint;w.Enum(c.type,3,"ragdoll constraint");w.Boolean(c.enabled);w.Vector(c.anchorA);w.Vector(c.anchorB);w.Quaternion(c.frameA);w.Quaternion(c.frameB);w.Boolean(c.limits);w.Number(c.lower);w.Number(c.upper);}
    }}
    if(layered){w.Text("Judas.PoseLayers.1");for(const auto& o:scene.Objects())if(o.animation){std::string error;if(!ValidAnimationLayers(o.animation->layers,error))w.Fail(error);w.U64(o.id);w.U64(o.animation->layers.size());for(const auto& l:o.animation->layers){w.Text(l.id);w.Text(l.clip);w.Boolean(l.enabled);w.Boolean(l.additive);w.Number(l.weight);w.Number(l.speed);w.Number(l.time);w.Text(l.referenceClip);w.Number(l.referenceTime);w.U64(l.mask.size());for(const auto& key:l.mask)w.Text(key);}}}
    size_t joints=0;for(const auto& o:scene.Objects())if(o.joint)++joints;
    if(joints){w.Text("Judas.RigidJoints.1");w.U64(joints);for(const auto& o:scene.Objects())if(o.joint){const auto& j=*o.joint;const auto& s=j.settings;
        if(!ValidJointSettings(s))w.Fail("invalid joint settings");
        const auto* a=scene.Find(j.bodyA);const auto* b=scene.Find(j.bodyB);
        if(!a||!a->body||(j.bodyB&&(!b||!b->body))||j.bodyA==j.bodyB||(a&&a->body&&a->body->motion!=SceneBodyMotion::Dynamic&&(!b||!b->body||b->body->motion!=SceneBodyMotion::Dynamic)))w.Fail("invalid joint body reference");
        w.U64(o.id);w.U64(j.bodyA);w.U64(j.bodyB);w.U32(int(s.type));w.Boolean(s.enabled);w.Vector(s.anchorA);w.Vector(s.anchorB);
        w.Quaternion(s.frameA);w.Quaternion(s.frameB);w.Boolean(s.limits);w.Boolean(s.motor);w.Boolean(s.spring);
        w.Number(s.lower);w.Number(s.upper);w.Number(s.speed);w.Number(s.maxForce);w.Number(s.rest);w.Number(s.stiffness);w.Number(s.damping);
    }}
    // Optional extension leaves every legacy zero-resistance fingerprint intact.
    for(const auto& o:scene.Objects()){
        if(o.joint&&o.joint->settings.rotationalResistance){w.Text("Judas.JointResistance.1");w.U64(o.id);w.Number(o.joint->settings.rotationalResistance);}
        if(o.ragdoll)for(const auto& bone:o.ragdoll->bones)if(bone.constraint.rotationalResistance){w.Text("Judas.BoneResistance.1");w.U64(o.id);w.Text(bone.joint);w.Number(bone.constraint.rotationalResistance);}
    }
    for(const auto& o:scene.Objects())if(o.body&&!o.body->physicalMaterial.empty()){w.Text("Judas.PhysicalMaterial.1");w.U64(o.id);w.Text(o.body->physicalMaterial);}
    for(const auto& o:scene.Objects())if(o.body&&o.body->physicalMaterialOverride){w.Text("Judas.PhysicalMaterialOverride.1");w.U64(o.id);w.U64(1);}
    // Optional authored angular motion does not rewrite legacy fingerprints.
    for(const auto& o:scene.Objects())if(o.body&&o.body->initialAngularVelocity!=glm::vec3(0)){w.Text("Judas.BodyAngularVelocity.1");w.U64(o.id);w.Vector(o.body->initialAngularVelocity);}
    bool collisionExtension=false;for(const auto& o:scene.Objects())if(o.body){const auto& b=*o.body;collisionExtension|=!b.collisionAsset.empty()||b.shape==SceneShape::ConvexHull||b.shape==SceneShape::TriangleMesh;for(auto& c:b.compoundBoxes)collisionExtension|=c.rotation!=glm::quat(1,0,0,0)||c.type!=ShapeType::Box||!c.assetId.empty()||c.key;}
    if(collisionExtension){w.Text("Judas.CookedCollision.1");for(const auto& o:scene.Objects())if(o.body){const auto& b=*o.body;w.U64(o.id);w.Text(b.collisionAsset);w.U64(b.compoundBoxes.size());for(const auto& c:b.compoundBoxes){w.Quaternion(c.rotation);w.Enum(c.type,5,"compound child type");w.Number(c.radius);w.Text(c.assetId);w.U32(c.key);if(c.type!=ShapeType::Box&&c.type!=ShapeType::Sphere&&c.type!=ShapeType::ConvexHull)w.Fail("unsupported compound child");if(c.type==ShapeType::Sphere&&!(c.radius>0))w.Fail("compound sphere radius must be positive");if(c.type==ShapeType::ConvexHull&&c.assetId.empty())w.Fail("compound hull requires cooked asset");}if((b.shape==SceneShape::ConvexHull||b.shape==SceneShape::TriangleMesh||!b.collisionAsset.empty()||std::any_of(b.compoundBoxes.begin(),b.compoundBoxes.end(),[](const CompoundBox& c){return c.rotation!=glm::quat(1,0,0,0)||c.type!=ShapeType::Box;}))&&o.transform.scale!=glm::vec3(1))w.Fail("cooked collision instance requires unit scale; bake source scale");if(b.shape==SceneShape::TriangleMesh&&(b.motion!=SceneBodyMotion::Static||b.sensor))w.Fail("concave triangle surface cannot be dynamic or volume sensor");}}
    if (!w.Error().empty()) { outError = w.Error(); return false; }
    outFingerprint = SceneFingerprintSha256(w.Bytes());
    return true;
}
