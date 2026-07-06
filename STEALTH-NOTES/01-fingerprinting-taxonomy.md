# Browser Fingerprinting: Complete Taxonomy

## 1. HTTP & TLS Fingerprinting (Pre-JS Layer)

### TLS Fingerprint (JA3 / JA4)
TLS fingerprinting operates **before any HTML, CSS, or JavaScript runs** — the server classifies the client during the TLS handshake itself. A JA3 hash is computed from:
- Ordered list of TLS cipher suites
- TLS extensions (supported groups, key share, SNI, ALPN, signature algorithms, PSK modes)
- Supported elliptic curves (x25519, secp256r1, etc.)
- Signature algorithms (rsa_pss_rsae_sha256, ecdsa_secp256r1_sha256)
- ALPN values (h2, http/1.1)

JA4 extends JA3 by incorporating:
- HTTP/2 ALPN settings and stream priority parameters
- Frame ordering and flow control behavior
- TLS extension ordering (not just presence)
- QUIC/HTTP3 transport parameters

JA4 provides finer-grained differentiation — two browsers sharing the same JA3 hash can have different JA4 fingerprints. **This is the hardest vector to spoof** — only CloakBrowser (Chromium 148) claims JA3/JA4 identity with real Chrome via its 66 C++ patches. At the HTTP library level, Scrapling's `curl_cffi` is the only open-source attempt to mimic browser TLS fingerprints.

### HTTP Headers
- `User-Agent` — browser, OS, device model (~8-10 bits entropy)
- `Accept`, `Accept-Encoding`, `Accept-Language`, `Accept-Charset` — header ordering and values vary by browser
- `Sec-CH-UA`, `Sec-CH-UA-Platform`, `Sec-CH-UA-Mobile`, `Sec-CH-UA-Model` — Client Hints
- `Sec-CH-UA-Platform-Version`, `Sec-CH-UA-Arch`, `Sec-CH-UA-Bitness`, `Sec-CH-UA-Full-Version-List`
- `Sec-Fetch-Dest`, `Sec-Fetch-Mode`, `Sec-Fetch-Site`, `Sec-Fetch-User`
- `Upgrade-Insecure-Requests`
- `DNT` (Do Not Track)
- Cookie headers — presence/absence and ordering

## 2. Navigator & Window Properties (JS Layer)

### Core Identification
| Property | Entropy | Notes |
|----------|---------|-------|
| `navigator.userAgent` | ~8-10 bits | Frozen in most browsers but still checked |
| `navigator.platform` | ~3-4 bits | OS platform string |
| `navigator.language` / `navigator.languages` | ~5-6 bits | Primary and fallback languages |
| `navigator.hardwareConcurrency` | ~4-5 bits | CPU logical core count |
| `navigator.deviceMemory` | ~2-3 bits | Approximate RAM (0.25, 0.5, 1, 2, 4, 8 GB) |
| `navigator.webdriver` | 1 bit | Automation flag — **must always be false** |
| `navigator.doNotTrack` | 1 bit | |
| `navigator.cookieEnabled` | 1 bit | |
| `navigator.pdfViewerEnabled` | 1 bit | |
| `navigator.productSub` | ~2 bits | Build identifier |
| `navigator.vendor` / `navigator.vendorSub` | ~2-3 bits | |
| `navigator.product` | 1 bit | Almost always "Gecko" |
| `navigator.appVersion` | ~5 bits | Legacy; mirrors UA fragments |
| `navigator.appName` | 1 bit | Always "Netscape" |
| `navigator.appCodeName` | 1 bit | Always "Mozilla" |
| `navigator.buildID` | ~3 bits | Firefox-specific build identifier |

### Plugins & MIME Types
- `navigator.plugins` — array of installed browser plugins (PDF Viewer, Chrome PDF Viewer, etc.)
- `navigator.mimeTypes` — supported MIME types
- Both are iterable; length and named entries create a fingerprint

### Automation Leak Vectors (CDP Detection)
- `navigator.webdriver` — Boolean that automation tools set to `true`
- `window.chrome` — Chromium-only; must be a non-null object in Chrome, undefined in Firefox
- `__puppeteer_utility_world__` — detectable world name in Puppeteer
- `//# sourceURL=pptr:...` — script source URL annotation left by Puppeteer
- `Runtime.enable` — CDP command that anti-bot scripts detect via JS probes
- `console` bindings — detectable Proxy objects on console methods

## 3. Screen & Display

### Direct Properties
- `screen.width` / `screen.height` — physical resolution (~4-6 bits)
- `screen.availWidth` / `screen.availHeight` — minus taskbar/dock
- `screen.colorDepth` / `screen.pixelDepth` — color bit depth
- `window.devicePixelRatio` — HiDPI scaling (1, 1.25, 1.5, 2, 3)
- `window.innerWidth` / `window.innerHeight` — viewport dimensions
- `window.outerWidth` / `window.outerHeight` — window chrome included
- `window.screenX` / `window.screenY` — window position on screen
- `window.screenLeft` / `window.screenTop` — alternative position properties

### Media Query Fingerprinting
- `prefers-color-scheme` — dark/light mode preference
- `prefers-reduced-motion` — accessibility setting
- `prefers-contrast` — high contrast mode
- `forced-colors` — forced colors mode
- `prefers-reduced-data` — data saving preference
- `prefers-reduced-transparency` — transparency reduction
- Color gamut detection (sRGB, P3, Rec.2020) via `matchMedia`
- HDR support via `dynamic-range` media query
- Screen dimensions via step-wise media query matching

## 4. Canvas Fingerprinting (Highest Entropy)

Canvas fingerprinting renders hidden text and geometric shapes to a `<canvas>` element, then hashes the pixel buffer. Hardware, OS, GPU driver, and browser differences produce distinct hashes.

### Rendering Variables
- **Font rasterization** — varies by OS font renderer (ClearType/DirectWrite on Windows, CoreText on macOS, FreeType on Linux), sub-pixel anti-aliasing method, hinting level
- **Text rendering** — specific text in specific font with specific size and color; emoji rendering differs across OS/browser versions
- **Shape rendering** — geometric primitives with gradients, shadows, transforms, and alpha compositing
- **Anti-aliasing** — GPU and driver determine exact pixel values at edges
- **Color space** — sRGB, P3, or other color spaces affect final pixel values

### Entropy & Uniqueness
- ~15-20 bits of entropy per canvas
- Combined text + emoji + shape canvases provide multiplicative uniqueness
- "Canvassing the Fingerprinters" (Luo et al., IMC 2025) — 12.7% of top 20k sites use canvas fingerprinting; ~500 distinct test canvases dominate; fingerprinting grew from 5.5% (2014) to 12.7% (2025)

### Consistency Detection
- Anti-fingerprinting tools that add noise per-read are detectable by rendering the same canvas twice and checking for pixel-level differences
- Fp-Scanner (Vastel et al., USENIX 2018) demonstrated this against FPRandom

## 5. WebGL Fingerprinting

### Unmasked Hardware Information
- `WEBGL_debug_renderer_info.UNMASKED_VENDOR_WEBGL` — GPU vendor (NVIDIA Corporation, Intel, AMD, Apple, etc.)
- `WEBGL_debug_renderer_info.UNMASKED_RENDERER_WEBGL` — specific GPU model with PCI/SSE tags
- `GL_VERSION`, `GL_SHADING_LANGUAGE_VERSION` — driver version strings
- `GL_VENDOR`, `GL_RENDERER` — masked versions (often "WebKit" or "Mozilla")

### WebGL Parameters (~20 values)
- `MAX_TEXTURE_SIZE`, `MAX_VIEWPORT_DIMS`, `MAX_VERTEX_ATTRIBS`
- `MAX_VERTEX_TEXTURE_IMAGE_UNITS`, `MAX_RENDERBUFFER_SIZE`
- `MAX_CUBE_MAP_TEXTURE_SIZE`, `MAX_COMBINED_TEXTURE_IMAGE_UNITS`
- `ALIASED_LINE_WIDTH_RANGE`, `ALIASED_POINT_SIZE_RANGE`

### Extensions
- Set of supported WebGL extensions (~30-50 extensions)
- Order and exact set vary by GPU/driver

### Shader-Based Fingerprinting
- Compiling a vertex + fragment shader, rendering a 3D scene, and hashing the output
- Shader math precision differs by GPU microarchitecture
- Floating-point precision in GLSL varies by GPU and driver
- UNIGL (Wu, JHU 2023) demonstrated that floating-point operations cause rendering discrepancies; proposed rewriting GLSL to uniformize rendering

### Hash-Based Fingerprinting
- Render a 3D scene → hash output pixels
- Depends on GPU microarchitecture, driver version, and OS

## 6. Audio Fingerprinting

### Signal Path
Create `OscillatorNode` → connect to `DynamicsCompressorNode` → connect to `AnalyserNode` → read `getByteFrequencyData()` or `getFloatTimeDomainData()`. The compressor's gain reduction curve and the analyser's output are then hashed.

### Variables Affected
- CPU floating-point precision (IEEE 754 differences across architectures)
- Audio stack implementation (OS-level audio mixing)
- Browser DSP implementation (Web Audio API internals)
- Sample rate (`audioContext.sampleRate` — typically 44100 or 48000)
- Max channel count
- Dynamics compressor gain reduction values
- FFT analysis results from analyser node

### Entropy
- ~12-18 bits
- Among the hardest vectors to spoof consistently — the compressor curve must be mathematically coherent

## 7. Font Fingerprinting

### JavaScript Enumeration
- Measure width of a test string in a known-missing font, then measure in candidate fonts — matching width indicates font is installed
- A typical system has 50-500+ fonts installed
- Fonts reveal: OS version, design tools (Adobe, Sketch), office suites, language packs, GPU driver fonts

### Canvas-Based Detection
- Render text with specific font → if missing, fallback glyph changes pixel hash
- CSS `@font-face` fallback behavior is also fingerprintable

### Font Metrics Fingerprinting
- Even without enumerating fonts, spacing, glyph bounding boxes, and kerning pairs are fingerprintable
- `CanvasRenderingContext2D.measureText()` — precise glyph metrics
- Font shaping engine behavior varies by OS (HarfBuzz on Linux, DirectWrite on Windows, CoreText on macOS)

### Entropy
- ~13-15 bits from font list alone
- Combined font list + metrics can uniquely identify systems

## 8. WebRTC Fingerprinting

### IP Leakage
- `RTCPeerConnection.createDataChannel()` + ICE candidate gathering exposes:
  - Local/private IP addresses (192.168.x.x, 10.x.x.x, etc.)
  - Public IP address via STUN/TURN (can bypass VPNs)
- `getStats()` API returns detailed connection statistics
- SDP (Session Description Protocol) offers contain IP addresses

### Media Device Enumeration
- `navigator.mediaDevices.enumerateDevices()` — list of cameras, microphones, speakers
- Device labels, group IDs, and capabilities
- Requires permission in some browsers but fingerprintable from device count alone

## 9. CSS & Layout Fingerprinting

### Computed Style Quirks
- OS/browser-specific default values for system colors
- Scrollbar width and style (macOS overlay vs Windows permanent)
- Form element rendering differences (select, input, button)
- CSS `@supports` queries for property support detection

### Layout-Based
- `getBoundingClientRect()` — fractional pixel values vary by layout engine
- `getComputedStyle()` — default styles differ across browsers
- `element.offsetWidth` / `element.offsetHeight` — measurement differences

### StylisticFP
- Novel CSS-only fingerprinting that infers system characteristics (including font lists) **without JavaScript**
- Uses `@font-face` with `local()` and `@supports` rules to probe system state
- Comparable in accuracy to JS-based fingerprinting libraries

## 10. Timezone & Locale

- `Date().getTimezoneOffset()` — UTC offset in minutes (~5 bits)
- `Intl.DateTimeFormat().resolvedOptions().timeZone` — IANA timezone name
- `Intl.DateTimeFormat().resolvedOptions().locale` — locale
- `Intl.DateTimeFormat()` formatting — AM/PM, 24h, date ordering (DD/MM vs MM/DD)
- `Intl.NumberFormat()` — decimal/thousands separator, currency formatting
- `Intl.Collator()` — string comparison behavior
- `Intl.PluralRules()` — pluralization rules
- Daylight saving behavior — can be inferred from historical offset data

## 11. Hardware & System

- `navigator.hardwareConcurrency` — CPU logical cores (high entropy, ~4-5 bits)
- `navigator.deviceMemory` — RAM in GB
- `navigator.getBattery()` — level, charging, chargingTime, dischargingTime (deprecated but still exposed in Chromium)
- `navigator.maxTouchPoints` — touch support (0 on desktop, varies on mobile)
- `ontouchstart` in window — touch event support detection
- `navigator.connection` — network type (4g, 3g), downlink, RTT
- Math computation quirks — `Math.tan()`, `Math.sin()`, floating-point precision across CPU architectures
- `Error().stack` — stack trace format differs by browser
- `RegExp.prototype.toString` — format differences
- `performance.now()` — precision and timing quirks

## 12. Behavioral & Timing Signals

### Mouse Movement
- Trajectory curvature, speed, acceleration/deceleration
- Micro-pauses, idle jitter, overshoot on targets
- Bot-like movements are linear or perfectly smooth; humans have natural tremor

### Keyboard
- Key press duration (hold time)
- Inter-key delay (time between keystrokes)
- Typing patterns for common words

### Scroll
- Scroll velocity and smoothness
- Pause points, bounce-back at page boundaries

### Request Timing
- Precise timing of network requests
- Page load phases, resource fetch ordering
- DNS/connect/SSL timing zeroing is detectable as anomaly

## 13. Storage & Database Availability

- `localStorage` / `sessionStorage` availability
- `indexedDB` availability
- `openDatabase` (WebSQL) availability
- `addBehavior` (legacy IE)
- `SharedWorker`, `ServiceWorker` support
- Cache API availability

## 14. Additional Vectors

### Permissions API
- `navigator.permissions.query()` — which permissions are granted/denied/ask
- Camera, microphone, geolocation, notifications, midi, etc.

### Device Orientation & Motion
- `DeviceOrientationEvent`, `DeviceMotionEvent`
- Accelerometer, gyroscope calibration differences

### Emoji Rendering
- Emoji glyphs differ across OS/browser versions
- Can be fingerprinted via canvas rendering

### Performance API
- `performance.memory` — JS heap size limit, total heap size, used heap size (Chromium only)
- `performance.getEntriesByType()` — resource timing entries

### Console API Detection
- `console.log` output format differences detectable via `console.error` stack inspection
- Custom `console` bindings placed by automation tools are detectable via Proxy inspection

## 15. WebGPU Fingerprinting (Emerging)

WebGPU is the successor to WebGL, shipping in Chromium 113+ and Firefox 121+. It exposes more detailed hardware information than WebGL and is increasingly used for fingerprinting.

### Adapter Information
- `navigator.gpu.requestAdapter()` → returns GPU adapter with properties:
  - `adapter.name` — GPU device name (e.g., "NVIDIA GeForce RTX 3080")
  - `adapter.architecture` — architecture type ("gen-12lp" for Intel, "ampere" for NVIDIA)
  - `adapter.vendor` — vendor ID string
  - `adapter.device` — device ID string
  - `adapter.features` — set of supported GPU features (timestamp-query, pipeline-statistics-query, etc.)

### Adapter Limits
- `adapter.limits` — ~30+ limit values (maxTextureDimension2D, maxStorageBufferBindingSize, maxComputeWorkgroupSizeX, etc.)
- These values differ by GPU generation and driver version
- More granular than WebGL's limited MAX_* constants

### Shader Compilation
- WebGPU uses a different shading language (WGSL vs GLSL)
- WGSL compiler behavior varies by GPU vendor
- Floating-point precision differences in compute shaders create unique output hashes

### Entropy & Current Status
- Still emerging; fewer fingerprinters use it than WebGL or Canvas
- ~8-12 bits of potential entropy from adapter + limits alone
- Some anti-bot systems are beginning to query WebGPU alongside WebGL
- **Spoofing difficulty**: High (requires C++-level interception of the WebGPU implementation)

---

## Entropy Summary

| Vector Category | Typical Entropy (bits) | Spoofing Difficulty |
|---|---|---|
| User-Agent | 8-10 | Low |
| Screen resolution | 4-6 | Low |
| Timezone | 5 | Low |
| Font list | 13-15 | Medium |
| Canvas hash | 15-20 | High |
| WebGL renderer | 10-15 | High |
| WebGPU adapter | 8-12 | High (emerging) |
| Audio fingerprint | 12-18 | Very High |
| TLS fingerprint (JA3/JA4) | 8-12 | Extreme |
| Hardware concurrency | 4-5 | Low |
| Combined (30+ signals) | 30-40+ | — |

> Combined entropy of 30+ signals is typically 30-40+ bits, making >80% of browsers uniquely identifiable (EFF Panopticlick; confirmed by 2024 arXiv analyses).
