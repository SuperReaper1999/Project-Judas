#!/usr/bin/env python3
"""Explicitly author the lab policies from its current versioned content.
Run only after deliberate lab edits; this is not a validation hash updater.
Policy is excluded from content identity to avoid a self-referential hash.
"""
from pathlib import Path
import subprocess,json,copy
R=Path(__file__).resolve().parents[1];root=R/'projects/save_evolution_lab'
def path(v):return root/v/'save_evolution.judasproj'
def identity(v):return json.loads(subprocess.check_output([R/'build/judas_save','identity',path(v)],text=True))['content']
def save(v,policy):
 p=path(v);j=json.loads(p.read_text());j['data']['saveEvolution']=json.dumps(policy,separators=(',',':'));p.write_text(json.dumps(j,indent=2)+'\n')
v1=identity('v1');v2=identity('v2')
s0={'from':0,'to':1,'operations':[{'kind':'script','entity':10,'slot':1,'patch':[{'op':'move','from':'/salvage','path':'/inventory/ore'}]}]}
# RFC6902 does not create missing parent objects: create inventory explicitly first.
s0['operations'][0]['patch'].insert(0,{'op':'add','path':'/inventory','value':{}})
s1={'from':1,'to':2,'operations':[{'kind':'scene','patch':[{'op':'replace','path':'/data/objects/1/name','value':'Upgraded survivor'},{'op':'replace','path':'/data/objects/2/fields/render.color','value':[0.25,0.4,0.32]}]},{'kind':'script','entity':10,'slot':1,'patch':[{'op':'add','path':'/chapter','value':'arrival'}]}]}
p2={'format':1,'compatibility':'judas.save-evolution.lab','version':1,'sources':[{'content':v1,'compatibility':'','version':0}],'steps':[s0]}
p3={'format':1,'compatibility':'judas.save-evolution.lab','version':2,'sources':[{'content':v1,'compatibility':'','version':0},{'content':v2,'compatibility':'judas.save-evolution.lab','version':1}],'steps':[s0,s1]}
save('v2',p2);save('v3',p3)
p=copy.deepcopy(p3);p['sources']=[];save('incompatible',p)
p=copy.deepcopy(p3);p['steps'][1]['operations'].append({'kind':'script','entity':10,'slot':1,'patch':[{'op':'test','path':'/inventory/ore','value':-999}]});save('failure',p)
identities={'v1':v1,'v2':v2,'v3':identity('v3')}
(root/'SOURCE_IDENTITIES.json').write_text(json.dumps(identities,indent=2)+'\n')
print(json.dumps(identities,indent=2))
