#include "CrashRig.h"
#include "StructuralMass.h"
#include <algorithm>
#include <stdexcept>

DeformableAsset MakeCrashStructure(unsigned longitudinal, unsigned layers, unsigned columns) {
    auto a = MakeFractureBlock({columns, layers, longitudinal}, {1.9, layers == 2 ? 1.2 : .7, 4.7}, true);
    if (layers == 2) {
        // An authored car profile, not a solid rectangular block enclosing air
        // above its bonnet/boot. Retain a real upper cabin/roof structure.
        auto source = std::move(a);
        a = DeformableAsset{};
        a.kind = DeformableKind::Solid;
        a.fracture = std::make_shared<FractureCook>();
        a.fracture->rigid = true;
        std::vector<unsigned> nodes(source.nodes.size(), ~0u), parts(source.fracture->parts.size(), ~0u);
        for (unsigned old = 0; old < source.fracture->parts.size(); ++old) {
            const auto &original = source.fracture->parts[old];
            if (original.proxyCenter.y > 0 && std::abs(original.proxyCenter.z) > 1.2)
                continue;
            auto part = original;
            part.nodes.clear();
            part.tets.clear();
            parts[old] = unsigned(a.fracture->parts.size());
            for (auto n : original.nodes) {
                nodes[n] = unsigned(a.nodes.size());
                part.nodes.push_back(nodes[n]);
                a.nodes.push_back(source.nodes[n]);
            }
            for (auto t : original.tets) {
                part.tets.push_back(unsigned(a.tetrahedra.size()));
                auto tet = source.tetrahedra[t];
                for (unsigned k = 0; k < 4; ++k)
                    tet[k] = nodes[tet[k]];
                a.tetrahedra.push_back(tet);
            }
            a.fracture->parts.push_back(std::move(part));
        }
        for (auto bond : source.fracture->bonds) {
            if (parts[bond.a] == ~0u || parts[bond.b] == ~0u)
                continue;
            bond.a = parts[bond.a];
            bond.b = parts[bond.b];
            for (auto &pair : bond.pairs) {
                pair.x = nodes[pair.x];
                pair.y = nodes[pair.y];
            }
            a.fracture->bonds.push_back(std::move(bond));
        }
        for (const auto &[name, group] : source.groups)
            for (auto n : group)
                if (nodes[n] != ~0u)
                    a.groups[name].push_back(nodes[n]);
    }
    a.fracture->structural = true;
    a.fracture->filterNeighbours = true;
    for (const auto &part : a.fracture->parts)
        a.groups[part.key] = part.nodes;
    for (auto &bond : a.fracture->bonds) {
        auto &m = bond.material;
        m.enabled = true;
        // Effective crush-frame calibration: stiff elastic response, finite
        // plastic capacity. Scale by actual interface area when refining cells.
        const unsigned axis = std::abs(bond.normal.x) > .5 ? 0 : std::abs(bond.normal.y) > .5 ? 1 : 2;
        const double referenceArea = axis == 0   ? .7 * (4.7 / 8)
                                     : axis == 2 ? .7 * (1.9 / 3)
                                                 : (1.9 / 3) * (4.7 / 8);
        const float areaScale = float(bond.area / referenceArea);
        const auto &left = a.fracture->parts[bond.a];
        const auto &right = a.fracture->parts[bond.b];
        const double length = std::abs(left.proxyCenter[axis] - right.proxyCenter[axis]);
        const double referenceLength = axis == 0 ? .95 : axis == 1 ? .6 : 4.7 / 8;
        const float lengthScale = float(referenceLength / length);
        m.stiffness = 3500000 * areaScale * lengthScale;
        m.angularStiffness = 180000 * areaScale * lengthScale;
        m.damping = 1100 * areaScale;
        m.angularDamping = 140 * areaScale;
        m.yieldForce = 35000 * areaScale;
        m.yieldTorque = 3500 * areaScale;
        m.hardeningForce = 150000 * areaScale * lengthScale;
        m.hardeningTorque = 500 * areaScale;
        m.maximumTranslation = .35f / lengthScale;
        m.maximumAngle = 1.0f;
    }
    std::string error;
    if (!PrepareDeformableAsset(a, error))
        throw std::runtime_error(error);
    return a;
}
CrashRig AddCrashRig(Scene &scene, const DeformableAsset &asset, const std::string &assetId,
                     glm::vec3 position, const MeshData &exterior, const std::array<CrashWheelMount,4>* mounts, bool panelSeams) {
    auto &object = scene.CreateObject("Deforming vehicle");
    const auto owner = object.id;
    object.transform.position = position;
    object.deformable = DeformableSettings{};
    object.deformable->asset = assetId;
    object.deformable->substeps = 2;
    object.deformable->iterations = 4;
    object.deformable->selfContact = false;
    double volume = 0;
    for (const auto &tet : asset.solids)
        volume += tet.volume;
    object.deformable->material.density = 1500 / volume;
    object.deformable->material.friction = .5;
    CrashRig rig;
    rig.owner = owner;
    std::string error;
    if (!rig.skin.Bind(asset, exterior, error, panelSeams))
        throw std::runtime_error(error);
    auto nearest = [&](glm::dvec3 p) {
        unsigned part = 0;
        double best = 1e100;
        for (unsigned i = 0; i < asset.fracture->parts.size(); ++i) {
            auto d = asset.fracture->parts[i].proxyCenter - p;
            double value = glm::dot(d, d);
            if (value < best) {
                part = i;
                best = value;
            }
        }
        return part;
    };
    rig.referencePart = nearest({0, 0, .25});
    unsigned index = 0;
    for (double z : {-1.48, 1.48})
        for (double x : {-.94, .94}) {
            glm::dvec3 mount{x, -.12, z};
            if(mounts){
                // Author nominal loaded ride height, not an unrelated mesh offset.
                // Each spring supports one quarter of 1500 kg at uniform gravity.
                const double loadedLength=.30-1500.*9.81/(4*35000.);
                const auto& m=mounts->at(index);
                mount=glm::dvec3(m.center)+glm::dvec3(m.axleFrame*glm::vec3(0,float(loadedLength),0));
            }
            auto part = nearest(mount);
            auto &wheel = scene.CreateObject("Physical wheel");
            rig.wheels[index++] = wheel.id;
            wheel.wheel = WheelSettings{};
            auto &s = *wheel.wheel;
            s.chassis = owner;
            s.part = asset.fracture->parts[part].key;
            s.mount = glm::vec3(mount - asset.fracture->parts[part].proxyCenter);
            if(mounts)s.orientation=mounts->at(index-1).axleFrame;
            s.radius = mounts ? mounts->at(index-1).radius : .35f;
            s.width = mounts ? mounts->at(index-1).width : .25f;
            s.restLength = .30f;
            s.maxLength = .50f;
            s.minLength = .04f;
            s.springStiffness = 35000;
            s.damping = 3000;
        }
    const auto sourcePart = nearest({-.94, -.12, 1.48});
    auto &train = scene.Find(owner)->powertrain.emplace();
    PowertrainShaft source;
    source.name = "engine";
    source.inertia = .35;
    source.bearing = rig.wheels[2];
    train.shafts.push_back(source);
    PowertrainSource motor;
    motor.name = "motor";
    motor.kind = PowertrainSourceKind::Electric;
    motor.maxTorque = 350;
    motor.maxPower = 100000;
    train.sources.push_back(motor);
    for (unsigned i = 0; i < rig.wheels.size(); ++i) {
        PowertrainShaft shaft;
        shaft.name = "wheel-" + std::to_string(i);
        shaft.kind = PowertrainShaftKind::Wheel;
        shaft.target = rig.wheels[i];
        train.shafts.push_back(shaft);
        PowertrainLink gear;
        gear.name = "gear-" + std::to_string(i);
        gear.input = 0;
        gear.output = i + 1;
        gear.ratio = .2;
        gear.connection = {
            owner, asset.fracture->parts[nearest({i % 2 ? .94 : -.94, -.12, i < 2 ? -1.48 : 1.48})].key,
            asset.fracture->parts[sourcePart].key, true};
        train.links.push_back(gear);
        PowertrainBrake brake;
        brake.name = "brake-" + std::to_string(i);
        brake.shaft = i + 1;
        brake.maxTorque = 1600;
        train.brakes.push_back(brake);
    }
    return rig;
}
