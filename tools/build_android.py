#!/usr/bin/env python3
"""Offline-after-SDK-setup native Judas APK builder (no Gradle/Node dependency).
Uses the ordinary validated M38 content export and the Android-only CMake target.
Signing credentials stay outside source control. Default signing is DEVELOPMENT ONLY.
"""
import argparse, datetime, hashlib, json, os, re, shutil, subprocess, sys, zipfile
try:
    import fcntl
except ImportError:
    fcntl=None
from pathlib import Path
from xml.sax.saxutils import escape
from package_sdk import identity, probe_identity

ROOT = Path(__file__).resolve().parents[1]
NDK = '28.2.13676358'
BUILD_TOOLS = '36.0.0'

def run(args, **kw):
    print('+', ' '.join(map(str,args)), flush=True)
    subprocess.run(list(map(str,args)), check=True, **kw)

def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream, 'sha256').hexdigest()

def zip_file(archive,path,name,compress_type=zipfile.ZIP_DEFLATED):
    """Stable generated entries; full native reproducibility still requires a fixed toolchain."""
    epoch=int(os.environ.get('SOURCE_DATE_EPOCH','315532800'))
    if not 315532800<=epoch<=4354819199:raise ValueError('SOURCE_DATE_EPOCH must be in the ZIP 1980..2107 range')
    stamp=datetime.datetime.fromtimestamp(epoch,datetime.timezone.utc)
    entry=zipfile.ZipInfo(name,(stamp.year,stamp.month,stamp.day,stamp.hour,stamp.minute,stamp.second))
    entry.create_system=3;entry.external_attr=0o100644<<16;entry.compress_type=compress_type
    archive.writestr(entry,path.read_bytes())

def matching_identity(info,reference):
    fields=('version','channel','source_revision','source_tree','source_dirty','source_fingerprint','source_origin')
    if info.get('source_origin')=='git' and reference.get('source_origin')=='git':
        fields+=('source_untracked_count','source_untracked_fingerprint')
    return all(field in info and field in reference and info[field]==reference[field] for field in fields)

def parse_args(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('project', type=Path)
    p.add_argument('--output',required=True,type=Path)
    p.add_argument('--sdk',type=Path,default=Path(os.environ.get('ANDROID_HOME',Path.home()/'Android/Sdk')))
    p.add_argument('--cache',type=Path,help='Isolated native/staging cache; default .cache/android/<channel>')
    p.add_argument('--preset',choices=['development','release'],default='development',help='Release requires explicit app version and private signing credentials')
    p.add_argument('--channel',choices=['main','Experimental','Local'],default='Experimental',help='Engine build channel, independent of Android app version/signing')
    p.add_argument('--engine-version',help='Override checked-in engine version; does not change the game save/content identity')
    p.add_argument('--abis',default='arm64-v8a,x86_64')
    p.add_argument('--shadows',choices=['off','on'],default='off',help='Mobile graphics profile; physics is unchanged')
    p.add_argument('--jobs',type=int,default=4)
    p.add_argument('--icu-host-tools',type=Path,help='Reuse a matching ICU 76.1 host build without rebuilding its tools')
    p.add_argument('--id')
    p.add_argument('--name')
    p.add_argument('--version-code',type=int)
    p.add_argument('--version-name')
    p.add_argument('--exporter',type=Path,default=ROOT/'build/judas_export')
    p.add_argument('--desktop-runtime',type=Path,default=ROOT/'build/judas')
    p.add_argument('--keystore',type=Path)
    p.add_argument('--key-alias')
    p.add_argument('--password-env',default='JUDAS_SIGNING_PASSWORD')
    a=p.parse_args(argv)
    if a.preset=='release':
        missing=[name for name in ('id','name','version_code','version_name','keystore','key_alias') if getattr(a,name) is None]
        if missing:p.error('Release preset requires explicit '+', '.join('--'+name.replace('_','-') for name in missing))
    stem=re.sub(r'[^a-z0-9_]', '_', a.project.stem.lower())
    if not stem or not stem[0].isalpha():stem='game_'+stem
    a.id=a.id or 'org.projectjudas.'+stem
    a.name=a.name or a.project.stem.replace('_',' ').title()
    a.version_code=a.version_code if a.version_code is not None else 1
    a.version_name=a.version_name or 'development'
    a.key_alias=a.key_alias or 'judas-development'
    a.cache=a.cache or ROOT/'.cache/android'/a.channel
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+',a.id):p.error('Invalid Android application ID')
    if not 1<=a.version_code<=2100000000:p.error('version-code must be 1..2100000000')
    if not a.version_name.strip() or not a.name.strip():p.error('Application name/version-name cannot be blank')
    abis=a.abis.split(',')
    if not abis or len(set(abis))!=len(abis) or any(x not in ('arm64-v8a','x86_64') for x in abis):p.error('Use unique arm64-v8a and/or x86_64 ABIs')
    if not 1<=a.jobs<=32:p.error('jobs must be 1..32')
    if a.keystore:
        if not a.keystore.is_file():p.error('Signing keystore does not exist')
        if not os.environ.get(a.password_env):p.error('Signing password environment variable is missing')
    return a,p

def main():
    a,p=parse_args()
    if sys.platform!='linux' or fcntl is None:p.error('APK packaging currently requires a Linux host; see docs/ANDROID.md')
    java_home=os.environ.get('JAVA_HOME')
    compiler=Path(java_home)/'bin/javac' if java_home else shutil.which('javac')
    if not compiler or not Path(compiler).is_file():p.error('A complete JDK is required; set JAVA_HOME/PATH to JDK 17+')
    abis=a.abis.split(',')
    sdk=a.sdk.resolve();cache=a.cache.resolve();cache.mkdir(parents=True,exist_ok=True)
    # One coherent native/staging/signing transaction per cache. Never race an
    # existing build or remove its staging tree during another export.
    cache_lock=(cache/'.android-build.lock').open('a')
    try:fcntl.flock(cache_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    except BlockingIOError:p.error('Another Android build owns this cache; wait or use --cache elsewhere')
    tools=sdk/'build-tools'/BUILD_TOOLS;android=sdk/'platforms/android-36/android.jar'
    ndk=sdk/'ndk'/NDK
    for f in [tools/'aapt2',tools/'d8',tools/'apksigner',tools/'zipalign',android,ndk/'build/cmake/android.toolchain.cmake']:
        if not f.is_file():p.error(f'Missing {f}; see docs/ANDROID.md SDK setup')
    native={};build_identities={}
    known=sorted(cache.glob('android-*/unicode/host/src/judas_icu_host-build/config/icucross.mk'))
    host_tools=a.icu_host_tools.resolve() if a.icu_host_tools else known[0].parent.parent if known else None
    for abi in abis:
        b=cache/('android-'+abi)
        cross_host=[f'-DJUDAS_ICU_HOST_TOOLS={host_tools}'] if host_tools else []
        version=[f'-DJUDAS_ENGINE_VERSION={a.engine_version or ""}']
        run(['cmake','-S',ROOT,'-B',b,f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',f'-DANDROID_ABI={abi}',
             '-DANDROID_PLATFORM=android-26','-DANDROID_STL=c++_shared','-DCMAKE_BUILD_TYPE=Release',
             f'-DJUDAS_BUILD_CHANNEL={a.channel}']+version+cross_host)
        run(['cmake','--build',b,'--target','main','-j',a.jobs]);native[abi]=b
        build_identities[abi]=identity(b/'judas-build-info-Release.json',release=a.preset=='release')
        own_host=b/'unicode/host/src/judas_icu_host-build'
        if (own_host/'config/icucross.mk').exists():host_tools=own_host
    identities=list(build_identities.values())
    if any(not matching_identity(info,identities[0]) for info in identities):
        p.error('Native ABI builds have different source/version/channel identities; rebuild from one unchanged candidate')
    desktop_info={}
    for name,executable in [('exporter',a.exporter),('desktopRuntime',a.desktop_runtime)]:
        desktop_info[name]=probe_identity(executable.resolve())
        if not matching_identity(desktop_info[name],identities[0]):
            p.error(f'{name} does not match native source/version/channel; rebuild desktop tools for this candidate')
    stage=cache/'apk-staging'
    if stage.exists():shutil.rmtree(stage)
    stage.mkdir()
    package=cache/'android-content'
    run([a.exporter.resolve(),a.project.resolve(),package,a.desktop_runtime.resolve()])
    assets=stage/'assets';content=assets/'content';content.mkdir(parents=True)
    (assets/'judas-build-info.json').write_text(json.dumps({'schema':1,'kind':'judas-android','abis':build_identities},sort_keys=True,indent=2)+'\n')
    # M38 produces a native desktop executable; Android replaces only that
    # platform artifact. Content, dependency validation and save identity remain.
    ignored={'judas','judas.exe','judas-runtime.txt','libSDL2-2.0.so.0','BUILD_PROVENANCE.json'}
    for path in sorted(package.rglob('*')):
        if not path.is_file() or path.relative_to(package).parts[0] in ignored:continue
        rel=path.relative_to(package)
        if len(rel.parts)==1 and (path.suffix in ('.so','.dll','.exe') or '.so.' in path.name):continue
        out=content/rel;out.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,out)
    licences=content/'engine/licenses';licences.mkdir(parents=True,exist_ok=True)
    for notice in ['SDL-LICENSE.txt','GLM-COPYING.txt']:shutil.copyfile(ROOT/'third_party/android'/notice,licences/notice)
    records=[{'path':f.relative_to(content).as_posix(),'bytes':f.stat().st_size,'sha256':digest(f)} for f in sorted(content.rglob('*')) if f.is_file()]
    (assets/'judas-content.json').write_text(json.dumps({'schema':1,'files':records},sort_keys=True,separators=(',',':')))
    res=stage/'res/drawable';res.mkdir(parents=True)
    shutil.copyfile(ROOT/'assets/branding/judas-icon.png',res/'judas_icon.png')
    manifest=stage/'AndroidManifest.xml'
    manifest.write_text(f'''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="{a.id}" android:versionCode="{a.version_code}" android:versionName="{escape(a.version_name,{chr(34):"&quot;"})}">
<uses-permission android:name="android.permission.INTERNET"/>
<uses-sdk android:minSdkVersion="26" android:targetSdkVersion="36"/>
<uses-feature android:glEsVersion="0x00030000" android:required="true"/>
<uses-feature android:name="android.hardware.touchscreen" android:required="false"/>
<uses-feature android:name="android.hardware.gamepad" android:required="false"/>
<application android:appCategory="game" android:label="{escape(a.name,{'"':'&quot;'})}" android:icon="@drawable/judas_icon" android:theme="@android:style/Theme.Material.NoActionBar.Fullscreen" android:allowBackup="false" android:debuggable="{'false' if a.keystore else 'true'}" android:extractNativeLibs="false" android:hasCode="true">
<meta-data android:name="judas.shadows" android:value="{str(a.shadows=='on').lower()}"/>
<meta-data android:name="judas.engineVersion" android:value="{escape(identities[0]['version'],{chr(34):'&quot;'})}"/>
<meta-data android:name="judas.buildChannel" android:value="{a.channel}"/>
<activity android:name="org.projectjudas.runtime.JudasActivity" android:exported="true" android:configChanges="keyboard|keyboardHidden|orientation|screenLayout|screenSize|smallestScreenSize|uiMode|layoutDirection|density|navigation" android:resizeableActivity="true">
<intent-filter><action android:name="android.intent.action.MAIN"/><category android:name="android.intent.category.LAUNCHER"/></intent-filter>
</activity></application></manifest>''')
    java=stage/'java';java.mkdir();classes=stage/'classes';classes.mkdir()
    first=next(iter(native.values()))
    # SDL 2.32's receivers predate target-36 export flags. Patch only the
    # staged Java copy; the pinned upstream archive and its hash stay intact.
    sdl_java=stage/'sdl-java';shutil.copytree(first/'_deps/judas_sdl-src/android-project/app/src/main/java',sdl_java)
    hid=sdl_java/'org/libsdl/app/HIDDeviceManager.java';text=hid.read_text()
    for receiver,flag in [('mUsbBroadcast','RECEIVER_NOT_EXPORTED'),('mBluetoothBroadcast','RECEIVER_EXPORTED')]:
        old=f'mContext.registerReceiver({receiver}, filter);'
        if text.count(old)!=1:raise RuntimeError('Pinned SDL receiver patch no longer applies')
        text=text.replace(old,f'if (Build.VERSION.SDK_INT >= 33) mContext.registerReceiver({receiver}, filter, Context.{flag}); else '+old)
    hid.write_text(text)
    sources=list(sdl_java.rglob('*.java'))
    sources+=list((ROOT/'platform/android/java').rglob('*.java'))
    run([tools/'aapt2','compile','--dir',stage/'res','-o',stage/'resources.zip'])
    run([tools/'aapt2','link','-o',stage/'resources.apk','--manifest',manifest,'-I',android,'--java',java,'-A',assets,stage/'resources.zip'])
    sources+=list(java.rglob('*.java'))
    args=stage/'javac.args';args.write_text('\n'.join('"'+str(x)+'"' for x in sources))
    compiler=[str(Path(os.environ['JAVA_HOME'])/'bin/javac')] if os.environ.get('JAVA_HOME') else ['javac']
    run(compiler+['-encoding','UTF-8','--release','8','-classpath',android,'-d',classes,'@'+str(args)])
    # d8 accepts a jar of classes, avoiding command-line length limits on Windows.
    jar=stage/'classes.jar'
    with zipfile.ZipFile(jar,'w') as z:
        for f in sorted(classes.rglob('*.class')):zip_file(z,f,f.relative_to(classes).as_posix())
    dex=stage/'dex';dex.mkdir()
    run([tools/'d8','--release','--min-api','26','--lib',android,'--output',dex,jar])
    unsigned=stage/'unsigned.apk';shutil.copyfile(stage/'resources.apk',unsigned)
    with zipfile.ZipFile(unsigned,'a') as z:
        zip_file(z,dex/'classes.dex','classes.dex')
        for abi,b in native.items():
            triple='aarch64-linux-android' if abi=='arm64-v8a' else 'x86_64-linux-android'
            shared=ndk/f'toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/{triple}/libc++_shared.so'
            for f in [b/'libmain.so',b/'_deps/judas_sdl-build/libSDL2.so',shared]:
                stripped=stage/'native'/abi/f.name;stripped.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(f,stripped)
                run([ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip','--strip-unneeded',stripped])
                zip_file(z,stripped,f'lib/{abi}/{f.name}',compress_type=zipfile.ZIP_STORED)
    aligned=stage/'aligned.apk'
    run([tools/'zipalign','-P','16','-f','4',unsigned,aligned])
    keystore=a.keystore
    env=os.environ.copy()
    if keystore is None:
        keystore=cache/'development-signing.p12';env['JUDAS_DEVELOPMENT_PASSWORD']='android'
        if not keystore.exists():run(['keytool','-genkeypair','-keystore',keystore,'-storepass:env','JUDAS_DEVELOPMENT_PASSWORD','-keypass:env','JUDAS_DEVELOPMENT_PASSWORD','-alias',a.key_alias,'-keyalg','RSA','-keysize','2048','-validity','3650','-dname','CN=Judas Development, O=Local Development, C=GB'],env=env)
        password='JUDAS_DEVELOPMENT_PASSWORD'
    else:
        password=a.password_env
        if not env.get(password):p.error('Signing password environment variable is missing')
    output=a.output.resolve();output.parent.mkdir(parents=True,exist_ok=True)
    signed=stage/'signed.apk'
    run([tools/'apksigner','sign','--ks',keystore,'--ks-key-alias',a.key_alias,'--ks-pass','env:'+password,'--out',signed,aligned],env=env)
    run([tools/'apksigner','verify','--verbose',signed]);run([tools/'zipalign','-c','-P','16','4',signed])
    # A failed signing/verification must leave any previous deliverable untouched.
    temporary=output.with_name(output.name+'.publishing-'+str(os.getpid()))
    try:
        shutil.copyfile(signed,temporary)
        os.replace(temporary,output)
    finally:
        if temporary.exists():temporary.unlink()
    report={'schema':1,'kind':'judas-android-apk','apk':str(output),'sha256':digest(output),'application':{'id':a.id,'name':a.name,'versionCode':a.version_code,'versionName':a.version_name},'preset':a.preset,'engine':build_identities,'desktopTools':desktop_info,'abis':abis,'targetSdk':36,'minSdk':26,'ndk':NDK,'buildTools':BUILD_TOOLS,'developmentSigning':a.keystore is None,'shadows':a.shadows,'contentFiles':len(records),'contentBytes':sum(r['bytes'] for r in records),'native':{abi:{str(f.relative_to(b)):digest(f) for f in [b/'libmain.so',b/'_deps/judas_sdl-build/libSDL2.so']} for abi,b in native.items()}}
    output.with_suffix('.build.json').write_text(json.dumps(report,indent=2)+'\n')
    manifest=output.with_suffix('.build.json')
    output.with_suffix('.sha256').write_text(f'{digest(output)}  {output.name}\n{digest(manifest)}  {manifest.name}\n')
    print(f'APK: {output}\nSHA256: {report["sha256"]}')
if __name__=='__main__':main()
