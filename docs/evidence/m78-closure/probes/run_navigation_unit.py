#!/usr/bin/env python3
"""Correctly supplied unchanged navigation dependency regression; initial no-argument receipt retained."""
from pathlib import Path
import hashlib,json,shutil,subprocess,time
r=Path.cwd();source=r/'.cache/m78-followup/dependency-negatives-project';case=r/'.cache/m78-followup/closure-dependency-unit';ev=r/'docs/evidence/m78-closure/navigation-unit';ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
inputs={str(p.relative_to(source)):h(p)for p in source.rglob('*')if p.is_file()};shutil.copytree(source,case);exe=r/'.cache/m78-closure/final-build/judas_navigation_dependency_tests';cmd=[str(exe),str(case/'landscape_fork.judasproj'),'Imports/landscape.judasterrain'];start=time.perf_counter();p=subprocess.run(cmd,cwd=r,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);(ev/'run.log').write_text(p.stdout)
(ev/'RESULTS.json').write_text(json.dumps(dict(scope=__doc__,command=cmd,exitCode=p.returncode,seconds=time.perf_counter()-start,executableSHA256=h(exe),fixtureSourceSHA256=h(r/'tests/NavigationDependencyTests.cpp'),sourceFixtureInputs=inputs,initialInvocation='docs/evidence/m78-closure/final-build/navigation_dependency.log',summary=p.stdout.splitlines()[-2:]),indent=2)+'\n');print(p.stdout.splitlines()[-2:]);raise SystemExit(p.returncode)
