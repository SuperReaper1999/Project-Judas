"""Stage a separately identified two-file tooling patch beside frozen native code."""
from pathlib import Path
import hashlib,json,shutil,subprocess,time
root=Path(r'P:\JudasValidation\PreAlphaPass4');source=root/'Source';stage=root/'ToolingStagingSource'
if stage.exists():raise RuntimeError('Preserve prior staging attempt rather than overwrite')
spec=json.loads((source/'packaging/sdk-inventory.json').read_text(encoding='utf-8'))
# Copy precisely the existing SDK source inventory, not C++ or build metadata.
files=set(spec['files'])|{'tools/package_sdk.py','packaging/sdk-inventory.json'}
files|={p.relative_to(source).as_posix() for p in (source/spec['current_markdown']).glob('*.md') if p.relative_to(source).as_posix() not in spec.get('excluded_markdown',[])}
for name in spec['directories']:
 files|={p.relative_to(source).as_posix() for p in (source/name).rglob('*') if p.is_file()}
for name in sorted(files):
 p=stage/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/name,p)
patches={'inspect_sdk.py':'e4017421e901809aaee1b46f12f89dc248151f0100136effad0646c996b3da4b','verify_android.py':'2d147363d310cdaa5394af29377f0a4981585b2c2f0caf05cbf5357430c4a9d2'}
for name,expected in patches.items():
 data=(root/('tooling-'+name)).read_bytes()
 if hashlib.sha256(data).hexdigest()!=expected:raise RuntimeError('Tooling patch hash mismatch')
 (stage/'tools'/name).write_bytes(data)
command=[r'C:\JudasValidation\Python\python.exe',str(stage/'tools/package_sdk.py'),'stage','--source',str(stage),'--inventory',str(stage/'packaging/sdk-inventory.json'),'--bin-dir',str(root/'Build/Release'),'--build-info',str(root/'Build/judas-build-info-Release.json'),'--output',str(root/'Build/windows-sdk'),'--replace','--probe-tools','--release']
started=time.time();p=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',timeout=120)
receipt={'schema':1,'utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'nativeSourceRevision':'9deb47d1667e5b6bd1abe708ed343c3fbf6fe847','toolingRevision':'df8929f73505d407697b5d35b115fc71d9100471','patches':patches,'command':command,'exit':p.returncode,'seconds':time.time()-started,'stdout':p.stdout,'stderr':p.stderr,'scope':'Native executable provenance retained truthfully; only separately committed Python inspector entrypoints replaced through normal inventory/hash staging; no C++ rebuild or expected-hash bypass'}
(root/'TOOLING_RESTAGE.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8');print(p.stdout,p.stderr);raise SystemExit(p.returncode)
