#!/usr/bin/env python3
"""Verify the hosted result, source identity and extracted qualification receipts."""
import hashlib
import json
from pathlib import Path
import re
import sys

OUT = Path(__file__).resolve().parents[1]
HEAD = '050016a179313c90584ce00f141cc5ca4c521808'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    final = json.loads((OUT / 'FINAL_RUN.json').read_text())
    require(final['headSha'] == HEAD and final['status'] == 'completed'
            and final['conclusion'] == 'success', 'Hosted run did not succeed at the exact head')
    steps = final['jobs'][0]['steps']
    require(all(s['status'] == 'completed' and s['conclusion'] == 'success' for s in steps),
            'A hosted job step did not pass')
    identity = json.loads((OUT / 'artifact/build/judas-build-info-Release.json').read_text())
    require(identity['source_revision'] == HEAD and identity['source_dirty'] is False
            and identity['source_untracked_count'] == 0, 'Hosted build was not clean at the exact head')
    # The SDK deliberately serializes this same object with sorted keys.
    require(identity == json.loads((OUT / 'artifact/sdk/BUILD_INFO.json').read_text()),
            'Native and packaged SDK identities differ')
    qualification = json.loads((OUT / 'artifact/tooling/sdk-qualification.json').read_text())
    require(qualification['passed'] is True and qualification['errors'] == []
            and qualification['tool_count'] == 17, 'SDK qualification did not pass all17 tools')
    require(qualification['build'] == identity, 'SDK qualification belongs to a different build')
    policy = qualification['policy']
    require(policy['host_check'] is True and policy['max_glibc'] == '2.39'
            and policy['require_os'] == 'ubuntu:24.04', 'Requested compatibility policy was not exercised')
    host = qualification['host']
    require(host['os_release']['ID'] == 'ubuntu' and host['os_release']['VERSION_ID'] == '24.04',
            'Qualification was not performed on the declared Ubuntu baseline')
    manifest_sha = hashlib.sha256((OUT / 'artifact/sdk/SDK_MANIFEST.json').read_bytes()).hexdigest()
    require(qualification['sdk_manifest_sha256'] == manifest_sha,
            'Qualification and collected SDK manifest hashes differ')
    checks = []
    for path in sorted((OUT / 'artifact/validation').glob('*/RESULTS.json')):
        result = json.loads(path.read_text())
        require(result['passed'] is True and result['sourceRevision'] == HEAD
                and result['sourceRevisionAfter'] == HEAD and result['trackedWorktreeStatus'] == ''
                and result['trackedWorktreeStatusAfter'] == '', 'Gate source identity/result mismatch')
        for test in result['tests']:
            require(test['status'] == 'passed' and test['exit'] == 0
                    and test['binarySha256'] == test['binarySha256After'],
                    'Native gate failed or its executable changed')
            require(test['buildIdentity'] == identity, 'Native gate build identity mismatch')
            checks.append({'name': test['name'], 'passed': True, 'exit': test['exit'],
                           'seconds': test['seconds'], 'binary_sha256': test['binarySha256'],
                           'result': path.relative_to(OUT).as_posix()})
    require(len(checks) == 36, 'Expected36 distinct native result receipts')
    log = (OUT / 'artifact/ctest/LastTest.log').read_text()
    require(len(re.findall(r'^\d+/38 Testing:', log, re.M)) == 38
            and len(re.findall(r'Test Passed\.', log)) == 38,
            'CTest receipt does not contain38 successful tests')
    strict = []
    for line in (OUT / 'job-117770346462.log').read_text().splitlines():
        if '\tQualify SDK on its declared Linux baseline\t' in line and '{"build":' in line:
            strict.append(json.loads(line[line.index('{"build":'):]))
    require(len(strict) == 1 and strict[0]['ok'] is True and strict[0]['tools'] == 17
            and strict[0]['files'] == 212 and strict[0]['build'] == identity,
            'Strict SDK verification after packaged inspection did not succeed')
    summary = {'schema': 1, 'run': final['databaseId'], 'run_url': final['url'], 'head': HEAD,
               'hosted_run_conclusion': 'success', 'completed_steps': len(steps),
               'ctest': {'passed': 38, 'failed': 0, 'total_seconds': 34.54,
                         'native_structured_results': len(checks),
                         'additional_controls': ['prealpha.gate-negative-controls',
                                                 'prealpha.build-organization']},
               'build': identity,
               'sdk': {'tool_count': qualification['tool_count'], 'host': host, 'policy': policy,
                       'highest_required_abi': qualification['highest_required_abi'],
                       'errors': qualification['errors'], 'sdk_manifest_sha256': manifest_sha,
                       'dependency_count': len(qualification['resolved_dependencies']),
                       'strict_after_inspection': strict[0]},
               'tests': checks,
               'scope': 'Hosted clean Ubuntu24.04 Release/loader and explicit focused gates only; '
                        'no GPU hardware or human acceptance; known failing contracts remain '
                        'excluded and classified in original gate results.'}
    (OUT / 'SUMMARY.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'Verified {len(checks)} native receipts, {len(steps)} successful hosted steps, '
          f'38 CTest passes, SDK17 tools/{len(qualification["resolved_dependencies"])} dependencies')
    return 0


if __name__ == '__main__':
    sys.exit(main())
