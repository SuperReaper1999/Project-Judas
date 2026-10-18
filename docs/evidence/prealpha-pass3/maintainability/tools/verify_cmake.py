"""Reconstruct old declarations and compare the entire CMake command stream."""
from pathlib import Path
import re,json,hashlib
ROOT=next(parent for parent in Path(__file__).resolve().parents if (parent/'engine-version.json').is_file() and (parent/'src/ScriptSystem.cpp').is_file())
(ROOT/'.cache/prealpha-pass3/maintainability').mkdir(parents=True,exist_ok=True);BASE=Path(__file__).resolve().parents[1]/'baseline'
def tokens(source):
 token=re.compile(r'"(?:\\.|[^"\\])*"|#[^\n]*|[^\s()"]+|[()]',re.S)
 return [m.group() for m in token.finditer(source) if not m.group().startswith('#')]
def expand(source):
 source=source.replace('include(cmake/JudasTestTargets.cmake)\n','')
 source=re.sub(r'include\((cmake/targets/\w+\.cmake)\)',lambda m:(ROOT/m[1]).read_text(),source)
 def old(m):
  args=m[1].split();target,main=args[:2];extra=args[2:];sources=[main];warnings=[]
  application=False;more=[];i=0
  while i<len(extra):
   word=extra[i];i+=1
   if word=='APPLICATION':application=True
   elif word in ('SOURCES','WARNINGS'):
    output=more if word=='SOURCES' else warnings
    while i<len(extra) and extra[i] not in ('APPLICATION','SOURCES','WARNINGS','LIBRARIES'):output.append(extra[i]);i+=1
   else:raise ValueError('unexpected helper argument '+word)
  if application:sources+=['src/Application.cpp','src/RuntimeOptions.cpp','src/RuntimeDiagnostics.cpp','src/TestHarness.cpp']
  sources+=more
  return f'add_executable({target} '+ ' '.join(sources)+f')\ntarget_link_libraries({target} PRIVATE judas_engine)\njudas_warnings({target}'+(' '+' '.join(warnings) if warnings else '')+')'
 # Already-existing helper calls remain as originally spelled; only reconstruct
 # target declarations that this pass actually converted.
 converted={x['target'] for x in json.loads((Path(__file__).resolve().parents[1]/'CONVERTED_ENGINE_TESTS.json').read_text())}
 return re.sub(r'judas_add_engine_test\(([^)]+)\)',lambda m:old(m) if m[1].split()[0] in converted else m[0],source)
def verify(candidate):
 expected=(BASE/'CMakeLists.txt').read_text()
 expected=expected.replace('    src/ScriptSystem.cpp','    src/ScriptSystem.cpp\n    src/ScriptBindingsUI.cpp\n    src/ScriptBindingsJoint.cpp',1)
 expected=expected.replace('    src/editor/EditorPanels.cpp','    src/editor/EditorPanels.cpp\n    src/editor/RenderPreview.cpp',1)
 a=tokens(expected);b=tokens(expand(candidate))
 if a!=b:
  for i,(x,y) in enumerate(zip(a,b)):
   if x!=y:raise AssertionError(f'CMake token {i}: expected {x!r}, got {y!r}')
  raise AssertionError(f'CMake token counts {len(a)} != {len(b)}')
 return {'tokens':len(b),'sha256':hashlib.sha256(json.dumps(b,separators=(',',':')).encode()).hexdigest()}
if __name__=='__main__':
 candidate=(ROOT/'CMakeLists.txt').read_text()
 additions=['judas_add_engine_test(judas_origin_admission_tests tests/OriginAdmissionTests.cpp)']
 for addition in additions:
  if candidate.count(addition)!=1:raise AssertionError('missing/duplicate separately authorized declaration: '+addition)
  candidate=candidate.replace(addition+'\n','',1)
 result={'parity':verify(candidate),'separately_authorized_pass3_additions':additions,'negative_controls':{}}
 for name,bad in [('missing application flag',candidate.replace(' APPLICATION','',1)),('changed warning',candidate.replace('WARNINGS -Wpedantic','WARNINGS -Wextra',1)),('missing target',re.sub(r'judas_add_engine_test\(judas_script_tests[^\n]*\)\n','',candidate,count=1))]:
  try:verify(bad)
  except AssertionError:result['negative_controls'][name]='detected'
  else:raise AssertionError('negative control undetected: '+name)
 (ROOT/'.cache/prealpha-pass3/maintainability/CMAKE_SOURCE_PARITY.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
