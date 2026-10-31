"""Copy Traversal unchanged except for public pose/status observations.
Usage: python3 ... PROJECT FRESH_OUTPUT
"""
from pathlib import Path
import shutil
import sys

source, output = (Path(p).resolve() for p in sys.argv[1:])
if output.exists():
    raise SystemExit('Output must be fresh')
shutil.copytree(source, output, ignore=shutil.ignore_patterns('Saves', '.cache'))
controls = output / 'Assets/controls.js'
text = controls.read_text(encoding='utf-8')
if text.count(' uiUpdate(){') != 1:
    raise SystemExit('Expected one controls uiUpdate entry')
text = text.replace('{input,scenes,saves,ui}', '{input,scenes,saves,ui,entity,console}')
text = text.replace(' uiUpdate(){', ''' uiUpdate(){
 console.log('BROWSER_PROBE '+JSON.stringify({position:entity(1).transform.position,
  velocity:entity(1).velocity,requestTwo:saves.status(2),status:this.state.request?saves.status(this.state.request):null}));''')
controls.write_text(text, encoding='utf-8')
print('Fixture:', output)
