import hashlib, json, pathlib, subprocess, time
root = pathlib.Path(__file__).resolve().parents[2]
old = root / '.cache/dense-scalability-20261019/final-support-checks'
out = root / '.cache/dense-scalability-20261019/final-support-checks-refreshed'
out.mkdir(exist_ok=False)
previous = json.loads((old / 'RESULTS.json').read_text())
results = []
for entry in previous:
    if entry['pass_expected']:
        continue
    name = entry['name']
    argv = [arg.replace(str(old), str(out)) for arg in entry['argv']]
    start = time.monotonic()
    log = out / (name + '.log')
    with log.open('w') as stream:
        try:
            code = subprocess.run(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=145).returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    row = dict(name=name, argv=argv, exit=code, expected_exit=0, pass_expected=code == 0,
               seconds=time.monotonic()-start, log_sha256=hashlib.sha256(log.read_bytes()).hexdigest())
    results.append(row)
    (out / 'RESULTS.json').write_text(json.dumps(results, indent=2)+'\n')
    print(name, code, flush=True)
raise SystemExit(0 if all(row['pass_expected'] for row in results) else 1)
