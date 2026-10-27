#!/usr/bin/env python3
"""Assemble a successful, explicitly selected CI preparation run; no publication."""
import argparse
import json
from pathlib import Path
import shutil
import tarfile
import tempfile
from types import SimpleNamespace
import prepare_release


def one(root, name):
    paths=list(root.rglob(name))
    if len(paths)!=1:raise ValueError('Expected exactly one '+name)
    return paths[0]


def extract(path, destination):
    with tarfile.open(path) as tar:
        for item in tar.getmembers():
            relative=Path(item.name)
            if relative.is_absolute() or '..' in relative.parts or not (item.isfile() or item.isdir()):
                raise ValueError('Unsafe SDK intermediate archive member')
            target=destination/relative
            if item.isdir():target.mkdir(parents=True,exist_ok=True);continue
            target.parent.mkdir(parents=True,exist_ok=True)
            with tar.extractfile(item) as source,target.open('wb') as output:shutil.copyfileobj(source,output)
            target.chmod(0o755 if item.mode&0o111 else 0o644)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--inputs',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--accepted-commit',required=True);a=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='judas-release-assemble-') as tmp:
        root=Path(tmp)
        extract(one(a.inputs,'linux-sdk.tar.gz'),root)
        extract(one(a.inputs,'windows-sdk.tar.gz'),root)
        sdk=root/'linux-sdk';identity=json.loads((sdk/'BUILD_INFO.json').read_text())
        if identity['source_revision']!=a.accepted_commit:raise ValueError('CI SDK revision differs from approval')
        gates=[p for p in a.inputs.rglob('RESULTS.json') if p.parent.name=='release-qualification']
        if len(gates)!=1 or json.loads(gates[0].read_text()).get('passed') is not True:raise ValueError('Missing successful coordinated qualification')
        workflows=one(a.inputs,'COMPLETE.json')
        if json.loads(workflows.read_text()).get('passed') is not True:raise ValueError('SDK workflow qualification failed')
        source=one(a.inputs,'source.tar.gz')
        with tarfile.open(source) as tar:epoch=tar.getmembers()[0].mtime
        q=root/'QUALIFICATION.json'
        q.write_text(json.dumps({'source_revision':a.accepted_commit,'passed':True,'Linux':'CI native gates and actual SDK/editor/export workflows',
            'Windows':'CI MSVC SDK/tool/selected native contracts; desktop/GPU qualification is a separate final operator requirement',
            'humanAcceptance':'Required separately by protected publication environment and exact main source approval'},indent=2)+'\n')
        prepare_release.prepare(SimpleNamespace(linux_sdk=sdk,windows_sdk=root/'windows-sdk',source_archive=source,
            notes=Path(__file__).resolve().parents[1]/'docs/ALPHA_V1_0_0_NOTES.md',qualification=q,
            channel=identity['channel'],epoch=epoch,output=a.output))

if __name__=='__main__':main()
