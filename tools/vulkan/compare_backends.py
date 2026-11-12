#!/usr/bin/env python3
"""Compare the experimental Vulkan backend against OpenGL on real projects.

For every project the same JUDAS_TEST_SCRIPT is run three times with one
binary: OpenGL, OpenGL again, and Vulkan. Captured frames are compared pixel
by pixel. The second OpenGL run measures how much the project itself varies
between identical runs (asynchronous loading, on-screen timings), so a
difference is only attributed to the backend when it exceeds that baseline.

This is measurement tooling. It never edits a project and it does not turn a
difference into a pass: every number is written to RESULTS.json and printed.
Exit status is non-zero when a run fails, a capture is missing or the Vulkan
validation layer reported a message. Needs only the Python standard library.

    python3 tools/vulkan/compare_backends.py --build-dir build \
        --output /tmp/judas-vulkan-parity projects/renderer_v2_lab/renderer_v2_lab.judasproj
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]


def read_png(path):
    """Minimal 8-bit RGB/RGBA PNG reader returning (width, height, channels, rows)."""
    data = path.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError(f'{path} is not a PNG')
    cursor, idat, header = 8, bytearray(), None
    while cursor < len(data):
        length, kind = struct.unpack('>I4s', data[cursor:cursor + 8])
        body = data[cursor + 8:cursor + 8 + length]
        cursor += 12 + length
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            idat += body
    width, height, depth, colour, _, _, interlace = header
    if depth != 8 or colour not in (2, 6) or interlace:
        raise ValueError(f'{path}: only 8-bit non-interlaced RGB/RGBA is supported')
    channels = 3 if colour == 2 else 4
    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        mode, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        if mode == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif mode == 2:
            for i in range(stride):
                line[i] = (line[i] + previous[i]) & 255
        elif mode == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 255
        elif mode == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = previous[i]
                c = previous[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        previous = line
    return width, height, channels, rows


def compare(first, second):
    a, b = read_png(first), read_png(second)
    if a[:2] != b[:2]:
        return {'error': f'sizes differ: {a[0]}x{a[1]} vs {b[0]}x{b[1]}'}
    width, height = a[0], a[1]
    differing = large = maximum = total = 0
    for row_a, row_b in zip(a[3], b[3]):
        if row_a == row_b and a[2] == b[2]:
            continue
        for x in range(width):
            worst = 0
            for c in range(3):
                delta = abs(row_a[x * a[2] + c] - row_b[x * b[2] + c])
                total += delta
                worst = max(worst, delta)
            if worst:
                differing += 1
                large += worst > 8
                maximum = max(maximum, worst)
    pixels = width * height
    return {'width': width, 'height': height, 'maxChannelDelta': maximum,
            'meanChannelDelta': round(total / (pixels * 3), 5),
            'differingPixelsPercent': round(100 * differing / pixels, 4),
            'pixelsOver8Percent': round(100 * large / pixels, 4)}


def run(binary, project, script_lines, shots, directory, backend, arguments):
    directory.mkdir(parents=True, exist_ok=True)
    lines = [f'FRAMES {arguments.frames}'] + script_lines
    captures = []
    for frame in shots:
        capture = directory / f'frame{frame:04d}.png'
        captures.append(capture)
        lines.append(f'SCREENSHOT {frame} {capture}')
    script = directory / 'harness.script'
    script.write_text('\n'.join(lines) + '\n')
    environment = {k: v for k, v in os.environ.items() if not k.startswith('JUDAS_')}
    environment.update(JUDAS_TEST_SCRIPT=str(script), JUDAS_GL_PROFILE=arguments.profile,
                       JUDAS_ENGINE_ROOT=str(ROOT), SDL_AUDIODRIVER='dummy',
                       XDG_DATA_HOME=str(directory / 'user-data'))
    if backend == 'vulkan':
        environment.update(JUDAS_GRAPHICS='vulkan', JUDAS_VULKAN_VALIDATION='1' if arguments.validation else '0')
    command = [str(binary), str(project)]
    if arguments.xvfb:
        command = ['xvfb-run', '-a', '-s', f'-screen 0 {arguments.screen}x24'] + command
    log = directory / 'run.log'
    with log.open('wb') as stream:
        try:
            status = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, env=environment,
                                    cwd=ROOT, timeout=arguments.timeout).returncode
        except subprocess.TimeoutExpired:
            status = 'timeout'
    text = log.read_text(errors='replace')
    return {'exit': status, 'captures': [str(c) for c in captures],
            'missingCaptures': [str(c) for c in captures if not c.is_file()],
            'validationMessages': text.count('[Vulkan validation]'),
            'backendWarnings': sum(1 for line in text.splitlines()
                                   if line.startswith('[Vulkan] ') and 'validation messages' not in line),
            'device': next((line for line in text.splitlines() if line.startswith('[Graphics] Vulkan backend')), '')}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('projects', nargs='+', type=Path)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--output', type=Path, required=True, help='new directory for captures and RESULTS.json')
    parser.add_argument('--frames', type=int, default=40)
    parser.add_argument('--shot', type=int, action='append', help='frame to capture (repeatable, default 30)')
    parser.add_argument('--script-line', action='append', default=[],
                        help='extra JUDAS_TEST_SCRIPT line, e.g. "CONTROL key:2 1 5 6"')
    parser.add_argument('--profile', choices=['auto', 'portable', 'modern'], default='auto')
    parser.add_argument('--timeout', type=float, default=600)
    parser.add_argument('--screen', default='1280x720')
    parser.add_argument('--no-xvfb', dest='xvfb', action='store_false', help='use the current display')
    parser.add_argument('--no-validation', dest='validation', action='store_false')
    arguments = parser.parse_args()
    shots = arguments.shot or [30]
    binary = arguments.build_dir / ('judas.exe' if os.name == 'nt' else 'judas')
    if not binary.is_file():
        parser.error(f'{binary} does not exist; build the judas target first')
    if arguments.xvfb and not shutil.which('xvfb-run'):
        parser.error('xvfb-run is not installed; pass --no-xvfb to use the current display')
    arguments.output.mkdir(parents=True, exist_ok=False)
    report = {'format': 1, 'profile': arguments.profile, 'frames': arguments.frames, 'shots': shots,
              'scriptLines': arguments.script_line, 'projects': [],
              'note': 'Automated image comparison only; not human validation or performance evidence.'}
    failed = False
    for project in arguments.projects:
        project = project.resolve()
        name = project.stem
        entry = {'project': str(project), 'runs': {}, 'frames': []}
        for label, backend in (('opengl', 'opengl'), ('opengl-repeat', 'opengl'), ('vulkan', 'vulkan')):
            entry['runs'][label] = run(binary, project, arguments.script_line, shots,
                                       arguments.output / name / label, backend, arguments)
        for index, frame in enumerate(shots):
            paths = {label: Path(entry['runs'][label]['captures'][index]) for label in entry['runs']}
            row = {'frame': frame}
            if all(path.is_file() for path in paths.values()):
                row['openglRepeatability'] = compare(paths['opengl'], paths['opengl-repeat'])
                row['vulkanVsOpengl'] = compare(paths['opengl'], paths['vulkan'])
                row['vulkanVsOpenglRepeat'] = compare(paths['opengl-repeat'], paths['vulkan'])
            else:
                row['error'] = 'capture missing'
                failed = True
            entry['frames'].append(row)
        for label, result in entry['runs'].items():
            failed |= result['exit'] != 0 or bool(result['missingCaptures'])
        failed |= entry['runs']['vulkan']['validationMessages'] > 0
        report['projects'].append(entry)
        vulkan = entry['runs']['vulkan']
        print(f'{name}: exits gl={entry["runs"]["opengl"]["exit"]} vk={vulkan["exit"]} '
              f'validation={vulkan["validationMessages"]} warnings={vulkan["backendWarnings"]}')
        for row in entry['frames']:
            if 'error' in row:
                print(f'  frame {row["frame"]}: {row["error"]}')
                continue
            noise, best = row['openglRepeatability'], min(
                (row['vulkanVsOpengl'], row['vulkanVsOpenglRepeat']),
                key=lambda value: value.get('differingPixelsPercent', 100))
            print(f'  frame {row["frame"]}: GL-vs-GL differing {noise.get("differingPixelsPercent")}% | '
                  f'Vulkan-vs-GL differing {best.get("differingPixelsPercent")}% '
                  f'(>8/255: {best.get("pixelsOver8Percent")}%, max {best.get("maxChannelDelta")}, '
                  f'mean {best.get("meanChannelDelta")})')
    report['passed'] = not failed
    (arguments.output / 'RESULTS.json').write_text(json.dumps(report, indent=2) + '\n')
    print('RESULTS:', arguments.output / 'RESULTS.json', '| runs completed without failure:', not failed)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
