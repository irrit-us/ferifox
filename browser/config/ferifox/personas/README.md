# Ferifox Personas

A Persona is a process-wide Firefox identity loaded from an external JSON file.
It is intended to keep headed, headless, worker, and automation-driven paths on
the same native browser state. It is not a per-tab or per-container override.

## Loading a Persona

Set `FERIFOX_CONFIG` to an absolute file path before Firefox starts. Restart the
browser after changing the file.

```powershell
$env:FERIFOX_CONFIG = (Resolve-Path .\browser\config\ferifox\personas\win11-firefox.json)
.\mach run
```

```sh
FERIFOX_CONFIG="$PWD/browser/config/ferifox/personas/linux-firefox.json" ./firefox
```

The parent process reads the file after profile and AutoConfig preferences are
loaded, applies Persona-controlled preferences as locked defaults, and passes a
minified copy to child processes. `FERIFOX_CONFIG_JSON` is internal transport
state and must not be set by launchers. The source file must contain a JSON
object and be no larger than 1 MiB; its minified form must not exceed 8 KiB.

## Supported Fields

The checked-in Personas show the recommended baseline. Supported fields are:

| Section | Fields | Behavior |
| --- | --- | --- |
| `navigator` | `webdriver`, `platform`, `oscpu`, `userAgent`, `hardwareConcurrency`, `maxTouchPoints`, `vendor`, `vendorSub`, `product`, `productSub`, `buildID`, `appVersion`, `languages`, `appCodeName`, `appName` | Applies to the corresponding window APIs; applicable identity fields also apply to workers and HTTP user-agent generation. |
| `screen` | `width`, `height`, `availWidth`, `availHeight`, `colorDepth`, `pixelDepth`, `devicePixelRatio`, `orientation.type`, `orientation.angle` | Drives DOM screen values, CSS device media features, effective layout scale, and headless screen geometry. |
| `window` | `screenX`, `screenY`, `mozInnerScreenX`, `mozInnerScreenY` | Sets screen origins and trusted input-event screen coordinates. Viewport and outer-window sizes remain native. |
| `intl` | `locale`, `timezone` | Sets Gecko regional preferences and JavaScript realm defaults. Invalid locale or IANA timezone values are ignored. |
| `geolocation` | `latitude`, `longitude`, `accuracy`, `altitude`, `altitudeAccuracy` | Supplies a position after normal site permission approval without using the host provider. `altitudeAccuracy` requires `altitude`. |
| `headers` | `acceptLanguage` | Sets Firefox's normal `Accept-Language` value. |
| `fonts` | `visible` | Sets the native system-font whitelist. Only installed listed families are normally visible; Firefox ignores a whitelist with no visible match. |
| `webgl` | `unmaskedVendor`, `unmaskedRenderer`, `extensions`, `maxTex2dSize`, `maxTexCubeSize`, `maxViewportDim`, `maxVertexAttribs`, `maxTexUnits`, `maxVertexTextureImageUnits`, `maxFragmentTextureImageUnits`, `pointSizeRangeMin`, `pointSizeRangeMax`, `lineWidthRangeMin`, `lineWidthRangeMax`, `forceEnabled`, `forceEGL` | Filters or caps real WebGL capabilities and controls startup backend preferences. It does not change the renderer's output. |
| `audio` | `sampleRate`, `outputLatency`, `maxChannelCount`, `noiseSeed` | Controls supported AudioContext metadata and optional analyser noise. Silence remains unchanged. |
| `canvas` | `noiseSeed` | Enables deterministic Canvas 2D readback and canvas encoding protection for opaque pixels. |
| `mediaDevices` | `audioInputCount`, `audioOutputCount`, `videoInputCount` | Caps real enumerated devices; it never creates devices. |
| `speech` | `voices`, `voiceCount` | Filters and caps voices installed on the host. |
| `webrtc` | `noHostCandidates`, `defaultAddressOnly` | Locks Firefox's corresponding ICE privacy preferences. |
| `automation` | `stealth` | Locks the regular-browser preference baseline before automation services start. |

Unknown fields have no effect. Values are consumed only when their expected JSON
type and local validity checks pass.

## Coherence Rules

A Persona must describe the runtime that actually executes it:

- Use a Persona only on its matching compiled operating system and graphics
  backend. Identity strings cannot emulate platform rendering.
- Keep the Firefox version in `userAgent` aligned with the build. Use Firefox's
  public compatibility build ID, `20181001000000`, for web content.
- Keep locale, `navigator.languages`, `Accept-Language`, timezone,
  geolocation, and proxy egress in one plausible region.
- Install every expected font and verify the exposed intersection. At least one
  non-hidden listed family must be installed or Firefox ignores the whitelist.
  Matching names do not make font metrics portable across platforms.
- Size the real window and automation viewport to the Persona. Do not add
  getter-only `innerWidth`, `innerHeight`, `outerWidth`, or `outerHeight`
  values.
- Pin the outer window origin when declaring `window` values. Keep the
  difference between `mozInnerScreenX/Y` and `screenX/Y` equal to the real or
  emulated browser chrome offset, and verify trusted mouse and touch event
  coordinates against that origin.
- Match touch, media-device, audio, GPU, and accessibility claims to available
  host behavior. Capability caps can hide entries but cannot fabricate them.
- Share canvas and audio noise seeds across a sufficiently large cohort. A
  unique persistent seed is a stable identifier.
- Keep one Persona for the complete browser process lifetime. Do not rotate
  fields independently within a session.

## Intentionally Native State

Cookie availability, PDF support, online and Network Information state, DNT,
GPC, permission decisions, storage quota and persistence, compressor reduction,
WebMIDI identity, WebRTC statistics, and layout geometry remain native. A static
getter override for these surfaces would disagree with browser actions, events,
headers, resource enforcement, or protocol results.

Personas do not normalize TLS, HTTP/2 or HTTP/3 behavior, IP reputation, actual
WebGL/WebGPU rendering or direct GPU buffer readback, media codecs and DRM,
screenshots, text rasterization, timing, or interaction behavior. Validate those
properties on the target host and network rather than treating the JSON file as
an anonymity guarantee.

## Review Checklist

Before adding or changing a checked-in Persona:

1. Parse every JSON file and keep the minified Persona below 8 KiB.
2. Compare window and worker navigator, locale, and timezone values.
3. Compare screen getters with CSS device-size, color, and resolution queries.
4. Confirm the actual headless and headed viewport, screenshots, and input
   coordinates match the declared screen story.
5. Verify geolocation only after browser permission and confirm no host provider
   is required.
6. Check HTTP user agent and language headers against DOM identity.
7. Record the host OS, graphics backend, installed fonts, network egress, and
   automation path used for validation.
