#!/usr/bin/env python3
"""Explicit, guarded GitHub publication. Default is validation/dry-run only.

Only run after the operator has approved the exact candidate and promoted it.
GitHub's protected publication environment is an additional required boundary.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from release_manifest import REPOSITORY, read_manifest, verify_download


def validate_candidate(directory, accepted_commit):
    if not re.fullmatch('[0-9a-f]{40}', accepted_commit):
        raise ValueError('Approval must name an exact 40-character source commit')
    if (directory/'INCOMPLETE.txt').exists():raise ValueError('Incomplete candidate cannot be published')
    manifest=read_manifest(directory/'release-manifest.json')
    candidate=json.loads((directory/'CANDIDATE.json').read_text())
    if candidate.get('state')!='awaiting-operator-approval' or any(candidate.get(k)!=manifest[k] for k in ('version','source_revision','channel')):
        raise ValueError('Candidate identity differs from release manifest')
    if accepted_commit!=manifest['source_revision']:raise ValueError('Operator approval names a different candidate')
    recorded=set()
    for line in (directory/'SHA256SUMS').read_text().splitlines():
        digest,name=line.split('  ',1)
        if not re.fullmatch('[0-9a-f]{64}',digest) or Path(name).name!=name or name in recorded or not (directory/name).is_file():
            raise ValueError('Invalid candidate checksum inventory')
        recorded.add(name)
        with (directory/name).open('rb') as f:actual=hashlib.file_digest(f,'sha256').hexdigest()
        if actual!=digest:raise ValueError('Candidate file changed: '+name)
    actual={p.name for p in directory.iterdir() if p.is_file()}-{'SHA256SUMS'}
    if recorded!=actual:raise ValueError('Candidate has missing/unrecorded files')
    for artifact in manifest['artifacts']:verify_download(manifest,directory/artifact['name'])
    qualification=json.loads((directory/'QUALIFICATION.json').read_text())
    if qualification.get('source_revision')!=accepted_commit or qualification.get('passed') is not True:
        raise ValueError('Candidate qualification is absent, failed or for another source')
    platforms={a['platform'] for a in manifest['artifacts']}
    if platforms!={'Linux','Windows','Source'}:raise ValueError('Initial desktop release requires both SDKs and source')
    return manifest


def gh(*args):
    return subprocess.check_output(['gh',*args],text=True)


def check_remote(manifest):
    branch='Experimental' if manifest['channel']=='Experimental' else 'main'
    current=json.loads(gh('api',f'repos/{REPOSITORY}/branches/{branch}'))
    if current['commit']['sha']!=manifest['source_revision']:
        raise ValueError('Candidate is not the exact accepted '+branch+' head; no reset/promotion is performed')
    environment=json.loads(gh('api',f'repos/{REPOSITORY}/environments/judas-publication'))
    rules=environment.get('protection_rules',[])
    if not any(r.get('type')=='required_reviewers' and r.get('reviewers') for r in rules):
        raise ValueError('Configure required human reviewers on the judas-publication environment first')
    gh('release','view','update-manifests','--repo',REPOSITORY)
    tag_probe=subprocess.run(['gh','api',f'repos/{REPOSITORY}/git/ref/tags/'+manifest['tag']],text=True,capture_output=True)
    if tag_probe.returncode==0:
        raise ValueError('Final tag already exists; refusing to replace/reuse it')
    if 'HTTP 404' not in tag_probe.stderr:
        raise ValueError('Could not establish that the final tag is absent')


def publish(directory, manifest):
    # A draft prevents advertising an incomplete upload. No existing tag/release
    # is clobbered. gh itself refuses an existing tag/release collision.
    tag=manifest['tag']
    title='Judas Engine '+manifest['channel']+' v'+manifest['version'].split('-')[0]
    command=['release','create',tag,'--repo',REPOSITORY,'--target',manifest['source_revision'],
             '--title',title,'--notes-file',str(directory/'RELEASE_NOTES.md'),'--draft','--latest=false']
    if manifest['channel']!='Stable':command.append('--prerelease')
    gh(*command,*[str(p) for p in sorted(directory.iterdir()) if p.is_file()])
    with tempfile.TemporaryDirectory(prefix='judas-published-integrity-') as tmp:
        for artifact in manifest['artifacts']:
            gh('release','download',tag,'--repo',REPOSITORY,'--pattern',artifact['name'],'--dir',tmp)
            verify_download(manifest,Path(tmp)/artifact['name'])
    gh('release','edit',tag,'--repo',REPOSITORY,'--draft=false','--latest='+('true' if manifest['channel']=='Stable' else 'false'))
    # Update the authoritative channel only after all published bytes verify.
    # The metadata release must be provisioned explicitly once by the operator.
    gh('release','view','update-manifests','--repo',REPOSITORY)
    with tempfile.TemporaryDirectory(prefix='judas-channel-manifest-') as tmp:
        path=Path(tmp)/(manifest['channel']+'.json')
        shutil.copyfile(directory/'release-manifest.json',path)
        gh('release','upload','update-manifests',str(path),'--repo',REPOSITORY,'--clobber')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('candidate',type=Path)
    p.add_argument('--accepted-commit',required=True)
    p.add_argument('--human-approval',default='')
    p.add_argument('--publish',action='store_true',help='Actually create final tag/release, only after explicit operator acceptance')
    args=p.parse_args()
    try:
        manifest=validate_candidate(args.candidate,args.accepted_commit)
        if not args.publish:
            print(json.dumps({'action':'dry-run only','version':manifest['version'],'commit':manifest['source_revision'],'publication':False},indent=2));return
        if args.human_approval!='PUBLISH '+manifest['tag']:
            raise ValueError('Explicit final operator approval phrase required')
        check_remote(manifest)
        publish(args.candidate,manifest)
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:p.exit(1,str(error)+'\n')

if __name__=='__main__':main()
