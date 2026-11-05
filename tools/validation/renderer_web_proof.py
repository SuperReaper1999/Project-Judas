#!/usr/bin/env python3
"""Build only the changed production owners into a bounded WebGL2 proof.

No full Judas/application build, dependency download, browser launch or service is
performed by the build action. Run the generated observer separately, inside the
bounded user scope described in web-proof/COMMANDS.txt.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.cache/renderer-v2/web-proof'
SOURCES = [
    'tests/RendererPortableWebTests.cpp',
    'src/render/GraphicsCapabilities.cpp',
    'src/render/ShaderLibrary.cpp',
    'src/render/ViewUniforms.cpp',
    'src/render/ParticlePass.cpp',
    'src/render/StreamArena.cpp',
    'src/render/ComputeDevice.cpp',
]
HEADERS = [
    'src/GraphicsApi.h', 'src/VisualParticles.h',
    *['src/render/' + name + '.h' for name in (
        'GraphicsCapabilities', 'ShaderLibrary', 'ViewUniforms',
        'ParticlePass', 'StreamArena', 'ComputeDevice')],
]


def hashes(paths):
    return {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in paths}


def write_commands():
    node = shutil.which('node') or 'node'
    timeout = shutil.which('timeout') or 'timeout'
    command = ['systemd-run', '--user', '--wait', '--pipe', '--collect',
               '--unit=judas-renderer-webproof-chromium', '--working-directory=' + str(ROOT),
               '--property=MemoryMax=768M', '--property=MemorySwapMax=0',
               '--property=RuntimeMaxSec=60s', timeout, '--kill-after=5s', '50s', node,
               str(ROOT / 'tools/validation/renderer_web_observer.cjs'), 'chromium']
    (OUT / 'COMMANDS.txt').write_text(
        'Run after other compilation/benchmarks finish. One browser; fixed 32 MiB Wasm; '
        '768 MiB whole-process-group memory ceiling; zero swap; 60 second lifetime.\n\n' +
        shlex.join(command) + '\n\n' +
        'The Node path and working directory are explicit because user services may not '
        'inherit the interactive shell PATH or project directory. No persistent profile or '
        'long-running observer is created. The observer closes its browser and HTTP server.\n',
        encoding='utf-8')


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    compiler = ROOT / '.cache/browser-sdk/emsdk/upstream/emscripten/em++'
    glm = ROOT / '.cache/browser-build/_deps/judas_glm-src'
    if not compiler.is_file() or not (glm / 'glm/glm.hpp').is_file():
        raise SystemExit('Existing Emscripten SDK or GLM cache is missing; no downloads were attempted.')
    before = hashes(SOURCES + HEADERS)
    # Match the production browser build: ParticlePass catches allocation
    # failure, so exception handling must remain enabled at compile and link.
    command = [str(compiler), '-std=c++17', '-O2', '-fexceptions', '-Wall', '-Wextra',
               '-I' + str(ROOT / 'src'), '-I' + str(glm),
               *[str(ROOT / source) for source in SOURCES],
               '-sMIN_WEBGL_VERSION=2', '-sMAX_WEBGL_VERSION=2',
               '-sINITIAL_MEMORY=33554432', '-sALLOW_MEMORY_GROWTH=0',
               '-sSTACK_SIZE=1048576', '-sENVIRONMENT=web', '-sEXIT_RUNTIME=0',
               '-sASSERTIONS=1', "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString']",
               '-o', str(OUT / 'proof.js')]
    started = time.time()
    environment = dict(os.environ, EMCC_CORES='1')
    (OUT / 'COMMAND.json').write_text(json.dumps(command, indent=2) + '\n', encoding='utf-8')
    with (OUT / 'build.log').open('w', encoding='utf-8') as log:
        try:
            code = subprocess.run(command, cwd=ROOT, env=environment, stdout=log,
                                  stderr=subprocess.STDOUT, timeout=240).returncode
        except (OSError, subprocess.TimeoutExpired) as error:
            log.write('\n' + str(error) + '\n')
            code = 125
    receipt = {'command': command, 'exitCode': code,
               'elapsedSeconds': time.time() - started, 'sources': before,
               'unchangedDuringBuild': before == hashes(SOURCES + HEADERS),
               'wasmMemoryBytes': 33554432, 'allowMemoryGrowth': False,
               'scope': 'Production render owners on WebGL2; no whole-engine/editor/game qualification.'}
    (OUT / 'BUILD.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    if code:
        raise SystemExit('Build failed; inspect ' + str(OUT / 'build.log'))
    if not receipt['unchangedDuringBuild']:
        raise SystemExit('Production source changed during compilation; discard the result and rebuild.')
    (OUT / 'index.html').write_text('''<!doctype html>
<meta charset="utf-8"><title>Renderer WebGL2 proof</title>
<style>body{background:#141822;color:#d8e5ed;font:14px monospace;padding:20px}canvas{border:1px solid #39bab7}pre{white-space:pre-wrap}</style>
<h1>Production renderer owners — WebGL2 qualification</h1>
<p>One context, fixed 32 MiB Wasm memory, no game loop or background observer.</p>
<canvas id="canvas" width="64" height="64"></canvas><pre id="result">Starting…</pre>
<script>
var Module={canvas:document.getElementById('canvas'),
 print:function(line){console.log(line)},printErr:function(line){console.error(line)},
 onAbort:function(reason){window.rendererPortableResult={passed:false,aborted:String(reason)};
 document.getElementById('result').textContent=JSON.stringify(window.rendererPortableResult,null,2)}};
</script><script src="proof.js"></script>
''', encoding='utf-8')
    artifacts = {name: hashlib.sha256((OUT / name).read_bytes()).hexdigest()
                 for name in ('index.html', 'proof.js', 'proof.wasm')}
    (OUT / 'SHA256SUMS').write_text(''.join(value + '  ' + name + '\n'
                                          for name, value in artifacts.items()), encoding='utf-8')
    write_commands()
    print(json.dumps(receipt, indent=2))
    print('Build complete. Run the separately bounded command in COMMANDS.txt when scheduled.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['build'])
    parser.parse_args()
    build()
