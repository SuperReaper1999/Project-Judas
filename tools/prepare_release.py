#!/usr/bin/env python3
"""Prepare immutable local release-candidate files. Never publishes or tags."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import tarfile

import package_sdk
from release_manifest import BASE, CHANNELS, validate, verify_download


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def archive(directory, output, epoch):
    files = sorted(p for p in directory.rglob('*') if p.is_file())
    if any(p.is_symlink() for p in directory.rglob('*')):
        raise ValueError('Release SDK may not contain symbolic links')
    top=output.name.removesuffix('.tar.gz')
    with output.open('wb') as raw,gzip.GzipFile(fileobj=raw,mode='wb',filename='',mtime=epoch,compresslevel=9) as gz,tarfile.open(fileobj=gz,mode='w') as tar:
        for p in files:
            info=tar.gettarinfo(str(p),top+'/'+p.relative_to(directory).as_posix())
            info.uid=info.gid=0;info.uname=info.gname='';info.mtime=epoch
            info.mode=0o755 if p.stat().st_mode&0o111 else 0o644
            with p.open('rb') as f:tar.addfile(info,f)


def verify_source_archive(path, identity):
    with tarfile.open(path) as archive:
        info_file=archive.extractfile('JUDAS_SOURCE_INFO.json')
        files_file=archive.extractfile('JUDAS_SOURCE_FILES.json')
        if info_file is None or files_file is None:raise ValueError('Source provenance is missing')
        info_bytes=info_file.read(16385);files_bytes=files_file.read(16*1024*1024+1)
        if len(info_bytes)>16384 or len(files_bytes)>16*1024*1024:raise ValueError('Source provenance exceeds bounds')
        info=json.loads(info_bytes);inventory=json.loads(files_bytes)
        if (info.get('source_dirty') is not False or info.get('channel') != identity['channel'] or
            any(info.get(key)!=identity[field] for key,field in
                [('revision','source_revision'),('tree','source_tree'),('source_fingerprint','source_fingerprint')])):
            raise ValueError('Source archive embedded identity differs from SDK')
        if hashlib.sha256(files_bytes).hexdigest()!=info.get('source_files_sha256') or inventory.get('schema')!=1:
            raise ValueError('Source archive inventory is invalid')
        expected={}
        for row in inventory['files']:
            package_sdk.relative_path(row['path'])
            if row['path'] in expected:raise ValueError('Duplicate source path')
            expected[row['path']]=row['sha256']
        seen=set()
        for member in archive.getmembers():
            if not member.isfile() or member.name in seen:raise ValueError('Unsafe or duplicate source archive member')
            seen.add(member.name)
            if member.name in ('JUDAS_SOURCE_INFO.json','JUDAS_SOURCE_FILES.json'):continue
            if member.name not in expected:raise ValueError('Uninventoried source archive file')
            with archive.extractfile(member) as stream:actual=hashlib.file_digest(stream,'sha256').hexdigest()
            if actual!=expected[member.name]:raise ValueError('Source archive checksum mismatch: '+member.name)
        if seen-set(('JUDAS_SOURCE_INFO.json','JUDAS_SOURCE_FILES.json'))!=set(expected):raise ValueError('Missing source archive file')


def prepare(args):
    if not 0 <= args.epoch <= 4102444800:raise ValueError('Invalid source timestamp')
    output=args.output.resolve()
    if output.exists():raise ValueError('Candidate output already exists; choose a fresh directory')
    sdk = args.linux_sdk
    package_sdk.verify(sdk)
    first = package_sdk.identity(sdk/'BUILD_INFO.json', release=True)
    if first['platform'] != 'Linux' or first['architecture'] != 'x86_64' or first['channel'] != args.channel:
        raise ValueError('Expected a Linux x86-64 SDK for the candidate channel')
    canonical = json.loads((sdk/'engine-version.json').read_text())
    if any(first[key] != canonical[key] for key in ('version','maturity','release_name')):
        raise ValueError('Official release identity differs from authoritative engine-version.json')
    if first['channel']=='Alpha' and first['version'].startswith('2.') and first.get('vulkan') is not True:
        raise ValueError('Alpha V2 distribution requires a dual-backend Linux SDK')
    qualification=json.loads(args.qualification.read_text())
    if qualification.get('source_revision')!=first['source_revision'] or qualification.get('passed') is not True:
        raise ValueError('Exact-candidate successful qualification receipt required')
    source_receipt=json.loads(args.source_archive.with_name(args.source_archive.name+'.build.json').read_text())
    if source_receipt['source']['revision']!=first['source_revision'] or source_receipt['sha256']!=sha(args.source_archive):
        raise ValueError('Source archive does not match binary candidate')
    verify_source_archive(args.source_archive,first)
    notes=args.notes.read_text(encoding='utf-8')
    output.mkdir(parents=True)
    artifacts=[]
    try:
        name = f'JudasEngine-{first["version"]}-{args.channel}-Linux-x86_64.tar.gz'
        target = output/name
        archive(sdk, target, args.epoch)
        artifacts.append({'name':name,'platform':'Linux','architecture':'x86_64','bytes':target.stat().st_size,'sha256':sha(target),'url':BASE+'download/v'+first['version']+'/'+name})
        target=output/f'JudasEngine-{first["version"]}-Source.tar.gz'
        shutil.copyfile(args.source_archive,target)
        artifacts.append({'name':target.name,'platform':'Source','architecture':'any','bytes':target.stat().st_size,'sha256':sha(target),'url':BASE+'download/v'+first['version']+'/'+target.name})
        manifest={'schema':1,'channel':args.channel,'version':first['version'],'tag':'v'+first['version'],'source_revision':first['source_revision'],
                  'compatibility':{'update_protocol':1,'engine_major':int(first['version'].split('.')[0])},'notes':notes[:8192],
                  'notes_url':BASE+'tag/v'+first['version'],'artifacts':artifacts}
        validate(manifest,args.channel)
        (output/'release-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        (output/'RELEASE_NOTES.md').write_text(notes,encoding='utf-8')
        shutil.copyfile(args.qualification,output/'QUALIFICATION.json')
        (output/'CANDIDATE.json').write_text(json.dumps({'schema':1,'state':'awaiting-operator-approval','source_revision':first['source_revision'],'version':first['version'],'channel':args.channel,'publication_authorized':False,'sdk_identities':[first]},indent=2)+'\n')
        for a in artifacts:verify_download(manifest,output/a['name'])
        files=sorted(p for p in output.iterdir() if p.is_file())
        (output/'SHA256SUMS').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in files))
    except Exception:
        # Keep failure evidence/output, clearly incomplete; never silently repair.
        (output/'INCOMPLETE.txt').write_text('Preparation failed. Do not publish. See command diagnostics.\n')
        raise
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--linux-sdk',type=Path,required=True)
    p.add_argument('--source-archive',type=Path,required=True)
    p.add_argument('--qualification',type=Path,required=True)
    p.add_argument('--notes',type=Path,required=True)
    p.add_argument('--channel',choices=CHANNELS,default='Alpha')
    p.add_argument('--epoch',type=int,required=True,help='Source commit timestamp, not the wall clock')
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    try:
        manifest=prepare(args)
        print(json.dumps({'candidate':str(args.output),'source_revision':manifest['source_revision'],'state':'awaiting operator approval'},indent=2))
    except (OSError,ValueError,KeyError,tarfile.TarError) as error:p.exit(1,str(error)+'\n')

if __name__=='__main__':main()
