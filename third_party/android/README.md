# Optional Android build sources

These archives are used only when CMake's ANDROID target is enabled. Desktop
builds keep their existing system/vcpkg dependencies. No configure-time download.

| Source | Upstream | SHA-256 | Licence |
|---|---|---|---|
| SDL2-2.32.10.tar.gz | https://www.libsdl.org/release/SDL2-2.32.10.tar.gz | 5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165 | zlib, SDL-LICENSE.txt |
| glm-1.0.1.tar.gz | https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz | 9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c | MIT, GLM-COPYING.txt |

Archives are unmodified. tools/build_android.py copies SDL Java sources into its
staging directory and makes two asserted receiver-registration adaptations for
Android 13+ target SDK rules, described in docs/ANDROID.md. No other vendor source
changes are applied. Existing pinned FreeType/HarfBuzz/ICU/QuickJS/Recast sources
are reused; Android does not substitute a different physics or scripting runtime.
