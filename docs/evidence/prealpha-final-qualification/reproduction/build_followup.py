from pathlib import Path
import json,subprocess,time
r=Path.cwd();o=r/'.cache/prealpha-final-qualification';cmd=json.loads((o/'BUILD_COMMAND.json').read_text())
for t in ('judas_player_curved_locomotion_tests','judas_spacecraft_frame_integration_tests','judas_boundary_migration_tests','judas_powertrain_application_tests'):cmd+=['--extra-target',t]
start=time.monotonic()
with (o/'followup-build.log').open('w') as f:code=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT).returncode
(o/'FOLLOWUP_BUILD_RESULT.json').write_text(json.dumps({'exit':code,'seconds':time.monotonic()-start,'command':cmd},indent=2)+'\n');print('Follow-up build',code)
