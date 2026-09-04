/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "FerifoxTestUtils.h"

#include "FerifoxConfig.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(FerifoxTestUtils, nsIFerifoxTestUtils)

NS_IMETHODIMP
FerifoxTestUtils::SetConfigForTesting(const nsACString& aJson) {
  FerifoxConfig::SetConfigForTesting(aJson);
  return NS_OK;
}

NS_IMETHODIMP
FerifoxTestUtils::ClearConfigForTesting() {
  FerifoxConfig::ClearConfigForTesting();
  return NS_OK;
}

}  // namespace mozilla
