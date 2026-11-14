#pragma once
#include "Deformable.h"
#include "StructuralMass.h"
#include "deformables/StructuralSkin.h"
#include "render/StructuralSkinState.h"
#include <iostream>
#include <stdexcept>
inline unsigned RunStructuralProofChecks() {
    unsigned checks = 0;
    auto check = [&](bool ok, const char *name) {
        ++checks;
        if (!ok)
            throw std::runtime_error(name);
        std::cout << "PASS " << name << '\n';
    };
    auto a = MakeFractureBlock({2, 1, 1}, {2, 1, 1}, true);
    for (const auto &p : a.fracture->parts) {
        auto m = IntegrateStructuralMass(a, p, 1);
        check(std::abs(m.mass - 1) < 1e-12, "uniform cube mass");
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                check(std::abs(m.inertia[i][j] - (i == j ? 1. / 6 : 0)) < 1e-12,
                      "uniform cube exact inertia tensor");
    }
    auto box = MakeFractureBlock({2, 1, 1}, {4, 3, 4}, true);
    auto m = IntegrateStructuralMass(box, box.fracture->parts[0], 2);
    check(std::abs(m.mass - 48) < 1e-12 && std::abs(m.inertia[0][0] - 100) < 1e-10,
          "rectangular volume analytic tensor");
    for (auto &n : box.nodes) {
        n += glm::dvec3(1e6, -2e6, 3e6);
    }
    auto shifted = IntegrateStructuralMass(box, box.fracture->parts[0], 2);
    check(glm::length(shifted.center - m.center - glm::dvec3(1e6, -2e6, 3e6)) < 1e-8 &&
              glm::length(shifted.inertia[0] - m.inertia[0]) < 1e-8,
          "mass properties translation invariance");
    DeformableAsset simplex;
    simplex.nodes = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    simplex.tetrahedra = {{0, 1, 2, 3}};
    FracturePart part;
    part.tets = {0};
    auto tetra = IntegrateStructuralMass(simplex, part, 6);
    check(std::abs(tetra.mass - 1) < 1e-12 && glm::length(tetra.center - glm::dvec3(.25)) < 1e-12,
          "simplex analytic mass and centroid");
    check(std::abs(tetra.inertia[0][0] - .075) < 1e-12 && std::abs(tetra.inertia[0][1] - .0125) < 1e-12,
          "non-box simplex preserves true off-diagonal inertia");
    auto cube = IntegrateStructuralMass(a, a.fracture->parts[0], 1);
    auto positions = a.nodes, velocities = a.nodes;
    glm::dvec3 offset(100, 50, -25), bulk(3, 5, 7), omega(2, 3, -4);
    for (size_t n = 0; n < positions.size(); ++n) {
        positions[n] += offset;
        velocities[n] = bulk + glm::cross(omega, positions[n] - cube.center - offset);
    }
    auto momentum = IntegrateStructuralAngularMomentum(a, a.fracture->parts[0], 1, positions, velocities,
                                                       cube.center + offset, bulk);
    check(glm::length(momentum - cube.inertia * omega) < 1e-12,
          "affine rigid velocity integrates true volume angular momentum");
    MeshData mesh;
    mesh.vertices.resize(3);
    mesh.vertices[0].position = {-.3f, .1f, 0};
    mesh.vertices[1].position = {.3f, .1f, 0};
    mesh.vertices[2].position = {0, .3f, 0};
    for (auto &v : mesh.vertices)
        v.normal = {0, 0, 1};
    mesh.indices = {0, 1, 2};
    mesh.primitives = {{0, 3, 0}};
    StructuralSkin skin;
    std::string error;
    check(skin.Bind(a, mesh, error), "detailed skin binds independent geometry");
    DeformableInstance simulation;
    simulation.Initialize(std::make_shared<DeformableAsset>(a), {}, glm::dmat4(1), 1);
    MeshData mapped;
    skin.Map(simulation, 1, mapped);
    check(glm::length(mapped.vertices[1].position - mesh.vertices[1].position) < 1e-6f,
          "skin rest shape preserved");
    for (auto &p : simulation.positions) {
        p += glm::dvec3(7, 2, -3);
    }
    simulation.previous = simulation.positions;
    skin.Map(simulation, 1, mapped);
    check(glm::length(mapped.vertices[1].position - mesh.vertices[1].position - glm::vec3(7, 2, -3)) < 1e-5f,
          "skin follows actual material frame");
    simulation.fracture.broken[0] = 1;
    simulation.fracture.Connectivity(*a.fracture);
    for (auto n : a.fracture->parts[1].nodes)
        simulation.positions[n] += glm::dvec3(5, 0, 0);
    simulation.previous = simulation.positions;
    skin.Map(simulation, 1, mapped);
    check(std::abs((mapped.vertices[0].position.x - mapped.vertices[1].position.x) -
                   (mesh.vertices[0].position.x - mesh.vertices[1].position.x)) < 1e-5,
          "torn face cannot stretch across disconnected frames");
    MeshData gpu;
    skin.BuildGpuMesh(simulation, gpu);
    std::vector<glm::mat4> palette;
    skin.Palette(simulation, 1, palette);
    for (size_t i = 0; i < gpu.vertices.size(); ++i) {
        glm::vec4 point(0);
        for (unsigned k = 0; k < 4; ++k)
            point += gpu.skinVertices[i].weights[k] * palette[gpu.skinVertices[i].joints[k]] *
                     glm::vec4(gpu.vertices[i].position, 1);
        check(glm::length(glm::vec3(point) - mapped.vertices[i].position) < 1e-5f,
              "GPU palette agrees with CPU torn skin");
    }
    MeshData resident;skin.BuildResidentGpuMesh(simulation,resident);
    std::vector<int> sections;
    check(skin.UpdateSectionState(simulation,sections),"resident skin publishes initial physical ownership");
    check(!skin.UpdateSectionState(simulation,sections),"unchanged ownership needs no damage publication");
    for(size_t i=0;i<resident.vertices.size();++i){
        auto weights=JudasRender::ResolveStructuralWeights(resident.skinVertices[i],sections);
        glm::vec4 point(0);
        for(unsigned k=0;k<4;++k)point+=weights.weights[k]*palette[weights.joints[k]]*glm::vec4(resident.vertices[i].position,1);
        check(glm::length(glm::vec3(point)-mapped.vertices[i].position)<1e-5f,
              "resident ownership shader arithmetic agrees with actual CPU torn geometry");
    }
    const auto residentIndices=resident.indices;
    const auto residentWeights=resident.skinVertices;
    // A failed redundant bond changes damage revision without changing the
    // connected material frames that determine visible skin weights/indices.
    auto loop = MakeFractureBlock({2,2,1}, {2,2,1}, true);
    StructuralSkin loopSkin;
    check(loopSkin.Bind(loop,mesh,error),"cyclic structural skin binds");
    DeformableInstance loopSimulation;
    loopSimulation.Initialize(std::make_shared<DeformableAsset>(loop),{},glm::dmat4(1),2);
    MeshData loopGpu;loopSkin.BuildGpuMesh(loopSimulation,loopGpu);
    const auto priorSkin=loopGpu.skinVertices;const auto priorIndices=loopGpu.indices;
    const auto priorComponents=loopSimulation.fracture.component;
    loopSimulation.fracture.pending[0]=1;loopSimulation.fracture.Commit(*loop.fracture);
    check(loopSimulation.fracture.broken[0]&&loopSimulation.fracture.component==priorComponents,
          "redundant structural failure preserves connected ownership");
    check(loopSkin.UpdateGpuTopology(loopSimulation,loopGpu),"redundant failure permits topology evaluation");
    bool unchanged=loopGpu.indices==priorIndices&&loopGpu.skinVertices.size()==priorSkin.size();
    for(size_t i=0;unchanged&&i<priorSkin.size();++i)for(unsigned k=0;k<4;++k)
        unchanged&=loopGpu.skinVertices[i].joints[k]==priorSkin[i].joints[k]&&loopGpu.skinVertices[i].joints1[k]==priorSkin[i].joints1[k]&&loopGpu.skinVertices[i].weights[k]==priorSkin[i].weights[k]&&loopGpu.skinVertices[i].weights1[k]==priorSkin[i].weights1[k];
    check(unchanged,"damage revision alone does not change detailed skin topology");
    auto invalid = mesh;
    invalid.indices[0] = 99;
    check(!skin.Bind(a, invalid, error), "invalid skin rejected transactionally");
    skin.Map(simulation, 1, mapped);
    check(mapped.vertices.size() == 3, "rejected rebind retains published skin");
    auto implicit = mesh;
    implicit.primitives.clear();
    StructuralSkin plain;
    check(plain.Bind(a, implicit, error), "indexed skin without explicit material ranges binds");
    plain.Map(simulation, 1, mapped);
    check(mapped.indices.size() == 3, "implicit primitive retains real indexed faces");
    auto badRange = mesh;
    badRange.primitives[0].count = 6;
    check(!plain.Bind(a, badRange, error), "out-of-bounds material range rejects transactionally");
    simulation.fracture.removed.assign(a.fracture->parts.size(), 1);
    skin.Map(simulation, 1, mapped);
    check(mapped.vertices.empty() && mapped.indices.empty(),
          "removed physical owners cannot reappear as nonindexed CPU geometry");
    skin.BuildGpuMesh(simulation, gpu);
    check(gpu.vertices.empty() && gpu.indices.empty() && gpu.skinVertices.empty(),
          "removed physical owners cannot reappear in GPU geometry");
    check(skin.UpdateSectionState(simulation,sections)&&std::all_of(sections.begin(),sections.end(),[](int state){return state<0;}),
          "all removed physical owners publish immediate removal in the resident section table");
    check(resident.indices==residentIndices&&resident.skinVertices.size()==residentWeights.size(),
          "physical removal does not rebuild immutable resident exterior buffers");
    return checks;
}
