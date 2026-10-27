#!/usr/bin/env python3
"""Prepare immutable local release-candidate files. Never publishes or tags."""
import argparse
import datetime
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import zipfile

import package_sdk
from release_manifest import BASE, CHANNELS, validate, verify_download


def sha(path):
    with Path(path).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def archive(directory, output, epoch):
    files = sorted(p for p in directory.rglob('*') if p.is_file())
    if any(p.is_symlink() for p in directory.rglob('*')):
        raise ValueError('Release SDK may not contain symbolic links')
    if output.name.endswith('.zip'):
        # ZIP cannot represent epochs before 1980. No host paths/timestamps.
        stamp = datetime.datetime.fromtimestamp(max(epoch,315532800),datetime.timezone.utc).timetuple()[:6]
        with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for p in files:
                entry=zipfile.ZipInfo(output.stem+'/'+p.relative_to(directory).as_posix(),stamp)
                entry.create_system=3;entry.external_attr=(0o100755 if p.suffix.lower()=='.exe' else 0o100644)<<16
                entry.compress_type=zipfile.ZIP_DEFLATED
                z.writestr(entry,p.read_bytes())
    else:
        top=output.name.removesuffix('.tar.gz')
        with output.open('wb') as raw,gzip.GzipFile(fileobj=raw,mode='wb',filename='',mtime=epoch,compresslevel=9) as gz,tarfile.open(fileobj=gz,mode='w') as tar:
            for p in files:
                info=tar.gettarinfo(str(p),top+'/'+p.relative_to(directory).as_posix())
                info.uid=info.gid=0;info.uname=info.gname='';info.mtime=epoch
                info.mode=0o755 if p.stat().st_mode&0o111 else 0o644
                with p.open('rb') as f:tar.addfile(info,f)


def prepare(args):
    if not 0 <= args.epoch <= 4102444800:raise ValueError('Invalid source timestamp')
    output=args.output.resolve()
    if output.exists():raise ValueError('Candidate output already exists; choose a fresh directory')
    sdks=[('Linux',args.linux_sdk),('Windows',args.windows_sdk)]
    identities=[]
    for platform,directory in sdks:
        package_sdk.verify(directory)
        identity=package_sdk.identity(directory/'BUILD_INFO.json',release=True)
        if identity['platform']!=platform or identity['channel']!=args.channel:
            raise ValueError('SDK platform/channel does not match candidate')
        canonical=json.loads((directory/'engine-version.json').read_text())
        if any(identity[key]!=canonical[key] for key in ('version','maturity','release_name')):
            raise ValueError('Official release identity differs from authoritative engine-version.json')
        identities.append(identity)
    first=identities[0]
    for identity in identities[1:]:
        for field in ('source_revision','source_tree','source_fingerprint','version','maturity','channel'):
            if identity[field]!=first[field]:raise ValueError('SDKs disagree: '+field)
    qualification=json.loads(args.qualification.read_text())
    if qualification.get('source_revision')!=first['source_revision'] or qualification.get('passed') is not True:
        raise ValueError('Exact-candidate successful qualification receipt required')
    source_receipt=json.loads(args.source_archive.with_name(args.source_archive.name+'.build.json').read_text())
    if source_receipt['source']['revision']!=first['source_revision'] or source_receipt['sha256']!=sha(args.source_archive):
        raise ValueError('Source archive does not match binary candidate')
    notes=args.notes.read_text(encoding='utf-8')
    output.mkdir(parents=True)
    artifacts=[]
    try:
        for platform,directory in sdks:
            name=f'JudasEngine-{first["version"]}-{args.channel}-{platform}-x86_64'+('.zip' if platform=='Windows' else '.tar.gz')
            target=output/name;archive(directory,target,args.epoch)
            artifacts.append({'name':name,'platform':platform,'architecture':'x86_64','bytes':target.stat().st_size,'sha256':sha(target),'url':BASE+'download/v'+first['version']+'/'+name})
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
        (output/'CANDIDATE.json').write_text(json.dumps({'schema':1,'state':'awaiting-operator-approval','source_revision':first['source_revision'],'version':first['version'],'channel':args.channel,'publication_authorized':False,'sdk_identities':identities},indent=2)+'\n')
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
    p.add_argument('--windows-sdk',type=Path,required=True)
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
    except (OSError,ValueError,KeyError) as error:p.exit(1,str(error)+'\n')

if __name__=='__main__':main()
