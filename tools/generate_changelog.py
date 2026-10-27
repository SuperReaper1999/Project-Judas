#!/usr/bin/env python3
"""Generate factual commit notes between explicit revisions; never invent acceptance."""
import argparse
from pathlib import Path
import re
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--base',required=True);p.add_argument('--head',default='HEAD');p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
def revision(value):
    resolved=subprocess.check_output(['git','rev-parse','--verify','--end-of-options',value+'^{commit}'],text=True).strip()
    if not re.fullmatch('[0-9a-f]{40}',resolved):raise ValueError('Commit identity unavailable')
    return resolved
base,head=revision(a.base),revision(a.head)
lines=subprocess.check_output(['git','log','--reverse','--format=%H%x09%s',base+'..'+head],text=True).splitlines()
text='# Source changelog\n\n'+base+' → '+head+'\n\nGenerated commit history; not evidence of human or platform acceptance.\n\n'
for line in lines:
    sha,title=line.split('\t',1);text+='- '+title+' (`'+sha+'`)\n'
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(text,encoding='utf-8')
