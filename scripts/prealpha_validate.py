#!/usr/bin/env python3
"""Run bounded focused gates; preserve failures and provenance, never build implicitly.

No skipped or deferred finding is counted as a pass. The manifest is an explicit
subset of tests, not a claim of engine-wide coverage or human acceptance.
"""
import argparse
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import signal
import string
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'tools/validation/suites.json'
sys.path.insert(0, str(ROOT / 'tools'))
from package_sdk import probe_identity  # Same bounded tool probe as distribution verification.


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def git(*arguments):
    try:
        result = subprocess.run(['git', *arguments], cwd=ROOT, capture_output=True,
                                text=True, timeout=10)
        return result.stdout.strip() if result.returncode == 0 else None
    except (OSError, subprocess.TimeoutExpired):
        return None


def source_identity():
    top = git('rev-parse', '--show-toplevel')
    if top and Path(top).resolve() == ROOT:
        tree = git('rev-parse', 'HEAD^{tree}')
        revision = git('rev-parse', 'HEAD')
        if not tree or not revision:
            raise ValueError('validation requires an existing source checkpoint')
        result = subprocess.run(['git', 'diff', 'HEAD', '--binary', '--no-ext-diff',
                                 '--full-index', '--no-color', '--no-renames', '--no-textconv',
                                 '--diff-algorithm=myers'], cwd=ROOT, capture_output=True, timeout=10)
        if result.returncode:
            raise ValueError('cannot compute canonical source fingerprint')
        untracked = subprocess.check_output(['git', '-c', 'core.quotepath=false', 'ls-files', '--others', '--exclude-standard', '--',
                    'CMakeLists.txt', 'engine-version.json', 'cmake', 'src', 'tools', 'scripts',
                    'tests', 'platform', 'packaging', 'third_party', 'assets'], cwd=ROOT, text=True, timeout=10)
        observations = ''
        for name in sorted(untracked.strip().splitlines()):
            if re.search(r'(^|/)(__pycache__|[.]cache|node_modules)/|[.]py[co]$', name):
                continue
            path = ROOT / name
            if path.is_file():
                observations += '\n' + name + ':' + digest(path)
        return {'source_revision': revision,
                'source_fingerprint': hashlib.sha256(tree.encode() + b'\n' + result.stdout).hexdigest(),
                'source_dirty': bool(result.stdout),
                'source_untracked_fingerprint': hashlib.sha256(observations.encode()).hexdigest()}
    # Do not mistake a containing Git checkout for a source archive's provenance.
    info = json.loads((ROOT / 'JUDAS_SOURCE_INFO.json').read_text())
    manifest_path = ROOT / 'JUDAS_SOURCE_FILES.json'
    if digest(manifest_path) != info['source_files_sha256']:
        raise ValueError('source archive inventory identity is invalid')
    recorded = set()
    for row in json.loads(manifest_path.read_text())['files']:
        path = (ROOT / row['path']).resolve()
        if ROOT not in path.parents or digest(path) != row['sha256']:
            raise ValueError('source archive changed; create a current checkpoint/archive before gating')
        recorded.add(row['path'])
    for directory in ('src', 'cmake', 'tools', 'scripts', 'tests', 'platform', 'packaging', 'third_party', 'assets'):
        for path in (ROOT / directory).rglob('*'):
            name = path.relative_to(ROOT).as_posix()
            if path.is_file() and not re.search(r'(^|/)(__pycache__|[.]cache|node_modules)/|[.]py[co]$', name):
                if name not in recorded:
                    raise ValueError('source archive has added build input: ' + name)
    return {'source_revision': info['revision'], 'source_fingerprint': info['source_fingerprint'],
            'source_dirty': info['source_dirty']}


def validate_provenance(actual, expected):
    if actual.get('schema') != 1:
        raise ValueError('test has no supported embedded build identity; enable JUDAS_ENABLE_VALIDATION')
    for key, value in expected.items():
        if actual.get(key) != value:
            raise ValueError('stale test binary: ' + key + ' differs; reconfigure and rebuild focused targets')
    return actual


def load_manifest(path):
    data = json.loads(path.read_text())
    if (not isinstance(data, dict) or data.get('format') != 1 or
            not isinstance(data.get('tests'), list) or not data['tests']):
        raise ValueError('unsupported validation manifest')
    names = set()
    invocations = set()
    for entry in data['tests']:
        if not isinstance(entry, dict):
            raise ValueError('test entries must be objects')
        name = entry.get('name', '')
        if not isinstance(name, str) or not re.fullmatch(r'[a-z0-9-]+', name) or name in names:
            raise ValueError('invalid or duplicate test name')
        names.add(name)
        target = entry.get('target', '')
        if not isinstance(target, str) or not re.fullmatch(r'judas_[a-z0-9_]+', target):
            raise ValueError('invalid native target')
        groups = entry.get('groups')
        if (not isinstance(groups, list) or not groups or
                not all(isinstance(g, str) and re.fullmatch(r'[a-z0-9-]+', g) for g in groups) or
                len(groups) != len(set(groups))):
            raise ValueError('test groups must be a nonempty list of unique names')
        arguments = entry.get('args', [])
        if not isinstance(arguments, list) or not all(isinstance(a, str) for a in arguments):
            raise ValueError('test arguments must be a list of strings')
        for argument in arguments:
            for _, field, spec, conversion in string.Formatter().parse(argument):
                if field is not None and (field not in {'root', 'output'} or spec or conversion):
                    raise ValueError('test argument placeholders must be {root} or {output}')
        invocation = (target, tuple(arguments))
        if invocation in invocations:
            raise ValueError('duplicate native invocation: ' + name)
        invocations.add(invocation)
        if not isinstance(entry.get('manual', False), bool):
            raise ValueError('manual must be a Boolean')
        if 'known-failures' in groups and not entry.get('manual', False):
            raise ValueError('known failures must be explicitly manual')
    findings = {}
    if not isinstance(data.get('known_findings', []), list):
        raise ValueError('known_findings must be a list')
    for finding in data.get('known_findings', []):
        if not isinstance(finding, dict):
            raise ValueError('known findings must be objects')
        identity = finding.get('id')
        if (not isinstance(identity, str) or not re.fullmatch(r'[A-Z0-9-]+', identity) or
                identity in findings or not isinstance(finding.get('scheduled'), bool) or
                not all(isinstance(finding.get(key), str) and finding[key]
                        for key in ('status', 'reason', 'evidence'))):
            raise ValueError('invalid or duplicate known finding')
        findings[identity] = finding
    for entry in data['tests']:
        if 'finding' not in entry:
            continue
        finding = entry['finding']
        if not isinstance(finding, str) or finding not in findings:
            raise ValueError('test references an unknown finding: ' + entry['name'])
        if not findings[finding]['scheduled'] and not entry.get('manual', False):
            raise ValueError('unscheduled finding cannot enter an automatic gate: ' + entry['name'])
    return data


def run_case(command, log_path, cwd, environment, timeout, expected_sha256=None):
    started = time.monotonic()
    record = {'argv': list(map(str, command)), 'cwd': str(cwd), 'timeoutSeconds': timeout}
    try:
        executable = Path(command[0])
        record['binarySha256'] = digest(executable)
        if expected_sha256 is not None and record['binarySha256'] != expected_sha256:
            raise ValueError('test binary changed after its identity probe; rebuild and retry')
        with log_path.open('wb') as log:
            child = subprocess.Popen(command, cwd=cwd, env=environment,
                                     stdout=log, stderr=subprocess.STDOUT,
                                     start_new_session=os.name != 'nt')
            try:
                record['exit'] = child.wait(timeout=timeout)
                record['status'] = 'passed' if record['exit'] == 0 else 'failed'
            except subprocess.TimeoutExpired:
                if os.name != 'nt':
                    os.killpg(child.pid, signal.SIGKILL)
                else:
                    child.kill()
                child.wait()
                record.update(status='timeout', exit=child.returncode)
            except BaseException:
                if os.name != 'nt':
                    os.killpg(child.pid, signal.SIGKILL)
                else:
                    child.kill()
                child.wait()
                raise
        record['binarySha256After'] = digest(executable)
        if record['binarySha256After'] != record['binarySha256']:
            record['status'] = 'binary-changed'
    except (OSError, ValueError) as error:
        record.update(status='unavailable', error=str(error), exit=None)
        log_path.write_text(str(error) + '\n')
    record['seconds'] = time.monotonic() - started
    return record


def run_verified_case(command, log_path, cwd, environment, timeout, expected_source):
    executable = Path(command[0])
    before = digest(executable)
    identity = validate_provenance(probe_identity(executable, timeout=5), expected_source)
    if digest(executable) != before:
        raise ValueError('test binary changed during its identity probe; rebuild and retry')
    result = run_case(command, log_path, cwd, environment, timeout, expected_sha256=before)
    result['buildIdentity'] = identity
    return result


def annotate_finding(result, entry, manifest):
    if 'finding' not in entry:
        return
    identity = entry['finding']
    finding = next(row for row in manifest.get('known_findings', []) if row['id'] == identity)
    result['finding'] = identity
    result['findingStatus'] = finding['status']
    result['classification'] = (finding['status'] +
                                '; observed result remains authoritative')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--manifest', type=Path, default=MANIFEST)
    parser.add_argument('--group', action='append', default=[])
    parser.add_argument('--test', action='append', default=[], help='Select exact manifest names; no implicit core group')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--timeout', type=float, default=120)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--check-manifest', action='store_true',
                        help='Validate gate declarations without configuring or executing native tests')
    args = parser.parse_args()
    if not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error('timeout must be finite and positive')
    try:
        manifest = load_manifest(args.manifest)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    if args.check_manifest:
        print(json.dumps({'format': 1, 'tests': len(manifest['tests']),
                          'nativeTargets': len({entry['target'] for entry in manifest['tests']}),
                          'manual': sum(entry.get('manual', False) for entry in manifest['tests']),
                          'knownFindings': len(manifest.get('known_findings', []))}))
        return 0
    groups = set(args.group or ([] if args.test else ['core']))
    known_names = {item['name'] for item in manifest['tests']}
    if set(args.test) - known_names:
        parser.error('unknown test names: ' + ', '.join(sorted(set(args.test) - known_names)))
    selected = [item for item in manifest['tests']
                if item['name'] in args.test or groups.intersection(item['groups'])]
    if not selected:
        parser.error('no tests match requested groups')
    if args.list:
        print(json.dumps({'selected': selected, 'knownFindings': manifest.get('known_findings', [])}, indent=2))
        return 0
    now = datetime.datetime.now(datetime.timezone.utc)
    output = (args.output or ROOT / '.cache/prealpha/validation' /
              (now.strftime('%Y%m%dT%H%M%SZ') + '-' + uuid.uuid4().hex[:8])).resolve()
    output.mkdir(parents=True, exist_ok=False)
    environment = {k: v for k, v in os.environ.items() if not k.startswith('JUDAS_')}
    environment.update(SDL_AUDIODRIVER='dummy', JUDAS_WORLD_STATE='none',
                       XDG_DATA_HOME=str(output / 'user-data'))
    revision = git('rev-parse', 'HEAD')
    tracked = git('status', '--porcelain', '--untracked-files=no')
    source_diff = git('diff', 'HEAD', '--binary', '--no-ext-diff')
    diff_hash = hashlib.sha256((source_diff or '').encode()).hexdigest()
    report = {'format': 1, 'startedUtc': now.isoformat(), 'sourceRevision': revision,
              'trackedWorktreeStatus': tracked, 'manifestSha256': digest(args.manifest),
              'trackedDiffSha256': diff_hash,
              'platform': platform.platform(), 'machine': platform.machine(),
              'cpuCount': os.cpu_count(), 'groups': sorted(groups), 'tests': [],
              'knownFindings': manifest.get('known_findings', []),
              'humanAcceptance': 'not-established', 'scope': 'explicit manifest subset'}
    result_path = output / 'RESULTS.json'
    def publish():
        temporary = result_path.with_suffix('.tmp')
        temporary.write_text(json.dumps(report, indent=2) + '\n')
        temporary.replace(result_path)
    publish()
    try:
        expected_source = source_identity()
        report['expectedSourceIdentity'] = expected_source
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        report.update(passed=False, provenanceError=str(error))
        publish()
        print('Source provenance unavailable:', error)
        return 1
    extension = '.exe' if os.name == 'nt' else ''
    for entry in selected:
        command = [str(args.build_dir.resolve() / (entry['target'] + extension))]
        command += [a.format(output=output, root=ROOT) for a in entry.get('args', [])]
        try:
            result = run_verified_case(command, output / (entry['name'] + '.log'), ROOT,
                                       environment, args.timeout, expected_source)
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            result = dict(status='unavailable', exit=None, argv=command, error=str(error))
            (output / (entry['name'] + '.log')).write_text(str(error) + '\n')
        result['name'] = entry['name']
        annotate_finding(result, entry, manifest)
        report['tests'].append(result)
        publish()
        print(entry['name'], result['status'], flush=True)
    report['passed'] = all(row['status'] == 'passed' for row in report['tests'])
    report['finishedUtc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    report['sourceRevisionAfter'] = git('rev-parse', 'HEAD')
    report['trackedWorktreeStatusAfter'] = git('status', '--porcelain', '--untracked-files=no')
    after_diff = git('diff', 'HEAD', '--binary', '--no-ext-diff')
    report['trackedDiffSha256After'] = hashlib.sha256((after_diff or '').encode()).hexdigest()
    try:
        report['expectedSourceIdentityAfter'] = source_identity()
    except (OSError, ValueError, KeyError, subprocess.SubprocessError):
        report['expectedSourceIdentityAfter'] = None
    if (report['sourceRevisionAfter'] != revision or report['trackedWorktreeStatusAfter'] != tracked or
            report['trackedDiffSha256After'] != diff_hash or
            report['expectedSourceIdentityAfter'] != expected_source):
        report['passed'] = False
        report['sourceChangedDuringRun'] = True
    publish()
    print(str(result_path))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
