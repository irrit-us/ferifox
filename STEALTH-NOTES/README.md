# STEALTH-NOTES: Browser Anonymity Research

Comprehensive research on browser fingerprinting, anti-detection techniques, and open-source stealth implementations. This document set synthesizes findings from Patchright, Rebrowser Patches, Camoufox, CloakBrowser, and academic literature.

## Documents

| # | Document | Contents |
|---|----------|----------|
| 01 | [Browser Fingerprinting Taxonomy](01-fingerprinting-taxonomy.md) | Complete catalog of 14 categories of fingerprinting vectors with entropy estimates and spoofing difficulty |
| 02 | [Academic Research Survey](02-academic-research.md) | Key papers, defense approaches (FP-Inspector, FPRandom, FP-Block, PriVaricator, UNIGL), limitations, and the arms race dynamic |
| 03 | [Implementations Deep-Dive](03-implementations-deep-dive.md) | Technical architecture, patch categories, and cross-comparison of Patchright, Rebrowser Patches, Camoufox, and CloakBrowser |
| 04 | [Synthesis & Recommendations](04-synthesis-and-recommendations.md) | Five-layer defense model, consistency imperative, strategy taxonomy, recommended stacks by use case |

## Quick Summary

Browser anonymity requires addressing detection at **five layers**:

1. **Protocol Detection** — CDP leaks (`Runtime.enable`), automation flags (`navigator.webdriver`)
2. **Browser Fingerprinting** — Canvas, WebGL, audio, fonts, screen, navigator properties
3. **Behavioral Analysis** — Mouse trajectories, typing rhythm, scroll patterns
4. **Network Analysis** — TLS fingerprint (JA3/JA4), WebRTC leaks, IP reputation
5. **Layout & Rendering Probes** — `getBoundingClientRect`, computed styles, CSS quirks

### Key Findings

- **C++-level patching (Camoufox, CloakBrowser)** is the current state of the art — spoofing occurs below the JS runtime, making detection via prototype inspection impossible
- **The Consistency Principle** is the fundamental constraint: every spoofed value must be internally consistent with every other spoofed value (GPU, OS, fonts, timezone, locale, screen)
- **Firefox Puppeteer and Playwright automation must be treated as the same startup risk class** for Ferifox: both can enter through Firefox remote automation surfaces that apply WebDriver-oriented recommended prefs before page code runs
- **Passive request context also matters** — HTTP Priority and top-window URI state can reveal automation/browser context even when JS-visible APIs are patched
- **TLS fingerprint spoofing** remains the hardest unsolved problem — no fully general open-source solution exists
- **IP reputation trumps everything** — the best fingerprint spoofing fails with datacenter IPs; residential proxies are essential
- **The arms race is permanent** — detection adapts; what passes CreepJS today may fail tomorrow

### Recommended Tools by Use Case

| Use Case | Primary Tool | Rationale |
|----------|------------|-----------|
| Maximum anonymity | CloakBrowser Pro v0.4.8 (66 patches) + residential proxies | Chromium C++ patches, TLS match, SOCKS5, humanize, 0.9 reCAPTCHA |
| High-volume scraping | Camoufox v150.0.2 + residential proxies | 312 fingerprints (BrowserForge), auto-geolocation, per-context isolation |
| Quick automation | Patchright v1.61.2 + real Chrome | Drop-in Playwright replacement, CDP evasion, isolated_context API |
| Testing/CI | SeleniumBase UC Mode | Built-in CAPTCHA solving, pytest integration |

### Detection Testing Resources

- [CreepJS](https://abrahamjuliot.github.io/creepjs/) — most comprehensive open-source fingerprint leak detector
- [BrowserScan](https://www.browserscan.net/) — commercial-grade detection test
- [Pixelscan](https://pixelscan.net/) — bot detection and fingerprint analysis
- [AmIUnique](https://amiunique.org/) — academic fingerprinting research tool
- [Rebrowser Bot Detector](https://bot-detector.rebrowser.net/) — CDP leak detection test (Runtime.enable, sourceURL, etc.)

---

*Research compiled July 2026. Detection is an arms race — validate any specific claim against current tests.*
