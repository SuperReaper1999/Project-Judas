"""Linux diagnostic: binary64 intermediates without changing the public ABI.

Run from the checkout after a Release liquid-rotation target build:
  python3 docs/evidence/windows-portability-followup/reproduce_binary64.py \
      --output /tmp/liquid-binary64-before --source-ref c62cfc6c
  python3 docs/evidence/windows-portability-followup/reproduce_binary64.py \
      --output /tmp/liquid-binary64-after
This is a precision-isolation experiment, not a substitute for MSVC execution.
"""
import argparse
from pathlib import Path
import re
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', type=Path, default=Path('build'))
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--source-ref', help='Git revision; omitted uses candidate files')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
flags = (args.build / 'CMakeFiles/judas_liquid_rotation_tests.dir/flags.make').read_text(encoding='utf-8')
includes = shlex.split(next(line.split(' = ', 1)[1] for line in flags.splitlines()
                           if line.startswith('CXX_INCLUDES =')))
objects = []
for source in ('src/LiquidGeometry.cpp', 'src/LiquidSurface.cpp', 'tests/LiquidRotationTests.cpp'):
    text = (subprocess.check_output(['git', 'show', f'{args.source_ref}:{source}']).decode('utf-8')
            if args.source_ref else Path(source).read_text(encoding='utf-8'))
    text = re.sub(r'(?<=[\d.])L\b', '', text.replace('long double', 'double'))
    # Keep the original public signatures to link against the unchanged engine.
    for method in ('Volume', 'Area', 'Integral'):
        text = text.replace(f'LiquidCapacityPolynomial::{method}(double fraction)',
                            f'LiquidCapacityPolynomial::{method}(long double fraction)')
    target = args.output / Path(source).name
    target.write_text(text, encoding='utf-8')
    obj = target.with_suffix('.o')
    subprocess.run(['c++', '-O3', '-DNDEBUG', '-std=c++17', *includes,
                    '-c', str(target), '-o', str(obj)], check=True)
    objects.append(str(obj))
link = shlex.split((args.build / 'CMakeFiles/judas_liquid_rotation_tests.dir/link.txt').read_text(encoding='utf-8'))
libs = link[link.index('libjudas_engine.a'):]
libs = [str(args.build / item) if not item.startswith(('-', '/')) else item for item in libs]
executable = args.output / 'probe'
# Put numerical objects before the archive so their complete implementations are used.
subprocess.run(['c++', *objects, '-o', str(executable), *libs], check=True)
with (args.output / 'result.log').open('w', encoding='utf-8') as log:
    result = subprocess.run([str(executable)], stdout=log, stderr=subprocess.STDOUT)
print('Diagnostic return code:', result.returncode)
raise SystemExit(result.returncode)
