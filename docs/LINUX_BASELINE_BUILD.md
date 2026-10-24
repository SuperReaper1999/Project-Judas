# Build Linux binaries for Ubuntu 24.04 and newer

A binary built on a newer distribution is **not** automatically portable to an
older glibc. Use an isolated Ubuntu 24.04 compiler **and its complete development
libraries**, then qualify and run the output there. This optional workflow does
not replace host libraries, change engine versions/channels, publish a release,
or add container dependencies to ordinary native builds.

## Why the Ubuntu 26.04 SDK needs glibc 2.43

The host's `libm.so.6` exports new default versions of `sqrtf`, `atan2f`, `asinf`,
`acosf`, `remainderf` and `remainder` at `GLIBC_2.43`. Linking ordinary calls against
that library records those versions. Engine physics, navigation, IK, networking,
powertrains and other source objects call these functions. This is not an
advanced-fluid dependency or a compiler setting that can safely be removed.
The old and new function versions coexist on the new host; manually relabelling
the binary or forcing old symbol aliases is not the compatibility solution.

An older build environment supplies the matching glibc, libstdc++, SDL2 and
headers together. Ubuntu 24.04 uses glibc 2.39; the SDK inspector independently
checks required symbol versions and all 17 tool loader dependencies. A passing
receipt applies to that environment, not every distribution or GPU driver.

## Docker or Podman

Install one container engine using your distribution's supported method. Prepare
this small build image (the build context contains only the Dockerfile):

```sh
docker build -t judas-build-ubuntu24 -f packaging/linux/Ubuntu24.Dockerfile packaging/linux
python3 tools/build_linux_baseline.py --engine docker --build build/linux-ubuntu24-Experimental --jobs 2
```

Use `podman` in both commands if preferred. The helper never pulls an image during
a build. `--image` accepts a previously prepared image name or immutable digest.
The Dockerfile's Ubuntu 24.04 package updates are intentionally available; it does
not promise byte-identical future compilers. The receipt records the complete
installed package versions, commands, source/build identity and SDK hashes.
Pin and archive your chosen image digest when reproducing an exact distribution.

The source is mounted read-only, the build directory separately writable, no
host home or credentials are mounted, and container build networking is disabled.
Use a new build directory rather than mixing host and baseline object files.
Output is `build/linux-ubuntu24-Experimental/linux-sdk/`; receipts are under
`baseline-evidence/<UTC timestamp>/`. This builds a development SDK with existing
Experimental provenance; it does not promote any branch or claim human testing.

## Rootless bubblewrap alternative

For a host without Docker/Podman, `--rootfs PATH` uses an explicitly prepared
Ubuntu 24.04 root. It requires `bwrap` and permitted unprivileged user namespaces.
A container image/root is trusted build tooling, not a security boundary for
malicious source. Do not point this option at your host `/`.

One reproducible base is Canonical's
[Ubuntu Base 24.04.3 amd64 archive](https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/ubuntu-base-24.04.3-base-amd64.tar.gz),
SHA-256 `6bc2cde3930ad088b3bb46fa45279e96d25bc3810f209850ecbe4722711874f9`.
Compare it with the official
[checksum list](https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/SHA256SUMS)
before extraction. Fetching/checking these URLs is setup, not part of the build.

Extract to an owned directory outside tracked source, create empty `/source` and
`/build` directories inside it, and install the Dockerfile's prerequisites
**inside that root**, using its package manager. In a single-ID rootless
namespace apt needs `-o APT::Sandbox::User=root`; do not change the host's apt
configuration. Rootless package installation can warn about unmapped `_apt`/`adm`
file ownership; inspect the result and require successful package configuration.
For example, after extraction:

```sh
rootfs="$PWD/.cache/ubuntu24-root"
mkdir -p "$rootfs/source" "$rootfs/build"
bwrap --bind "$rootfs" / --dev /dev --proc /proc \
  --ro-bind /etc/resolv.conf /etc/resolv.conf --unshare-user --uid 0 --gid 0 \
  /bin/sh -ec 'apt-get -o APT::Sandbox::User=root update; \
  DEBIAN_FRONTEND=noninteractive apt-get -o APT::Sandbox::User=root install -y --no-install-recommends \
  build-essential cmake ninja-build python3 git ca-certificates libsdl2-dev libglm-dev libgl1-mesa-dri xvfb xauth'
python3 tools/build_linux_baseline.py --rootfs "$rootfs" --build build/linux-ubuntu24-Experimental --jobs 2
```

The build clears inherited host environment variables, mounts the prepared root
and source read-only, and writes only the chosen
build directory and temporary namespace files, and first rejects any OS other
than Ubuntu 24.04. A denied namespace operation is a setup failure, never a
compatibility pass. Use Docker/Podman if the host disallows this backend. The
bubblewrap backend was exercised here; Docker/Podman command construction has
focused tests, but was not executed on this machine.

## Qualification and application testing

`--verify-only` repeats SDK inventory, ABI and loader qualification without a
rebuild. `--extra-target NAME` can build additional focused native checks. The
helper does **not** claim an application passed just because ELF inspection did.
Run editor Play/Stop, actual gameplay and a relocated export inside the baseline
environment as well. A headless Mesa/Xvfb run checks real software rendering;
it is not a physical GPU or audio-device acceptance result.

Keep platform libraries supplied by the target distribution. Packaged games need
compatible SDL2, libstdc++, OpenGL and audio/window-system dependencies; Python
and the compiler are build/SDK tools, not game runtime requirements. See
[distribution qualification](DISTRIBUTION.md#binary-compatibility-qualification)
and [first-project workflow](FIRST_PROJECT.md).
