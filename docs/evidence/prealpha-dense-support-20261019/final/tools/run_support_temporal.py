from pathlib import Path
import hashlib
import importlib.util
import json
import types

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('measure', root / 'scripts/physics_dense_measure.py')
measure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measure)
cache = Path(__file__).resolve().parent
build = cache / 'support-temporal-qualification'
output = cache / 'runs/support-temporal-A1'
output.mkdir(exist_ok=False)
binary = build / 'judas_physics_stress_live_benchmark'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
receipt = json.loads((build / 'BUILD.json').read_text())
assert sha(binary) == receipt['files'][str(binary)]['sha256']
argv = [binary, root / measure.SCENE, output / 'live', '100', '270', '0', '0', '1',
        '0.016666667', '0', '0', '1']
context = {'schema': 'judas.dense.observer.run.v1', 'source_head': '8f6bd18b876158c131bee61fa0dc71625d912c30',
           'isolated_change': 'support certificate subdivision completeness',
           'driver': str(build / 'PhysicsStressLiveBenchmark.observer.cpp'),
           'driver_sha256': receipt['original_driver_sha256'], 'binary_sha256': sha(binary),
           'build_receipt_sha256': sha(build / 'BUILD.json'), 'archive_sha256': receipt['archive_sha256'],
           'fixture_sha256': sha(root / measure.SCENE), 'profile': 0, 'mode': 'exact',
           'timing_note': 'Unchanged temporal observer runs outside measured physics; wall/FPS is not benchmark evidence.'}
measure.write(output / 'CONTEXT.json', context)
result = measure.execute(argv, output, 130, root)
measure.live_summary(output / 'live', result, types.SimpleNamespace(mode='exact', steps=270, seconds=100),
                     {'fixed_cadence_hz': 60, 'maximum_fixed_steps_per_frame': 8})
result['context'] = context
measure.write(output / 'RESULTS.json', result)
print(json.dumps({'exit': result['exit'], 'valid_completion': result['valid_completion'],
                  'completed_steps': result['completed_steps']}, indent=2))
raise SystemExit(0 if result['valid_completion'] else 1)
