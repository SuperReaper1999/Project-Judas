#!/usr/bin/env python3
"""Verify an exact main promotion using completed, same-repository native CI.

This saves duplicate compilation, not validation. Missing proof fails explicitly;
workflow_dispatch still runs both native channels on either branch.
"""
import json
import os
import re
import subprocess
import sys


def qualifies(run, jobs, repository, revision):
    if (run.get('head_sha') != revision or run.get('conclusion') != 'success'
            or (run.get('head_repository') or {}).get('full_name') != repository
            or run.get('head_branch') != 'Experimental'):
        return False
    workflow = run.get('path')
    if workflow == '.github/workflows/prealpha.yml':
        if run.get('event') not in ('push', 'workflow_dispatch'):
            return False
        required = {'linux (Alpha)', 'linux (Experimental)'}
    elif workflow == '.github/workflows/release-candidate.yml':
        if run.get('event') != 'workflow_dispatch':
            return False
        required = {'linux', 'windows'}
    else:
        return False
    selected = [j for j in jobs if j.get('name') in required]
    return (required <= {j.get('name') for j in selected}
            and all(j.get('status') == 'completed' and j.get('conclusion') == 'success'
                    for j in selected))


def api(path):
    return json.loads(subprocess.check_output(['gh', 'api', path], text=True))


def main():
    repository = os.environ['GITHUB_REPOSITORY']
    revision = os.environ['GITHUB_SHA']
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        raise ValueError('Exact source revision required')
    for workflow in ('prealpha.yml', 'release-candidate.yml'):
        runs = api(f'repos/{repository}/actions/workflows/{workflow}/runs'
                   f'?head_sha={revision}&status=success&per_page=100')['workflow_runs']
        for run in runs:
            jobs = api(f'repos/{repository}/actions/runs/{run["id"]}/jobs?per_page=100')['jobs']
            if qualifies(run, jobs, repository, revision):
                print(json.dumps({'passed': True, 'revision': revision,
                                  'proofRun': run['html_url'], 'humanAcceptance': False}))
                return
    raise ValueError('No completed native qualification for this exact Experimental commit. '
                     'Finish Experimental CI, or manually dispatch Engine focused gates on main. '
                     'A skipped job, tooling-only pass or different commit is not proof.')


if __name__ == '__main__':
    try:
        main()
    except (KeyError, ValueError, OSError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
