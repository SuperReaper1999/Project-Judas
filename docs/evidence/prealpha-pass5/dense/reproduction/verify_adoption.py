#!/usr/bin/env python3
"""Verify the named namespace/include/format-only adoption from the measured prototype."""
from pathlib import Path
import hashlib,json,re
r=Path.cwd();d=r/'.cache/prealpha-pass5/dense'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
# Ordinary strings/chars are single tokens; comments and spacing do not affect this proof.
def tokens(s):
 return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_0-9]+|[^\s]',re.sub(r'//[^\n]*|/\*.*?\*/','',s,flags=re.S))
pairs=[]
for old,new,kind in [('ImpactCandidateCache.h','src/physics/ImpactCandidateCache.h','helper'),('ImpactCandidateCacheTests.cpp','tests/ImpactCandidateCacheTests.cpp','test'),('WorldImpactEvents.fat-cache.cpp','src/physics/WorldImpactEvents.cpp','events')]:
 a=(d/old).read_text();b=(r/new).read_text()
 if kind=='helper':
  b=b.replace('#include <utility>\n','').replace('namespace JudasPhysicsDetail {','',1)
  # Remove the sole matching outer namespace end; all class/method braces remain.
  pos=b.rfind('} // namespace JudasPhysicsDetail');assert pos>=0;b=b[:pos]+b[pos:].replace('} // namespace JudasPhysicsDetail','',1)
 if kind=='test':b=b.replace('#include "physics/ImpactCandidateCache.h"','#include "ImpactCandidateCache.h"').replace('using JudasPhysicsDetail::ImpactCandidateCache;','')
 aa,bb=tokens(a),tokens(b)
 assert aa==bb,(old,new)
 pairs.append({'prototype':str((d/old).relative_to(r)),'prototypeSha256':sha(d/old),'production':new,'productionSha256':sha(r/new),'tokensEqualAfterNamedNonsemanticChanges':True,'tokenCount':len(aa)})
result={'schema':1,'allowedChanges':['JudasPhysicsDetail namespace around helper and explicit test using declaration','direct <utility> include for std::move','project-relative test include','format/comments only'],'formatter':'clang-format23.1.3 using existing src/physics/.clang-format; no historical file formatting','files':pairs}
(d/'ADOPTION_PROOF.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
