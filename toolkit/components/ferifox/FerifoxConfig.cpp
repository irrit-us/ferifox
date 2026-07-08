/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FerifoxConfig.h"

#include "json/json.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Span.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/intl/TimeZone.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsXULAppAPI.h"
#include "prenv.h"

#include <inttypes.h>

namespace mozilla {

static LazyLogModule sFerifoxLog("Ferifox");
static StaticMutex sFerifoxConfigMutex;

FerifoxConfig* FerifoxConfig::sSingleton;

/* static */
FerifoxConfig* FerifoxConfig::GetSingleton() {
  StaticMutexAutoLock lock(sFerifoxConfigMutex);
  if (!sSingleton) {
    sSingleton = new FerifoxConfig();
  }
  return sSingleton;
}

FerifoxConfig::FerifoxConfig()
    : mRoot(MakeUnique<Json::Value>()), mLoaded(false) {
  Load();
}

FerifoxConfig::~FerifoxConfig() = default;

void FerifoxConfig::Load() {
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

  Json::Reader reader;
  if (!reader.parse(content.BeginReading(), content.EndReading(), *mRoot,
                    false)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: JSON parse error"));
    return;
  }
  if (!mRoot->isObject()) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: root must be an object"));
    return;
  }

  mLoaded = true;
  MOZ_LOG(sFerifoxLog, LogLevel::Info,
          ("FERIFOX_CONFIG: loaded from '%s'", path));

  const Json::Value* tz = Resolve("intl.timezone"_ns);
  if (tz && tz->isString()) {
    nsAutoCString tzid(tz->asCString());
    if (!tzid.IsEmpty()) {
      mozilla::Span<const char> tzSpan(tzid.BeginReading(), tzid.Length());
      auto setResult = mozilla::intl::TimeZone::SetDefaultTimeZone(tzSpan);
      if (setResult.isOk() && setResult.unwrap()) {
        MOZ_LOG(sFerifoxLog, LogLevel::Info,
                ("FERIFOX_CONFIG: ICU timezone set to '%s'", tzid.get()));
      }

      SetPersistentEnv(mTimeZoneEnv, "FERIFOX_TZ"_ns, tzid);
#ifndef XP_WIN
      SetPersistentEnv(mPosixTimeZoneEnv, "TZ"_ns, tzid);
#endif
    }
  }

  const Json::Value* locale = Resolve("intl.locale"_ns);
  if (locale && locale->isString()) {
    nsAutoCString localeStr(locale->asCString());
    if (!localeStr.IsEmpty()) {
      SetPersistentEnv(mLocaleEnv, "LANG"_ns, localeStr);
      MOZ_LOG(sFerifoxLog, LogLevel::Info,
              ("FERIFOX_CONFIG: locale set to '%s'", localeStr.get()));
    }
  }

  if (XRE_IsParentProcess()) {
    const Json::Value* webrtc = Resolve("webrtc"_ns);
    if (webrtc && webrtc->isObject()) {
      if (const Json::Value& noHost = (*webrtc)["noHostCandidates"];
          noHost.isBool()) {
        Preferences::SetBool("media.peerconnection.ice.no_host",
                             noHost.asBool());
      }
      if (const Json::Value& defaultOnly = (*webrtc)["defaultAddressOnly"];
          defaultOnly.isBool()) {
        Preferences::SetBool("media.peerconnection.ice.default_address_only",
                             defaultOnly.asBool());
      }
      MOZ_LOG(sFerifoxLog, LogLevel::Info,
              ("FERIFOX_CONFIG: WebRTC privacy prefs applied"));
    }

    const Json::Value* webgl = Resolve("webgl"_ns);
    if (webgl && webgl->isObject()) {
      if (const Json::Value& forceEnabled = (*webgl)["forceEnabled"];
          forceEnabled.isBool()) {
        Preferences::SetBool("webgl.force-enabled", forceEnabled.asBool());
      }
      if (const Json::Value& forceEGL = (*webgl)["forceEGL"];
          forceEGL.isBool() && forceEGL.asBool()) {
        SetPersistentEnv(mWebGLForceEGLEnv, "MOZ_WEBGL_FORCE_EGL"_ns, "1"_ns);
      }
    }

    if (auto stealth = GetBool("automation.stealth"_ns); stealth && *stealth) {
      Preferences::SetBool("browser.dom.window.dump.enabled", false);
      Preferences::SetBool("devtools.debugger.remote-enabled", false);
      Preferences::SetInt("devtools.debugger.remote-port", 6000);
      Preferences::SetBool("devtools.debugger.remote-websocket", false);
      Preferences::SetBool("dom.disable_open_during_load", true);
      Preferences::SetUint("dom.input_events.security.minNumTicks", 3);
      Preferences::SetUint("dom.input_events.security.minTimeElapsedInMS", 100);
      Preferences::SetInt("dom.max_script_run_time", 10);
      Preferences::SetUint("dom.navigation.navigationRateLimit.count", 1000);
      Preferences::SetBool("dom.permissions.testing.enabled", false);
      Preferences::SetBool("dom.push.connection.enabled", true);
      Preferences::SetBool("focusmanager.testmode", false);
      Preferences::SetBool("geo.provider.testing", false);
      Preferences::SetBool("network.manage-offline-status", true);
      Preferences::SetBool("remote.bidi.dismiss_file_pickers.enabled", false);
      Preferences::SetBool("remote.prefs.recommended", false);
      Preferences::SetBool("screenshots.browser.component.enabled", true);
      (void)Preferences::ClearUser("geo.wifi.scan");
      (void)Preferences::ClearUser("hangmonitor.timeout");
    }
  }
}

void FerifoxConfig::SetPersistentEnv(nsCString& aStorage,
                                     const nsACString& aName,
                                     const nsACString& aValue) {
  aStorage.Assign(aName);
  aStorage.Append('=');
  aStorage.Append(aValue);
  (void)PR_SetEnv(aStorage.get());
}

const Json::Value* FerifoxConfig::Resolve(const nsACString& aPath) const {
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

Maybe<bool> FerifoxConfig::GetBool(const nsACString& aPath) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isBool()) {
    return Nothing();
  }
  return Some(val->asBool());
}

Maybe<int32_t> FerifoxConfig::GetInt32(const nsACString& aPath) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isInt()) {
    return Nothing();
  }
  return Some(val->asInt());
}

Maybe<uint32_t> FerifoxConfig::GetUint32(const nsACString& aPath) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isUInt()) {
    return Nothing();
  }
  return Some(val->asUInt());
}

Maybe<uint64_t> FerifoxConfig::GetUint64(const nsACString& aPath) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isUInt64()) {
    return Nothing();
  }
  return Some(val->asUInt64());
}

Maybe<double> FerifoxConfig::GetDouble(const nsACString& aPath) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isDouble()) {
    return Nothing();
  }
  return Some(val->asDouble());
}

bool FerifoxConfig::GetString(const nsACString& aPath,
                              nsAString& aResult) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isString()) {
    return false;
  }
  aResult.Truncate();
  CopyUTF8toUTF16(MakeStringSpan(val->asCString()), aResult);
  return true;
}

bool FerifoxConfig::GetStringList(const nsACString& aPath,
                                  nsTArray<nsString>& aResult) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isArray()) {
    return false;
  }
  aResult.Clear();
  for (const auto& item : *val) {
    if (item.isString()) {
      nsString str;
      CopyUTF8toUTF16(MakeStringSpan(item.asCString()), str);
      aResult.AppendElement(str);
    }
  }
  return true;
}

}  // namespace mozilla
