---
name: ferifox
description: Build, run, and drive the Ferifox browser (the Firefox stealth fork in this tree). Use when working on FerifoxConfig/personas, .ferifox-cookie import, MCP-driven automation, or crawler integration.
---

# Ferifox

Ferifox is a Firefox-based stealth browser fork (`MOZ_APP_NAME=ferifox`, binary
`ferifox` / `ferifox.exe`). Downstream features live in
`toolkit/components/ferifox/` (FerifoxConfig persona engine),
`browser/components/ferifox/` (startup cookie import), and
`browser/config/ferifox/personas/` (checked-in personas, packaged next to the
binary). See `STEALTH-NOTES/` for the fingerprinting research and
`CHANGELOG.md` for release history.

## Build and run

```sh
./mach build            # or ./mach build binaries for C++-only changes
./mach run              # launches ferifox with a scratch profile
```

## Personas

A persona is a process-wide JSON identity (UA, screen, WebGL, fonts, locale,
timezone, geolocation, WebRTC, ...). Full field reference and coherence rules:
`browser/config/ferifox/personas/README.md`.

- `FERIFOX_CONFIG=<abs path to persona.json>` — explicit persona; always wins.
- Unset — the packaged platform default (`browser/personas/win11-cn-firefox.json`
  on Windows — the zh-CN persona shared with the crawler —
  `browser/personas/macos-firefox.json` on macOS, or
  `browser/personas/linux-firefox.json` on Linux, inside the browser's install
  directory) is loaded automatically when present.
- `FERIFOX_CONFIG=none` — disables persona loading entirely.

Launchers must never set `FERIFOX_CONFIG_JSON`; it is internal parent→child
transport state.

## Startup cookie import (.ferifox-cookie)

At startup (before the first window), cookies are imported from the file named
by `FERIFOX_COOKIES`, or from `.ferifox-cookie` in the profile directory when
the variable is unset. Format: a JSON array of cookie objects.

```json
[
  {
    "name": "session",
    "value": "abc123",
    "domain": ".example.com",
    "path": "/",
    "secure": true,
    "httpOnly": true,
    "sameSite": "Lax",
    "expires": 1893456000
  }
]
```

`name`/`value`/`domain` are required; a leading dot in `domain` marks a domain
cookie; `path` defaults to `/`; `sameSite` accepts `None`/`Lax`/`Strict`;
`expires` is Unix seconds (omit for a session cookie). Invalid entries are
skipped and logged to the Browser Console. Automation should write the file
into the profile directory before launch (e.g. the crawler's
`profile_dir`, geckodriver's `-profile`, or the MCP `--profile-path`).

## MCP listener

The in-tree `.mcp.json` (mirrored in `.codex/config.toml`) registers the
`firefox-devtools` MCP server; an MCP-capable agent spawns it automatically.
It runs `./mach firefox-devtools-mcp`, which targets the ferifox build via
`MOZ_APP_NAME`. Manual spawn:

```sh
./mach firefox-devtools-mcp                                   # local build
./mach firefox-devtools-mcp --firefox-path /path/to/ferifox   # installed build
./mach firefox-devtools-mcp --env FERIFOX_CONFIG=/abs/persona.json
```

The default persona applies with no `--env`; use `FERIFOX_CONFIG=none` to opt
out. Runtime persona changes need a browser restart
(`restart_firefox(env=["FERIFOX_CONFIG=..."])`). Capabilities (navigate,
snapshot, click, network/console capture, screenshot, chrome-context
evaluation): `docs/ai-agent-tools/firefox-devtools-mcp.md`.

## Crawler usage

The reference crawler (`irrit-us/ferifox_crawler_skills` on GitHub) drives
ferifox through geckodriver + the stock selenium Firefox API — no
ferifox-specific client code. Environment contract:

- `FERIFOX_BINARY` — path to the ferifox executable.
- `GECKODRIVER_BINARY` — path to geckodriver (Selenium Manager resolves it
  when unset).
- `FERIFOX_CONFIG` — persona JSON path, or a bundled persona name resolved
  against the crawler's `personas/` directory.

Entry points: `FerifoxConfig` + `launch()` + `FerifoxSession` in `ferifox.py`;
CLI via `python -m crawler crawl <url>`. Never commit personas, profiles,
`output/`, or `local/credentials.json`.

## Help and reference docs

- `ferifox --help` lists the Ferifox environment block; the Linux manpage
  (`browser/installer/linux/app/debian/manpage.1.in`) documents the same in
  ENVIRONMENT and FILES.
- Persona authoring checklist and native-state boundaries:
  `browser/config/ferifox/personas/README.md`.

## Verification

```sh
python -m json.tool browser/config/ferifox/personas/*.json > /dev/null
./mach build binaries
./mach build faster   # after touching browser/components/ferifox JS
./mach xpcshell-test browser/components/ferifox/tests/xpcshell/
./mach format         # before committing
git diff --check
```
