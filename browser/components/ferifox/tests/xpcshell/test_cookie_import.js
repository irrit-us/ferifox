/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FerifoxCookieImport } = ChromeUtils.importESModule(
  "resource:///modules/FerifoxCookieImport.sys.mjs"
);
const { importCookiesFromFile, importFerifoxCookiesForStartup } =
  FerifoxCookieImport;

do_get_profile();

const NOW_S = Math.floor(Date.now() / 1000);
const MAXAGE_CAP_S = Services.prefs.getIntPref(
  "network.cookie.maxageCap",
  34560000
);

const COOKIES = [
  {
    name: "session_id",
    value: "abc123",
    domain: ".example.com",
    path: "/",
    secure: true,
    httpOnly: true,
    sameSite: "Lax",
    expires: NOW_S + 3600,
  },
  {
    // Persistent cookie past the client-side lifetime cap; it is clamped
    // rather than rejected.
    name: "longterm",
    value: "xyz",
    domain: ".example.com",
    expires: 4102444800, // 2100-01-01
  },
  {
    // Host-only session cookie relying on defaults.
    name: "pref",
    value: "dark",
    domain: "example.org",
  },
  // Invalid entries must be skipped without failing the import.
  { name: "", value: "x", domain: "example.com" },
  { name: "no_domain", value: "x" },
  { name: "bad_path", value: "x", domain: "example.com", path: "relative" },
  "not an object",
];

function findCookie(host, name) {
  return Services.cookies.cookies.find(c => c.host == host && c.name == name);
}

add_task(async function test_import_from_file() {
  const path = PathUtils.join(PathUtils.tempDir, "ferifox-cookies-test.json");
  await IOUtils.writeJSON(path, COOKIES);

  const result = await importCookiesFromFile(path);
  Assert.equal(result.loaded, 3);
  Assert.equal(result.failed, 4);

  const domainCookie = findCookie(".example.com", "session_id");
  Assert.ok(domainCookie, "domain cookie imported");
  Assert.equal(domainCookie.value, "abc123");
  Assert.equal(domainCookie.path, "/");
  Assert.ok(domainCookie.isSecure);
  Assert.ok(domainCookie.isHttpOnly);
  Assert.ok(!domainCookie.isSession);
  Assert.equal(domainCookie.expiry, (NOW_S + 3600) * 1000);
  Assert.equal(domainCookie.sameSite, Ci.nsICookie.SAMESITE_LAX);
  Assert.equal(domainCookie.schemeMap, Ci.nsICookie.SCHEME_HTTPS);

  const clampedCookie = findCookie(".example.com", "longterm");
  Assert.ok(clampedCookie, "capped cookie imported");
  Assert.ok(!clampedCookie.isSession);
  Assert.ok(clampedCookie.expiry > Date.now(), "expiry is in the future");
  Assert.ok(
    clampedCookie.expiry <= Date.now() + MAXAGE_CAP_S * 1000,
    "expiry clamped to the maxage cap"
  );

  const hostCookie = findCookie("example.org", "pref");
  Assert.ok(hostCookie, "host cookie imported");
  Assert.equal(hostCookie.path, "/");
  Assert.ok(!hostCookie.isSecure);
  Assert.ok(!hostCookie.isHttpOnly);
  Assert.ok(hostCookie.isSession);
  Assert.equal(hostCookie.sameSite, Ci.nsICookie.SAMESITE_UNSET);
  Assert.equal(
    hostCookie.schemeMap,
    Ci.nsICookie.SCHEME_HTTP | Ci.nsICookie.SCHEME_HTTPS
  );
});

add_task(async function test_startup_import_via_env() {
  Services.cookies.removeAll();
  const path = PathUtils.join(PathUtils.tempDir, "ferifox-cookies-env.json");
  await IOUtils.writeJSON(path, [
    { name: "env_cookie", value: "1", domain: "example.net" },
  ]);

  Services.env.set("FERIFOX_COOKIES", path);
  try {
    const result = await importFerifoxCookiesForStartup();
    Assert.equal(result.loaded, 1);
    Assert.equal(result.failed, 0);
    Assert.ok(findCookie("example.net", "env_cookie"));
  } finally {
    Services.env.set("FERIFOX_COOKIES", "");
  }
});

add_task(async function test_startup_import_from_profile() {
  Services.cookies.removeAll();
  const path = PathUtils.join(PathUtils.profileDir, ".ferifox-cookie");
  await IOUtils.writeJSON(path, [
    { name: "profile_cookie", value: "2", domain: "example.edu" },
  ]);
  try {
    const result = await importFerifoxCookiesForStartup();
    Assert.equal(result.loaded, 1);
    Assert.ok(findCookie("example.edu", "profile_cookie"));
  } finally {
    await IOUtils.remove(path);
  }
});

add_task(async function test_startup_import_absent() {
  const result = await importFerifoxCookiesForStartup();
  Assert.equal(result, null, "no cookie file resolves to null");
});

add_task(async function test_invalid_json_rejects() {
  const path = PathUtils.join(PathUtils.tempDir, "ferifox-cookies-bad.json");
  await IOUtils.writeUTF8(path, "not json");
  await Assert.rejects(importCookiesFromFile(path), /JSON\.parse/);

  const objectPath = PathUtils.join(
    PathUtils.tempDir,
    "ferifox-cookies-object.json"
  );
  await IOUtils.writeJSON(objectPath, { name: "x" });
  await Assert.rejects(importCookiesFromFile(objectPath), /JSON array/);
});
