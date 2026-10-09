# `localization` — project messages and horizontal Unicode text

Current localization API (introduced in M58), using ICU4C 76.1 MessageFormat and number data. Import
`localization` from the virtual `judas` module. QuickJS has no `Intl` requirement.
Judas owns primitives; projects supply fonts/catalogs; scripts choose language policy.

## Namespace

| Member | Contract |
|---|---|
| `locale: string` | Currently published canonical BCP 47 tag. |
| `available: string[]` | Explicit configured tags, sorted by tag. |
| `revision: number` | Changes when catalogs or locale publish; cache dynamic HUD strings against it. |
| `direction: "ltr" \| "rtl"` | ICU locale orientation; does not mirror the game/world. |
| `setLocale(tag): boolean` | Queues a configured locale, returns true. Invalid tag throws TypeError. Publication waits for catalogs and fonts; old locale stays live meanwhile. |
| `format(key, args = {}): string` | Named string/finite-number arguments; whole message formatted with resolved catalog's locale. Missing key returns `[key]` and bounded diagnostic; wrong/missing argument throws TypeError. |
| `number(value, options = {}): string` | Current display locale; finite number. `minimumFraction=0`, `maximumFraction=3`, `grouping=true`; fraction bounds 0–12 and min ≤ max. Invalid input throws TypeError. |
| `reload(): void` | Invalidates active catalog chain, asynchronously validates replacement before publication. Invalid replacement leaves prior texts live; inspect ResourceManager/editor diagnostics. |

At first asynchronous startup, formatting may temporarily return `[key]` until
the configured resources publish. The `localization catalog not yet published`
diagnostic is non-throwing, just like `missing localization key`; genuinely absent
keys retain their normal missing-key diagnostic. Invalid arguments still throw.
Refresh cached HUD strings against `revision`;
do not assume one-time formatting in `start()` is permanently ready.

Formatting accepts at most 32 named arguments. Keys ≤128 UTF-8 bytes, string
arguments ≤16 KiB, output ≤64 KiB. Arguments are data, never nested templates.
Use `uiUpdate` for paused menus or ordinary `update` for HUD changes. This is
presentation; never use translated text/locale numbers as simulation input.

```js
import {localization, ui} from 'judas';
const hud = ui.get('hud');
if (hud) hud.get('remaining').text =
  localization.format('hud.remaining', {count: 22});
```

## Catalogs / project settings

Normal registered `.judasloc` UTF-8 asset, stable asset ID. Example:

```text
JudasCatalog 1 "ru"
"hud.remaining" "{count,plural,one{# цель}few{# цели}many{# целей}other{# цели}}"
"menu.choice" "{mode,select,new{Новая игра}other{Продолжить}}"
"greeting" "{name}, добро пожаловать!"
```

Quoted strings use backslash to escape quote/backslash (not JSON `\n` decoding;
actual newlines are permitted within quoted messages). Keys are stable identifiers;
use `table.key` namespaces by convention, not translated sentences. No normalization
of source strings or keys. Duplicate keys, malformed UTF-8/patterns, incompatible
argument use and positional arguments fail import. Limits: 2 MiB/catalog,
4096 messages, 16 KiB/message. Supported ICU patterns: named plain arguments,
`number`, `plural`, `selectordinal`, `select`. Date/time/legacy choice unsupported.
ICU apostrophe quoting: quote `{`, `}`, `#` when literal syntax is intended; double
apostrophe represents apostrophe. See [ICU syntax](https://unicode-org.github.io/icu/userguide/format_parse/messages/).

Project settings record a default, up to 32 explicit canonical BCP 47 locales,
one catalog each, optional configured fallback and ordered font IDs (≤16/list).
Fallback order: requested tag → configured fallback chain → registered parent
tags → default chain, deduplicated. Cycles reject configuration. No host-locale
fallback. A fallback message uses **its catalog locale** for plural/embedded-number
rules; `number()` uses **the published display locale**.

Locale state is project/runtime-session owned. Scene load/reload preserves it;
Stop/project replacement creates fresh defaults. It is not persistent user settings,
a legacy save delta or fingerprint input. Explicit [M61 slots](saves.md) preserve
the selected locale alongside session state; this is not automatic user-preference
storage. Authored catalogs/font bytes/configuration are
content fingerprint inputs. Resource replacement publishes a complete validated
chain, never part of a changed catalog. A failed resource is not retried automatically;
fix the file then call `reload()`.

## Runtime UI text

`UIElement.textKey` binds a static message without arguments. Assign `.text` to
show a dynamic formatted result and **clear textKey**. Assign `.textKey` to restore
binding. Key assignments are ≤128 UTF-8 bytes without NUL/newline; invalid keys
throw TypeError. `.font` is a registered font ID or empty engine-default selection.
`.direction`: `auto` (inherit parent, else paragraph Unicode analysis), `ltr`, `rtl`.
`.textAlignment`: `left`, `right`, `center`, `start`, `end`; legacy numeric alignment
reads empty string until explicitly set. Start/end follow each paragraph direction.

Primary UI font → locale ordered fonts → global ordered fonts. On absent fonts,
the engine's packaged DejaVu font provides safe fallback. Source assets are validated;
invalid runtime UTF-8 is replaced with U+FFFD, not blindly byte-truncated. Missing
coverage uses the primary font's `.notdef` with bounded diagnostics. Fonts are
on-demand resources, not desktop-installed families. No universal font claim.

Text and measurement use the same immutable layout: ICU paragraph bidi and Unicode
line/grapheme breaks, HarfBuzz directional/script/font shaping, FreeType grayscale
outline rasterization. Mixed scripts and marks preserve grapheme clusters/context.
Whole script runs are preferred for fallback; if none covers a run, whole grapheme
clusters select fonts. A cluster no font covers remains visibly unsupported. Wrapped
lines resolve visual bidi order and are reshaped; long tokens emergency-wrap only
at both grapheme and shaping-cluster boundaries, otherwise overflow/clip. Horizontal
UI supports font metrics/mark extents, reference scaling and normal container clips.

Direction does not automatically mirror images, icons, cameras or input axes. An
authored horizontal container may explicitly enable `mirrorRow` for RTL locale.
Focus order stays authored order. UI remains after tone mapping, independent of
world exposure. See [UI](ui.md) and [font/layout implementation](../M58.md).

## Scope / tooling

Scalable Unicode outline TTF/OTF, first face only, horizontal grayscale rendering.
No color emoji/SVG glyph paints, vertical writing, ruby, rich text, advanced
justification/hyphenation, text entry/IME/caret/selection or automatic translation.
Supplementary decoding is supported independently of font coverage. Demo translations
have not received native-speaker review. No process-global locale mutation.

[Executed example](examples/localization.js) · [API inventory](API_INVENTORY.md) ·
[Text lab](../../projects/text_lab/text_lab.judasproj) · [Lifecycle](lifecycle.md)
