#!/usr/bin/env python3
"""Split actual captured commands and seeds; preserve the original closure."""
import pathlib, re, json, hashlib, shutil

r = pathlib.Path.cwd()
base = r / '.cache/m78-cold-boundary/first-boundary-closure'
out = r / '.cache/m78-cold-boundary/first-boundary-closure-split'
out.mkdir(exist_ok=False)
s = (base / 'JointSolver.cpp').read_text()
s = s.replace('bjReplayCapturedBoundary=false;', 'bjReplayCapturedBoundary=false,bjReplayCommands=true,bjReplaySeeds=true;std::string bjReplayMode="both";')
needle = 'bjReplayCapturedBoundary=replay&&std::string(replay)=="1";'
assert s.count(needle) == 1
s = s.replace(needle, needle + 'const char* mode=std::getenv("JUDAS_BOUNDARY_REPLAY_MODE");bjReplayMode=mode?mode:"both";if(bjReplayMode!="both"&&bjReplayMode!="commands"&&bjReplayMode!="seeds")throw std::runtime_error("invalid diagnostic replay mode");bjReplayCommands=bjReplayMode!="seeds";bjReplaySeeds=bjReplayMode!="commands";')
needle = '  if(inputs.size()!=6)throw std::runtime_error("captured boundary joint count mismatch");'
assert s.count(needle) == 1
s = s.replace(needle, needle + '''
  for(std::size_t index=0;index<inputs.size();++index){const auto& input=inputs[index];const auto& d=input.state->angularDrive;
   bjWrite({{"kind","captured_boundary_before_replay"},{"jointOrdinal",index},{"mode",bjReplayMode},{"warm",*input.warm},
    {"drive",{{"enabled",d.enabled},{"target",bjQuat(d.target)},{"stiffness",d.stiffness},{"damping",d.damping},{"maxTorque",d.maxTorque},
     {"relativeAngularVelocity",bjVec(d.relativeAngularVelocity)},{"impulses",bjVec(d.impulses)}}}});}
''')
s = s.replace('*input.warm={', 'if(bjReplaySeeds)*input.warm={')
s, count = re.subn(r'command\.impulses=(glm::vec3\([^;]+\));input\.state->angularDrive=command;',
                  r'command.impulses=input.state->angularDrive.impulses;if(bjReplayCommands)input.state->angularDrive=command;if(bjReplaySeeds)input.state->angularDrive.impulses=\1;', s)
assert count == 6, count
s = s.replace('{"kind","actual_native_first_boundary_replayed"}', '{"kind","actual_native_first_boundary_replayed"},{"mode",bjReplayMode}')
(out / 'JointSolver.cpp').write_text(s)
for name in ['PhysicalBalance.cpp', 'BalanceLabBoundaryObserver.cpp']:
    shutil.copyfile(base / name, out / name)
d = json.loads((base / 'IDENTITY.json').read_text())
d['scope'] = 'Diagnostic only: exact original a106 input guards, separately replay actual first-source commands and actual prior numerical seeds. Commands mode preserves current impulses; seeds mode preserves current actuator command fields. Pre-overwrite commands recorded. No production cache persistence or fixture change.'
d['overlaySHA256'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.cpp')}
(out / 'IDENTITY.json').write_text(json.dumps(d, indent=2) + '\n')
print(d['overlaySHA256'])
