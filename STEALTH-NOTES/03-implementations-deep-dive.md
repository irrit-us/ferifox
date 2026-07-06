# Open-Source Stealth Implementations: Technical Deep-Dive

## 1. Patchright

**Type:** Playwright binary/library patch suite
**Languages:** Python, Node.js, .NET
**Repository:** Kaliiiiiiiiii-Vinyzu/patchright-python (GitHub)
**Active Maintainer:** Vinyzu
**Stars:** ~1.4k
**Latest Release:** v1.61.2 (July 5, 2026)
**License:** Apache 2.0

### Architecture
Patchright is a **drop-in replacement for Playwright** that applies stealth patches to the automation library. It is NOT a custom browser build — it patches the Playwright library's CDP protocol usage. It only supports Chromium (not Firefox or WebKit).

### Key Patches

**Runtime.enable Leak Fix (Primary)**
The most important patch. Playwright normally calls `Runtime.enable` in the CDP to get execution context IDs. Anti-bot scripts (Cloudflare, DataDome) detect this CDP command usage. Patchright avoids `Runtime.enable` entirely, instead executing JS in isolated `ExecutionContexts` obtained through alternative means.

**Console.enable Leak Fix**
Disables the Console API entirely to prevent detection via console binding leaks. Anti-bot scripts can detect Proxy-wrapped console objects placed by automation.

**Command-Line Flag Modifications**
| Flag | Action | Purpose |
|------|--------|---------|
| `--disable-blink-features=AutomationControlled` | Added | Hides `navigator.webdriver` |
| `--enable-automation` | Removed | Removes automation indicator |
| `--disable-popup-blocking` | Removed | Avoids popup crashes signature |
| `--disable-component-update` | Removed | Avoids stealth driver detection |
| `--disable-default-apps` | Removed | Enables default apps (normal behavior) |
| `--disable-extensions` | Removed | Enables extensions (normal behavior) |

**Closed Shadow Root Support**
Can interact with elements inside closed shadow roots using standard locators — capability normal Playwright lacks.

### Best-Practice Configuration (Patchright's Own Recommendation)
```python
context = playwright.chromium.launch_persistent_context(
    user_data_dir="./user-data",
    channel="chrome",       # Real Chrome, not Chromium
    headless=False,         # Headless is more detectable
    no_viewport=True,       # No viewport override
    # Do NOT add custom user_agent or headers
)
```

### Extended API
Patchright adds an `isolated_context` parameter (boolean, defaults `True`) to all `evaluate`, `evaluate_handle`, and `evaluate_all` methods — letting callers choose Main or Isolated execution context per-call. This granularity is unique among CDP-level stealth tools.

### Evasion Results
Claims to pass: Brotector (with CDP-Patches), Cloudflare, DataDome, Akamai, Kasada, Fingerprint.com, CreepJS, Sannysoft, Incolumitas, IPHey, Browserscan, Pixelscan, Bet365, Shape/F5.

### Limitations
- Chromium only — no Firefox or WebKit support
- No C++-level fingerprint spoofing (canvas, WebGL, audio — relies on the browser's own fingerprint)
- No behavioral humanization (mouse, typing)
- Success rate drops against enterprise WAFs without residential proxies

---

## 2. Rebrowser Patches

**Type:** JavaScript-level patches for Puppeteer/Playwright libraries
**Package:** `rebrowser-patches` (npm), `rebrowser-playwright` (npm/PyPI)
**Repository:** rebrowser/rebrowser-patches (GitHub)
**Stars:** ~1.4k
**Latest:** v1.0.19 (May 2025) — stable; development pace has slowed
**Tested Against:** Puppeteer 24.8.1, Playwright 1.52.0

### Architecture
Patches the **Node.js library source code** — NOT the Chromium binary. Works by modifying Puppeteer/Playwright's JS files on disk (via `patch` command) or by providing pre-patched drop-in packages.

### Key Patches

**Runtime.enable Fix (Three Modes)**
Configurable via `REBROWSER_PATCHES_RUNTIME_FIX_MODE`:
- `addBinding` (default) — Creates a binding in the main world to get context ID without triggering detection. Works with web workers and iframes. Described as the "ultimate approach."
- `alwaysIsolated` — Executes all scripts via `Page.createIsolatedWorld`. Trade-off: cannot access main context variables/code.
- `enableDisable` — Calls `Runtime.enable` then immediately `Runtime.disable` to grab context ID. Low risk but not zero.

**sourceURL Normalization**
Replaces Puppeteer's `//# sourceURL=pptr:...` trace with generic `app.js` (customizable via `REBROWSER_PATCHES_SOURCE_URL`).

**Utility World Name Change**
Changes `__puppeteer_utility_world__` (detectable by anti-bot scripts) to `util` (customizable via `REBROWSER_PATCHES_UTILITY_WORLD_NAME`).

### Drop-in Replacement Packages
| Original | Pre-patched |
|----------|-------------|
| `puppeteer` | `rebrowser-puppeteer` |
| `puppeteer-core` | `rebrowser-puppeteer-core` |
| `playwright` | `rebrowser-playwright` |
| `playwright-core` | `rebrowser-playwright-core` |

Usage via npm alias: `"puppeteer": "npm:rebrowser-puppeteer@^24.8.1"`

### Limitations
- Fragile — `npm install` overwrites patches; must re-apply
- Chromium only
- No C++-level fingerprint spoofing
- README explicitly warns: "Not a full anti-detection solution — you also need good proxies, proper user-agents, and fingerprint spoofing"
- No behavioral humanization

### Comparison with Patchright
Both target the same CDP detection vectors. Patchright takes a Python-first approach with deeper CDP integration; Rebrowser provides finer-grained runtime fix modes and npm-level drop-in convenience. Patchright and Rebrowser effectively compete at the same detection layer (Layer 1: Protocol Detection).

---

## 3. Camoufox

**Type:** Custom Firefox build with C++-level patches
**Language:** Python interface; C++ browser core
**Repository:** daijro/camoufox (GitHub) — active development at CloverLabsAI/camoufox
**Stars:** ~9.9k
**Latest Browser Release:** v150.0.2-beta.25 (May 11, 2026) — based on Firefox 150
**Latest Python Wrapper:** 0.4.11 (January 2025)
**License:** MPL-2.0
**Status:** Under active development — may not be suitable for stable production use
**Architecture:** Forked Firefox + 140+ distinct C++ patches

### Core Innovation: C++-Level Spoofing
All fingerprint manipulation happens **inside the browser engine's C++ code**, before JavaScript ever sees the values. This is fundamentally more robust than JS-level spoofing because:
- `Object.defineProperty` hooks are detectable via prototype inspection
- Proxy objects are detectable via `toString()` and constructor checks
- C++ modifications leave no JS-visible trace

### Architecture Components

**MaskConfig** — Central C++ class that reads fingerprint config from environment variables at startup. Provides typed retrieval methods (GetString, GetInt32, GetUint32, GetDouble, GetBool, GetStringList) used across 140+ injection sites. Each returns `std::optional` so sites fall back to genuine hardware defaults when no config exists.

**RoverfoxStorageManager** — Cross-process key-value store synchronized via Firefox's IPDL (Inter-Process Communication). Maintains local cache for fast reads; propagates updates across parent/content/gpu processes. Keyed by `userContextId` for per-context isolation.

**Self-Destructing APIs** — Temporary JS functions (`window.setTimezone()`, `window.setWebGLVendor()`, etc.) injected into `window`. Once called, they store the value in RoverfoxStorageManager and self-delete, leaving no trace of the control surface.

**BrowserForge Integration** — Uses real-world traffic statistics to generate realistic fingerprint distributions per OS (Windows, macOS, Linux, Android, iOS). The Python interface calculates timezone, locale, and geolocation automatically based on proxy egress region.

### Patch Categories (140+ Vectors)

**Navigator & Device (patched in `nsGlobalWindowInner.cpp`, `Navigator.cpp`)**
- `navigator.userAgent`, `navigator.platform`, `navigator.oscpu`
- `navigator.hardwareConcurrency`, `navigator.deviceMemory`
- `navigator.webdriver` → always false (patched in `Navigator::Webdriver()`)

**Screen & Window (patched in `nsScreen.cpp`, `nsGlobalWindowInner.cpp`)**
- `screen.width/height`, `screen.availWidth/availHeight`, `screen.colorDepth`
- `window.innerWidth/innerHeight`, `window.outerWidth/outerHeight`
- `window.devicePixelRatio`

**WebGL (patched in `WebGLParamsManager.cpp`)**
- GPU vendor/renderer strings (UNMASKED_VENDOR/RENDERER_WEBGL)
- Shader precision formats
- Context attributes
- Supported extensions list
- 3D scene render hashing

**Canvas (CanvasFingerprintManager)**
- ±1 noise to a single non-zero RGB channel per pixel
- Seeded per context for deterministic replay
- Cannot be detected via consistency checks (stable per context)

**Audio (AudioFingerprintManager)**
- Linear Congruential Generator with 0.8% variance
- Non-linear polynomial modification of wave data
- Spoofs sample rate, output latency, max channel count

**Fonts (gfxPlatformFontList.cpp, FontFace.cpp)**
- Strict whitelist enforcement at C++ level
- Hijacks `kFontSystemWhitelistPref`
- Non-whitelisted fonts return error status at C++ level
- FontSpacingSeedManager — deterministic spacing values prevent metrics fingerprinting
- Bundled with Windows/macOS/Linux system fonts
- Correct subpixel antialiasing and hinting for target OS

**WebRTC (WebRTCIPManager)**
- Spoofs local IPv4/IPv6 in ICE candidates
- SDP string modification
- `getStats()` return value modification
- Per-context IP isolation

**Geolocation (Geolocation.cpp)**
- Latitude, longitude, accuracy override
- Auto-approval of position requests

**Timezone & Locale (TimezoneManager.cpp)**
- `Intl.DateTimeFormat` override
- `Date` API override
- DST behavior consistency

**Speech/Voice (nsSynthVoiceRegistry.cpp)**
- Filters voices returned by `speechSynthesis.getVoices()`
- Spoofs voice characteristics

**Network**
- `User-Agent` and `Accept-*` headers spoofed at network layer
- Matches navigator properties

**Battery API**
- `charging`, `level`, `chargingTime`, `dischargingTime` spoofed

**Additional Stealth Patches**
- Juggler isolation for `Runtime.enable` bypass (Firefox-specific, unique to Camoufox)
- Fixes Firefox headless detection via pointer type
- Removes leaking anti-zoom/meta-viewport patches
- Re-enables Fission content isolation and PDF.js
- Strips telemetry, CSS animations, Mozilla services (~200MB memory usage)
- LibreWolf/Ghostery/PeskyFox debloating
- FastFox speed/network optimizations

### Human-Like Mouse
Ported from `riflosnake/HumanCursor` to C++. Bézier curve trajectories, distance-aware paths.

### Detection Results
| Test | Result |
|------|--------|
| CreepJS | 71.5% (spoofs all OS predictions, 0% headless score) |
| BrowserScan | 100% |
| reCaptcha v3 | Score 0.9 |
| Cloudflare | Pass (Turnstile, Interstitial) |
| Imperva | Pass |
| DataDome | Pass |
| Rebrowser Bot Detector | Pass |
| SannySoft | Pass |

### Limitations
- Firefox-only (~3% market share) — some WAFs flag Firefox users more aggressively
- Not a drop-in for Chromium-based workflows
- 312 built-in fingerprints; rotation may not cover all edge cases

---

## 4. CloakBrowser

**Type:** Custom Chromium build with C++ source-level patches
**Repository:** CloakHQ/CloakBrowser (GitHub)
**Stars:** ~27.8k
**Forks:** ~2.2k
**Languages:** Python, Node.js, .NET (C# NuGet package)
**Latest Release:** v0.4.8 (July 5, 2026)
**Current:** Chromium 148.0.7778.215.5, **66 C++ patches** (up from 59)
**License:** MIT (wrapper), delayed free-release (v146 free, v148+ Pro)

### Architecture
CloakBrowser modifies Chromium's C++ source code and compiles the patches directly into the binary. Unlike Patchright/Rebrowser (which patch the automation library) or Camoufox (which patches Firefox), CloakBrowser is the **Chromium counterpart to Camoufox's approach**.

### Patch Evolution
| Version | Patches | Key Additions |
|---------|---------|---------------|
| Early 2024 | 26 | Canvas, WebGL, audio, fonts, GPU, screen, automation signals |
| Mid 2024 | 33 | CDP input, network timing, hardware reporting |
| Late 2024 | 49 | WebRTC, WebAuthn, window position, WebGL/canvas consistency |
| Early 2025 | 58 | AAC audio, rendering consistency, GPU passthrough |
| Mid 2025 | 59 | Proxy signal removal, Chromium 148 rebase |
| Mid 2026 | **66** | SOCKS5 proxy, `humanize=True`, persistent contexts, Windows GPU passthrough, Ed25525 binary signature verification, fingerprint management, per-call human_config |

### Patch Categories (Current 66 Patches)

**Canvas / WebGL / Audio**
- Noise injected at the rendering layer
- Fingerprints vary per session but remain internally consistent
- Spoofed GPU vendor/renderer strings, extensions, shader precision
- Audio context sample rate, compressor curve consistent with claimed hardware

**Navigator Automation Signals**
- `navigator.webdriver` → always `false`
- `navigator.plugins.length` → 5 (normal Chrome)
- `window.chrome` → restored as a proper object (removed in headless Chromium)

**User-Agent**
- Removes `HeadlessChrome` from UA string
- Returns normal `Chrome/146.0.0.0` pattern

**WebRTC**
- ICE candidates and IP addresses auto-matched to proxy egress
- `getStats()` modified
- SDP string sanitized

**TLS Fingerprint**
- JA3/JA4/Akamai fingerprints match real Chrome identically
- Cipher suite ordering identical to stock Chrome
- TLS extensions match Chrome's expected set

**CDP / Input Behavior (5 dedicated patches)**
- Keyboard, mouse, scroll events use isolated worlds
- Trusted event dispatch (not synthesized `Event` objects)
- CDP input commands mimic real user input path

**Network Timing**
- DNS/connect/SSL timing zeroed to avoid timing-based detection
- Proxy cache headers stripped (no `X-Forwarded-For`, `Via`, etc.)

**Hardware Spoofing**
- CPU cores, memory size, screen resolution
- GPU vendor/renderer strings

**Behavioral Humanization**
- `humanize=True` flag enables:
  - Bézier curve mouse movement
  - Human-like typing rhythm
  - Natural scroll patterns
- All implemented in the C++ binary, not injected JS

### Detection Results

| Test | Stock Playwright | CloakBrowser |
|------|-----------------|-------------|
| reCAPTCHA v3 | 0.1 (bot) | **0.9 (human)** — server-verified |
| Cloudflare Turnstile | FAIL | **PASS** |
| FingerprintJS | DETECTED | **PASS** (with specific config) |
| BrowserScan | DETECTED | **NORMAL (4/4)** |
| `navigator.webdriver` | `true` | **`false`** |
| TLS fingerprint | Mismatch | **Identical to Chrome** |

### Known Detection Risk: FingerprintJS
FingerprintJS can sometimes detect CloakBrowser even on the latest binary. The recommended workaround for Chromium 148+ Pro:
```python
browser = launch(
    headless=False,
    proxy="http://user:pass@residential-proxy:port",
    geoip=True,
    args=[
        "--fingerprint-noise=false",          # prevents tampering detection
        "--fingerprint-windows-font-metrics", # align font metrics
    ],
)
```
Residential proxy, geoip=True, and Windows fonts on Linux are required.

### Other Notable Features (v0.4.8)
- **SOCKS5 proxy** — native support via `proxy="socks5://user:pass@host:port"`, QUIC/HTTP3 tunnel via UDP ASSOCIATE
- **Persistent contexts** — `launch_persistent_context()` for cookies/localStorage, bypasses incognito detection
- **Binary verification** — every download verified against pinned Ed25519 signature + GPG/Sigstore attestation
- **Fingerprint seed management** — fixed seed for consistent device identity across sessions
- **Auto-download** — binary auto-downloads on first run (~200MB, cached)
- **Custom extension loading** — `extension_paths` parameter for Chrome extensions
- **Per-call human_config** — override humanize settings on individual method calls

### Licensing
- Wrapper: MIT open-source
- Older Chromium binary (v146): free for personal and commercial use (no redistribution)
- Newest Pro binary (v148+): paid subscription required
- Binary download verified via pinned Ed25519 signature; additional GPG + Sigstore attestation

### Comparison with Camoufox
CloakBrowser explicitly contrasts with Camoufox: Camoufox patches Firefox (~3% market share) while CloakBrowser patches Chromium (~70% market share). CloakBrowser claims production-readiness, while the Camoufox project self-describes as "under development — may not be suitable for stable production use." Chromium-based means native Playwright support, a larger ecosystem, and TLS fingerprints matching real Chrome.

---

## Cross-Implementation Comparison

### Defense Layer Coverage

| Layer | Patchright | Rebrowser | Camoufox | CloakBrowser |
|-------|-----------|-----------|----------|-------------|
| **Layer 1: Protocol** (CDP leaks, Runtime.enable) | ✅✅✅✅✅ | ✅✅✅✅ | ✅✅✅ (Juggler) | ✅✅✅✅✅ |
| **Layer 2: Fingerprinting** (canvas, WebGL, audio, fonts) | ❌ | ❌ | ✅✅✅✅✅ | ✅✅✅✅✅ |
| **Layer 3: Behavioral** (mouse, typing, scroll) | ❌ | ❌ | ✅✅✅✅ | ✅✅✅✅✅ |
| **Layer 4: Network** (TLS, WebRTC, DNS) | ❌ | ❌ | ✅✅✅ | ✅✅✅✅ |
| **Layer 5: Layout** (getBoundingClientRect, styles) | ❌ | ❌ | ✅✅✅ | ✅✅✅✅ |

### Implementation Depth

| Tool | Patch Level | Browser Engine | Detectability of Spoof |
|------|------------|----------------|------------------------|
| Patchright | CDP protocol library | Stock Chromium | JS-visible |
| Rebrowser | JS source patching | Stock Chromium | JS-visible |
| Camoufox | C++ browser engine | Custom Firefox | JS-invisible |
| CloakBrowser | C++ browser engine | Custom Chromium | JS-invisible |

### Strengths Matrix

| Tool | Best For |
|------|----------|
| Patchright | Quick drop-in for existing Playwright scripts; CDP evasion (v1.61.2, Jul 2026) |
| Rebrowser | npm ecosystem; fine-grained Runtime.enable mode selection (v1.0.19, May 2025) |
| Camoufox | Maximum fingerprint rotation; Firefox-specific; 312 fingerprints (v150.0.2, 9.9k stars) |
| CloakBrowser | Chromium C++ patches; SOCKS5; humanize; FPJS workaround (v0.4.8, 66 patches, 27.8k stars) |
