#!/usr/bin/env python3
"""Link an unchanged observer source to one existing native engine build."""
import argparse, hashlib, json, os, shlex, struct, subprocess
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--build',type=Path,required=True)
p.add_argument('--label',required=True)
p.add_argument('--freeflight',action='store_true')
a=p.parse_args()
base=Path(__file__).resolve().parent
build=a.build.resolve()
source=base/'ScriptEndpointProbe.cpp'
output=base/a.label
output.mkdir(exist_ok=False)
flags={}
for line in (build/'CMakeFiles/judas_script_tests.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key,value=line.split(' = ',1);flags[key]=shlex.split(value)
obj=output/'probe.o';binary=output/'probe'
compile=['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)]
with (output/'compile.log').open('w') as log:
    result=subprocess.run(compile,stdout=log,stderr=subprocess.STDOUT)
if result.returncode: raise RuntimeError('Compile failed: '+str(output/'compile.log'))
link=shlex.split((build/'CMakeFiles/judas_script_tests.dir/link.txt').read_text())
link=[str(obj) if value=='CMakeFiles/judas_script_tests.dir/tests/ScriptTests.cpp.o' else str(binary) if value=='judas_script_tests' else value for value in link]
with (output/'link.log').open('w') as log:
    result=subprocess.run(link,cwd=build,stdout=log,stderr=subprocess.STDOUT)
if result.returncode: raise RuntimeError('Link failed: '+str(output/'link.log'))
(output/'commands.json').write_text(json.dumps([compile,link],indent=2)+'\n')
env={key:value for key,value in os.environ.items() if not key.startswith('JUDAS_')}
env.update(SDL_AUDIODRIVER='dummy')
fixture=output/'fixture';rows=[]
for mode in ['full','read','negative']:
    command=[str(binary),mode,str(fixture)]+(['freeflight'] if a.freeflight else [])
    result=subprocess.run(command,cwd=output,env=env,capture_output=True,text=True,timeout=60)
    text=result.stdout+result.stderr
    (output/(mode+'.log')).write_text(text)
    rows.append(dict(mode=mode,command=command,exit=result.returncode))
    if result.returncode: raise RuntimeError(mode+' failed: '+text[-2000:])
full=(output/'full.log').read_text().splitlines()
read=(output/'read.log').read_text().splitlines()
negative=(output/'negative.log').read_text().splitlines()
phys=lambda lines:[line for line in lines if line.startswith('PHYS ')]
reference=[line for line in phys(full) if int(line.split()[1])>=60]
actual=phys(read)
# This observer records existing numerical behaviour. A pre-existing cold
# continuation mismatch is evidence, not permission to change engine acceptance.
first=next(((left,right) for left,right in zip(reference,actual) if left!=right),None)
def decode(line):
    return [struct.unpack('!f',bytes.fromhex(word))[0] for word in line.split()[3:]]
maximum=max((abs(a-b) for left,right in zip(reference,actual) for a,b in zip(decode(left),decode(right))),default=0)
assert phys(negative)!=actual, 'physical negative control failed to change observed trajectory'
summary={
 'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
 'commands':rows,
 'steps':120,
 'fixed_dt':'binary32 1/60',
 'full_native_pose_velocity_sha256':hashlib.sha256(('\n'.join(phys(full))+'\n').encode()).hexdigest(),
 'native_cold_boundaries_compared':len(actual),
 'native_cold_exact':reference==actual,
 'native_cold_first_mismatch':first,
  'native_cold_maximum_absolute_scalar_difference':maximum,
 'native_cold_first_differing_fields':([] if not first else [
   dict(field=['position.x','position.y','position.z','rotation.w','rotation.x','rotation.y','rotation.z',
               'linear.x','linear.y','linear.z','angular.x','angular.y','angular.z'][i],
        uninterrupted=left,restored=right,absolute=abs(left-right))
   for i,(left,right) in enumerate(zip(decode(first[0]),decode(first[1]))) if left!=right]),
 'native_cold_maximum_position_difference_m':max((sum((left[i]-right[i])**2 for i in range(3))**.5
     for left,right in ((decode(x),decode(y)) for x,y in zip(reference,actual))),default=0),
 'native_cold_maximum_linear_velocity_difference_mps':max((sum((left[i]-right[i])**2 for i in range(7,10))**.5
     for left,right in ((decode(x),decode(y)) for x,y in zip(reference,actual))),default=0),
 'native_cold_maximum_angular_velocity_difference_radps':max((sum((left[i]-right[i])**2 for i in range(10,13))**.5
     for left,right in ((decode(x),decode(y)) for x,y in zip(reference,actual))),default=0),
 'impulse_negative_control_changes_native_trajectory':True,
 'physical_120':phys(full)[-1],
 'native_settings_120':next(line for line in reversed(full) if line.startswith('NATIVE ')),
 'native_save_chunks':[line for line in full if line.startswith('CHUNK ')],
 'humanAccepted':False,
}
(output/'RESULTS.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
