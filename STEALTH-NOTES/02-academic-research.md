# Academic Research Survey: Browser Fingerprinting & Anti-Detection

## Key Papers

### 1. Fingerprinting and Tracing Shadows (Lawall, 2024)
- **arXiv:** 2411.12045, SECURWARE 2024
- **Scope:** Comprehensive survey of browser fingerprinting techniques
- **Covers:** Canvas (III-C), WebGL (III-D), Audio (III-E), Font (III-F), Screen, WebRTC, CSS, ML-based
- **Key findings:** Shadow fingerprinting can identify users with 94% accuracy even when anti-tracking is active; fonts + canvas + WebGL together provide the highest discriminatory power; users have no control over data collection

### 2. Canvassing the Fingerprinters (Luo, Ritter, Savage, Voelker, 2025)
- **Venue:** ACM IMC '25
- **Scope:** Large-scale measurement of canvas fingerprinting on the web
- **Key findings:**
  - 12.7% of Top 20k sites engage in canvas fingerprinting (up from 5.5% in 2014)
  - ~500 distinct test canvases dominate the landscape
  - Groups canvases by similarity to identify specific fingerprinting services
  - Analyzes evasion techniques used to circumvent anti-fingerprinting defenses

### 3. Double-edged Sword (Wu, JHU Ph.D., 2023)
- **Advisor:** Yinzhi Cao
- **Scope:** Browser fingerprints for web tracking AND bot defenses
- **Key contributions:**
  - **UNIGL system** — rewrites GLSL programs to uniformize WebGL rendering, first to identify floating-point operations as the root cause of cross-browser rendering discrepancies
  - **ML-based bot detection** — measurement study across 14 major commercial websites; adversarial fingerprints differ from benign in entropy, unique rate, and evolution speed
  - Characterized anti-detection tools and strategies used by adversaries

### 4. A Survey of Browser Fingerprint Research and Application (2022)
- **Venue:** Wireless Communications and Mobile Computing
- **Scope:** Categorization of fingerprinting methods, data collection, and countermeasures
- **Contribution:** Taxonomy of fingerprinting vectors with entropy quantification

### 5. Characterizing Browser Fingerprinting and its Mitigations (Ukani, 2023)
- **arXiv:** 2311.12197
- **Scope:** Survey of fingerprinting vector coverage across mitigation tools
- **Key finding:** Most tools cover only a subset of vectors; consistency across vectors is the primary weakness of existing mitigations

---

## Defense Approaches in Academia

### FP-Inspector (Iqbal et al., USENIX Security 2018)
- **Type:** ML-based detection (not prevention)
- **Method:** Static + dynamic JS analysis; transforms scripts to ASTs; extracts API features; decision-tree classifier via OpenWPM
- **Result:** Detects 26% more fingerprinting scripts than prior state-of-the-art
- **Relevance:** Used as a benchmark for evaluating anti-fingerprinting tools; demonstrates which API calls are most fingerprintable

### FPRandom (Laperdrix et al., 2016–2017)
- **Type:** Randomization-based defense (Firefox modification)
- **Method:** Adds random values to canvas, AudioContext, and JS property enumeration outputs; different per browsing session
- **Vulnerability:** Fp-Scanner (Vastel et al., USENIX 2018) detects it via pixel-level inconsistency — reading the same canvas twice produces different hashes

### FP-Block (Torres, Jonker, Mauw, ESORICS 2015)
- **Type:** Per-domain identity separation (Firefox extension)
- **Method:** Creates distinct, consistent "web identities" per domain using Markov chain-generated attribute combinations (UA, screen, language, OS/CPU, timezone); breaks cross-site linkability while preserving first-party functionality
- **Coverage:** 22 attributes across 5 classes
- **Vulnerability:** Markov-chain-generated combinations sometimes mismatch real-world configurations

### PriVaricator (Nikiforakis et al., ACM CCS 2014)
- **Type:** Randomized policy-based defense
- **Method:** Parameterized randomization of high-entropy attributes (offsetWidth, offsetHeight, getBoundingClientRect, plugin enumeration); ±5% noise; detection-then-randomization
- **Result:** Renders tested commercial fingerprinters ineffective; minimal breakage on Alexa top 1000
- **Vulnerability:** Detected by Fp-Scanner; leaks original OS/browser family

### Canvas Deceiver (Obidat et al.)
- **Type:** Canvas-specific defense
- **Method:** New canvas rendering approach designed to prevent pixel-level fingerprinting

### The WASM Cloak (2025)
- **arXiv:** 2508.21219
- **Scope:** Evaluates fingerprinting defenses under WebAssembly-based obfuscation
- **Key insight:** WASM can be used to hide fingerprinting code from static analysis

---

## Cross-Cutting Academic Insights

### The Consistency Principle
Every academic defense acknowledges the same fundamental truth: **internal consistency across all fingerprint vectors is the critical requirement**. A canvas that says "NVIDIA GPU" but a WebGL renderer string that says "Intel HD Graphics" is instantly detected as spoofed. FP-Block's Markov chain approach was the first systematic attempt at consistency; modern tools like Camoufox's BrowserForge integration represent the industrial evolution of this idea.

### The Arms Race Dynamic
- **2014–2016:** Academic defenses (PriVaricator, FPRandom, FP-Block) demonstrated feasibility
- **2017–2019:** Detection tools (Fp-Scanner, FP-Inspector) emerged to defeat them
- **2020–2023:** ML-based detection raised the bar; consistency checking became automated
- **2024–2025:** C++-level spoofing (Camoufox, CloakBrowser) represents the state of the art — values are spoofed before JS can inspect them, defeating JS-level consistency checks
- **2025–2026:** Ecosystem consolidation — 40+ projects across 5 categories; CloakBrowser reaches 66 C++ patches (27.8k stars), Camoufox at Firefox 150 (9.9k stars); WASM-based obfuscation emerges as a new defense bypass technique; WebGPU becomes a new fingerprinting vector

### Key Unresolved Problems
1. **TLS fingerprint spoofing** — no fully general solution exists; the TLS stack is deep in the OS/browser networking layer
2. **Audio fingerprint consistency** — the dynamics compressor curve must be mathematically coherent with the claimed hardware
3. **WebGL floating-point precision** — UNIGL demonstrated it's possible but requires rewriting GLSL programs
4. **Layout engine divergence** — Firefox vs Chromium layout produces different `getBoundingClientRect` values even with identical content

---

## Selected References

```
@article{lawall2024fingerprinting,
  title={Fingerprinting and Tracing Shadows: The Development and Impact
         of Browser Fingerprinting on Digital Privacy},
  author={Lawall, Alexander},
  journal={arXiv preprint arXiv:2411.12045},
  year={2024}
}

@inproceedings{luo2025canvassing,
  title={Canvassing the Fingerprinters: Characterizing Canvas Fingerprinting
         Use Across the Web},
  author={Luo, Elisa and Ritter, Tom and Savage, Stefan and Voelker, Geoffrey M.},
  booktitle={ACM IMC},
  year={2025}
}

@phdthesis{wu2023double,
  title={Double-edged Sword: An In-depth Analysis of Browser Fingerprints
         for Web Tracking and Bot Defenses},
  author={Wu, Shujiang},
  school={Johns Hopkins University},
  year={2023}
}

@inproceedings{iqbal2018fpinspector,
  title={Fingerprinting the Fingerprinters: Learning to Detect Browser
         Fingerprinting Behaviors},
  author={Iqbal et al.},
  booktitle={USENIX Security},
  year={2018}
}

@inproceedings{torres2015fpblock,
  title={FP-Block: Usable Web Privacy by Controlling Browser Fingerprinting},
  author={Torres, Christof Ferreira and Jonker, Hugo and Mauw, Sjouke},
  booktitle={ESORICS},
  year={2015}
}

@inproceedings{nikiforakis2014privaricator,
  title={PriVaricator: Deceiving Fingerprinters with Little White Lies},
  author={Nikiforakis, Nick and Joosen, Wouter and Livshits, Benjamin},
  booktitle={ACM CCS/WPES},
  year={2014}
}

@inproceedings{laperdrix2016fprandom,
  title={FPRandom: Randomizing Core Browser Objects to Break Advanced
         Device Fingerprinting Techniques},
  author={Laperdrix, Pierre et al.},
  booktitle={RAID},
  year={2016}
}

@inproceedings{vastel2018fpscanner,
  title={Fp-Scanner: The Privacy Implications of Browser Fingerprint
         Inconsistencies},
  author={Vastel, Antoine and Laperdrix, Pierre and Rudametkin, Walter
          and Rouvoy, Romain},
  booktitle={USENIX Security},
  year={2018}
}

@article{ukani2023characterizing,
  title={Characterizing Browser Fingerprinting and its Mitigations},
  author={Ukani, Arjun et al.},
  journal={arXiv preprint arXiv:2311.12197},
  year={2023}
}

@article{wasmcloak2025,
  title={The WASM Cloak: Evaluating Fingerprinting Defenses
         Under WebAssembly-based Obfuscation},
  author={Anonymous et al.},
  journal={arXiv preprint arXiv:2508.21219},
  year={2025}
}
```
