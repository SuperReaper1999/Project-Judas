#!/usr/bin/env python3
"""Small helper controls; no timed game workload."""
from pathlib import Path
import hashlib,json,shlex,subprocess
owned=Path(__file__).resolve().parent
root=owned.parents[2]
build=root/'build'
flags={}
for line in (build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key,value=line.split(' = ',1); flags[key]=shlex.split(value)
binary=owned/'support-guess-tests'
command=['/usr/bin/c++',*flags['CXX_DEFINES'],
    '-I'+str(owned/'include/src/physics'),'-I'+str(owned/'include/src'),'-I'+str(owned),
    *flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],str(owned/'SupportGuessTests.cpp'),'-o',str(binary)]
subprocess.run(command,cwd=root,check=True)
result=subprocess.run([binary],cwd=root,text=True,capture_output=True)
(owned/'support-guess-tests.log').write_text(result.stdout+result.stderr)
(owned/'TESTS.json').write_text(json.dumps({'command':command,'exit':result.returncode,
    'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
    'source_sha256':hashlib.sha256((owned/'SupportGuessTests.cpp').read_bytes()).hexdigest()},indent=2)+'\n')
print(result.stdout,result.stderr)
result.check_returncode()
