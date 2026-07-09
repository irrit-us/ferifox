/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy.html";
const EXPECTED_STORAGE_QUOTA = 222222;
const EXPECTED_STORAGE_USAGE = 111111;
const EXPECTED_STORAGE_PERSISTED = true;
const EXPECTED_NAVIGATOR_APP_VERSION = "5.0 (X11; Linux x86_64) Ferifox/128.0";
const EXPECTED_NAVIGATOR_PLATFORM = "Linux x86_64";
const EXPECTED_NAVIGATOR_LANGUAGES = ["en-US", "en"];
const EXPECTED_NAVIGATOR_HARDWARE_CONCURRENCY = 8;
const EXPECTED_WEBGL_MAX_TEX_UNITS = 8;
const WEBRTC_PLACEHOLDER_ADDRESSES = new Set(["0.0.0.0", "::"]);
const PERSONA_CONFIG_CONTENT = JSON.stringify({
  layout: { noiseSeed: 1311768467463790320 },
  audio: { noiseSeed: 305419896 },
  navigator: {
    appVersion: EXPECTED_NAVIGATOR_APP_VERSION,
    platform: EXPECTED_NAVIGATOR_PLATFORM,
    languages: EXPECTED_NAVIGATOR_LANGUAGES,
    hardwareConcurrency: EXPECTED_NAVIGATOR_HARDWARE_CONCURRENCY,
  },
  storage: {
    estimate: {
      usage: EXPECTED_STORAGE_USAGE,
      quota: EXPECTED_STORAGE_QUOTA,
    },
    persisted: EXPECTED_STORAGE_PERSISTED,
  },
  webrtc: {
    stripStats: true,
  },
  webgl: {
    maxTexUnits: EXPECTED_WEBGL_MAX_TEX_UNITS,
  },
});

function assertNear(actual, expected, message) {
  ok(
    Math.abs(actual - expected) < 0.001,
    `${message}: ${actual} ~= ${expected}`
  );
}

function changedBy(actual, base, limit, message) {
  const delta = Math.abs(actual - base);
  ok(delta > 0 && delta <= limit, `${message}: delta ${delta} within ${limit}`);
}

function getIceCandidateAddress(candidate) {
  if (!candidate) {
    return "";
  }
  const tokens = candidate.trim().split(/\s+/);
  return tokens.length > 4 ? tokens[4] : "";
}

function isWebRTCPlaceholderAddress(address) {
  return WEBRTC_PLACEHOLDER_ADDRESSES.has(address);
}

function isIPAddressLiteral(address) {
  return (
    isWebRTCPlaceholderAddress(address) ||
    /^\d{1,3}(?:\.\d{1,3}){3}$/.test(address) ||
    (address.includes(":") && /^[0-9a-fA-F:.]+$/.test(address))
  );
}

async function withFerifoxContentTask(task) {
  Services.ppmm.releaseCachedProcesses();
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: PAGE_URL,
    forceNewProcess: true,
  });

  try {
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [PERSONA_CONFIG_CONTENT],
      personaConfigContent => {
        SpecialPowers.setFerifoxConfigForTesting(personaConfigContent);
      }
    );

    const estimate = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      async () => {
        return content.navigator.storage.estimate();
      }
    );
    is(
      estimate.quota,
      EXPECTED_STORAGE_QUOTA,
      "Ferifox quota override is active"
    );
    is(
      estimate.usage,
      EXPECTED_STORAGE_USAGE,
      "Ferifox usage override is active"
    );

    const persisted = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      async () => {
        return content.navigator.storage.persisted();
      }
    );
    is(
      persisted,
      EXPECTED_STORAGE_PERSISTED,
      "Ferifox persisted override is active"
    );

    await task(tab.linkedBrowser);
  } finally {
    await SpecialPowers.spawn(tab.linkedBrowser, [], () => {
      SpecialPowers.setFerifoxConfigForTesting("");
    });
    await BrowserTestUtils.removeTab(tab);
  }
}

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processPrelaunch.enabled", false],
      ["media.peerconnection.ice.obfuscate_host_addresses", false],
      ["media.peerconnection.ice.loopback", true],
    ],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    Services.ppmm.releaseCachedProcesses();
  });
});

add_task(async function test_ferifox_worker_navigator_and_storage_overrides() {
  await withFerifoxContentTask(async browser => {
    const snapshot = await SpecialPowers.spawn(browser, [], async () => {
      const source = `
        self.onmessage = async () => {
          try {
            const estimate = await navigator.storage.estimate();
            const persisted = await navigator.storage.persisted();
            self.postMessage({
              appVersion: navigator.appVersion,
              platform: navigator.platform,
              languages: Array.from(navigator.languages),
              hardwareConcurrency: navigator.hardwareConcurrency,
              storage: {
                quota: estimate.quota,
                usage: estimate.usage,
                persisted,
              },
            });
          } catch (error) {
            self.postMessage({ error: String(error && error.message || error) });
          }
        };
      `;
      const url = content.URL.createObjectURL(
        new content.Blob([source], { type: "text/javascript" })
      );
      let worker;
      try {
        const workerSnapshot = await new content.Promise((resolve, reject) => {
          worker = new content.Worker(url);
          worker.onmessage = event => {
            if (event.data.error) {
              reject(new Error(event.data.error));
              return;
            }
            resolve(event.data);
          };
          worker.onerror = event => {
            event.preventDefault();
            reject(new Error(event.message));
          };
          worker.postMessage(null);
        });

        return {
          window: {
            appVersion: content.navigator.appVersion,
            platform: content.navigator.platform,
            languages: Array.from(content.navigator.languages),
            hardwareConcurrency: content.navigator.hardwareConcurrency,
          },
          worker: workerSnapshot,
        };
      } finally {
        if (worker) {
          worker.terminate();
        }
        content.URL.revokeObjectURL(url);
      }
    });

    for (const scope of ["window", "worker"]) {
      is(
        snapshot[scope].appVersion,
        EXPECTED_NAVIGATOR_APP_VERSION,
        `Ferifox ${scope} appVersion override is active`
      );
      is(
        snapshot[scope].platform,
        EXPECTED_NAVIGATOR_PLATFORM,
        `Ferifox ${scope} platform override is active`
      );
      Assert.deepEqual(
        snapshot[scope].languages,
        EXPECTED_NAVIGATOR_LANGUAGES,
        `Ferifox ${scope} languages override is active`
      );
      is(
        snapshot[scope].hardwareConcurrency,
        EXPECTED_NAVIGATOR_HARDWARE_CONCURRENCY,
        `Ferifox ${scope} hardwareConcurrency override is active`
      );
    }

    is(
      snapshot.worker.storage.quota,
      EXPECTED_STORAGE_QUOTA,
      "Ferifox worker quota override is active"
    );
    is(
      snapshot.worker.storage.usage,
      EXPECTED_STORAGE_USAGE,
      "Ferifox worker usage override is active"
    );
    is(
      snapshot.worker.storage.persisted,
      EXPECTED_STORAGE_PERSISTED,
      "Ferifox worker persisted override is active"
    );
  });
});

add_task(async function test_ferifox_layout_noise_consistency() {
  await withFerifoxContentTask(async browser => {
    const result = await SpecialPowers.spawn(browser, [], () => {
      content.document.documentElement.style.margin = "0";
      content.document.body.style.margin = "0";
      content.document.body.replaceChildren();

      const target = content.document.createElement("div");
      target.textContent = "target";
      Object.assign(target.style, {
        position: "absolute",
        left: "10px",
        top: "20px",
        width: "137px",
        height: "53px",
        margin: "0",
        padding: "0",
        border: "0",
      });

      content.document.body.appendChild(target);

      const toRect = rect => ({
        x: rect.x,
        y: rect.y,
        width: rect.width,
        height: rect.height,
      });

      return {
        rect1: toRect(target.getBoundingClientRect()),
        rect2: toRect(target.getBoundingClientRect()),
        clientRect: toRect(target.getClientRects()[0]),
        computedWidth: parseFloat(content.getComputedStyle(target).width),
      };
    });

    Assert.deepEqual(result.rect1, result.rect2, "Bounding rect is stable");
    changedBy(result.rect1.x, 10, 0.5, "Bounding rect x is noise-adjusted");
    changedBy(result.rect1.y, 20, 0.5, "Bounding rect y is noise-adjusted");
    changedBy(
      result.rect1.width,
      137,
      0.5,
      "Bounding rect width is noise-adjusted"
    );
    changedBy(
      result.rect1.height,
      53,
      0.5,
      "Bounding rect height is noise-adjusted"
    );

    assertNear(result.clientRect.x, result.rect1.x, "Client rect x matches");
    assertNear(result.clientRect.y, result.rect1.y, "Client rect y matches");
    assertNear(
      result.clientRect.width,
      result.rect1.width,
      "Client rect width matches"
    );
    assertNear(
      result.clientRect.height,
      result.rect1.height,
      "Client rect height matches"
    );
    changedBy(
      result.computedWidth,
      137,
      0.5,
      "Computed width is noise-adjusted"
    );
  });
});

add_task(async function test_ferifox_webgl_texture_unit_caps_are_consistent() {
  await withFerifoxContentTask(async browser => {
    const caps = await SpecialPowers.spawn(browser, [], () => {
      const canvas = content.document.createElement("canvas");
      const gl = canvas.getContext("webgl");
      if (!gl) {
        return null;
      }

      return {
        combined: gl.getParameter(gl.MAX_COMBINED_TEXTURE_IMAGE_UNITS),
        vertex: gl.getParameter(gl.MAX_VERTEX_TEXTURE_IMAGE_UNITS),
        fragment: gl.getParameter(gl.MAX_TEXTURE_IMAGE_UNITS),
      };
    });

    if (!caps) {
      info("WebGL context unavailable; skipping WebGL cap assertions");
      return;
    }

    is(
      caps.combined,
      EXPECTED_WEBGL_MAX_TEX_UNITS,
      "Ferifox combined texture unit cap is applied"
    );
    ok(
      caps.vertex <= caps.combined,
      `Vertex texture unit cap is not above combined cap: ${caps.vertex} <= ${caps.combined}`
    );
    ok(
      caps.fragment <= caps.combined,
      `Fragment texture unit cap is not above combined cap: ${caps.fragment} <= ${caps.combined}`
    );
  });
});

add_task(async function test_ferifox_audio_noise_is_clamped() {
  await withFerifoxContentTask(async browser => {
    const { min, max } = await SpecialPowers.spawn(browser, [], async () => {
      const win = content.wrappedJSObject;
      const context = new win.AudioContext();
      await context.resume();

      const source = new win.ConstantSourceNode(context);
      source.offset.value = 1;
      const analyser = new win.AnalyserNode(context);
      analyser.fftSize = 32;
      source.connect(analyser);
      analyser.connect(context.destination);
      source.start();

      await new win.Promise(resolve => content.setTimeout(resolve, 100));

      const data = new win.Float32Array(analyser.fftSize);
      analyser.getFloatTimeDomainData(data);

      let min = Infinity;
      let max = -Infinity;
      for (let i = 0; i < data.length; ++i) {
        min = Math.min(min, data[i]);
        max = Math.max(max, data[i]);
      }

      source.stop();
      source.disconnect();
      analyser.disconnect();
      await context.close();

      return {
        min,
        max,
      };
    });

    ok(min >= -1, `Analyser minimum stays in range: ${min}`);
    ok(max <= 1, `Analyser maximum stays in range: ${max}`);
  });
});

add_task(async function test_ferifox_webrtc_stats_strip_preserves_signaling() {
  await withFerifoxContentTask(async browser => {
    const result = await SpecialPowers.spawn(browser, [], async () => {
      const pc = new content.wrappedJSObject.RTCPeerConnection();
      pc.createDataChannel("ferifox");

      let firstCandidate = null;
      const gatheringComplete = new content.Promise(resolve => {
        pc.onicecandidate = event => {
          if (event.candidate && !firstCandidate) {
            firstCandidate = event.candidate.candidate;
          }
          if (!event.candidate) {
            resolve();
          }
        };
      });

      await pc.setLocalDescription();
      await gatheringComplete;

      const stats = await pc.getStats();
      const candidateAddresses = [];
      stats.forEach(stat => {
        if (stat.type === "local-candidate" && "address" in stat) {
          candidateAddresses.push(stat.address);
        }
      });

      const localDescription = pc.localDescription.sdp;
      pc.close();

      return { firstCandidate, localDescription, candidateAddresses };
    });

    const firstCandidateAddress = getIceCandidateAddress(result.firstCandidate);
    const localCandidateAddresses = result.localDescription
      .split(/\r?\n/)
      .filter(line => line.startsWith("a=candidate:"))
      .map(getIceCandidateAddress)
      .filter(Boolean);
    const literalStatsAddresses =
      result.candidateAddresses.filter(isIPAddressLiteral);

    ok(result.firstCandidate, "Received a local ICE candidate");
    ok(firstCandidateAddress, "Parsed the local ICE candidate address");
    ok(
      !isWebRTCPlaceholderAddress(firstCandidateAddress),
      `ICE candidate address is usable: ${result.firstCandidate}`
    );
    ok(
      localCandidateAddresses.length > 0,
      "Local description contains gathered ICE candidates"
    );
    ok(
      localCandidateAddresses.some(
        address => !isWebRTCPlaceholderAddress(address)
      ),
      "Local description keeps usable candidate addresses for signaling"
    );
    ok(
      result.candidateAddresses.length > 0,
      "Page-visible getStats() includes local candidate addresses"
    );
    ok(
      literalStatsAddresses.length > 0,
      "Page-visible getStats() includes IP literal candidate addresses to check"
    );
    ok(
      literalStatsAddresses.every(isWebRTCPlaceholderAddress),
      `Page-visible getStats() IP literal addresses are stripped: ${result.candidateAddresses.join(", ")}`
    );
  });
});
