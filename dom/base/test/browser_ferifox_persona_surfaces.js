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
const PERSONA_CONFIG_CONTENT = JSON.stringify({
  layout: { noiseSeed: 1311768467463790320 },
  audio: { noiseSeed: 305419896 },
  storage: {
    estimate: {
      usage: EXPECTED_STORAGE_USAGE,
      quota: EXPECTED_STORAGE_QUOTA,
    },
  },
  webrtc: {
    stripSDPIPs: true,
    stripStats: true,
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
        computedWidth1: parseFloat(content.getComputedStyle(target).width),
        computedWidth2: parseFloat(content.getComputedStyle(target).width),
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
      result.computedWidth1,
      137,
      0.1,
      "Computed style width is noise-adjusted"
    );
    is(
      result.computedWidth1,
      result.computedWidth2,
      "Computed style width is stable across reads"
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

add_task(async function test_ferifox_webrtc_strips_page_visible_ips() {
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

    const ipv4Matches = result.localDescription.match(
      /\b\d{1,3}(?:\.\d{1,3}){3}\b/g
    );
    const statsIpv4 = result.candidateAddresses.filter(address =>
      /^\d{1,3}(?:\.\d{1,3}){3}$/.test(address)
    );

    ok(result.firstCandidate, "Received a local ICE candidate");
    ok(
      result.firstCandidate.includes("0.0.0.0"),
      `ICE candidate is stripped: ${result.firstCandidate}`
    );
    ok(
      ipv4Matches && ipv4Matches.length > 0,
      "Local description contains IPv4 text"
    );
    ok(
      ipv4Matches.every(match => match === "0.0.0.0"),
      `Local description only exposes stripped IPv4 values: ${result.localDescription}`
    );
    ok(
      result.candidateAddresses.length > 0,
      "Page-visible getStats() includes local candidate addresses"
    );
    ok(
      statsIpv4.length > 0,
      "Page-visible getStats() includes IPv4 candidate addresses to check"
    );
    ok(
      statsIpv4.every(address => address === "0.0.0.0"),
      `Page-visible getStats() IPv4 addresses are stripped: ${result.candidateAddresses.join(", ")}`
    );
  });
});
