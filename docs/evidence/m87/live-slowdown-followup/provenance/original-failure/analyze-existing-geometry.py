from pathlib import Path
import json,hashlib
base=Path('/tmp/m87-live-diagnostics');result={}
for dirname in ['dense-fixed-tight-query','dense-rotation-profile']:
 p=base/dirname/'profile-final.json';d=json.loads(p.read_text());frames=[f for f in d['frames']if f.get('fixed',{}).get('steps')]
 def span(f):return max(s['end_ns']-s['start_ns']for s in f['fixed']['steps'])
 frames.sort(key=span);out={'profile_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'retained_fixed_frames':len(frames),'predicate_counters_available':False,'note':'Existing M56 captures do not publish geometryPredicates/geometryExactFallbacks/numericGapFallbacks/preparedOrientationMisses/unresolved, so these cannot be inferred per frame.'}
 for kind,f in [('median_retained_frame',frames[len(frames)//2]),('worst_retained_frame',frames[-1])]:
  counters={c['name']:c['value']for c in f['counters']};out[kind]={'frame':f['id'],'step_ids':[s['id']for s in f['fixed']['steps']],'maximum_fixed_span_ms':span(f)*1e-6,'counters':{k:v for k,v in counters.items()if any(t in k for t in ['CCD geometry','CCD search','island discovery','impact events','impact solve','impact queries','actual-contact','sampling','orientation','observation'])},'physics_scopes_ms':[{'name':s['name'],'inclusive_ms':s['inclusive_ns']*1e-6}for s in f['scopes']if 'Physics' in s['name']]}
 result[dirname]=out
result['original_human_stall_core']=json.loads((base/'original-core-predicate-delta.json').read_text())
result['conclusion']='Exact arithmetic fallbacks did not occur in the captured unfinished stalled step; all3187842 observed predicates resolved using intervals. Current profile files cannot provide a per-step normal-vs-worst predicate comparison without extra publication. Inclusive geometry times overlap discovery/search and must not be added to them.'
(base/'geometry-cost-summary.json').write_text(json.dumps(result,indent=2)+'\n')
files=[base/'analyze-existing-geometry.py',base/'geometry-cost-summary.json',base/'read-core-predicates.gdb',base/'read-all-core-predicates.gdb',base/'read-core-step-predicates.gdb',base/'original-core-predicate-tls.txt',base/'original-core-all-predicate-tls.txt',base/'original-core-predicate-delta.json',base/'original-core-predicate-delta.txt',base/'original-step-disassembly.txt']
(base/'geometry-cost-fingerprints.json').write_text(json.dumps({str(f):hashlib.sha256(f.read_bytes()).hexdigest()for f in files},indent=2)+'\n')
print('Saved',base/'geometry-cost-summary.json')
