#pragma once

#include "FluidHydrostatics.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>

// The original moving-pool inputs, frozen before forces are applied. This
// regression tests integration/continuity, not a fitted physical water surface.
inline void FluidBandIntegrationChecks() {
    using Json = nlohmann::json;
    const auto path = std::filesystem::path(__FILE__).parent_path() /
                      "fixtures/fluid/support-band-334.json";
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Missing frozen fluid support fixture: " + path.string());
    Json input;
    file >> input;
    const auto vector = [](const Json& j) {
        return glm::vec3(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>());
    };
    const auto rotation = [](const Json& j) {
        return glm::quat(j.at(0).get<float>(), j.at(1).get<float>(),
                         j.at(2).get<float>(), j.at(3).get<float>());
    };
    const auto identity = [](const Json& j) {
        BodyHandle owner;
        owner.id = j.at(0).get<unsigned>();
        owner.world = j.at(1).get<decltype(owner.world)>();
        return owner;
    };
    const auto transform = [&](const Json& j) {
        BodyTransform pose;
        pose.position = vector(j.at("position"));
        pose.rotation = rotation(j.at("rotation"));
        return pose;
    };
    std::vector<FluidParticle> particles;
    bool exactSpacing = true;
    for (const auto& j : input.at("particles")) {
        FluidParticle p;
        p.position = vector(j.at("position"));
        p.previousPosition = p.position;
        p.velocity = vector(j.at("velocity"));
        p.acceleration = vector(j.at("acceleration"));
        p.mass = j.at("mass").get<float>();
        // The captured native field stored spacing/volume. In this fixture the
        // ordinary Build inputs reproduce both exactly; don't hide conversion.
        exactSpacing &= std::cbrt(p.mass / input.at("density").get<float>()) ==
                        j.at("spacing").get<float>();
        particles.push_back(p);
    }
    Check(exactSpacing, "frozen support replay preserves native particle spacing");
    std::vector<CompoundBox> boxes;
    for (const auto& j : input.at("solidBoxes")) {
        CompoundBox box;
        box.localCenter = vector(j.at("center"));
        box.halfExtents = vector(j.at("half"));
        box.rotation = rotation(j.at("rotation"));
        boxes.push_back(box);
    }
    const auto volume = MakeCompoundFluidVolume(boxes);
    const auto body = transform(input.at("solidPose"));
    const auto owner = identity(input.at("solidOwner"));
    FluidHydrostaticField field;
    field.Build(particles, input.at("density").get<float>());
    for (const auto& j : input.at("boundaries")) {
        Check(!j.at("domainAvailable").get<bool>(), "native replay does not invent a pre-cut domain");
        Check(identity(j.at("owner")).id == owner.id && identity(j.at("owner")).world == owner.world,
              "frozen support replay retains the captured owner");
        std::vector<FluidDisplacementMarker> cells;
        for (const auto& c : j.at("cells")) {
            FluidDisplacementMarker cell;
            cell.owner = identity(c.at("owner"));
            cell.position = vector(c.at("position"));
            cell.rotation = rotation(c.at("rotation"));
            cell.halfWidth = c.at("halfWidth").get<float>();
            cell.materialPoint = vector(c.at("materialPoint"));
            cell.contactPoint = vector(c.at("contactPoint"));
            cells.push_back(cell);
        }
        field.RetainDisplacementCells(owner, volume, transform(j.at("pose")), cells);
    }
    const auto point = vector(input.at("point")), up = vector(input.at("up"));
    const auto tangent = vector(input.at("tangent"));
    const float half = input.at("halfHeight").get<float>();
    const float radius = input.at("radius").get<float>();
    const FluidSolidQuery solid{volume, body, owner};
    const auto fraction = field.Query(point, up, half, radius, tangent, &solid, true).fraction;
    // Independent numerical integration of the zero-height admission function.
    // It does not share the finite-height partition or its midpoint decisions.
    // This is a numerical contract, not continuum buoyancy certification.
    double integrated = 0;
    constexpr int slices = 2048;
    for (int i = 0; i < slices; ++i) {
        const auto where = point + up * float(-half + (i + .5) * 2 * half / slices);
        integrated += field.Query(where, up, 0, radius, tangent, &solid, true).fraction;
    }
    integrated /= slices;
    Near(fraction, integrated, .001, "finite support agrees with height-resolved admission");
    // No finite support should disappear just because its midpoint crosses the
    // solid. Before the fix this frozen +/-0.1mm pair jumps by .2513.
    float bracket[2];
    for (int i = 0; i < 2; ++i) {
        const auto offset = up * (i ? .0001f : -.0001f);
        auto shifted = body;
        shifted.position += offset;
        const FluidSolidQuery moving{volume, shifted, owner};
        bracket[i] = field.Query(point + offset, up, half, radius, tangent, &moving, true).fraction;
    }
    Near(bracket[0], bracket[1], .001, "tiny motion does not switch off an entire support band");
    Check(fraction > .5 && fraction < 1, "frozen support stays genuinely partial");
    field.Build({}, 1000);
    Near(field.Query(point, up, half, radius, tangent, &solid, true).fraction, 0, 0,
         "empty material cannot be made wet by support integration");
}
