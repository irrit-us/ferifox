/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FerifoxConfig.h"

#include "MainThreadUtils.h"
#include "json/json.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Span.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/intl/TimeZone.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "prenv.h"

#include <cmath>
#include <inttypes.h>
#include <string>

namespace mozilla {

static LazyLogModule sFerifoxLog("Ferifox");
static StaticMutex sFerifoxConfigMutex;
static bool sParentProcessConfigReady;
static char sEmptySerializedConfigEnv[] = "FERIFOX_CONFIG_JSON=";
static constexpr size_t kMaxSerializedConfigLength = 8 * 1024;

static void LockDefaultPref(const char* aName, nsresult aSetResult) {
  nsresult result = aSetResult;
  if (NS_SUCCEEDED(result)) {
    result = Preferences::Lock(aName);
  }
  if (NS_FAILED(result)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: failed to lock pref '%s'", aName));
  }
}

// An available rectangle larger than its screen is geometry no real display
// reports; clamp it before any consumer can expose the contradiction.
static void NormalizeScreenGeometry(Json::Value& aRoot) {
  Json::Value& screen = aRoot["screen"];
  if (!screen.isObject()) {
    return;
  }
  auto positiveInt = [](const Json::Value& aValue, int64_t& aOut) {
    if (!aValue.isIntegral()) {
      return false;
    }
    int64_t v = aValue.asInt64();
    if (v <= 0 || v > INT32_MAX) {
      return false;
    }
    aOut = v;
    return true;
  };
  int64_t dimension, avail;
  if (positiveInt(screen["width"], dimension) &&
      positiveInt(screen["availWidth"], avail) && avail > dimension) {
    screen["availWidth"] = static_cast<Json::Int>(dimension);
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: clamped screen.availWidth to screen.width"));
  }
  if (positiveInt(screen["height"], dimension) &&
      positiveInt(screen["availHeight"], avail) && avail > dimension) {
    screen["availHeight"] = static_cast<Json::Int>(dimension);
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: clamped screen.availHeight to screen.height"));
  }
}

FerifoxConfig* FerifoxConfig::sSingleton;
nsCString FerifoxConfig::sTestingConfigJson;

/* static */
FerifoxConfig* FerifoxConfig::GetSingleton() {
  {
    StaticMutexAutoLock lock(sFerifoxConfigMutex);
    if (sSingleton) {
      return sSingleton;
    }
  }

  if (!NS_IsMainThread()) {
    nsresult rv = SyncRunnable::DispatchToThread(
        GetMainThreadSerialEventTarget(),
        NS_NewRunnableFunction("FerifoxConfig::GetSingleton",
                               [] { (void)FerifoxConfig::GetSingleton(); }));
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    StaticMutexAutoLock lock(sFerifoxConfigMutex);
    return sSingleton;
  }

  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  if (!sSingleton) {
    sSingleton = new FerifoxConfig();
  }
  return sSingleton;
}

/* static */
void FerifoxConfig::InitializeParentProcess() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  sParentProcessConfigReady = true;
  if (!sSingleton) {
    sSingleton = new FerifoxConfig();
  } else if (!sSingleton->mLoaded) {
    sSingleton->Load();
  }
}

FerifoxConfig::FerifoxConfig()
    : mRoot(MakeUnique<Json::Value>()), mLoaded(false) {
  Load();
}

FerifoxConfig::~FerifoxConfig() = default;

void FerifoxConfig::Load() {
  const bool isParentProcess = XRE_IsParentProcess();
  if (isParentProcess && !sParentProcessConfigReady &&
      sTestingConfigJson.IsEmpty()) {
    return;
  }

  if (isParentProcess) {
    if (PR_SetEnv(sEmptySerializedConfigEnv) != PR_SUCCESS) {
      MOZ_LOG(sFerifoxLog, LogLevel::Warning,
              ("FERIFOX_CONFIG: failed to clear serialized config"));
      return;
    }
  }

  if (!sTestingConfigJson.IsEmpty()) {
    (void)LoadFromJSONString(sTestingConfigJson, "ferifox testing override");
    return;
  }

  if (!isParentProcess) {
    const char* serializedConfig = PR_GetEnv("FERIFOX_CONFIG_JSON");
    if (serializedConfig && *serializedConfig) {
      (void)LoadFromJSONString(nsDependentCString(serializedConfig),
                               "parent process");
    }
    return;
  }

  const char* path = PR_GetEnv("FERIFOX_CONFIG");
  if (!path || !*path) {
    MOZ_LOG(sFerifoxLog, LogLevel::Debug, ("FERIFOX_CONFIG not set, skipping"));
    return;
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_NewLocalFile(NS_ConvertUTF8toUTF16(path), getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: invalid path '%s'", path));
    return;
  }

  bool exists = false;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: file not found '%s'", path));
    return;
  }

  int64_t fileSize = 0;
  rv = file->GetFileSize(&fileSize);
  if (NS_FAILED(rv) || fileSize <= 0 || fileSize > 1024 * 1024) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: bad file size %" PRId64, fileSize));
    return;
  }

  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), file);
  if (NS_FAILED(rv)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: failed to open file"));
    return;
  }

  nsAutoCString content;
  rv = NS_ReadInputStreamToString(inputStream, content,
                                  static_cast<uint64_t>(fileSize));
  if (NS_FAILED(rv)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: failed to read file"));
    return;
  }
  if (!LoadFromJSONString(content, path)) {
    return;
  }
}

bool FerifoxConfig::LoadFromJSONString(const nsACString& aContent,
                                       const char* aSource) {
  auto root = MakeUnique<Json::Value>();
  Json::Reader reader;
  if (!reader.parse(aContent.BeginReading(), aContent.EndReading(), *root,
                    false)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: JSON parse error"));
    return false;
  }
  if (!root->isObject()) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: root must be an object"));
    return false;
  }

  NormalizeScreenGeometry(*root);

  if (XRE_IsParentProcess()) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string serializedConfig = Json::writeString(builder, *root);
    if (serializedConfig.size() > kMaxSerializedConfigLength) {
      MOZ_LOG(sFerifoxLog, LogLevel::Warning,
              ("FERIFOX_CONFIG: serialized config is too large"));
      return false;
    }
    if (!SetPersistentEnv("FERIFOX_CONFIG_JSON"_ns,
                          nsDependentCString(serializedConfig.c_str(),
                                             static_cast<uint32_t>(
                                                 serializedConfig.size())))) {
      return false;
    }
  }

  mRoot = std::move(root);
  mLoaded = true;
  MOZ_LOG(sFerifoxLog, LogLevel::Info,
          ("FERIFOX_CONFIG: loaded from '%s'", aSource));

  const Json::Value* tz = ResolveNoLock("intl.timezone"_ns);
  if (tz && tz->isString()) {
    nsAutoCString tzid(tz->asCString());
    if (!tzid.IsEmpty()) {
      mozilla::Span<const char> tzSpan(tzid.BeginReading(), tzid.Length());
      auto setResult = mozilla::intl::TimeZone::SetDefaultTimeZone(tzSpan);
      if (setResult.isOk() && setResult.unwrap()) {
        mTimeZone = tzid;
        MOZ_LOG(sFerifoxLog, LogLevel::Info,
                ("FERIFOX_CONFIG: ICU timezone set to '%s'", tzid.get()));
        (void)SetPersistentEnv("FERIFOX_TZ"_ns, tzid);
#ifndef XP_WIN
        (void)SetPersistentEnv("TZ"_ns, tzid);
#endif
      } else {
        MOZ_LOG(sFerifoxLog, LogLevel::Warning,
                ("FERIFOX_CONFIG: invalid timezone '%s'", tzid.get()));
      }
    }
  }

  const Json::Value* locale = ResolveNoLock("intl.locale"_ns);
  if (locale && locale->isString()) {
    nsAutoCString localeStr(locale->asCString());
    if (!localeStr.IsEmpty()) {
      nsAutoCString canonicalLocale(localeStr);
      if (intl::LocaleService::CanonicalizeLanguageId(canonicalLocale)) {
        mCanonicalLocale = canonicalLocale;
        (void)SetPersistentEnv("LANG"_ns, localeStr);
        MOZ_LOG(sFerifoxLog, LogLevel::Info,
                ("FERIFOX_CONFIG: locale set to '%s'", localeStr.get()));
      } else {
        MOZ_LOG(sFerifoxLog, LogLevel::Warning,
                ("FERIFOX_CONFIG: invalid locale '%s'", localeStr.get()));
      }
    }
  }

  if (XRE_IsParentProcess()) {
    if (const Json::Value* dpr = ResolveNoLock("screen.devicePixelRatio"_ns)) {
      if (dpr->isDouble() && std::isfinite(dpr->asDouble()) &&
          dpr->asDouble() > 0.0 && dpr->asDouble() <= 10.0) {
        LockDefaultPref(
            "layout.css.devPixelsPerPx",
            Preferences::SetFloat("layout.css.devPixelsPerPx",
                                  static_cast<float>(dpr->asDouble()),
                                  PrefValueKind::Default));
        LockDefaultPref("browser.display.os-zoom-behavior",
                        Preferences::SetInt("browser.display.os-zoom-behavior",
                                            0, PrefValueKind::Default));
      } else {
        MOZ_LOG(sFerifoxLog, LogLevel::Warning,
                ("FERIFOX_CONFIG: invalid screen.devicePixelRatio"));
      }
    }

    if (const Json::Value* fonts = ResolveNoLock("fonts.visible"_ns)) {
      nsAutoCString whitelist;
      bool valid = fonts->isArray() && !fonts->empty();
      if (valid) {
        for (const auto& font : *fonts) {
          if (!font.isString()) {
            valid = false;
            break;
          }
          nsAutoCString name(font.asCString());
          if (name.IsEmpty() || name.FindChar(',') != kNotFound) {
            valid = false;
            break;
          }
          if (!whitelist.IsEmpty()) {
            whitelist.Append(',');
          }
          whitelist.Append(name);
        }
      }
      if (valid) {
        LockDefaultPref(
            "font.system.whitelist",
            Preferences::SetCString("font.system.whitelist", whitelist,
                                    PrefValueKind::Default));
      } else {
        MOZ_LOG(sFerifoxLog, LogLevel::Warning,
                ("FERIFOX_CONFIG: invalid fonts.visible"));
      }
    }

    const Json::Value* webrtc = ResolveNoLock("webrtc"_ns);
    if (webrtc && webrtc->isObject()) {
      if (const Json::Value& noHost = (*webrtc)["noHostCandidates"];
          noHost.isBool()) {
        LockDefaultPref(
            "media.peerconnection.ice.no_host",
            Preferences::SetBool("media.peerconnection.ice.no_host",
                                 noHost.asBool(), PrefValueKind::Default));
      }
      if (const Json::Value& defaultOnly = (*webrtc)["defaultAddressOnly"];
          defaultOnly.isBool()) {
        LockDefaultPref("media.peerconnection.ice.default_address_only",
                        Preferences::SetBool(
                            "media.peerconnection.ice.default_address_only",
                            defaultOnly.asBool(), PrefValueKind::Default));
      }
      if (const Json::Value& proxyOnly = (*webrtc)["proxyOnlyIfBehindProxy"];
          proxyOnly.isBool()) {
        LockDefaultPref(
            "media.peerconnection.ice.proxy_only_if_behind_proxy",
            Preferences::SetBool(
                "media.peerconnection.ice.proxy_only_if_behind_proxy",
                proxyOnly.asBool(), PrefValueKind::Default));
      }
      MOZ_LOG(sFerifoxLog, LogLevel::Info,
              ("FERIFOX_CONFIG: WebRTC privacy prefs applied"));
    }

    const Json::Value* webgl = ResolveNoLock("webgl"_ns);
    if (webgl && webgl->isObject()) {
      if (const Json::Value& forceEnabled = (*webgl)["forceEnabled"];
          forceEnabled.isBool()) {
        LockDefaultPref(
            "webgl.force-enabled",
            Preferences::SetBool("webgl.force-enabled", forceEnabled.asBool(),
                                 PrefValueKind::Default));
      }
      if (const Json::Value& forceEGL = (*webgl)["forceEGL"];
          forceEGL.isBool() && forceEGL.asBool()) {
        (void)SetPersistentEnv("MOZ_WEBGL_FORCE_EGL"_ns, "1"_ns);
      }
    }

    if (auto stealth = GetBoolNoLock("automation.stealth"_ns);
        stealth && *stealth) {
      LockDefaultPref("browser.dom.window.dump.enabled",
                      Preferences::SetBool("browser.dom.window.dump.enabled",
                                           false, PrefValueKind::Default));
      LockDefaultPref("devtools.debugger.remote-enabled",
                      Preferences::SetBool("devtools.debugger.remote-enabled",
                                           false, PrefValueKind::Default));
      LockDefaultPref("devtools.debugger.remote-port",
                      Preferences::SetInt("devtools.debugger.remote-port", 6000,
                                          PrefValueKind::Default));
      LockDefaultPref("devtools.debugger.remote-websocket",
                      Preferences::SetBool("devtools.debugger.remote-websocket",
                                           false, PrefValueKind::Default));
      LockDefaultPref("dom.disable_open_during_load",
                      Preferences::SetBool("dom.disable_open_during_load", true,
                                           PrefValueKind::Default));
      LockDefaultPref(
          "dom.input_events.security.minNumTicks",
          Preferences::SetUint("dom.input_events.security.minNumTicks", 3,
                               PrefValueKind::Default));
      LockDefaultPref(
          "dom.input_events.security.minTimeElapsedInMS",
          Preferences::SetUint("dom.input_events.security.minTimeElapsedInMS",
                               100, PrefValueKind::Default));
      LockDefaultPref("dom.max_script_run_time",
                      Preferences::SetInt("dom.max_script_run_time", 10,
                                          PrefValueKind::Default));
      LockDefaultPref(
          "dom.navigation.navigationRateLimit.count",
          Preferences::SetUint("dom.navigation.navigationRateLimit.count", 1000,
                               PrefValueKind::Default));
      LockDefaultPref("dom.permissions.testing.enabled",
                      Preferences::SetBool("dom.permissions.testing.enabled",
                                           false, PrefValueKind::Default));
      LockDefaultPref("dom.push.connection.enabled",
                      Preferences::SetBool("dom.push.connection.enabled", true,
                                           PrefValueKind::Default));
      LockDefaultPref("focusmanager.testmode",
                      Preferences::SetBool("focusmanager.testmode", false,
                                           PrefValueKind::Default));
      LockDefaultPref("geo.provider.testing",
                      Preferences::SetBool("geo.provider.testing", false,
                                           PrefValueKind::Default));
      LockDefaultPref("network.manage-offline-status",
                      Preferences::SetBool("network.manage-offline-status",
                                           true, PrefValueKind::Default));
      LockDefaultPref(
          "remote.bidi.dismiss_file_pickers.enabled",
          Preferences::SetBool("remote.bidi.dismiss_file_pickers.enabled",
                               false, PrefValueKind::Default));
      LockDefaultPref("remote.prefs.recommended",
                      Preferences::SetBool("remote.prefs.recommended", false,
                                           PrefValueKind::Default));
      LockDefaultPref(
          "screenshots.browser.component.enabled",
          Preferences::SetBool("screenshots.browser.component.enabled", true,
                               PrefValueKind::Default));
    }
  }

  return true;
}

/* static */
void FerifoxConfig::SetConfigForTesting(const nsACString& aJson) {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  sTestingConfigJson = aJson;
  if (!sSingleton) {
    sSingleton = new FerifoxConfig();
    return;
  }

  sSingleton->mRoot = MakeUnique<Json::Value>();
  sSingleton->mCanonicalLocale.Truncate();
  sSingleton->mTimeZone.Truncate();
  sSingleton->mLoaded = false;
  sSingleton->Load();
}

/* static */
void FerifoxConfig::ClearConfigForTesting() {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  sTestingConfigJson.Truncate();
  if (!sSingleton) {
    return;
  }

  sSingleton->mRoot = MakeUnique<Json::Value>();
  sSingleton->mCanonicalLocale.Truncate();
  sSingleton->mTimeZone.Truncate();
  sSingleton->mLoaded = false;
  sSingleton->Load();
}

bool FerifoxConfig::SetPersistentEnv(const nsACString& aName,
                                     const nsACString& aValue) {
  auto storage = MakeUnique<nsCString>(aName);
  storage->Append('=');
  storage->Append(aValue);
  nsCString& kept = *storage;
  mPersistentEnvStrings.AppendElement(std::move(storage));
  if (PR_SetEnv(kept.get()) == PR_SUCCESS) {
    return true;
  }
  MOZ_LOG(sFerifoxLog, LogLevel::Warning,
          ("FERIFOX_CONFIG: failed to set environment variable '%s'",
           PromiseFlatCString(aName).get()));
  return false;
}

bool FerifoxConfig::IsLoaded() const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  return mLoaded;
}

bool FerifoxConfig::GetCanonicalLocale(nsACString& aResult) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  if (mCanonicalLocale.IsEmpty()) {
    return false;
  }
  aResult = mCanonicalLocale;
  return true;
}

bool FerifoxConfig::GetTimeZone(nsACString& aResult) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  if (mTimeZone.IsEmpty()) {
    return false;
  }
  aResult = mTimeZone;
  return true;
}

const Json::Value* FerifoxConfig::ResolveNoLock(const nsACString& aPath) const {
  if (!mLoaded) {
    return nullptr;
  }

  const Json::Value* current = mRoot.get();
  nsCString path(aPath);
  int32_t start = 0;

  while (start < static_cast<int32_t>(path.Length())) {
    int32_t dot = path.FindChar('.', start);
    if (dot == kNotFound) {
      dot = path.Length();
    }
    if (dot == start) {
      return nullptr;
    }

    if (!current->isObject()) {
      return nullptr;
    }

    nsCString key(Substring(path, start, dot - start));
    current = &(*current)[key.get()];
    if (current->isNull()) {
      return nullptr;
    }

    start = dot + 1;
  }

  return current;
}

Maybe<bool> FerifoxConfig::GetBoolNoLock(const nsACString& aPath) const {
  const Json::Value* val = ResolveNoLock(aPath);
  if (!val || !val->isBool()) {
    return Nothing();
  }
  return Some(val->asBool());
}

Maybe<bool> FerifoxConfig::GetBool(const nsACString& aPath) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  return GetBoolNoLock(aPath);
}

Maybe<int32_t> FerifoxConfig::GetInt32(const nsACString& aPath) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  const Json::Value* val = ResolveNoLock(aPath);
  if (!val || !val->isInt()) {
    return Nothing();
  }
  return Some(val->asInt());
}

Maybe<uint32_t> FerifoxConfig::GetUint32(const nsACString& aPath) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  const Json::Value* val = ResolveNoLock(aPath);
  if (!val || !val->isUInt()) {
    return Nothing();
  }
  return Some(val->asUInt());
}

Maybe<double> FerifoxConfig::GetDouble(const nsACString& aPath) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  const Json::Value* val = ResolveNoLock(aPath);
  if (!val || !val->isDouble() || !std::isfinite(val->asDouble())) {
    return Nothing();
  }
  return Some(val->asDouble());
}

bool FerifoxConfig::GetString(const nsACString& aPath,
                              nsAString& aResult) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  const Json::Value* val = ResolveNoLock(aPath);
  if (!val || !val->isString()) {
    return false;
  }
  aResult.Truncate();
  CopyUTF8toUTF16(MakeStringSpan(val->asCString()), aResult);
  return true;
}

bool FerifoxConfig::GetStringList(const nsACString& aPath,
                                  nsTArray<nsString>& aResult) const {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  const Json::Value* val = ResolveNoLock(aPath);
  if (!val || !val->isArray()) {
    return false;
  }
  aResult.Clear();
  for (const auto& item : *val) {
    if (!item.isString()) {
      aResult.Clear();
      return false;
    }
    nsString str;
    CopyUTF8toUTF16(MakeStringSpan(item.asCString()), str);
    aResult.AppendElement(str);
  }
  return true;
}

}  // namespace mozilla
