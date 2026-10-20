/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Ferifox startup cookie import.
 *
 * Cookies are loaded once, early in startup, from a JSON file:
 * the path in the FERIFOX_COOKIES environment variable when set, otherwise
 * a `.ferifox-cookie` file in the profile directory when present.
 *
 * File format: a JSON array of cookie objects, e.g.
 *
 *   [
 *     {
 *       "name": "session",
 *       "value": "abc123",
 *       "domain": ".example.com",
 *       "path": "/",
 *       "secure": true,
 *       "httpOnly": true,
 *       "sameSite": "Lax",
 *       "expires": 1893456000
 *     }
 *   ]
 *
 * `name`, `value`, and `domain` are required strings. A leading dot in
 * `domain` marks a domain cookie. `path` defaults to "/", `secure` and
 * `httpOnly` default to false, and `sameSite` accepts "None" (requires
 * `secure: true`, otherwise the cookie service rejects the entry), "Lax", or
 * "Strict" (unset otherwise). `expires` is a Unix timestamp in seconds;
 * without it the cookie is a session cookie, and values beyond the
 * client-side lifetime cap (`network.cookie.maxageCap`, 400 days by
 * default) are clamped to the cap. Invalid entries are skipped and
 * logged; the file failing to parse aborts the whole import.
 */

const COOKIE_FILE_NAME = ".ferifox-cookie";
const ENV_COOKIE_FILE = "FERIFOX_COOKIES";

// Expiry placeholder for session cookies; the IDL honors the more
// restrictive of session state and expiry, so any far-future value works.
const SESSION_EXPIRY_PLACEHOLDER_MS = 4102444800000; // 2100-01-01

const SAMESITE_VALUES = new Map([
  ["none", Ci.nsICookie.SAMESITE_NONE],
  ["lax", Ci.nsICookie.SAMESITE_LAX],
  ["strict", Ci.nsICookie.SAMESITE_STRICT],
]);

function log(message) {
  Services.console.logStringMessage(`ferifox cookie import: ${message}`);
}

function addCookie(entry) {
  if (typeof entry != "object" || entry === null) {
    throw new TypeError("cookie entry must be an object");
  }
  const { name, value, domain } = entry;
  if (typeof name != "string" || !name) {
    throw new TypeError("cookie name must be a non-empty string");
  }
  if (typeof value != "string") {
    throw new TypeError(`cookie "${name}": value must be a string`);
  }
  if (typeof domain != "string" || !domain) {
    throw new TypeError(`cookie "${name}": domain must be a non-empty string`);
  }

  let path = "/";
  if (entry.path !== undefined) {
    if (typeof entry.path != "string" || !entry.path.startsWith("/")) {
      throw new TypeError(`cookie "${name}": path must start with "/"`);
    }
    path = entry.path;
  }

  let sameSite = Ci.nsICookie.SAMESITE_UNSET;
  if (entry.sameSite !== undefined) {
    sameSite = SAMESITE_VALUES.get(String(entry.sameSite).toLowerCase());
    if (sameSite === undefined) {
      throw new TypeError(`cookie "${name}": invalid sameSite value`);
    }
  }

  let isSession = true;
  let expiry = SESSION_EXPIRY_PLACEHOLDER_MS;
  if (entry.expires !== undefined) {
    if (!Number.isFinite(entry.expires) || entry.expires <= 0) {
      throw new TypeError(
        `cookie "${name}": expires must be a positive Unix timestamp`
      );
    }
    isSession = false;
    expiry = Math.round(entry.expires * 1000);
    // The cookie service rejects persistent cookies beyond the client-side
    // lifetime cap (400 days by default); clamp instead of failing.
    const maxageCap = Services.prefs.getIntPref(
      "network.cookie.maxageCap",
      34560000
    );
    if (maxageCap > 0) {
      expiry = Math.min(expiry, Date.now() + maxageCap * 1000 - 1000);
    }
  }

  const secure = entry.secure === true;
  const cv = Services.cookies.add(
    domain,
    path,
    name,
    value,
    secure,
    entry.httpOnly === true,
    isSession,
    expiry,
    {},
    sameSite,
    secure
      ? Ci.nsICookie.SCHEME_HTTPS
      : Ci.nsICookie.SCHEME_HTTP | Ci.nsICookie.SCHEME_HTTPS
  );
  if (cv.result != Ci.nsICookieValidation.eOK) {
    throw new Error(`cookie "${name}" rejected: ${cv.errorString}`);
  }
}

export const FerifoxCookieImport = {
  /**
   * Import cookies from a JSON file. Throws when the file is unreadable or
   * not a JSON array; individual invalid entries are skipped.
   *
   * @returns {{path: string, loaded: number, failed: number}}
   */
  async importCookiesFromFile(path) {
    const entries = await IOUtils.readJSON(path);
    if (!Array.isArray(entries)) {
      throw new TypeError("cookie file must contain a JSON array");
    }
    let loaded = 0;
    let failed = 0;
    for (const entry of entries) {
      try {
        addCookie(entry);
        loaded++;
      } catch (e) {
        failed++;
        log(`skipping entry: ${e.message ?? e}`);
      }
    }
    return { path, loaded, failed };
  },

  /**
   * Resolve and import the startup cookie file, if any. Never throws.
   *
   * @returns {Promise<object|null>} import result, or null when no file exists.
   */
  async importFerifoxCookiesForStartup() {
    try {
      let envPath = Services.env.get(ENV_COOKIE_FILE);
      let path =
        envPath || PathUtils.join(PathUtils.profileDir, COOKIE_FILE_NAME);
      if (!(await IOUtils.exists(path))) {
        if (envPath) {
          log(`${ENV_COOKIE_FILE} file not found: ${path}`);
        }
        return null;
      }
      const result = await FerifoxCookieImport.importCookiesFromFile(path);
      log(
        `imported ${result.loaded} cookie(s) from ${path}` +
          (result.failed ? `, ${result.failed} rejected` : "")
      );
      return result;
    } catch (e) {
      log(`startup import failed: ${e.message ?? e}`);
      return null;
    }
  },
};
