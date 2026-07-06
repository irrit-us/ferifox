/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FerifoxConfig_h
#define mozilla_FerifoxConfig_h

#include "json/json.h"
#include "mozilla/Maybe.h"
#include "nsStringFwd.h"
#include "nsTArray.h"

namespace mozilla {

class FerifoxConfig {
 public:
  static FerifoxConfig* GetSingleton();

  bool IsLoaded() const { return mLoaded; }

  Maybe<bool> GetBool(const nsACString& aPath) const;
  Maybe<int32_t> GetInt32(const nsACString& aPath) const;
  Maybe<uint32_t> GetUint32(const nsACString& aPath) const;
  Maybe<double> GetDouble(const nsACString& aPath) const;
  void GetString(const nsACString& aPath, nsAString& aResult) const;
  void GetStringList(const nsACString& aPath,
                     nsTArray<nsString>& aResult) const;

 private:
  FerifoxConfig();
  ~FerifoxConfig() = default;

  void Load();

  const Json::Value* Resolve(const nsACString& aPath) const;

  static FerifoxConfig* sSingleton;

  Json::Value mRoot;
  bool mLoaded;
};

}  // namespace mozilla

#endif  // mozilla_FerifoxConfig_h
