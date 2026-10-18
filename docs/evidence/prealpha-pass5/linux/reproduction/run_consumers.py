from pathlib import Path
if not __debug__:
    raise SystemExit('This qualification runner requires normal Python execution, without -O/PYTHONOPTIMIZE')
import importlib.util, subprocess
root=Path.cwd()
spec=importlib.util.spec_from_file_location('qualified',root/'.cache/prealpha-pass5/linux/run_final.py')
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
soft={'SDL_VIDEODRIVER':'offscreen','LIBGL_ALWAYS_SOFTWARE':'1'}
assert r.run('shooter_real_scripts',[root/'build/judas_shooter_game_tests'],extra=soft)
assert r.run('shooter_application',[root/'build/judas_shooter_application_tests',root/'projects/shooter_game/shooter_game.judasproj',r.OUT/'shooter-app'],extra=soft,timeout=120)
script=r.OUT/'lastlight-startup.txt'
script.write_text('FRAMES 150\nFRAME_SECONDS .016666667\nLOG_EVERY 0\nWAIT_SERVICES scene 10000 1\nACTION camera_toggle 1 80 81\nSCREENSHOT 110 '+str(r.OUT/'lastlight.png')+'\n')
paths=subprocess.check_output(['git','ls-files','-z','--','projects/lastlight'],text=True).split('\0')
pins={p:r.digest(root/p) for p in paths if p}
assert r.run('lastlight_startup',[root/'build/linux-sdk/judas',root/'projects/lastlight/lastlight.judasproj'],extra={**soft,'JUDAS_TEST_SCRIPT':str(script)},timeout=120)
assert pins=={p:r.digest(root/p) for p in pins},'Consumer tracked source changed'
log=(r.EV/'logs/lastlight_startup.log').read_text()
assert not any(line.startswith('script asset=') for line in log.splitlines()), 'Lastlight runtime script fault'
assert '[TestHarness] service scene ready' in log, 'Missing Lastlight scene readiness'
assert '[TestHarness] Wrote screenshot: '+str(r.OUT/'lastlight.png') in log and (r.OUT/'lastlight.png').is_file(), 'Missing actual screenshot'
r.save(r.EV/'CONSUMER_INPUTS.json',{'lastlightTrackedFiles':pins,'lastlightScope':'Original ordinary project startup and presentation-mode input only; not a boarding/combat/flight or human acceptance proof.','archiveSha256':r.digest(root/'build/libjudas_engine.a')})
