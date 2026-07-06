/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FerifoxConfig.h"

#include "js/Date.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Span.h"
#include "mozilla/intl/TimeZone.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "prenv.h"

#include <inttypes.h>

namespace mozilla {

static LazyLogModule sFerifoxLog("Ferifox");

FerifoxConfig* FerifoxConfig::sSingleton;

/* static */
FerifoxConfig* FerifoxConfig::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new FerifoxConfig();
  }
  return sSingleton;
}

FerifoxConfig::FerifoxConfig() : mLoaded(false) { Load(); }

void FerifoxConfig::Load() {
  const char* path = PR_GetEnv("FERIFOX_CONFIG");
  if (!path || !*path) {
    MOZ_LOG(sFerifoxLog, LogLevel::Debug,
            ("FERIFOX_CONFIG not set, skipping"));
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
  file->Exists(&exists);
  if (!exists) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: file not found '%s'", path));
    return;
  }

  int64_t fileSize = 0;
  file->GetFileSize(&fileSize);
  if (fileSize <= 0 || fileSize > 1024 * 1024) {
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
  rv = NS_ReadInputStreamToString(inputStream, content, fileSize);
  if (NS_FAILED(rv)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: failed to read file"));
    return;
  }

  Json::Reader reader;
  if (!reader.parse(content.BeginReading(), mRoot, false)) {
    MOZ_LOG(sFerifoxLog, LogLevel::Warning,
            ("FERIFOX_CONFIG: JSON parse error"));
    return;
  }

  mLoaded = true;
  MOZ_LOG(sFerifoxLog, LogLevel::Info,
          ("FERIFOX_CONFIG: loaded from '%s'", path));

  // Apply timezone override early, before the JS engine first queries it.
  const Json::Value* tz = Resolve("intl.timezone"_ns);
  if (tz && tz->isString()) {
    nsAutoCString tzid(tz->asCString());

    // Set ICU default timezone so all Intl / Date APIs use this timezone.
    mozilla::Span<const char> tzSpan(tzid.BeginReading(), tzid.Length());
    auto setResult = mozilla::intl::TimeZone::SetDefaultTimeZone(tzSpan);
    if (setResult.isOk() && setResult.unwrap()) {
      MOZ_LOG(sFerifoxLog, LogLevel::Info,
              ("FERIFOX_CONFIG: ICU timezone set to '%s'", tzid.get()));
    }

    // Set env var for js/src C++ code that checks FERIFOX_TZ.
    nsAutoCString envStr("FERIFOX_TZ="_ns);
    envStr += tzid;
    PR_SetEnv(envStr.get());

    // Force the JS engine to discard any cached timezone so it picks up
    // the new default on next access.
    JS::ResetTimeZone();
  }

  const Json::Value* locale = Resolve("intl.locale"_ns);
  if (locale && locale->isString()) {
    nsAutoCString localeStr(locale->asCString());
    nsAutoCString envStr("LANG="_ns);
    envStr += localeStr;
    PR_SetEnv(envStr.get());
    MOZ_LOG(sFerifoxLog, LogLevel::Info,
            ("FERIFOX_CONFIG: locale set to '%s'", localeStr.get()));
  }

  // Apply WebRTC privacy prefs to suppress host IP leakage.
  const Json::Value* webrtc = Resolve("webrtc"_ns);
  if (webrtc && webrtc->isObject()) {
    if (const Json::Value& noHost = (*webrtc)["noHostCandidates"];
        noHost.isBool()) {
      Preferences::SetBool("media.peerconnection.ice.no_host", noHost.asBool());
    }
    if (const Json::Value& defaultOnly =
            (*webrtc)["defaultAddressOnly"];
        defaultOnly.isBool()) {
      Preferences::SetBool("media.peerconnection.ice.default_address_only",
                            defaultOnly.asBool());
    }
    MOZ_LOG(sFerifoxLog, LogLevel::Info,
            ("FERIFOX_CONFIG: WebRTC privacy prefs applied"));
  }
}

const Json::Value* FerifoxConfig::Resolve(const nsACString& aPath) const {
  if (!mLoaded) {
    return nullptr;
  }

  const Json::Value* current = &mRoot;
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

Maybe<double> FerifoxConfig::GetDouble(const nsACString& aPath) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isDouble()) {
    return Nothing();
  }
  return Some(val->asDouble());
}

void FerifoxConfig::GetString(const nsACString& aPath,
                              nsAString& aResult) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isString()) {
    return;
  }
  CopyUTF8toUTF16(MakeStringSpan(val->asCString()), aResult);
}

void FerifoxConfig::GetStringList(const nsACString& aPath,
                                  nsTArray<nsString>& aResult) const {
  const Json::Value* val = Resolve(aPath);
  if (!val || !val->isArray()) {
    return;
  }
  for (const auto& item : *val) {
    if (item.isString()) {
      nsString str;
      CopyUTF8toUTF16(MakeStringSpan(item.asCString()), str);
      aResult.AppendElement(str);
    }
  }
}

}  // namespace mozilla
