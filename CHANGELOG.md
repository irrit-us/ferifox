# Changelog

## Unreleased

### Added

- Added a Persona authoring guide covering the startup contract, supported fields, coherence rules, native-state boundaries, and validation checklist.

### Changed

- Applied persona device-pixel ratio through Firefox's native layout scale and the headless screen backend instead of overriding only `window.devicePixelRatio`.
- Aligned CSS device-size, color, and resolution media queries with Persona screen dimensions, depth, and effective pixel scale.
- Applied persona font visibility through Firefox's native system-font whitelist so CSS matching, metrics, and enumeration use the same filtered set.
- Applied the canonical persona locale to Gecko's regional preferences so default `Intl` formatting cannot inherit the host region.
- Loaded Persona state after profile preferences and before application services, then propagated the validated configuration to sandboxed child processes without reopening the source file.
- Applied Persona-controlled preferences as locked process defaults so automation cannot override them and Persona state is not written into the profile.
- Delivered configured geolocation after normal site permission approval without consulting the host provider or operating-system location permission.
- Made Persona canvas noise format-aware, deterministic across Canvas 2D readback and canvas encoding, idempotent for opaque pixels, and transparent-pixel preserving.
- Kept disconnected Web Audio analyser output at the native silent values.
- Restored native layout geometry, outer-window dimensions, HTTP priority/top-window state, and WebRTC statistics where the previous partial defenses created observable contradictions.
- Preserved Firefox's complete spec-defined PDF plugin and MIME arrays instead of allowing persona fields to expose truncated collections.
- Restored native cookie, PDF availability, online, Network Information, DNT, and GPC state so DOM getters agree with browser policy, events, and request headers.
- Restored native permission-query state instead of applying Persona clamps that did not control actual API access.
- Restored native storage, compressor reduction, and WebMIDI port identity state where getter-only overrides contradicted real behavior.
- Prevented host operating-system text scaling from multiplying a configured Persona device-pixel ratio.

### Fixed

- Made Persona loading transactional and parent-authoritative for child processes, preventing stale environment values, invalid source configurations, or partially parsed data from creating a split fingerprint.
- Preserved Firefox's native WebGL and WebGPU encoding paths when Persona canvas noise is disabled.
- Replaced impossible persona build IDs with Firefox's public compatibility value.
- Rejected invalid timezone, locale, geolocation, device-pixel ratio, font-list, string-array, and non-finite numeric values before applying host-visible state.
- Aligned Linux headless WebGL adapter strings with the regular Linux persona.
- Kept configured trusted mouse, pointer, and touch screen coordinates authoritative when fingerprinting resistance is also enabled.

## v0.2.0 - 2026-07-09

Second Ferifox stealth hardening release based on Firefox 152.0.4.

### Added

- Added computed-style coverage for Ferifox layout pixel noise.
- Added automated GitHub release packaging for full-feature Linux x86_64 and Win64 MSVC assets using the existing `v0.1.0` asset naming scheme.

### Changed

- Cached Ferifox layout noise seed lookup in `FerifoxConfig`.
- Adjusted layout noise hashing to avoid sibling index scans while preserving stable per-element variation.
- Kept WebGL persona texture unit limits internally consistent across combined and per-stage caps.

### Fixed

- Fixed computed style reporting so Ferifox layout noise applies consistently to CSS pixel dimensions.
- Fixed WebGL texture unit persona caps that could expose impossible combined and per-stage values.

### Verification

- `./mach format`
- `./mach build binaries`
- `git diff --check`

### Notes

- Release assets remain consistent with `v0.1.0`: Linux binary, Linux tarball, Windows executable or installer, Windows zip, and one `.sha256` file for each.

## v0.1.0 - 2026-07-07

Initial Ferifox stealth release based on Firefox 152.0.4.

### Added

- Added `FerifoxConfig`, an external JSON persona configuration layer loaded through `FERIFOX_CONFIG`.
- Added persona-backed overrides for navigator, screen, window geometry, WebGL, audio, fonts, geolocation, locale, timezone, HTTP headers, WebRTC address exposure, and network connection surfaces.
- Added Linux, Linux headless, Linux HiDPI, macOS, Windows 10, and Windows 11 persona configuration files.
- Added `ferifox` as the desktop executable name.
- Added documentation in `STEALTH-NOTES` for automation entry paths, Playwright and Puppeteer behavior, evaluator boundaries, crawler inspection limits, recommended preference baselines, and Cloudflare-relevant consistency checks.

### Changed

- Program-driven sessions now inherit regular browser/profile/header state instead of applying high-signal automation defaults through Remote Agent, Marionette, geckodriver, or Puppeteer profile setup.
- Headless startup geometry, `screen.orientation`, legacy orientation, `mozInnerScreenX/Y`, `screenX/Y`, and trusted mouse and pointer event coordinates now align with persona state.
- HTTP and passive browsing context surfaces now read from the synchronized Ferifox persona state where configured.

### Fixed

- Fixed Ferifox headless persona startup so runtime configuration is initialized early enough for headless operation.
- Fixed WebGL persona fallback behavior when configured vendor and renderer values are absent.

### Verification

- `python3 -m json.tool browser/config/ferifox/personas/*.json`
- `./mach format`
- `./mach build binaries`
- `node --check remote/shared/RecommendedPreferences.sys.mjs`
- `python3 -m py_compile testing/marionette/client/marionette_driver/geckoinstance.py`
- `git diff --check`
- Targeted `./mach lint` runs for modified automation preference files.

### Notes

- This release reduces inconsistencies between regular headed browsing and program-driven browsing at the configured fingerprint and algorithm surfaces.
- Passing third-party robot checks is not guaranteed by the patch alone and remains target- and environment-specific.
