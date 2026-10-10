#!/usr/bin/env python3
"""Compact kit-only fixtures. Calls the shipped author services; no raw archive writer."""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('tool');p.add_argument('kit');p.add_argument('output');a=p.parse_args()
root=Path(a.output);root.mkdir(parents=True,exist_ok=True)
def run(*args):
    result=subprocess.run([a.tool,*map(str,args)],capture_output=True,text=True)
    if result.returncode:
        print(result.stdout+result.stderr,end='');result.check_returncode()
    print(result.stdout,end='');return result.stdout.strip()
def write(path,text):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)
def add(project,relative,kind,local,text):
    identity=run('identity',project,local);path=project.parent/relative;write(path,text)
    write(Path(str(path)+'.judasmeta'),f'JudasAssetMeta 1\nid "{identity}"\ntype {kind}\nsource "M76 original dependency fixture"\n');return identity
common=Path(run('new',a.kit,root/'Common Library','common'))
helper=add(common,'Assets/scripts/visit.js','script','a'*32,'export const value=7;\n')
private=add(common,'Assets/scripts/private.js','script','b'*32,'export const secret=99;\n')
manifest=json.loads((common.parent/'mod.json').read_text());manifest.update(executable=True,author='Judas',license='CC0',exports={'helper':{'kind':'script','value':helper}})
write(common.parent/'mod.json',json.dumps(manifest,indent=2));common_pack=root/'common.judasmod';run('package',common,common_pack)
consumer=Path(run('new',a.kit,root/'Dependent Prop','consumer'));run('bind-dependency',consumer,common_pack)
manifest=json.loads((consumer.parent/'mod.json').read_text());manifest.update(executable=True,author='Judas',license='CC0',dependencies=[{'id':'common','version':'1.0.0'}],imports=[{'owner':'common','name':'helper','reference':{'kind':'script','value':helper}}])
write(consumer.parent/'mod.json',json.dumps(manifest,indent=2))
script=add(consumer,'Assets/scripts/visit.js','script','a'*32,"import {value} from 'mod:common/helper';\nimport {signals} from 'judas';\nexport default class {constructor(){this.state={value:0};}fixedUpdate(){if(!this.state.value){this.state.value=value;signals.emit('lab.visit',{amount:value});}}}\n")
# Author a script slot on the kit's explicitly published ordinary prop template.
# This edits a known authored field, not IDs in opaque cooked bytes or JS strings.
source=(consumer.parent/'Kit/Assets/prefabs/drop.judasprefab').read_text()
line=next(line for line in source.splitlines() if line.startswith('object '))
source=source.replace(line,line+f'\n  scripts 1\n  script.0.id 1\n  script.0.asset "{script}"\n  script.0.enabled true\n  script.0.properties "{{}}"',1)
prefab=add(consumer,'Assets/prefabs/drop.judasprefab','prefab','b'*32,source)

manifest['contributions']=[{'point':'props','name':'dependency-prop','reference':{'kind':'prefab','value':prefab},'data':{'label':'Declared module dependency'}}];manifest['exports']={'prop':{'kind':'prefab','value':prefab}}
write(consumer.parent/'mod.json',json.dumps(manifest,indent=2));run('package',consumer,root/'consumer.judasmod')
write(root/'dependency-fixtures.json',json.dumps({'common':str(common),'consumer':str(consumer),'helper':helper,'private':private,'consumerScript':script,'prefab':prefab},indent=2))
