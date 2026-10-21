from pathlib import Path
import hashlib,json
r=Path.cwd();w=Path(__file__).resolve().parent;p=r/'src/physics/WorldImpactEvents.cpp';s=p.read_text();(w/'WorldImpactEvents.original.cpp').write_text(s)
a=s.index('                // A velocity-changing island invalidates its old load guesses,')
b=s.index('                auto replaced = observationKeys;',a)
s=s[:a]+'''                // These are estimates of ORDINARY support impulses, not the
                // new event impulses. They remain numerical guesses for the next
                // fixed-step solve; actual geometry/normal/anchor matching there
                // decides whether they apply. Never store the event impulse here.
'''+s[b:]
s=s.replace('if (incremental && !velocityChanged[c.key.slotA] &&\n                                           !velocityChanged[c.key.slotB]) {','if (at->SameBodies(c.key)) {')
s=s.replace('untouched ordinary support guess, not an\n                                                         // impact load','ordinary support guess, not an impact load')
(w/'WorldImpactEvents.cpp').write_text(s)
(w/'MANIFEST.json').write_text(json.dumps({'original_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'variant_sha256':hashlib.sha256(s.encode()).hexdigest(),'purpose':'Causal hypothesis: retain geometrically matched ordinary force estimates across inelastic free-primitive events; never publish/replay event impulses as warm force. Not production qualified.'},indent=2)+'\n')
