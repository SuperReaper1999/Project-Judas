from pathlib import Path
import datetime,hashlib,json,subprocess,time
root=Path.cwd();out=root/'docs/evidence/prealpha-pass5/android';out.mkdir(parents=True,exist_ok=False)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
build=root/'.cache/m88/android-arm64-v8a';binary=build/'libmain.so';sources={str(p.relative_to(root)):sha(p)for p in sorted((root/'src/physics').glob('*')) if p.is_file()};sources['src/PhysicsWorld.cpp']=sha(root/'src/PhysicsWorld.cpp')
r={'startedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'scope':'ARM64/API26 native runtime compile/link only; no APK, emulator, physical device or hardware acceptance.','beforeBinarySha256':sha(binary),'source':sources,'argv':['cmake','--build',str(build),'--target','main','-j2']}
start=time.monotonic()
with (out/'build.log').open('w')as f:p=subprocess.run(r['argv'],stdout=f,stderr=subprocess.STDOUT,timeout=600)
r.update(exit=p.returncode,seconds=time.monotonic()-start,sourceUnchanged=sources=={n:sha(root/n)for n in sources},binarySha256=sha(binary),binaryBytes=binary.stat().st_size,logSha256=sha(out/'build.log'),buildInfo=json.loads((build/'judas-build-info-Release.json').read_text()))
for option in ['-h','-l','-d']:
 (out/('ELF_'+{'-h':'HEADER','-l':'SEGMENTS','-d':'DYNAMIC'}[option]+'.txt')).write_bytes(subprocess.check_output(['readelf',option,binary]))
(out/'RESULTS.json').write_text(json.dumps(r,indent=2)+'\n');(out/'run.py').write_bytes(Path(__file__).read_bytes())
print(json.dumps({k:r[k]for k in ['exit','seconds','sourceUnchanged','binarySha256','binaryBytes']}))
if p.returncode or not r['sourceUnchanged']:raise SystemExit(1)
