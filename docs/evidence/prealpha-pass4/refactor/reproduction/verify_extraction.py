#!/usr/bin/env python3
"""Independent lexical verification of actual function bodies and ordered Impl declarations.
No compiler, source modifications or extraction-generator imports are used.
"""
from pathlib import Path
import argparse, re, json, hashlib, datetime, collections, subprocess, sys
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--root", type=Path, default=Path.cwd())
p.add_argument("--baseline", default="7da56c2cc6ce7acda827a6e4019d610622b237e2")
p.add_argument("--baseline-source", type=Path)
p.add_argument("--baseline-header", type=Path)
p.add_argument("--output", type=Path)
a = p.parse_args()
r=a.root.resolve()
preserved=Path(__file__).resolve().parents[1]/'baseline'
source_before=a.baseline_source or preserved/'PhysicsWorld.cpp'
header_before=a.baseline_header or preserved/'PhysicsWorld.h'
before=source_before.read_text() if source_before.is_file() else subprocess.check_output(['git','show',a.baseline+':src/PhysicsWorld.cpp'],cwd=r,text=True)
baseline_header=header_before.read_bytes() if header_before.is_file() else subprocess.check_output(['git','show',a.baseline+':src/PhysicsWorld.h'],cwd=r)
EXPECTED_SOURCE_SHA='5a08c72d8ae3f6fa1787e1aa42689e0638bc27323684f82b8699d07226988190'
EXPECTED_HEADER_SHA='eddadb36acc2cd46693859e844ca981d4b9f684fde68035b08ed37b757f71f03'
if hashlib.sha256(before.encode()).hexdigest()!=EXPECTED_SOURCE_SHA or hashlib.sha256(baseline_header).hexdigest()!=EXPECTED_HEADER_SHA:
 raise RuntimeError('Baseline bytes disagree with preserved accepted 7da56c2 source; refusing to replace the oracle')
files=[r/'src/PhysicsWorld.cpp',*sorted((r/'src/physics').glob('*.cpp')),*sorted((r/'src/physics').glob('*.h'))]
RAW=re.compile(r'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')
QUOTED=re.compile(r'(?:u8|u|U|L)?([\"\'])')
IDENT=re.compile(r'[a-zA-Z_0-9$]+')
def lex(s):
 out=[];i=0
 while i<len(s):
  if s[i].isspace():i+=1;continue
  if s.startswith('//',i):i=s.find('\n',i)if '\n'in s[i:]else len(s);continue
  if s.startswith('/*',i):j=s.find('*/',i+2);assert j>=0;i=j+2;continue
  raw=RAW.match(s,i)
  if raw:
   end=s.find(')'+raw[1]+'"',raw.end());assert end>=0;end+=len(raw[1])+2;out.append(s[i:end]);i=end;continue
  quoted=QUOTED.match(s,i)
  if quoted:
   q=quoted[1];j=quoted.end()
   while j<len(s):
    if s[j]=='\\':j+=2;continue
    if s[j]==q:j+=1;break
    j+=1
   out.append(s[i:j]);i=j;continue
  m=IDENT.match(s,i)
  if m:out.append(m[0]);i+=len(m[0]);continue
  out.append(s[i]);i+=1
 return out

def close(t,i,opening='(',closing=')'):
 assert t[i]==opening
 depth=1;j=i+1
 while j<len(t):
  if t[j]==opening:depth+=1
  elif t[j]==closing:
   depth-=1
   if depth==0:return j
  j+=1
 raise RuntimeError('unclosed '+opening)

def params(t):
 out=[];depth=0;i=0
 while i<len(t):
  if t[i]=='='and depth==0:
   i+=1;nest=0
   while i<len(t):
    if t[i]in ['(','[','{','<']:nest+=1
    elif t[i]in [')',']','}','>']:nest-=1
    if nest==0 and t[i]==',':break
    i+=1
   continue
  if t[i]in ['(','[','{','<']:depth+=1
  elif t[i]in [')',']','}','>']:depth-=1
  out.append(t[i]);i+=1
 return tuple(out)

def key(name,t,p,end):
 tail=t[end+1:p]
 return name,params(t[t.index('(')+1:end]),'const'in tail

def impl(t):
 pat=['struct','PhysicsWorld',':',':','Impl','{'];start=next(i for i in range(len(t))if t[i:i+len(pat)]==pat);begin=start+len(pat)-1;finish=close(t,begin,'{','}');return t[begin+1:finish]
old=lex(before);oldimpl=impl(old);methods={};state=[];i=0
while i<len(oldimpl):
 # Class-level method signature is followed by a balanced parenthesized list,
 # optional const, and its actual body. Nested types stay byte-token intact.
 start=i;j=i
 nest=0
 while j<len(oldimpl):
  if oldimpl[j]in ['(','[']:nest+=1
  elif oldimpl[j]in [')',']']:nest-=1
  if nest==0 and oldimpl[j]in ['{',';']:break
  j+=1
 if j==len(oldimpl):state.extend(oldimpl[i:]);break
 if oldimpl[j]==';':state.extend(oldimpl[i:j+1]);i=j+1;continue
 sig=oldimpl[i:j]
 if '('in sig and sig[-1]in [')','const']:
  p=sig.index('(');end=close(sig,p);name=sig[p-1]
  if re.fullmatch(r'\w+',name)and end>=p:
   finish=close(oldimpl,j,'{','}');k=(name,params(sig[p+1:end]),'const'in sig[end+1:]);methods[k]=oldimpl[j:finish+1];state.extend(sig+[';']);i=finish+1;continue
 finish=close(oldimpl,j,'{','}');state.extend(oldimpl[i:finish+1]);i=finish+1
newmethods={};public={};newclass=None
for f in files:
 t=lex(f.read_text())
 if f.name=='WorldInternal.h':newclass=impl(t)
 for i in range(len(t)):
  if t[i:i+3]!=['PhysicsWorld',':',':']:continue
  p=i+3;internal=t[p:p+3]==['Impl',':',':']
  if internal:p+=3
  if p>=len(t):continue
  if t[p]=='~':name='~'+t[p+1];p+=2
  else:name=t[p];p+=1
  if p>=len(t)or t[p]!='(':continue
  end=close(t,p);j=end+1
  while j<len(t)and t[j]not in ['{',';']:j+=1
  if j==len(t)or t[j]!='{':continue
  finish=close(t,j,'{','}');k=(name,params(t[p+1:end]),'const'in t[end+1:(t.index('-',end+1,j)if '-'in t[end+1:j]else j)]);value=t[j:finish+1]
  dest=newmethods if internal else public
  if k in dest and dest[k]!=value:raise RuntimeError('conflicting duplicate '+name)
  dest[k]=value
# Public definitions extracted independently from original qualified names.
oldpublic={}
for i in range(len(old)):
 if old[i:i+3]!=['PhysicsWorld',':',':']:continue
 p=i+3
 if old[p]=='~':name='~'+old[p+1];p+=2
 else:name=old[p];p+=1
 if p>=len(old)or old[p]!='(':continue
 end=close(old,p);j=end+1
 while j<len(old)and old[j]not in ['{',';']:j+=1
 if j==len(old)or old[j]!='{':continue
 finish=close(old,j,'{','}');oldpublic[(name,params(old[p+1:end]),'const'in old[end+1:j])]=old[j:finish+1]
diffs=[]
for label,left,right in [('private',methods,newmethods),('public',oldpublic,public)]:
 for k in left.keys()|right.keys():
  if k not in left or k not in right or left[k]!=right[k]:diffs.append({'kind':label,'name':k[0],'args':list(k[1]),'const':k[2],'oldTokens':len(left.get(k,[])),'newTokens':len(right.get(k,[]))})
stateequal=state==newclass

# Shared helper types must preserve both method tokens and member initialization order.
def type_tokens(t, name):
 for index in range(len(t)-2):
  if t[index] in ['class', 'struct'] and t[index+1] == name and t[index+2] == '{':
   return t[index:close(t,index+2,'{','}')+1]
 raise RuntimeError('missing shared type '+name)

all_new_text='\n'.join(f.read_text() for f in files)
all_new_tokens=lex(all_new_text)
shared_types=['PhysicsWorkSample','PhysicsWorkTimer','ContactKey','CachedContact','ClosestBodyResult']
type_results={name:type_tokens(old,name)==type_tokens(all_new_tokens,name) for name in shared_types}

def free_body(text, name):
 # These fixed pre-existing free helpers have ordinary return types. Anchoring
 # the declaration prefix avoids mistaking a call before an if-body for a definition.
 pattern=re.compile(r'(?m)^[ \t]*(?:inline[ \t]+)?(?:bool|double|std::vector<BodyBox>|glm::mat3)[ \t]+(?:JudasPhysicsDetail::)?'+re.escape(name)+r'[ \t]*\(')
 found=[]
 for match in pattern.finditer(text):
  t=lex(text[match.start():]); opening=t.index('('); end=close(t,opening); body=end+1
  while body<len(t) and t[body]not in ['{',';']:body+=1
  if body<len(t) and t[body]=='{':
   finish=close(t,body,'{','}'); found.append(t[body:finish+1])
 if len(found)!=1:raise RuntimeError('expected one helper '+name+', got '+str(len(found)))
 return found[0]
helpers=['MillisecondsBetween','FiniteVector','UsableVelocity','UsableKinematicPose','KinematicShape','BoxesAt','CompoundInverseInertia']
helper_results={name:free_body(before,name)==free_body(all_new_text,name) for name in helpers}

# Named-detail constants are the same values/units; namespace qualification is
# the deliberately changed linkage boundary. Private slot constants are already
# covered by the ordered Impl check.
def constants(t):
 result={}
 for i in range(len(t)):
  if t[i]!='constexpr':continue
  end=t.index(';',i); statement=t[i:end+1]
  if '=' in statement:result[statement[statement.index('=')-1]]=statement
 return result
constant_results=constants(old)==constants(all_new_tokens)
public_header=(r/'src/PhysicsWorld.h').read_bytes()
header_equal=public_header==baseline_header

# Negative controls exercise each separate guard using only in-memory copies.
# They do not compile or modify a production/source file.
negative={}
changed=dict(newmethods);key=next(k for k in changed if k[0]=='CanRespond');changed[key]=changed[key]+['false'];negative['changed_method_body_detected']=changed[key]!=methods[key]
removed=dict(newmethods);del removed[next(k for k in removed if k[0]=='PairAt')];negative['missing_method_detected']=set(removed)!=set(methods)
changed_state=list(newclass);changed_state[changed_state.index('1')]='2';negative['changed_default_initializer_detected']=state!=changed_state
negative['changed_public_header_detected']=baseline_header!=public_header+b'\n'

out={'schema':'judas.prealpha.pass4.refactor.independent_tokens.v2','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baseline':a.baseline,'baseline_source_sha256':hashlib.sha256(before.encode()).hexdigest(),'old_private_functions':len(methods),'new_private_functions':len(newmethods),'old_public_functions':len(oldpublic),'new_public_functions':len(public),'function_differences':diffs,'ordered_Impl_declaration_tokens_identical':stateequal,'old_Impl_declaration_tokens':len(state),'new_Impl_declaration_tokens':len(newclass or[]),'shared_types_identical':type_results,'free_helper_bodies_identical':helper_results,'all_constexpr_constants_identical':constant_results,'public_PhysicsWorld_header_byte_identical':header_equal,'public_header_sha256':hashlib.sha256(public_header).hexdigest(),'negative_controls':negative,'pass':not diffs and stateequal and all(type_results.values()) and all(helper_results.values()) and constant_results and header_equal and all(negative.values()),'note':'Independent lexical implementation. Exact string/character/raw literals preserved; only whitespace/comments ignored. Method return spelling/qualification changes ignored for body mapping only; ordered declarations/default initializers independently checked. No extraction-generator records imported. Actual native behavioral checks are separate evidence.'}
encoded=json.dumps(out,indent=2)+'\n'
if a.output:a.output.write_text(encoded)
print(encoded,end='')
sys.exit(0 if out['pass'] else 1)
