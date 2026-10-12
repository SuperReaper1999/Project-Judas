#!/usr/bin/env python3
"""Add failure-only read observations to an owned PhysicalBalance source copy.

This is diagnostic tooling, not an implementation repair. It leaves every
physical expression, branch predicate, clock, input and acceptance bound intact.
Run from the repository root; supply a distinct output directory under .cache
or /tmp. JUDAS_M78_BALANCE_FAILURE_LOG selects the JSONL receipt at runtime.
"""
from pathlib import Path
import argparse
import hashlib
import json

parser = argparse.ArgumentParser()
parser.add_argument("--input", default="src/PhysicalBalance.cpp")
parser.add_argument("--output", required=True)
args = parser.parse_args()
root = Path.cwd().resolve()
source = Path(args.input).resolve()
out = Path(args.output).resolve()
assert out != root / "src" and (
    out.is_relative_to(root / ".cache") or out.is_relative_to(Path("/tmp"))
), "Only an owned diagnostic copy under .cache or /tmp is permitted"
out.mkdir(parents=True, exist_ok=False)
text = source.read_text()
text = "#include <cstdlib>\n#include <fstream>\n" + text
helper = r'''
namespace {
using BF=nlohmann::ordered_json;
BF BfVector(glm::vec3 v){return BF::array({v.x,v.y,v.z});}
BF BfQuaternion(glm::quat q){return BF::array({q.w,q.x,q.y,q.z});}
BF BfPose(BodyTransform p){return {{"position",BfVector(p.position)},{"rotation",BfQuaternion(p.rotation)}};}
void BfWrite(const char* reason,const BalanceHistory& h,const BalanceObservation& o,
             const BalanceSettings& s,const std::vector<BalanceBody>& bodies,
             PhysicsWorld& world,BF detail){
    const char* path=std::getenv("JUDAS_M78_BALANCE_FAILURE_LOG");if(!path)return;
    BF record={{"reason",reason},{"elapsed",h.elapsed},{"referenceTime",h.referenceTime},
        {"swing",h.swing},{"lastSwing",h.lastSwing},{"swingTime",h.swingTime},
        {"stepSeconds",s.stepSeconds},{"transferTime",h.transferTime},{"landings",h.landings},
        {"lifted",h.lifted},{"available",o.available},{"com",BfVector(o.com)},
        {"velocity",BfVector(o.velocity)},{"up",BfVector(o.up)},
        {"upright",o.upright},{"height",o.height},{"speed",o.speed},
        {"plannedSupport",h.plannedSupport.id},{"stanceSupport",h.stanceSupport.id},
        {"frameSupport",h.frameSupport.id},{"startLocal",BfVector(h.startLocal)},
        {"landingLocal",BfVector(h.landingLocal)},{"landedLocal",BfVector(h.landedLocal)},
        {"stanceLocal",BfVector(h.stanceLocal)},{"rootStartLocal",BfVector(h.rootStartLocal)},
        {"comStartLocal",BfVector(h.comStartLocal)},{"detail",std::move(detail)},
        {"supports",BF::array()},{"physicalBodies",BF::array()}};
    for(const auto& support:o.supports)record["supports"].push_back({
        {"body",support.body.id},{"contacts",support.contacts},{"load",support.load},
        {"point",BfVector(support.point)},{"normal",BfVector(support.normal)},
        {"velocity",BfVector(support.velocity)}});
    for(const auto& body:bodies)record["physicalBodies"].push_back({
        {"body",body.body.id},{"enabled",world.IsBodyEnabled(body.body)},
        {"pose",BfPose(world.GetTransform(body.body))},
        {"linearVelocity",BfVector(world.GetLinearVelocity(body.body))},
        {"angularVelocity",BfVector(world.GetAngularVelocity(body.body))}});
    std::ofstream file(path,std::ios::app);if(file)file<<record.dump()<<'\n';
}
}
'''
needle = "bool PhysicalBalanceController::SwingReferences(PhysicsWorld& world,const glm::quat& upright,std::vector<glm::quat>& desired){"
assert text.count(needle) == 1
text = text.replace(needle, helper + "\n" + needle)
needle = '    JUDAS_PROFILE_SCOPE("Physical balance step references");'
text = text.replace(needle, needle + r'''
    auto diagnosticFailure=[&](const char* reason,BF detail=BF::object()){
        BfWrite(reason,m_history,m_observation,m_settings,m_bodies,world,std::move(detail));return false;
    };
''', 1)
replacements = {
    "if(!world.IsBodyEnabled(m_history.stanceSupport)||!world.IsBodyEnabled(m_history.plannedSupport))return false;":
    'if(!world.IsBodyEnabled(m_history.stanceSupport)||!world.IsBodyEnabled(m_history.plannedSupport))return diagnosticFailure("support_handle_disabled");',
    "if(!world.GetJoint(m_bodies[e.upper].joint,hip)||!world.GetJoint(m_bodies[e.lower].joint,knee)||!world.GetJoint(m_bodies[e.foot].joint,ankle))return false;":
    'if(!world.GetJoint(m_bodies[e.upper].joint,hip)||!world.GetJoint(m_bodies[e.lower].joint,knee)||!world.GetJoint(m_bodies[e.foot].joint,ankle))return diagnosticFailure("joint_handle_unavailable",{{"foot",f}});',
    "if(horizontal>=reach)return false;":
    'if(horizontal>=reach)return diagnosticFailure("horizontal_reference_unreachable",{{"foot",f},{"horizontal",horizontal},{"reach",reach},{"footTarget",BfVector(footTargets[f])},{"rootReference",BfPose(rootReference)}});',
    "if(a<.01f||b<.01f||distance>a+b+.01f||distance<std::abs(a-b)+.001f)return false;":
    'if(a<.01f||b<.01f||distance>a+b+.01f||distance<std::abs(a-b)+.001f)return diagnosticFailure("chain_reference_unreachable",{{"foot",f},{"a",a},{"b",b},{"distance",distance},{"start",BfVector(start)},{"end",BfVector(end)},{"rootReference",BfPose(rootReference)}});',
    "if(glm::dot(bend,bend)<1e-6f)return false;":
    'if(glm::dot(bend,bend)<1e-6f)return diagnosticFailure("degenerate_bend_reference",{{"foot",f},{"direction",BfVector(direction)},{"bend",BfVector(bend)}});',
    "    return m_history.swingTime<m_settings.stepSeconds*3;":
    r'''    if(!(m_history.swingTime<m_settings.stepSeconds*3)){
        const auto& observed=m_observation.supports[m_history.swing];
        BfWrite("touchdown_deadline",m_history,m_observation,m_settings,m_bodies,world,
            {{"t",t},{"lifted",m_history.lifted},{"hasContacts",observed.contacts!=0},
             {"supportMatches",observed.body.id==m_history.plannedSupport.id},
             {"contactError",glm::length(actualContact-World(support,m_history.landingLocal)-upright*effector.contactOffset)},
             {"touchdownTolerance",m_settings.touchdownTolerance},
             {"actualContact",BfVector(actualContact)},
             {"desiredContact",BfVector(World(support,m_history.landingLocal)+upright*effector.contactOffset)},
             {"footTargets",BF::array({BfVector(footTargets[0]),BfVector(footTargets[1])})},
             {"rootReference",BfPose(rootReference)}});
    }
    return m_history.swingTime<m_settings.stepSeconds*3;''',
}
for old, new in replacements.items():
    assert text.count(old) == 1, old
    text = text.replace(old, new)
# Logging of every eventual Fail() is separate from the branch-specific record.
# Preserve the original Release/Fail state mutation and reason exactly.
needle = "void PhysicalBalanceController::Fail(PhysicsWorld& world,const char* why){Release(world);"
replacement = "void PhysicalBalanceController::Fail(PhysicsWorld& world,const char* why){BfWrite(why,m_history,m_observation,m_settings,m_bodies,world,BF::object());Release(world);"
# Helper must be visible before Fail(); move the helper definition earlier.
text = text.replace(helper + "\n" + "bool PhysicalBalanceController::SwingReferences", "bool PhysicalBalanceController::SwingReferences", 1)
before = "void PhysicalBalanceController::Fail(PhysicsWorld& world,const char* why){"
assert text.count(before) == 1
text = text.replace(before, helper + "\n" + before)
assert text.count(needle) == 1
text = text.replace(needle, replacement)
target = out / "PhysicalBalance.cpp"
target.write_text(text)
(out / "IDENTITY.json").write_text(json.dumps({
    "scope": "Failure-only observer in copied source; no physical arithmetic, predicates, state transitions, thresholds or budgets changed",
    "inputPath": str(source),
    "inputSHA256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "outputPath": str(target),
    "outputSHA256": hashlib.sha256(target.read_bytes()).hexdigest(),
    "env": "JUDAS_M78_BALANCE_FAILURE_LOG",
}, indent=2) + "\n")
print(target)
