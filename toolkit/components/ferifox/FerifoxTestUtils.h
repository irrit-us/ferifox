/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FerifoxTestUtils_h
#define mozilla_FerifoxTestUtils_h

#include "nsIFerifoxTestUtils.h"

namespace mozilla {

class FerifoxTestUtils final : public nsIFerifoxTestUtils {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFERIFOXTESTUTILS

  FerifoxTestUtils() = default;

 private:
  ~FerifoxTestUtils() = default;
};

}  // namespace mozilla

#endif
