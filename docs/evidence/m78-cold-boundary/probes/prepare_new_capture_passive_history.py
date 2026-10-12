#!/usr/bin/env python3
"""WP-only proof of a new-save passive continuation section; diagnostic only.

This copied source uses the existing fracture helper through explicit field
encoding, without persisting its reaction fields. It is a diagnostic adapter,
not a proposed production byte-layout implementation or timestep contract.
"""
import pathlib, hashlib, json

r = pathlib.Path.cwd()
out = r / '.cache/m78-cold-boundary/new-capture-passive-history'
out.mkdir(exist_ok=False)
source = r / 'src/WorldPersistence.cpp'
s = source.read_text()
helper = '''
// Diagnostic adapter only: emit exactly12 passive solver floats, no reaction,
// drive or completed-contact fields. A production helper needs explicit dt
// identity/invalidation and format rather than depending on another payload.
void passiveArticulationHistoryDiagnostic(PhysicsWorld& physics,SaveArchive& a,JointHandle handle){
 std::array<float,12> values{};
 if(!a.reading){SaveArchive current;physics.PersistJointSolverState(current,handle);SaveArchive decode(current.bytes);for(auto& x:values)decode(x);}
 for(auto& x:values)a(x);
 if(a.reading){JointState state;a.Require(physics.GetJoint(handle,state),"diagnostic stale articulation continuation joint");SaveArchive encode;for(auto& x:values)encode(x);encode(state.reactionImpulse,state.reactionAngularImpulse);SaveArchive apply(encode.bytes);physics.PersistJointSolverState(apply,handle);apply.Finish();}
}
'''
assert s.count('namespace {') == 1
s = s.replace('namespace {', 'namespace {' + helper, 1)
needle = 'restored.push_back(joint);}}'
assert s.count(needle) == 1
s = s.replace(needle, 'restored.push_back(joint);}if(version>=4)passiveArticulationHistoryDiagnostic(w.Physics(),a,a.reading?restored.back():r.joints.at(j));}')
needle = 'if(name=="articulation")ArticulationExtended(w,a,version);else fn(w,a);'
assert s.count(needle) == 1
s = s.replace(needle, 'if(name=="articulation"&&std::any_of(w.m_ragdolls.begin(),w.m_ragdolls.end(),[](const auto& row){return !row.second.joints.empty();}))version=4;\n ' + needle)
needle = '(it->second.version==2||it->second.version==3)'
assert s.count(needle) == 1
s = s.replace(needle, '(it->second.version==2||it->second.version==3||it->second.version==4)')
(out / 'WorldPersistence.cpp').write_text(s)
identity = dict(scope='Diagnostic-only copied WorldPersistence.cpp. Newly generated articulated snapshots emit version4 with exactly12 passive joint history floats per existing durable owner/index/body-pair/settings record. Older1–3 input paths unchanged. Reactions/drives/contact observations not serialized. No production edit. Timestep metadata/invalidation intentionally not implemented by this first causal prototype.',
                sourceSHA256={str(source.relative_to(r)):hashlib.sha256(source.read_bytes()).hexdigest()},
                overlaySHA256=hashlib.sha256(s.encode()).hexdigest(),
                originalFixtureSHA256=hashlib.sha256((r/'tests/BalanceLabTests.cpp').read_bytes()).hexdigest())
(out / 'IDENTITY.json').write_text(json.dumps(identity, indent=2) + '\n')
print(identity)
