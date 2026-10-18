from pathlib import Path
import subprocess,tempfile,json
ROOT=next(parent for parent in Path(__file__).resolve().parents if (parent/'engine-version.json').is_file() and (parent/'src/ScriptSystem.cpp').is_file())
(ROOT/'.cache/prealpha-pass3/maintainability').mkdir(parents=True,exist_ok=True)
BASE=Path(__file__).resolve().parents[1]/'baseline'
COMMANDS=['add_executable','target_link_libraries','judas_warnings','target_include_directories','target_compile_definitions','set_target_properties']
PLATFORMS={'linux':{'UNIX':True,'WIN32':False,'ANDROID':False},'windows':{'UNIX':False,'WIN32':True,'ANDROID':False},'android':{'UNIX':True,'WIN32':False,'ANDROID':True}}
def execute(text,platform,helper=None):
 with tempfile.TemporaryDirectory(prefix='judas-cmake-refactor-') as folder:
  out=Path(folder);trace=out/'trace.txt';module=out/'module.cmake';module.write_text(text)
  helperFile=out/'helper.cmake';helperFile.write_text(helper if helper is not None else (ROOT/'cmake/JudasTestTargets.cmake').read_text())
  declarations='cmake_minimum_required(VERSION 3.20)\nset(CMAKE_CURRENT_SOURCE_DIR "'+ROOT.as_posix()+'")\n'
  for name,value in PLATFORMS[platform].items():declarations+=f'set({name} {"TRUE" if value else "FALSE"})\n'
  for command in COMMANDS:declarations+=f'function({command})\n file(APPEND "{trace.as_posix()}" "{command}|${{ARGV}}\\n")\nendfunction()\n'
  declarations+='include("'+helperFile.as_posix()+'")\ninclude("'+module.as_posix()+'")\n'
  script=out/'inspect.cmake';script.write_text(declarations)
  result=subprocess.run(['cmake','-P',str(script)],capture_output=True,text=True,timeout=15)
  if result.returncode:raise AssertionError(result.stdout+result.stderr)
  return [[line.split('|',1)[0],line.split('|',1)[1].split(';')] for line in trace.read_text().splitlines()]
def original():
 s=(BASE/'CMakeLists.txt').read_text();return s[s.index('# M88 device-independent'):s.index('add_executable(judas_prealpha_lifecycle_tests')]
def candidate():return '\n'.join((ROOT/('cmake/targets/'+name+'.cmake')).read_text() for name in ['LocalViews','RenderingAssets','DeveloperWorkflow','WorldLifecycle'])
if __name__=='__main__':
 expected={p:execute(original(),p) for p in PLATFORMS}
 actual={p:execute(candidate(),p) for p in PLATFORMS}
 assert expected==actual
 result={p:{'calls':len(actual[p]),'targets':sum(x[0]=='add_executable' for x in actual[p]),'exact_parity':True} for p in PLATFORMS}
 (ROOT/'.cache/prealpha-pass3/maintainability/CMAKE_EXECUTION_PARITY.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
