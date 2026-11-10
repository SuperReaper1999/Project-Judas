#pragma once
#include "PowertrainAuthoring.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"
#include <iostream>
#include <stdexcept>
inline unsigned RunPowertrainCarrierChecks() {
    unsigned checks = 0;
    auto check = [&](bool value, const char *label) {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    auto scene = [](bool carrier) {
        Scene s;
        auto &body = s.CreateObject("Motor housing");
        body.body = SceneBodyComponent{};
        body.body->motion = SceneBodyMotion::Dynamic;
        body.body->mass = 50;
        body.transform.rotation = glm::angleAxis(.6f, glm::vec3(0, 1, 0));
        body.powertrain = PowertrainSettings{};
        PowertrainShaft shaft;
        shaft.name = "rotor";
        shaft.bearing = carrier ? 2 : 1;
        body.powertrain->shafts.push_back(shaft);
        PowertrainSource source;
        source.name = "motor";
        source.maxTorque = 20;
        body.powertrain->sources.push_back(source);
        auto &wheel = s.CreateObject("Physical carrier reference");
        wheel.wheel = WheelSettings{};
        wheel.wheel->chassis = 1;
        return s;
    };
    auto direct = scene(false), carrier = scene(true);
    std::string error;
    check(ValidateScenePowertrains(carrier, error), "motor bearing accepts a wheel's physical carrier");
    RuntimeWorld a, b;
    check(a.Build(direct, nullptr, error) && b.Build(carrier, nullptr, error),
          "direct and carrier bearing fixtures build");
    for (auto *world : {&a, &b}) {
        auto *train = world->RuntimePowertrain(1);
        train->command.sources[0].customTorque = 10;
        world->UpdatePowertrains(.02f);
    }
    auto wa = a.Physics().GetAngularVelocity(a.RuntimeBody(1)),
         wb = b.Physics().GetAngularVelocity(b.RuntimeBody(1));
    check(glm::length(wa) > 1e-5f && glm::length(wa - wb) < 1e-6f,
          "carrier resolves the same live torque reaction as the direct rigid bearing");
    check(std::abs(a.RuntimePowertrain(1)->absoluteSpin[0] - b.RuntimePowertrain(1)->absoluteSpin[0]) < 1e-9,
          "carrier preserves actual rotor momentum evolution");
    auto *train = b.RuntimePowertrain(1);
    auto axis = b.Physics().GetTransform(b.RuntimeBody(1)).rotation * train->settings.shafts[0].axis;
    auto total = glm::dmat3(b.Physics().GetInertiaWorld(b.RuntimeBody(1))) * glm::dvec3(wb) +
                 glm::dvec3(axis) * train->settings.shafts[0].inertia * train->absoluteSpin[0];
    check(glm::length(total) < 1e-6, "motor impulse balances rotor and physical carrier angular momentum");
    auto encoded = EncodePowertrainSettings(train->settings);
    PowertrainSettings restored;
    check(DecodePowertrainSettings(encoded, restored, error) && restored.shafts[0].bearing == 2,
          "carrier reference survives normal authoring encoding");
    b.Physics().SetBodyEnabled(b.RuntimeBody(1), false);
    b.PublishPowertrains(0);
    check(!b.RuntimePowertrain(1)->state.shafts[0].available,
          "unavailable carrier disables its source instead of aliasing world space");
    auto aliased = scene(true);
    aliased.Find(1)->powertrain->shafts[0].kind = PowertrainShaftKind::Body;
    aliased.Find(1)->powertrain->shafts[0].target = 1;
    check(!ValidateScenePowertrains(aliased, error),
          "carrier alias cannot bypass the same-body rotor/bearing rejection");
    return checks;
}
