/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FerifoxConfig_h
#define mozilla_FerifoxConfig_h

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"
#include "nsTArray.h"

#include <cstdint>

namespace Json {
class Value;
}

namespace mozilla {

class FerifoxConfig {
 public:
  static FerifoxConfig* GetSingleton();
  static void SetConfigForTesting(const nsACString& aJson);
  static void ClearConfigForTesting();

  bool IsLoaded() const;

  Maybe<bool> GetBool(const nsACString& aPath) const;
  Maybe<int32_t> GetInt32(const nsACString& aPath) const;
  Maybe<uint32_t> GetUint32(const nsACString& aPath) const;
  Maybe<uint64_t> GetUint64(const nsACString& aPath) const;
  Maybe<double> GetDouble(const nsACString& aPath) const;
  bool GetString(const nsACString& aPath, nsAString& aResult) const;
  bool GetStringList(const nsACString& aPath,
                     nsTArray<nsString>& aResult) const;

 private:
  FerifoxConfig();
  ~FerifoxConfig();

  void Load();
  bool LoadFromJSONString(const nsACString& aContent, const char* aSource);
  void SetPersistentEnv(nsCString& aStorage, const nsACString& aName,
                        const nsACString& aValue);

  const Json::Value* ResolveNoLock(const nsACString& aPath) const;
  Maybe<bool> GetBoolNoLock(const nsACString& aPath) const;

  static FerifoxConfig* sSingleton;
  static nsCString sTestingConfigJson;

  UniquePtr<Json::Value> mRoot;
  nsCString mTimeZoneEnv;
  nsCString mLocaleEnv;
  nsCString mWebGLForceEGLEnv;
#ifndef XP_WIN
  nsCString mPosixTimeZoneEnv;
#endif
  bool mLoaded;
};

}  // namespace mozilla

#endif  // mozilla_FerifoxConfig_h
