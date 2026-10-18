"""Evidence-only independent source/negative controls for the bounded extraction.

This is deliberately not a frozen future API contract. It compares the recorded
pre-refactor source to this pass's candidate, so legitimate future API changes do
not have to preserve an obsolete hash. Actual runtime regressions are separate.
"""
from pathlib import Path
import re,json,hashlib
ROOT=next(parent for parent in Path(__file__).resolve().parents if (parent/'engine-version.json').is_file() and (parent/'src/ScriptSystem.cpp').is_file())
(ROOT/'.cache/prealpha-pass3/maintainability').mkdir(parents=True,exist_ok=True)
BASE=Path(__file__).resolve().parents[1]/'baseline'
OUT=ROOT/'.cache/prealpha-pass3/maintainability'
TOKEN=re.compile(r'R"(?P<delim>[^\s()\\]{0,16})\(.*?\)(?P=delim)"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|[A-Za-z_]\w*|(?:\d+(?:\.\d*)?|\.\d+)(?:[eEpP][+-]?\d+)?[A-Za-z_0-9]*|::|->\*|->|<<=|>>=|==|!=|<=|>=|&&|\|\||\+\+|--|\+=|-=|\*=|/=|%=|&=|\|=|\^=|<<|>>|\.\.\.|[^\s]',re.S)
def tokens(text):return [m.group() for m in TOKEN.finditer(text) if not m.group().startswith(('//','/*'))]
def digest(ts):return hashlib.sha256(json.dumps(ts,separators=(',',':')).encode()).hexdigest()
def block(text,start,end):return text[text.index(start):text.index(end,text.index(start))]
def original_native(text):return block(text,'JSValue ScriptSystem::Impl::Native(', 'ScriptSystem::ScriptSystem(')
def module_branches(path):
 text=path.read_text();return block(text,'    // Existing dispatch branches begin.','    // Existing dispatch branches end.')
def reconstruct(source,ui,joint):
 ui=re.sub(r'\bassets\b','s->assets',ui)
 ui=re.sub(r'\bowner\b','s->currentOwner',ui)
 ui=re.sub(r'\bslot\b','s->currentSlot',ui)
 # Replacement touches only identifiers; exact old UI branch has no local owner/slot.
 start=source.index('    if(op=="uiDiagnostics"')
 end=source.index('    if(op=="elapsed")',start)
 source=source[:start]+ui+'\n'+source[end:]
 start=source.index('    if(op=="jointCreate"')
 end=source.index('    if(op.rfind("region",0)==0)',start)
 source=source[:start]+joint+'\n'+source[end:]
 return source
def assert_native(source,ui,joint):
 expected=tokens(original_native((BASE/'src/ScriptSystem.cpp').read_text()))
 actual=tokens(original_native(reconstruct(source,ui,joint)))
 if expected!=actual:
  for i,(a,b) in enumerate(zip(expected,actual)):
   if a!=b:raise AssertionError(f'Native token {i}: expected {a!r}, got {b!r}')
  raise AssertionError(f'Native token length {len(expected)} != {len(actual)}')
 return {'tokens':len(actual),'sha256':digest(actual)}
def function_body(text,name):
 match=re.search(r'\b'+re.escape(name)+r'\s*\([^;]*?\)\s*(?:const\s*)?\{',text)
 if not match:raise AssertionError('missing function '+name)
 tail=text[match.end()-1:];depth=0
 for m in TOKEN.finditer(tail):
  token=m.group()
  if token=='{':depth+=1
  elif token=='}':
   depth-=1
   if not depth:return tokens(tail[:m.end()])
 raise AssertionError('unterminated '+name)
def assert_function(left,right,name):
 a=function_body(left,name);b=function_body(right,name)
 if a!=b:raise AssertionError('function changed: '+name)
 return {'tokens':len(a),'sha256':digest(a)}
def main():
 source=(ROOT/'src/ScriptSystem.cpp').read_text()
 ui=module_branches(ROOT/'src/ScriptBindingsUI.cpp')
 joint=module_branches(ROOT/'src/ScriptBindingsJoint.cpp')
 rows={'native':assert_native(source,ui,joint),'helpers':{},'editor':{},'negative_controls':{}}
 restored=reconstruct(source,ui,joint)
 for include in ['#include "ScriptBindings.h"\n','#include "ScriptBindingValues.h"\n']:
  restored=restored.replace(include,'',1)
 for name in ['String','Number','ReadVec']:
  restored=re.sub(r'using ScriptBindingValues::'+name+r';\s*','',restored,count=1)
 old=(BASE/'src/ScriptSystem.cpp').read_text()
 for prefix,before in [('std::string String(','std::string Exception('),('bool Number(','bool KnownOptions('),('bool ReadVec(','bool KnownOptions(')]:
  start=old.index(prefix);text=old[start:old.index('\n',start)+1]
  restored=restored.replace(before,text+before,1)
 if tokens(old)!=tokens(restored):raise AssertionError('full ScriptSystem token parity failed')
 rows['entire_script_system']={'tokens':len(tokens(restored)),'sha256':digest(tokens(restored))}

 helper=(ROOT/'src/ScriptBindingValues.h').read_text();original=(BASE/'src/ScriptSystem.cpp').read_text()
 for name in ['String','Number','ReadVec']:rows['helpers'][name]=assert_function(original,helper,name)
 original=(BASE/'src/editor/EditorPanels.cpp').read_text();preview=(ROOT/'src/editor/RenderPreview.cpp').read_text()
 for name in ['RenderHistory','PublishRenderPreview','RenderPreviewUndo','AppearanceAction','AppearancePreviewState','DrawRuntimeAppearance','DrawRuntimeRenderPreview']:
  rows['editor'][name]=assert_function(original,preview,name)
 for name,badui,badjoint in [
  ('changed UI bounds validation',ui.replace('16384','8192',1),joint),
  ('changed slot ownership',re.sub(r'owner\s*,\s*error\s*,\s*slot', 'owner,error,0',ui,count=1),joint),
  ('changed joint participant guard',ui,joint.replace('owner already has a joint','changed diagnostic',1)),
  ('missing joint lifetime rejection',ui,re.sub(r'if\s*\(\s*!valid\s*\)\s*return\s+JS_ThrowReferenceError\(c,\s*"stale joint handle"\);','',joint,count=1))]:
  if badui==ui and badjoint==joint:raise AssertionError('negative mutation was not applied: '+name)
  try:assert_native(source,badui,badjoint)
  except AssertionError:rows['negative_controls'][name]='detected'
  else:raise AssertionError('negative control was not detected: '+name)
 (OUT/'SOURCE_TOKEN_PARITY.json').write_text(json.dumps(rows,indent=2)+'\n')
 print(json.dumps(rows,indent=2))
if __name__=='__main__':main()
