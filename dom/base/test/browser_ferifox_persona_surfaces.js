/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "dummy.html";
const EXPECTED_NAVIGATOR_APP_VERSION = "5.0 (X11; Linux x86_64) Ferifox/128.0";
const EXPECTED_NAVIGATOR_BUILD_ID = "20181001000000";
const EXPECTED_NAVIGATOR_PLATFORM = "Linux x86_64";
const EXPECTED_NAVIGATOR_LANGUAGES = ["en-US", "en"];
const EXPECTED_NAVIGATOR_HARDWARE_CONCURRENCY = 8;
const EXPECTED_INTL_LOCALE = "en-US";
const EXPECTED_TIME_ZONE = "America/Chicago";
const EXPECTED_WEBGL_MAX_TEX_UNITS = 8;
const EXPECTED_SCREEN_WIDTH = 1440;
const EXPECTED_SCREEN_HEIGHT = 900;
const EXPECTED_GEOLOCATION = {
  latitude: 37.7749,
  longitude: -122.4194,
  accuracy: 40,
};
const PERSONA_CONFIG_CONTENT = JSON.stringify({
  audio: { noiseSeed: 305419896 },
  canvas: { noiseSeed: 305419896 },
  geolocation: EXPECTED_GEOLOCATION,
  intl: {
    locale: "en_US.UTF-8",
    timezone: EXPECTED_TIME_ZONE,
  },
  navigator: {
    appVersion: EXPECTED_NAVIGATOR_APP_VERSION,
    buildID: EXPECTED_NAVIGATOR_BUILD_ID,
    platform: EXPECTED_NAVIGATOR_PLATFORM,
    languages: EXPECTED_NAVIGATOR_LANGUAGES,
    hardwareConcurrency: EXPECTED_NAVIGATOR_HARDWARE_CONCURRENCY,
  },
  screen: {
    width: EXPECTED_SCREEN_WIDTH,
    height: EXPECTED_SCREEN_HEIGHT,
    availWidth: EXPECTED_SCREEN_WIDTH,
    availHeight: EXPECTED_SCREEN_HEIGHT - 40,
    colorDepth: 24,
    pixelDepth: 24,
  },
  webgl: {
    maxTexUnits: EXPECTED_WEBGL_MAX_TEX_UNITS,
  },
});

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
      ["geo.provider.network.url", "http://127.0.0.1:9/"],
      ["geo.timeout", 50],
    ],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    Services.ppmm.releaseCachedProcesses();
  });
});

add_task(async function test_ferifox_worker_navigator_overrides() {
  await withFerifoxContentTask(async browser => {
    const snapshot = await SpecialPowers.spawn(browser, [], async () => {
      const source = `
        self.onmessage = async () => {
          try {
            const intl = new Intl.DateTimeFormat().resolvedOptions();
            self.postMessage({
              appVersion: navigator.appVersion,
              platform: navigator.platform,
              languages: Array.from(navigator.languages),
              hardwareConcurrency: navigator.hardwareConcurrency,
              intl: { locale: intl.locale, timeZone: intl.timeZone },
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
            buildID: content.navigator.buildID,
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

    is(
      snapshot.window.buildID,
      EXPECTED_NAVIGATOR_BUILD_ID,
      "Ferifox window buildID matches Firefox's public value"
    );

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
      snapshot.worker.intl.locale,
      EXPECTED_INTL_LOCALE,
      "Ferifox worker uses the canonical Persona locale"
    );
    is(
      snapshot.worker.intl.timeZone,
      EXPECTED_TIME_ZONE,
      "Ferifox worker uses the validated Persona timezone"
    );
  });
});

add_task(async function test_ferifox_canvas_noise_is_consistent() {
  await withFerifoxContentTask(async browser => {
    const result = await SpecialPowers.spawn(browser, [], async () => {
      const win = content.wrappedJSObject;
      const canvas = content.document.createElement("canvas");
      canvas.width = 2;
      canvas.height = 2;
      const context = canvas.getContext("2d");
      context.fillStyle = "rgb(40, 80, 120)";
      context.fillRect(0, 0, 2, 1);

      const firstImageData = context.getImageData(0, 0, 2, 2);
      const first = Array.from(firstImageData.data);
      const second = Array.from(context.getImageData(0, 0, 2, 2).data);

      const png = canvas.toDataURL("image/png");
      const pngBytes = Array.from(
        new win.Uint8Array(await (await win.fetch(png)).arrayBuffer())
      );
      const blob = await new win.Promise(resolve =>
        canvas.toBlob(resolve, "image/png")
      );

      const bitmapCanvas = content.document.createElement("canvas");
      bitmapCanvas.width = 2;
      bitmapCanvas.height = 2;
      const bitmapContext = bitmapCanvas.getContext("bitmaprenderer");
      bitmapContext.transferFromImageBitmap(
        await win.createImageBitmap(canvas)
      );

      const offscreen = new win.OffscreenCanvas(2, 2);
      const offscreenContext = offscreen.getContext("2d");
      offscreenContext.fillStyle = "rgb(40, 80, 120)";
      offscreenContext.fillRect(0, 0, 2, 1);
      const offscreenBlob = await offscreen.convertToBlob({
        type: "image/png",
      });

      const webglReference = content.document.createElement("canvas");
      webglReference.width = 2;
      webglReference.height = 2;
      const webglReferenceContext = webglReference.getContext("2d");
      webglReferenceContext.fillStyle = "rgb(40, 80, 120)";
      webglReferenceContext.fillRect(0, 0, 2, 2);

      const webglCanvas = content.document.createElement("canvas");
      webglCanvas.width = 2;
      webglCanvas.height = 2;
      const webgl = webglCanvas.getContext("webgl", {
        preserveDrawingBuffer: true,
      });
      let webglPng = null;
      if (webgl) {
        webgl.clearColor(40 / 255, 80 / 255, 120 / 255, 1);
        webgl.clear(webgl.COLOR_BUFFER_BIT);
        webgl.finish();
        webglPng = webglCanvas.toDataURL("image/png");
      }

      context.putImageData(firstImageData, 0, 0);
      const afterPut = Array.from(context.getImageData(0, 0, 2, 2).data);

      return {
        first,
        second,
        afterPut,
        png,
        pngBytes,
        blobBytes: Array.from(new win.Uint8Array(await blob.arrayBuffer())),
        bitmapPng: bitmapCanvas.toDataURL("image/png"),
        offscreenBytes: Array.from(
          new win.Uint8Array(await offscreenBlob.arrayBuffer())
        ),
        webglPng,
        webglReferencePng: webglReference.toDataURL("image/png"),
      };
    });

    Assert.notStrictEqual(
      result.first[2],
      120,
      "Canvas noise changes the configured opaque pixel channel"
    );
    Assert.deepEqual(result.first, result.second, "Canvas reads are stable");
    Assert.deepEqual(
      result.first,
      result.afterPut,
      "Canvas noise is idempotent after putImageData"
    );
    Assert.deepEqual(
      result.pngBytes,
      result.blobBytes,
      "Canvas toDataURL and toBlob encode the same protected pixels"
    );
    is(
      result.bitmapPng,
      result.png,
      "Bitmap renderer encodes the same protected pixels"
    );
    Assert.deepEqual(
      result.offscreenBytes,
      result.pngBytes,
      "OffscreenCanvas encodes the same protected pixels"
    );
    Assert.deepEqual(
      result.first.slice(8),
      [0, 0, 0, 0, 0, 0, 0, 0],
      "Canvas noise preserves transparent pixels"
    );
    if (result.webglPng) {
      is(
        result.webglPng,
        result.webglReferencePng,
        "WebGL encodes the same protected pixels as 2D canvas"
      );
    } else {
      info("WebGL context unavailable; skipping WebGL extraction assertion");
    }
  });
});

add_task(async function test_ferifox_screen_media_queries_are_consistent() {
  await withFerifoxContentTask(async browser => {
    const result = await SpecialPowers.spawn(browser, [], () => ({
      width: content.screen.width,
      height: content.screen.height,
      availWidth: content.screen.availWidth,
      availHeight: content.screen.availHeight,
      deviceWidth: content.matchMedia(
        `(device-width: ${content.screen.width}px)`
      ).matches,
      deviceHeight: content.matchMedia(
        `(device-height: ${content.screen.height}px)`
      ).matches,
      colorDepth: content.matchMedia(
        `(color: ${content.screen.colorDepth / 3})`
      ).matches,
    }));

    is(result.width, EXPECTED_SCREEN_WIDTH, "Configured screen width is used");
    is(
      result.height,
      EXPECTED_SCREEN_HEIGHT,
      "Configured screen height is used"
    );
    is(
      result.availWidth,
      EXPECTED_SCREEN_WIDTH,
      "Configured available width is used"
    );
    is(
      result.availHeight,
      EXPECTED_SCREEN_HEIGHT - 40,
      "Configured available height is used"
    );
    ok(result.deviceWidth, "CSS device-width matches the Persona screen");
    ok(result.deviceHeight, "CSS device-height matches the Persona screen");
    ok(result.colorDepth, "CSS color depth matches the Persona screen");
  });
});

add_task(async function test_ferifox_geolocation_avoids_host_provider() {
  await withFerifoxContentTask(async browser => {
    const result = await SpecialPowers.spawn(browser, [], async () => {
      await SpecialPowers.pushPermissions([
        {
          type: "geo",
          allow: SpecialPowers.Services.perms.ALLOW_ACTION,
          context: content.document,
        },
      ]);

      const status = await content.navigator.permissions.query({
        name: "geolocation",
      });
      const coords = await new content.Promise((resolve, reject) => {
        const timeout = content.setTimeout(
          () => reject(new Error("Configured geolocation timed out")),
          2000
        );
        content.navigator.geolocation.getCurrentPosition(
          position => {
            content.clearTimeout(timeout);
            resolve(position.coords.toJSON());
          },
          error => {
            content.clearTimeout(timeout);
            reject(new Error(error.message));
          }
        );
      });
      return { coords, permission: status.state };
    });

    is(result.permission, "granted", "Geolocation permission is coherent");
    is(
      result.coords.latitude,
      EXPECTED_GEOLOCATION.latitude,
      "Configured latitude is returned"
    );
    is(
      result.coords.longitude,
      EXPECTED_GEOLOCATION.longitude,
      "Configured longitude is returned"
    );
    is(
      result.coords.accuracy,
      EXPECTED_GEOLOCATION.accuracy,
      "Configured accuracy is returned"
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
    Assert.lessOrEqual(
      caps.vertex,
      caps.combined,
      `Vertex texture unit cap is not above combined cap: ${caps.vertex} <= ${caps.combined}`
    );
    Assert.lessOrEqual(
      caps.fragment,
      caps.combined,
      `Fragment texture unit cap is not above combined cap: ${caps.fragment} <= ${caps.combined}`
    );
  });
});

add_task(async function test_ferifox_audio_noise_preserves_silence_and_range() {
  await withFerifoxContentTask(async browser => {
    const result = await SpecialPowers.spawn(browser, [], async () => {
      const win = content.wrappedJSObject;
      const context = new win.AudioContext();
      await context.resume();

      const analyser = new win.AnalyserNode(context);
      analyser.fftSize = 32;
      const silentFloats = new win.Float32Array(analyser.fftSize);
      const silentBytes = new win.Uint8Array(analyser.fftSize);
      analyser.getFloatTimeDomainData(silentFloats);
      analyser.getByteTimeDomainData(silentBytes);

      const source = new win.ConstantSourceNode(context);
      source.offset.value = 1;
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
        silentFloats: Array.from(silentFloats),
        silentBytes: Array.from(silentBytes),
      };
    });

    ok(
      result.silentFloats.every(value => value === 0),
      "Float analyser output preserves silence"
    );
    ok(
      result.silentBytes.every(value => value === 128),
      "Byte analyser output preserves silence"
    );
    Assert.greaterOrEqual(
      result.min,
      -1,
      `Analyser minimum stays in range: ${result.min}`
    );
    Assert.lessOrEqual(
      result.max,
      1,
      `Analyser maximum stays in range: ${result.max}`
    );
  });
});
