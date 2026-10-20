#!/usr/bin/python3
"""Task-local QGA transfer/exec helper. No domain boot/configuration operations."""
import argparse,base64,json,time
from pathlib import Path,PureWindowsPath
import libvirt,libvirt_qemu

parser=argparse.ArgumentParser();parser.add_argument('action',choices=['ping','upload','download','download-evidence','exec','exec-wait','status']);parser.add_argument('arguments',nargs='*');args=parser.parse_args()
connection=libvirt.open('qemu:///system');domain=connection.lookupByName('win10')
def q(name,arguments=None):
    request={'execute':name}
    if arguments is not None:request['arguments']=arguments
    value=json.loads(libvirt_qemu.qemuAgentCommand(domain,json.dumps(request),30,0))
    if 'error' in value:raise RuntimeError(value['error'])
    return value['return']
def own(path):
    value=PureWindowsPath(path)
    bases=[PureWindowsPath('C:/JudasValidation/CleanDistribution'),PureWindowsPath('P:/JudasValidation/CleanDistribution')]
    if '..' in value.parts or not any(value.is_relative_to(base) and value != base for base in bases):
        raise ValueError('Guest path outside owned task directory')
    return str(value)
if args.action=='ping':print(json.dumps(q('guest-ping')))
elif args.action=='upload':
    local,remote=args.arguments;handle=q('guest-file-open',{'path':own(remote),'mode':'wb'})
    try:
        with Path(local).open('rb') as stream:
            while data:=stream.read(49152):
                result=q('guest-file-write',{'handle':handle,'buf-b64':base64.b64encode(data).decode()})
                if result['count']!=len(data):raise RuntimeError('Short guest write')
    finally:q('guest-file-close',{'handle':handle})
    print('Uploaded',Path(local).stat().st_size,'bytes')
elif args.action in ('download','download-evidence'):
    remote,local=args.arguments;target=Path(local);target.parent.mkdir(parents=True,exist_ok=True)
    if args.action=='download-evidence':
        value=PureWindowsPath(remote)
        bases=[PureWindowsPath('C:/JudasValidation/'+p) for p in ['M78Final2RepairMovedPackage','M78ClosureMovedPackageV2','M78ContinuationMoved-combined-v2','Packages','Tooling/M75','Build/windows-sdk','Build/Release']]
        if '..' in value.parts or not any(value.is_relative_to(base) for base in bases):raise ValueError('Read-only evidence path outside inventoried stale package roots')
        remote=str(value)
    else:remote=own(remote)
    handle=q('guest-file-open',{'path':remote,'mode':'rb'})
    try:
        with target.open('wb') as stream:
            while True:
                result=q('guest-file-read',{'handle':handle,'count':65536});stream.write(base64.b64decode(result['buf-b64']))
                if result['eof']:break
    finally:q('guest-file-close',{'handle':handle})
    print('Downloaded',target.stat().st_size,'bytes')
elif args.action in ('exec','exec-wait'):
    script='$OutputEncoding=[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false);\n'+Path(args.arguments[0]).read_text(encoding='utf-8-sig');code=base64.b64encode(script.encode('utf-16le')).decode()
    launched=q('guest-exec',{'path':'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe','arg':['-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',code],'capture-output':True})
    if args.action=='exec':print(json.dumps(launched))
    else:
        deadline=time.monotonic()+30
        while True:
            result=q('guest-exec-status',{'pid':launched['pid']})
            if result.get('exited') or time.monotonic()>=deadline:break
            time.sleep(.1)
        for key in ['out-data','err-data']:
            if key in result:result[key]=base64.b64decode(result[key]).decode('utf-8',errors='replace')
        if not result.get('exited'):result['pid']=launched['pid']
        print(json.dumps(result))
else:
    result=q('guest-exec-status',{'pid':int(args.arguments[0])})
    for key in ['out-data','err-data']:
        if key in result:result[key]=base64.b64decode(result[key]).decode('utf-8',errors='replace')
    print(json.dumps(result))
