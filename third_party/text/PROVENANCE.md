# M58 pinned text stack

Local **source** archives are configure inputs, checked by CMake SHA256; no download
is performed by configure, runtime or export. Linux/GCC, Judas C++17/OpenGL 3.3.

| Library | Source release | SHA256 | Chosen license |
|---|---|---|---|
| FreeType | [2.13.3](https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.gz) | `5c3a8e78f7b24c20b25b54ee575d6daa40007a5f4eea2845861c3409b3021747` | [FTL](FreeType-FTL.txt) |
| HarfBuzz | [10.4.0](https://github.com/harfbuzz/harfbuzz/releases/download/10.4.0/harfbuzz-10.4.0.tar.xz) | `480b6d25014169300669aa1fc39fb356c142d5028324ea52b3a27648b9beaad8` | [MIT-style COPYING](HarfBuzz-COPYING.txt) |
| ICU4C | [76.1](https://github.com/unicode-org/icu/releases/download/release-76-1/icu4c-76_1-src.tgz) | `dfacb46bfe4747410472ce3e1144bf28a102feeaa4e3875bac9b4c6cf30f4f3e` | [Unicode License V3 + bundled third-party notices](ICU-LICENSE.txt) |

ICU data: Unicode 16.0, CLDR 46 (verified pinned headers/data). Full locale and
boundary data are linked statically, not found via desktop ICU installation or
`ICU_DATA`. Static ICU common/i18n/data, FreeType and HarfBuzz; no host development
packages needed for those libraries. Existing Linux system SDL2/glibc/libstdc++/
OpenGL dependencies remain documented package requirements.

FreeType compression/PNG/Brotli/HarfBuzz integrations disabled. HarfBuzz OT shaping
uses immutable font bytes; it is configured before FreeType to prevent upstream
CMake from enabling its optional FT bridge automatically. No ICU shaper bridge,
subset utilities, duplicate shaper or circular FT/HB dependency. ICU tools required
to build data are build-only; tests/samples/extras/icuio disabled. Source archive
licenses/notices remain intact. The runtime notice file contains the chosen license
texts and is copied by the existing package exporter.

Demo font provenance and OFL/DejaVu licenses live beside each project's fonts.
Noto families are pinned by repository commit and content SHA256. Fonts are authored
project assets, not engine-global compulsory fallback families.

Library responsibilities: [HarfBuzz boundaries](https://harfbuzz.github.io/what-harfbuzz-doesnt-do.html),
[cluster/line flags](https://harfbuzz.github.io/harfbuzz-hb-buffer.html),
[maintainer fallback discussion](https://github.com/harfbuzz/harfbuzz/discussions/4949),
[FreeType face lifetime](https://freetype.org/freetype2/docs/reference/ft2-face_creation.html),
[ICU bidi](https://unicode-org.github.io/icu/userguide/transforms/bidi.html),
[break boundaries](https://unicode-org.github.io/icu/userguide/boundaryanalysis/),
[MessageFormat](https://unicode-org.github.io/icu/userguide/format_parse/messages/).
Implementation is checked against headers/source in these pinned archives.

## Windows candidate (unvalidated v1)

The same pinned source archives and hashes are retained. FreeType/HarfBuzz use
their CMake builds; ICU 76.1 uses its upstream Visual Studio solution's makedata
target, real data DLL and import libraries. Windows SDKs/packages distribute
icuuc76.dll, icuin76.dll and icudt76.dll rather than Linux static ICU archives.
Native compilation and text-rendering validation remain pending; see
[Windows instructions](../../docs/WINDOWS.md). No upstream source is patched.
