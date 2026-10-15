set debuginfod enabled off
set language c++
add-symbol-file /tmp/judas-m87-PhysicsWorld-debug.o 0
set $w=('PhysicsWorld::Impl'*)$rbp
python
import gdb,collections,json,math
w=gdb.parse_and_eval('$w').dereference()
def vector(v):
 a=v['_M_impl']['_M_start'];b=v['_M_impl']['_M_finish'];return [a[i] for i in range(int(b-a))]
def vec3(v):return [float(v[k]) for k in ['x','y','z']]
def length(v):return math.sqrt(sum(x*x for x in v))
counts=[int(x) for x in vector(w['impactCounts'])]
now=float(gdb.parse_and_eval('*(double*) (*(unsigned long*)($r12+16))'))
events=vector(w['scheduledImpacts'])
forward=sorted((float(x['time']),[int(x['key']['_M_elems'][k]) for k in range(4)]) for x in events if float(x['time'])<1)
bodies=vector(w['bodies']);physical=[];allbegins=set();nonforward=0;maxseg=0;linearEnergy=0;angularBound=0
for idx,body in enumerate(bodies):
 if not bool(body['alive']) or not bool(body['isDynamic']):continue
 rb=body['rigidBody'];v=vec3(rb['linearVelocity']);om=vec3(rb['angularVelocity']);im=float(rb['inverseMass']);r=float(body['boundingRadius']);mass=1/im if im>0 else 0
 linearEnergy+=.5*mass*length(v)**2;angularBound+=.5*mass*r*r*length(om)**2
 segs=vector(body['motion']['segments']);begins=[float(s['begin']) for s in segs];allbegins.update(begins);maxseg=max(maxseg,len(segs));nonforward+=sum(begins[i]<=begins[i-1] for i in range(1,len(begins)))
 physical.append({'slot':idx,'impact_island_inclusions':counts[idx],'position':vec3(rb['position']),'linear_velocity':v,'angular_velocity':om,'speed_mps':length(v),'angular_speed_radps':length(om),'mass_kg':mass,'bounding_radius_m':r,'motion_segments':len(segs),'last_segment_begins':begins[-12:]})
times=sorted(allbegins);deltas=[times[i]-times[i-1] for i in range(1,len(times))]
stats={f.name:int(w['stats'][f.name]) if w['stats'][f.name].type.code not in (gdb.TYPE_CODE_FLT,) else float(w['stats'][f.name]) for f in w['stats'].type.fields()}
result={'evidence':'Unmodified original human-live process core. Paused part-way through one fixed step; counters are unfinished step totals. No executable execution during extraction.', 'core':'/tmp/judas-m87-live-slow.core','binary':'/tmp/judas-m87-watch-ecpbqnfz/judas','debug_types':'/tmp/judas-m87-PhysicsWorld-debug.o','impl_address':str(gdb.parse_and_eval('$w')),'now_s':now,'fixed_dt_s':float(w['stepDuration']),'completed_sleep_assessments':int(w['sleepTraceStep']),'step_stats':stats,'body_inclusions':{'sum':sum(counts),'maximum':max(counts),'nonzero_bodies':sum(x>0 for x in counts),'histogram':dict(collections.Counter(counts))},'scheduled_events':len(events),'remaining_events_before_fixed_endpoint':len(forward),'first_25_remaining_events':forward[:25],'spinning_pairs':len(vector(w['spinningPairs'])),'support_pairs':len(vector(w['supportPairs'])),'trajectory_progress':{'unique_segment_begin_times':len(times),'minimum_positive_begin_time_delta_s':min(deltas) if deltas else None,'nonincreasing_same_body_segment_begins':nonforward,'maximum_body_segments':maxseg},'physical_summary':{'max_speed_mps':max(x['speed_mps'] for x in physical),'max_angular_speed_radps':max(x['angular_speed_radps'] for x in physical),'linear_energy_j':linearEnergy,'angular_energy_upper_bound_j':angularBound,'note':'Angular bound uses m*r^2 as inertia upper bound for radius-enclosed primitives; not claimed exact kinetic energy.'},'highest_inclusion_bodies':sorted(physical,key=lambda x:x['impact_island_inclusions'],reverse=True)[:20],'fastest_bodies':sorted(physical,key=lambda x:x['speed_mps'],reverse=True)[:10]}
with open('/tmp/m87-live-diagnostics/original-core.json','w') as out:json.dump(result,out,indent=2)
print(json.dumps({k:v for k,v in result.items() if k not in ['highest_inclusion_bodies','fastest_bodies']},indent=2))
end
