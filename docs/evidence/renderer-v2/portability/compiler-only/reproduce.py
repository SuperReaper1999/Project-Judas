from pathlib import Path
import hashlib
import json
import os
import shlex
import subprocess
import time

root = Path.cwd()
out = root / '.cache/renderer-v2/portable'
out.mkdir(exist_ok=True)
ndk = Path('/home/conner/Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/linux-x86_64')
configs = [
    ('browser', root/'.cache/browser-sdk/emsdk/upstream/emscripten/em++', root/'.cache/browser-build', []),
    ('windows-cross', root/'.cache/windows-cross/toolchain/root/usr/bin/x86_64-w64-mingw32-g++-posix', root/'.cache/windows-cross/bootstrap', []),
    ('android-arm64', ndk/'bin/clang++', root/'.cache/windows-cross/android-configure',
     ['--target=aarch64-none-linux-android26', '--sysroot='+str(ndk/'sysroot')]),
]
sources = ['src/Renderer.cpp', 'src/Window.cpp', 'src/WorldPresentation.cpp'] + [
    str(path) for path in sorted(Path('src/render').glob('*.cpp'))]
rows, overlays = [], []
for label, compiler, cache, extra in configs:
    flags = {}
    for line in (cache/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
        if ' = ' in line:
            key, value = line.split(' = ', 1)
            flags[key] = shlex.split(value)
    overlay_flags = []
    if label != 'browser':
        # These are configure-only caches: SDL has not generated/copied its
        # public forwarding headers. Do not build it or modify the cache.
        # Use the actual cached target source headers plus its generated target
        # SDL_config.h in this isolated syntax-probe overlay instead.
        overlay = out/'headers'/label/'SDL2'
        overlay.mkdir(parents=True, exist_ok=True)
        public = cache/'_deps/judas_sdl-src/include'
        configuration = cache/'_deps/judas_sdl-build/include-config-release/SDL2/SDL_config.h'
        for header in sorted(public.glob('*.h')):
            target = configuration if header.name == 'SDL_config.h' else header
            link = overlay/header.name
            if not link.exists():
                link.symlink_to(target)
            overlays.append({'platform': label, 'header': str(link.relative_to(root)),
                             'source': str(target.relative_to(root)),
                             'sha256': hashlib.sha256(target.read_bytes()).hexdigest()})
        overlay_flags = ['-I'+str(overlay.parent)]
    for source in sources:
        command = [str(compiler), *extra, *flags.get('CXX_DEFINES', []), *overlay_flags,
                   *flags.get('CXX_INCLUDES', []), *flags.get('CXX_FLAGS', []),
                   '-fsyntax-only', str(root/source)]
        started = time.time()
        with (out/(label+'-'+Path(source).stem+'.log')).open('w', encoding='utf-8') as log:
            try:
                code = subprocess.run(command, cwd=cache, stdout=log, stderr=subprocess.STDOUT,
                                      timeout=120, env=dict(os.environ, EMCC_CORES='1')).returncode
            except (OSError, subprocess.TimeoutExpired) as error:
                log.write(str(error))
                code = 125
        rows.append({'platform': label, 'source': source, 'command': command, 'exit': code,
                     'seconds': time.time()-started,
                     'sha256': hashlib.sha256((root/source).read_bytes()).hexdigest()})
        print(label, source, code, flush=True)
        (out/'RESULTS.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
    (out/'HEADER-OVERLAYS.json').write_text(json.dumps(overlays, indent=2)+'\n', encoding='utf-8')
