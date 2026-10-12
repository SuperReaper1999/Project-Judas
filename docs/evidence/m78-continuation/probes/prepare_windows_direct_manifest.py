#!/usr/bin/env python3
"""Prepare source/recipe receipts for a root-authorized native build; never run it."""
import argparse, hashlib, json, pathlib, re, zipfile

p=argparse.ArgumentParser()
p.add_argument('label')
p.add_argument('--source-root',type=pathlib.Path,default=pathlib.Path.cwd(),help='Tree with final files under src/; repository files are fallback')
p.add_argument('--replace',action='append',default=[],help='Repository-relative cpp to replace in judas_engine.lib')
p.add_argument('--header',action='append',default=[],help='Repository-relative changed header, no ABI assumptions')
p.add_argument('--case',action='append',default=[],help='case-id=repository-relative cpp; unchanged fixture hash checked in guest')
p.add_argument('--extra',action='append',default=[],help='case-id=repository-relative additional cpp, e.g. EditorHistory or Application')
p.add_argument('--template',default='judas_balance_lab_tests',help='Existing native compiler/link recipe to use for one-TU cases')
p.add_argument('--case-template',action='append',default=[],help='case-id=existing-target recipe override; all cases still link the same candidate archive')
p.add_argument('--stage-tests',action='store_true',help='Package exact selected current test files under isolated source/tests; never overwrite the old native source inputs')
p.add_argument('--header-closure',action='store_true',help='Stage all existing src headers byte-for-byte so local peer includes cannot reopen the old source header copies')
a=p.parse_args(); root=pathlib.Path.cwd(); transfer=root/'.cache/windows-vm-transfer'; reports=transfer/'reports'
if not re.fullmatch(r'[a-z0-9_-]+',a.label):p.error('Simple unique label required')
manifestPath=transfer/(f'm78-continuation-direct-{a.label}.json')
archivePath=transfer/(f'm78-continuation-direct-{a.label}.zip')
if manifestPath.exists() or archivePath.exists():raise SystemExit('Preserve previous prepared manifests')
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
def inputSource(name):
 source=a.source_root/name
 if not source.exists() and name.startswith('src/'):
  # Isolated diagnostic source trees may store their src overlay at the root.
  source=a.source_root/name[len('src/'):]
 if not source.exists():raise SystemExit('Missing explicitly selected source '+str(source))
 return source
def recipe(target,kind):
 f=reports/f'm78-continuation-{target}-{kind}.command.1.tlog'
 return f.read_text(encoding='utf-16').splitlines(),sha(f)
def sourceRecipe(target,source):
 lines,h=recipe(target,'CL'); suffix=('\\'+source.replace('/','\\')).upper()
 for i,row in enumerate(lines):
  if row.startswith('^') and row.upper().endswith(suffix):return lines[i+1],h
 raise SystemExit('Missing exact compiler recipe '+source)
engineRecipes=recipe('judas_engine','CL')
def caseRecipe(target):
 linkLines,linkHash=recipe(target,'link')
 linkFlags=next(x for x in linkLines if x and not x.startswith('^'))
 linkFlags=re.sub(r'\s+[^\s"]+\.DIR\\RELEASE\\[^\s"]+\.OBJ(?=\s|$)','',linkFlags,flags=re.I)
 testLines,testHash=recipe(target,'CL')
 testFlags=next(x for x in testLines if x and not x.startswith('^'))
 testFlags=re.sub(r'\s+C:\\JUDASVALIDATION\\SOURCE\\[^\r\n]*\.CPP\s*$','',testFlags,flags=re.I)
 return testFlags,testHash,linkFlags,linkHash
caseTemplates={}
for value in a.case_template:
 name,target=value.split('=',1)
 if name in caseTemplates:raise SystemExit('Duplicate case recipe override '+name)
 caseTemplates[name]=target
old=json.loads((reports/'m78-continuation-existing-recipes.json').read_text(encoding='utf-8-sig'))
members=(reports/'m78-continuation-engine-members.txt').read_text(encoding='utf-8-sig').splitlines()
files={}; replacements=[]
for name in a.replace:
 if not name.startswith('src/') or not name.endswith('.cpp'):p.error('Only explicit src/*.cpp archive replacements')
 source=inputSource(name)
 flags,h=sourceRecipe('judas_engine',name)
 basename=pathlib.PurePosixPath(name).with_suffix('.obj').name
 matching=[m for m in members if m.replace('\\','/').rsplit('/',1)[-1].lower()==basename.lower()]
 if len(matching)!=1:raise SystemExit('Ambiguous/missing existing member '+name)
 files[name]=source;replacements.append({'path':name,'sha256':sha(source),'flags':flags,'recipeSHA256':h,'member':matching[0],'objectName':basename})
for name in a.header:
 if not name.startswith('src/') or not name.endswith(('.h','.hpp')):p.error('Only explicit src headers')
 source=inputSource(name)
 files[name]=source
if a.header_closure:
 for source in sorted((root/'src').rglob('*')):
  if source.is_file() and source.suffix in ('.h','.hpp'):
   name=str(source.relative_to(root))
   if name not in files:files[name]=source
cases=[]
for value in a.case:
 name,relative=value.split('=',1)
 if not re.fullmatch(r'[a-z0-9_-]+',name):p.error('Simple case id required')
 source=root/relative
 if not source.exists():raise SystemExit('Missing case input '+relative)
 # Tests may be staged in the same relative layout without overwriting the
 # original guest fixture sources. All expected hashes remain explicit.
 staged=relative.startswith('docs/evidence/')
 if staged:
  files['probes/'+name+'.cpp']=source;guest='@SOURCE@\\probes\\'+name+'.cpp'
 elif a.stage_tests and relative.startswith('tests/'):
  files[relative]=source;guest='@SOURCE@\\'+relative.replace('/','\\')
 else:guest='C:\\JudasValidation\\Source\\'+relative.replace('/','\\')
 template=caseTemplates.pop(name,a.template)
 testFlags,testHash,linkFlags,linkHash=caseRecipe(template)
 cases.append({'id':name,'recipeTemplate':template,'sources':[{'sourcePath':guest,'sourceRelativePath':relative,'sha256':sha(source)}],'compilerFlags':testFlags,'compilerRecipeSHA256':testHash,'linkFlags':linkFlags,'linkRecipeSHA256':linkHash,'arguments':[],'timeoutMilliseconds':600000})
if caseTemplates:raise SystemExit('Recipe override requires existing case '+','.join(caseTemplates))
for value in a.extra:
 name,relative=value.split('=',1);found=[x for x in cases if x['id']==name]
 if len(found)!=1:raise SystemExit('Extra source requires existing unique case '+name)
 source=root/relative
 if not source.exists():raise SystemExit('Missing explicit extra source '+relative)
 found[0]['sources'].append({'sourcePath':'C:\\JudasValidation\\Source\\'+relative.replace('/','\\'),'sourceRelativePath':relative,'sha256':sha(source)})
with zipfile.ZipFile(archivePath,'w',zipfile.ZIP_DEFLATED) as z:
 for relative,source in files.items():z.writestr(relative,source.read_bytes())
manifest={'label':a.label,'scope':'Isolated native library/one-TU test build; unchanged original fixture inputs; build only after explicit root coordination','archiveURL':'http://192.168.122.1:8872/'+archivePath.name,'archiveSHA256':sha(archivePath),'files':[{'path':n,'sha256':sha(f),'bytes':f.stat().st_size} for n,f in files.items()],'baselineLibrary':old['engineArchive'],'baselineMemberCount':len(members),'replacements':replacements,'cases':cases,'minimumFreeBeforeLibraryBytes':120*1024*1024,'minimumFreeBeforeCaseBytes':48*1024*1024}
manifestPath.write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'manifest':str(manifestPath),'sourceArchiveBytes':archivePath.stat().st_size,'replacements':len(replacements),'cases':len(cases)}))
