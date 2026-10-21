from pathlib import Path
import json,hashlib,struct
r=Path(__file__).resolve().parent
exec(compile((r/'sphere_box_exact.py').read_text().split('\ntraces=')[0],str(r/'sphere_box_exact.py'),'exec'))
def overlap_certificate(a,b,t0,t1):
 mid=(t0+t1)/2;ps,_=smooth(a,mid);pb,q=smooth(b,mid);rot=rotation(q);local=[dot(sub(ps,pb),axis) for axis in rot];half=vf(b['shape']['half_extents']);u=[min(h,max(-h,x)) for x,h in zip(local,half)]
 sp,_,_=precise_polys(a,t0,t1);bp,raw,norm=precise_polys(b,t0,t1)
 difference=[psub(pmul(psub(sp[i],bp[i]),norm),psum(pscale(raw[j][i],u[j]) for j in range(3))) for i in range(3)]
 radius=F(f32(a['shape']['radius']));p=psub(psum(pmul(d,d) for d in difference),pscale(pmul(norm,norm),radius*radius));upper=max(bernstein(p))
 return {'certified_continuously_overlapping':upper<0,'upper_bernstein_exact':str(upper),'fixed_local_box_witness':[str(x) for x in u],'polynomial_degree':len(p)-1,'method':'A fixed point inside the box remains strictly inside the sphere for the whole interval; exact negative Bernstein upper bound after multiplication by positive quaternion norm squared.'}
box=fixture([0,0,0],[0,0,0]);inside=fixture([F(5,4),0,0],[0,0,0],.5);outside=fixture([3,0,0],[0,0,0],.5)
assert overlap_certificate(inside,box,F(0),F(1))['certified_continuously_overlapping'];assert not overlap_certificate(outside,box,F(0),F(1))['certified_continuously_overlapping']
traces=[json.loads(l) for l in (r/'pair-trace.jsonl').read_text().splitlines()];j=json.loads((r/'sphere-box-exact.json').read_text());rows=[]
for q in j['queries']:
 if not q['selected_event']:continue
 e=q['selected_event'];trace=traces[q['record']-1];a,b=trace['body_a'],trace['body_b'];t0=F(f64(trace['start']));t1=F(e['event_time_s']);proof=q['whole_prefix_proof'];record={'event':e['chronological_event_in_step'],'island_bodies':e['island_bodies'],'contact_rows':e['contact_rows']}
 if proof['certified']:record['classification']='certified no continuous collision through native reported arrival';record['proof']=proof
 elif proof.get('reason')=='continuous start already in contact':
  p=overlap_certificate(a,b,t0,t1);record['classification']='certified same continuous contact throughout query' if p['certified_continuously_overlapping'] else 'unresolved existing contact';record['proof']=p
 else:record['classification']='continuous arrival possible; first arrival not certified'
 record['end_of_step_separated']=q['continuous_finish']['separated'];rows.append(record)
replay=json.loads((r/'pair-replay.json').read_text());q=next(q for q in j['queries'] if q['selected_event'] and q['selected_event']['chronological_event_in_step']==18);p=next(x for x in replay['queries'] if x['record']==q['record']);hit=next(x for x in p['probes'] if x['time']==p['expected_result']);miss=max((x for x in p['probes'] if not x['touching_boolean'] and x['time']<hit['time']),key=lambda x:x['time']);differences=[]
for who in ['a','b']:
 for attr in ['position_bits','orientation_bits_wxyz']:
  for i,(x,y) in enumerate(zip(miss[who][attr],hit[who][attr])):
   if x!=y:differences.append({'body':who,'field':attr,'component':i,'before':x,'after':y,'difference_float_value':f32(y)-f32(x)})
assert len(differences)==1 and differences[0]['body']=='b' and differences[0]['field']=='position_bits' and differences[0]['component']==2 and differences[0]['after']==differences[0]['before']+1
out={'schema':'judas.continuous-contact-episode-diagnosis.v1','independentOverlapControls':2,'classified_events':rows,'single_coordinate_causal_control':{'event':18,'nativeBeforeTime':miss['time'],'nativeArrivalTime':hit['time'],'nativeBeforeTouching':False,'nativeAfterTouching':True,'onlyGeometricInputDifference':differences,'continuousPrefixCertifiedSeparated':True,'continuousGapAtNativeArrivalMeters':next(x['continuous']['distance_to_closed_box_minus_radius_m'] for x in q['probes'] if x['time']==hit['time']),'island_bodies':q['selected_event']['island_bodies'],'contact_rows':q['selected_event']['contact_rows']},'limits':'This proves the mechanism for the most repeatedly selected sphere-box pair, not every event or an end-to-end repair. A prefix separation certificate must not discard an entire future interval.'}
(r/'episode-proof.json').write_text(json.dumps(out,indent=2)+'\n')
for x in rows:print(x['event'],x['classification'],x['island_bodies'])
print('Single-coordinate causal control',json.dumps(out['single_coordinate_causal_control']))
