#!/usr/bin/python3
"""Task-local QGA transfer/exec helper. No domain boot/configuration operations."""
import argparse,base64,json
from pathlib import Path,PureWindowsPath
import libvirt,libvirt_qemu

parser=argparse.ArgumentParser();parser.add_argument('action',choices=['ping','upload','download','exec','status']);parser.add_argument('arguments',nargs='*');args=parser.parse_args()
connection=libvirt.open('qemu:///system');domain=connection.lookupByName('win10')
def q(name,arguments=None):
    request={'execute':name}
    if arguments is not None:request['arguments']=arguments
    value=json.loads(libvirt_qemu.qemuAgentCommand(domain,json.dumps(request),30,0))
    if 'error' in value:raise RuntimeError(value['error'])
    return value['return']
def own(path):
    value=PureWindowsPath(path)
    base=PureWindowsPath('C:/JudasValidation/PreAlphaPass3')
    if not value.is_relative_to(base) or '..' in value.parts or value == base:
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
elif args.action=='download':
    remote,local=args.arguments;target=Path(local);target.parent.mkdir(parents=True,exist_ok=True);handle=q('guest-file-open',{'path':own(remote),'mode':'rb'})
    try:
        with target.open('wb') as stream:
            while True:
                result=q('guest-file-read',{'handle':handle,'count':65536});stream.write(base64.b64decode(result['buf-b64']))
                if result['eof']:break
    finally:q('guest-file-close',{'handle':handle})
    print('Downloaded',target.stat().st_size,'bytes')
elif args.action=='exec':
    script=Path(args.arguments[0]).read_text();code=base64.b64encode(script.encode('utf-16le')).decode()
    print(json.dumps(q('guest-exec',{'path':'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe','arg':['-NoProfile','-ExecutionPolicy','Bypass','-EncodedCommand',code],'capture-output':True})))
else:
    result=q('guest-exec-status',{'pid':int(args.arguments[0])})
    for key in ['out-data','err-data']:
        if key in result:result[key]=base64.b64decode(result[key]).decode('utf-8',errors='replace')
    print(json.dumps(result))
