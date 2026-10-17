#!/usr/bin/env python3
"""Real independent-process M99 session/physical endpoint and impaired-link proof."""
import argparse
import bisect
import json
import math
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time


def analyze_traces(out, first, second):
    def distance(a, b):
        return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))

    actual_trace = json.loads((out / 'host/trace.json').read_text())
    times = [sample['time'] for sample in actual_trace]

    def distribution(values):
        values = sorted(values)
        return dict(samples=len(values), median=(values[(len(values) - 1) // 2] + values[len(values) // 2]) / 2 if values else None,
                    p95=values[int((len(values) - 1) * .95)] if values else None,
                    worst=max(values) if values else None)

    synchronization = dict(
        clock='shared native steady-clock seconds across independent processes',
        reference='linear interpolation between actual resolved host physical endpoints',
        selection='connected observations whose actual host endpoint finite-difference speed exceeds 0.05 m/s; this selects motion, it is not an acceptance gate',
        warmup='includes initial connected baseline and interpolation warmup; no handpicked error threshold',
        units='position errors in metres, connection gaps in seconds')
    for label, delay, client in [('direct', .1, first), ('impaired', .175, second)]:
        observed = json.loads((out / label / 'trace.json').read_text())
        errors = dict(body=[], motor=[])
        for sample in observed:
            target = sample['time'] - delay
            index = bisect.bisect_right(times, target)
            if index == 0 or index >= len(times):
                continue
            a, b = actual_trace[index - 1], actual_trace[index]
            fraction = (target - a['time']) / (b['time'] - a['time'])
            for kind, position_key in [('body', 'position'), ('motor', 'motor')]:
                if sample.get(position_key) is None:
                    continue
                velocity = [(y - x) / (b['time'] - a['time']) for x, y in zip(a[position_key], b[position_key])]
                if math.sqrt(sum(value * value for value in velocity)) <= .05:
                    continue
                expected = [x + fraction * (y - x) for x, y in zip(a[position_key], b[position_key])]
                errors[kind].append(distance(sample[position_key], expected))
        presentation_gaps = []
        for gap in client['connectionGaps']:
            before = [sample['time'] for sample in observed if sample['time'] <= gap['start']]
            after = [sample['time'] for sample in observed if sample['time'] >= gap['end']]
            if before and after:
                presentation_gaps.append(dict(
                    lastPresentationBeforeDisconnect=max(before), firstPresentationAfterReconnect=min(after),
                    seconds=min(after) - max(before), secondsAfterConnected=min(after) - gap['end']))
        synchronization[label] = dict(
            nominalPresentationLagMs=delay * 1000,
            bodyInMotionError=distribution(errors['body']), motorInMotionError=distribution(errors['motor']),
            connectionGaps=client['connectionGaps'],
            reconnectPresentationGaps=presentation_gaps,
            largestPresentationIntervalSeconds=max((b['time'] - a['time'] for a, b in zip(observed, observed[1:])), default=0),
            maximumConnectedSnapshotAgeSeconds=max((sample['snapshotAge'] for sample in observed), default=0))
        if not all(errors.values()):
            raise RuntimeError(label + ' trace lacks actual in-motion comparisons')
    return synchronization


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=Path('build/judas_network_session_tests'))
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--analyze-only', action='store_true',
                        help='recompute corrected motion analysis from exact preserved traces; writes a separate report')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    binary = args.binary.resolve()
    out = args.output.resolve()
    if args.analyze_only:
        result = json.loads((out / 'RESULTS.json').read_text())
        result['synchronization'] = analyze_traces(out, result['direct'], result['impaired'])
        result['analysisCorrection'] = 'Classify motion from actual endpoint finite differences, not motor velocity observations that may include supported gravity contribution. Original process results and traces remain unchanged.'
        path = out / 'RESULTS.motion-corrected.json'
        if path.exists():
            raise RuntimeError('analysis report already exists; preserve its bytes')
        path.write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result['synchronization'], indent=2))
        return
    out.mkdir(parents=True, exist_ok=False)
    env = {key: value for key, value in os.environ.items() if not key.startswith('JUDAS_')}
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env.update(SDL_VIDEODRIVER='invalid', XDG_DATA_HOME=str(out / 'user-data'))
    processes = []
    logs = []
    commands = []

    def start(name, command, pipe=False):
        log = (out / (name + '.log')).open('w')
        logs.append(log)
        command = [str(item) for item in command]
        commands.append(dict(name=name, argv=command))
        process = subprocess.Popen(command, cwd=root, env=env, stdout=subprocess.PIPE if pipe else log,
                                   stderr=log, text=True)
        processes.append((name, process))
        return process

    def await_json(path, process, timeout=12):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if path.exists():
                return json.loads(path.read_text())
            if process.poll() is not None:
                raise RuntimeError('child exited before readiness: ' + str(path))
            time.sleep(.01)
        raise RuntimeError('readiness timeout: ' + str(path))

    began = time.monotonic()
    result = dict(status='failed', commands=commands, assertions=[])
    try:
        host = start('host', [binary, '--process-host', out / 'host', '9'])
        ready = await_json(out / 'host/ready.json', host)
        relay = start('proxy', [sys.executable, root / 'scripts/m99_network_proxy.py', '--port', '0',
            '--upstream-port', ready['port'], '--latency-ms', '75', '--jitter-ms', '15',
            '--drop-type', 'snapshot', '--drop-every', '5', '--cut-after', '2.5',
            '--seconds', '8', '--stats', out / 'proxy.json'], pipe=True)
        selector = selectors.DefaultSelector()
        selector.register(relay.stdout, selectors.EVENT_READ)
        if not selector.select(5):
            raise RuntimeError('impairment proxy did not become ready')
        proxy_ready = json.loads(relay.stdout.readline())
        selector.close()
        direct = start('direct-client', [binary, '--process-client', ready['project'], ready['port'],
                                          out / 'direct', '6', 'direct'])
        impaired = start('impaired-client', [binary, '--process-client', ready['project'], proxy_ready['port'],
                                              out / 'impaired', '6', 'reconnect'])
        for name, process in [('direct-client', direct), ('impaired-client', impaired), ('host', host)]:
            code = process.wait(timeout=25)
            if code:
                raise RuntimeError(f'{name} failed with exit {code}; original log preserved')
        relay.wait(timeout=12)
        if relay.returncode:
            raise RuntimeError('impairment proxy failed')
        server = json.loads((out / 'host/result.json').read_text())
        first = json.loads((out / 'direct/result.json').read_text())
        second = json.loads((out / 'impaired/result.json').read_text())
        proxy = json.loads((out / 'proxy.json').read_text())

        def check(condition, description):
            result['assertions'].append(dict(passed=bool(condition), description=description))
            if not condition:
                raise RuntimeError(description)

        def distance(a, b):
            return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))

        check(server['hits'] >= 2 and server['joins'] >= 3 and server['leaves'] >= 1,
              'two real client processes actuate host physics; connection cut and rejoin reach server')
        check(server['box'][0] > .5 and .4 < server['box'][1] < .7,
              'server-owned body is displaced by real impulses and rests on real collision geometry')
        check(server['motor'][0] > -2.5 and .8 < server['motor'][1] < 1.1,
              'server-owned CharacterMotor moves from actual delivered remote intent and stays supported')
        for label, client in [('direct', first), ('impaired', second)]:
            check(client['status'] == 'connected' and client['boxReady'] and client['motorReady'],
                  label + ' client ends connected with both physical replica endpoints')
            check(distance(client['box'], server['box']) < .1 and distance(client['motor'], server['motor']) < .1,
                  label + ' client presentation converges to independent host actual physical endpoint')
            check(client['receivedBytes'] > 0 and client['sentBytes'] > 0 and client['snapshots'] > 10,
                  label + ' session metrics describe actual bidirectional traffic and repeated snapshots')
        check(first['reconnects'] == 0 and second['reconnects'] >= 1 and
              len(set(second['identities'])) == 1,
              'implicit reconnect preserves only the reserved peer identity; direct connection stays intact')
        check(proxy['omitted'] > 0 and proxy['forcedDisconnects'] == 1 and proxy['connections'] >= 2,
              '75ms directional latency, jitter, application snapshot omission and real TCP interruption were exercised')
        check(proxy['peakQueuedBytes'] <= 1024 * 1024,
              'impaired relay queues remain inside their declared memory bound')
        synchronization = analyze_traces(out, first, second)
        for label in ['direct', 'impaired']:
            check(synchronization[label]['bodyInMotionError']['samples'] > 0 and
                  synchronization[label]['motorInMotionError']['samples'] > 0,
                  label + ' trace contains genuine in-motion body and motor comparisons')
        result.update(status='passed', host=server, direct=first, impaired=second, proxy=proxy,
            synchronization=synchronization,
            linkSemantics='TCP ordered latency/jitter; 1-in-5 application snapshots omitted; one TCP break. Not IP packet-loss emulation.')
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        for _, process in processes:
            if process.poll() is None:
                process.terminate()
        for _, process in processes:
            if process.poll() is None:
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
        for log in logs:
            log.close()
        result['processes'] = [dict(name=name, pid=process.pid, exit=process.returncode) for name, process in processes]
        result['wallSeconds'] = time.monotonic() - began
        (out / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(dict(status=result['status'], output=str(out), error=result.get('error')), indent=2))


if __name__ == '__main__':
    main()
