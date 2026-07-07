# Changelog

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
