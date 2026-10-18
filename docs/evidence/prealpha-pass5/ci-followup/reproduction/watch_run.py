#!/usr/bin/env python3
"""Read a bounded sequence of actual GitHub Actions statuses, without changing the run."""
import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

RUN = '39159743311'
EXPECTED_HEAD = '050016a179313c90584ce00f141cc5ca4c521808'
OUT = Path(__file__).resolve().parents[1]
FIELDS = 'databaseId,headSha,status,conclusion,url,createdAt,updatedAt,jobs'


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def fetch(argv, target):
    started = utc()
    result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=60)
    target.write_bytes(result.stdout)
    if result.stderr:
        target.with_suffix(target.suffix + '.stderr').write_bytes(result.stderr)
    return result, {'command': argv, 'started_utc': started, 'finished_utc': utc(),
                    'returncode': result.returncode, 'stdout_file': target.name,
                    'stdout_sha256': hashlib.sha256(result.stdout).hexdigest()}


def main():
    receipts, previous = [], None
    for index in range(30):
        target = OUT / f'poll-{index:02d}.json'
        argv = ['gh', 'run', 'view', RUN, '--json', FIELDS]
        result, receipt = fetch(argv, target)
        receipts.append(receipt)
        (OUT / 'POLL_RECEIPTS.json').write_text(json.dumps(receipts, indent=2) + '\n')
        if result.returncode:
            print(f'{utc()} status lookup failed: exit {result.returncode}', flush=True)
        else:
            state = json.loads(result.stdout)
            if state['headSha'] != EXPECTED_HEAD:
                raise RuntimeError('Run head differs from the reviewed CI repair')
            summary = {'status': state['status'], 'conclusion': state['conclusion'],
                       'jobs': [{'name': job['name'], 'status': job['status'],
                                 'conclusion': job['conclusion'], 'steps': job['steps']}
                                for job in state['jobs']]}
            if summary != previous:
                print(utc() + ' ' + json.dumps(summary), flush=True)
                previous = summary
            if state['status'] == 'completed':
                (OUT / 'FINAL_RUN.json').write_bytes(result.stdout)
                jobs = [('artifacts.json', ['gh', 'api',
                    f'repos/SuperReaper1999/Project-Judas/actions/runs/{RUN}/artifacts'])]
                for job in state['jobs']:
                    jobs.append((f"job-{job['databaseId']}.log", ['gh', 'run', 'view',
                        RUN, '--job', str(job['databaseId']), '--log']))
                final_receipts = []
                for name, command in jobs:
                    _, captured = fetch(command, OUT / name)
                    final_receipts.append(captured)
                (OUT / 'FINAL_RECEIPTS.json').write_text(
                    json.dumps(final_receipts, indent=2) + '\n')
                return 0 if state['conclusion'] == 'success' else 1
        if index != 29:
            time.sleep(45)
    print('Bounded watcher ended without a completed hosted result', flush=True)
    return 2


if __name__ == '__main__':
    sys.exit(main())
