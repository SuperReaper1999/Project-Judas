#!/usr/bin/env python3
"""Read actual temporal observables; report raw changes without inventing gates."""
import argparse,csv,datetime,hashlib,json
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('--run',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
path=a.run/'temporal-quality.csv';r=list(csv.DictReader(path.open()))
if not r:raise SystemExit('no actual temporal observations')
floats=lambda key:[float(x[key])for x in r if x[key]!='']
energy=[(int(x['step']),float(x['mechanical_energy_J']))for x in r if x['mechanical_energy_J']!='']
initial=energy[0][1] if energy else None
increases=[(energy[i][0],energy[i][1]-energy[i-1][1])for i in range(1,len(energy))]
geometry=[x for x in r if x['geometry_sampled']=='1']
out={
 'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'csv':str(path.resolve()),'csv_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
 'observation_rows':len(r),'first_step':int(r[0]['step']),'last_step':int(r[-1]['step']),
 'sequence_complete':all(int(x['step'])==i for i,x in enumerate(r)),
 'selected_entity_counts':sorted(set(floats('selected_entities'))),
 'actual_live_body_counts':sorted(set(floats('live_native_bodies'))),
 'masses_kg':sorted(set(floats('mass_kg'))),
 'finite_all_steps':all(x['finite']=='1'for x in r),
 'plane_gravity_potential_valid_all_steps':all(x['potential_valid']=='1'for x in r),
 'actual_floor_available_all_steps':all(x['floor_available']=='1'for x in r),
 'actual_floor_supported_counts':sorted(set(floats('floor_supported'))),
 'fully_inside_floor_counts':sorted(set(floats('fully_inside_floor'))),
 'minimum_floor_clearance_m':min(floats('min_floor_clearance_m'),default=None),
 'minimum_clearance_step':min((x for x in r if x['min_floor_clearance_m']!=''),key=lambda x:float(x['min_floor_clearance_m']),default={}).get('step'),
 'maximum_linear_kinetic_J':max(floats('kinetic_linear_J'),default=None),
 'maximum_rotational_kinetic_J':max(floats('kinetic_rotational_J'),default=None),
 'initial_mechanical_energy_J':initial,
 'final_mechanical_energy_J':energy[-1][1]if energy else None,
 'maximum_energy_increase_over_initial_J':max((e-initial for _,e in energy),default=None)if initial is not None else None,
 'largest_one_step_energy_increase':max(increases,key=lambda x:x[1],default=None),
 'sampled_native_geometry_steps':[int(x['step'])for x in geometry],
 'maximum_sampled_native_penetration_m':max((float(x['deepest_actual_penetration_m'])for x in geometry),default=None),
 'sampled_uncertain_pair_total':sum(int(x['geometry_uncertain'])for x in geometry),
 'sampled_unsupported_pair_total':sum(int(x['geometry_unsupported'])for x in geometry),
 'notes':[
  'Observer executes outside recorded physics spans; these are diagnostic runs, not wall/FPS comparisons.',
  'Clearance uses actual finite authored-floor frame/support extents and detects already-crossed bodies even without overlapping manifolds.',
  'Native pair geometry sampled at step0 and every10 steps; unsampled times have no invented contact-penetration result.',
  'Energy covers tagged loose pieces only; no general whole-world conservation claim or acceptance threshold is inferred.',
  'Potential only published when actual sampled body gravity agrees with one common plane equilibrium.',
 ]
}
if a.output.exists():raise SystemExit('refusing to overwrite an analysis')
a.output.write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
