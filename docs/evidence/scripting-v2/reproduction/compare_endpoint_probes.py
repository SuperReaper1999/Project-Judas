#!/usr/bin/env python3
"""Compare native before/after trajectories independently of cold-save quality."""
import argparse, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('baseline',type=Path);p.add_argument('candidate',type=Path);a=p.parse_args()
source=lambda directory:json.loads((directory/'RESULTS.json').read_text())['source_sha256']
assert source(a.baseline)==source(a.candidate), 'Observer fixture source changed between builds'
rows=[]
for mode in ['full','read','negative']:
    for prefix in ['PHYS ','SCRIPT ','AI ','NATIVE ']:
        left=[line for line in (a.baseline/(mode+'.log')).read_text().splitlines() if line.startswith(prefix)]
        right=[line for line in (a.candidate/(mode+'.log')).read_text().splitlines() if line.startswith(prefix)]
        rows.append(dict(mode=mode,kind=prefix.strip(),records=len(left),same=left==right,
          baseline_sha256=hashlib.sha256(('\n'.join(left)+'\n').encode()).hexdigest(),
          candidate_sha256=hashlib.sha256(('\n'.join(right)+'\n').encode()).hexdigest(),
          first_mismatch=next(([l,r] for l,r in zip(left,right) if l!=r),None)))
result=dict(pass_=all(row['same'] for row in rows),comparisons=rows,
  note='Cold versus uninterrupted quality is recorded independently. This gate compares each sequence to its exact accepted-source counterpart; entities chunk hashes are excluded because new fixture roots acquire distinct asset IDs.')
(a.candidate/'BASELINE_COMPARISON.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
assert result['pass_'], 'Candidate changed observed native or scripting continuation'
