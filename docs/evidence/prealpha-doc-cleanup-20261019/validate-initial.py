#!/usr/bin/env python3
"""Final cleanup validation; no rewriting of prior evidence or projects."""
from pathlib import Path
import datetime,hashlib,json,os,shutil,subprocess,time
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'.cache/prealpha-doc-cleanup-20261019'
BUILD=OUT/'build-make'
ROWS=[]
ENV=os.environ.copy()
ENV.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='offscreen',LIBGL_ALWAYS_SOFTWARE='1',
           XDG_DATA_HOME=str(OUT/'user-data'),XDG_CONFIG_HOME=str(OUT/'config'),JUDAS_WORLD_STATE='none')
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def run(name,args,expected=0,env=None,timeout=300,cwd=ROOT):
    log=OUT/(name+'.log');assert not log.exists(),log
    command=[str(x) for x in args];started=time.monotonic();row={'name':name,'command':command,'cwd':str(cwd),'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'expected_exit':expected}
    try:
        with log.open('w') as f:result=subprocess.run(command,cwd=cwd,env={**ENV,**(env or {})},stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
        row.update(exit=result.returncode,pass_=result.returncode==expected)
    except subprocess.TimeoutExpired:row.update(exit=None,pass_=False,timed_out=True)
    row.update(seconds=time.monotonic()-started,log=str(log.relative_to(ROOT)),log_sha256=digest(log))
    if Path(command[0]).is_file():row['executable_sha256']=digest(command[0])
    ROWS.append(row);(OUT/'CHECKS.json').write_text(json.dumps(ROWS,indent=2)+'\n');print(name,row['exit'],round(row['seconds'],3),flush=True)
    return row['pass_']
def main():
    assert run('ctest-focused',['ctest','--test-dir',BUILD,'-L','prealpha','--output-on-failure','--output-junit',OUT/'ctest.xml'],timeout=600)
    for name,target in [('script-core','judas_script_tests'),('script-debug-reload','judas_script_developer_tools_tests'),('script-editor-debug','judas_script_tools_editor_tests'),('script-real-consumers','judas_script_developer_consumer_tests')]:
        assert run(name,[BUILD/target],timeout=180)
    assert run('material-regression',[BUILD/'judas_material_tests',OUT/'materials'],timeout=180)
    assert run('api-examples',[BUILD/'judasjs_examples_tests',OUT/'cookbook'],timeout=240)
    assert run('api-live-check',['node','scripts/check_judasjs_api.mjs','--runtime',OUT/'cookbook/surface.json'])
    assert run('api-typescript-tooling',['node','tests/TypeScriptToolingTests.mjs'])
    assert run('docs-check',['python3','scripts/check_current_docs.py'])
    assert run('docs-check-controls',['python3','-m','unittest','discover','-s','tests','-p','test_current_docs.py','-v'])
    assert run('packaging-controls',['python3','scripts/prealpha_packaging_checks.py','--output',OUT/'packaging.json'])
    assert run('distribution-controls',['python3','scripts/prealpha_distribution_checks.py','--output',OUT/'distribution.json'])
    project=OUT/'editor-project';shutil.copytree(ROOT/'projects/character_demo',project)
    assert run('editor-authoring-play-stop',[BUILD/'judas_editor',project/'character_demo.judasproj'],env={'JUDAS_EDITOR_AUTOTEST':str(OUT/'editor'),'JUDAS_EDITOR_AUTOTEST_AUTHORING':'1'},timeout=180)
    text=(OUT/'editor-authoring-play-stop.log').read_text()
    for token in ['shared authoring batch + undo PASS','undo restores: IDENTICAL','authored scene after play/stop is IDENTICAL','save ok']:
        assert token in text,(token,text[-3000:])
    assert run('script-project-check',[BUILD/'judas_script_check',project/'character_demo.judasproj'])
    assert run('export-project',[BUILD/'judas_export',project/'character_demo.judasproj',OUT/'export',BUILD/'judas'],timeout=180)
    assert run('known-liquid-failure',['python3','scripts/prealpha_validate.py','--build-dir',BUILD,'--group','known-failures','--output',OUT/'known-failure'],expected=1)
    print('FINAL CHECKS PASSED',len(ROWS),flush=True)
if __name__=='__main__':main()
