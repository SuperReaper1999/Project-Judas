#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$source_dir/../../.." && pwd)
output=${1:?Usage: qualify_momentum_geometry.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
bash "$source_dir/qualify_internal_transport.sh" "$output/controls"
"$output/controls/CutMassMomentumTests" --reference | tee "$output/controls/CutMassMomentumReferenceTests.log"
mapfile -t objects < "$output/controls/objects.list"
flags=(-std=c++17 -O3 -Wall -Wextra -Wpedantic -Werror)
probe_budget=${JUDAS_FLUID_PROBE_SECONDS:-180}
if [[ ${JUDAS_FLUID_SANITIZE:-0} == 1 ]]; then
 flags=(-std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic -Werror)
 probe_budget=${JUDAS_FLUID_PROBE_SECONDS:-600}
fi
for name in SustainedPressureProbe CutMomentumBasinProbe CutDistanceGeometryProbe CutFastResidualOracle; do
 "${CXX:-c++}" "${flags[@]}" -I"$source_dir" \
  -I"$repo/docs/evidence/fluid-moving-grid-20261103/shared-cut-regions" \
  "$source_dir/$name.cpp" "${objects[@]}" -o "$output/$name"
done
python3 - "$output/CutMomentumBasinProbe" "$output/state" <<'PY'
import subprocess, sys
for arguments in [('0',),('-1',),('3garbage',),('not-a-number',),('3','--unknown')]:
 result=subprocess.run([sys.argv[1],sys.argv[2],*arguments],capture_output=True,text=True)
 if result.returncode!=2: raise SystemExit('invalid probe invocation did not reject: '+repr(arguments))
print('All 5 diagnostic invocation negative controls pass.')
PY
python3 "$source_dir/qualify_fast_residual.py" "$output/CutFastResidualOracle" | tee "$output/oracle.log"
run() {
 local name=$1
 shift
 set +e
 /usr/bin/time -v -o "$output/$name.resource" timeout "${probe_budget}s" "$@" > "$output/$name.log" 2>&1
 local result=$?
 set -e
 printf '%s\n' "$result" > "$output/$name.exit"
 return "$result"
}
# The three-interval motion diagnostic MUST use the same geometry and source
# as its actual admitted pressure states. It is not a coupled momentum run.
run pressure "$output/SustainedPressureProbe" 3 both "$output/state" --each
run momentum "$output/CutMomentumBasinProbe" "$output/state" 3 --fast "$output/momentum-fast"
run momentum-reference "$output/CutMomentumBasinProbe" "$output/state" 3 --reference "$output/momentum-reference"
python3 - "$output" <<'PY'
import json, math, pathlib, struct, sys
root=pathlib.Path(sys.argv[1]); rows=[]
for rotated in (0,1):
 for step in (1,2,3):
  for patterned in (0,1):
   suffix=f'-{rotated}-{step}-{patterned}.bin'
   a=(root/('momentum-fast'+suffix)).read_bytes()
   b=(root/('momentum-reference'+suffix)).read_bytes()
   if len(a)!=len(b) or len(a)%16: raise SystemExit('momentum comparison dimensions differ')
   av=struct.iter_unpack('=dd',a);bv=struct.iter_unpack('=dd',b)
   maximum=max(abs(math.fsum((x,y,-z,-w))) for (x,y),(z,w) in zip(av,bv))
   if not math.isfinite(maximum) or maximum>1e-10: raise SystemExit('reference momentum comparison failed')
   rows.append(dict(rotated=rotated,step=step,patterned=patterned,maximumVelocityDifference=maximum,byteIdentical=a==b))
(root/'momentum-comparison.json').write_text(json.dumps(rows,indent=2)+'\n')
print('All 12 original-geometry velocity comparisons pass the 1e-10 m/s gate.')
PY
run distance "$output/CutDistanceGeometryProbe"
