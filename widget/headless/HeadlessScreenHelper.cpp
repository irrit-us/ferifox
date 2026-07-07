/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessScreenHelper.h"

#include "FerifoxConfig.h"
#include "prenv.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace widget {

/* static */
LayoutDeviceIntRect HeadlessScreenHelper::GetScreenRect() {
  if (auto* cfg = FerifoxConfig::GetSingleton()) {
    auto w = cfg->GetInt32("screen.width"_ns);
    auto h = cfg->GetInt32("screen.height"_ns);
    if (w && h && *w > 0 && *h > 0) {
      return LayoutDeviceIntRect(0, 0, *w, *h);
    }
  }

  char* ev = PR_GetEnv("MOZ_HEADLESS_WIDTH");
  int width = 1366;
  if (ev) {
    width = atoi(ev);
  }
  ev = PR_GetEnv("MOZ_HEADLESS_HEIGHT");
  int height = 768;
  if (ev) {
    height = atoi(ev);
  }
  return LayoutDeviceIntRect(0, 0, width, height);
}

static uint32_t GetScreenDepth() {
  if (auto* cfg = FerifoxConfig::GetSingleton()) {
    if (auto depth = cfg->GetInt32("screen.pixelDepth"_ns)) {
      if (*depth > 0) {
        return *depth;
      }
    }
    if (auto depth = cfg->GetInt32("screen.colorDepth"_ns)) {
      if (*depth > 0) {
        return *depth;
      }
    }
  }
  return 24;
}

HeadlessScreenHelper::HeadlessScreenHelper() {
  AutoTArray<RefPtr<Screen>, 1> screenList;
  LayoutDeviceIntRect rect = GetScreenRect();
  uint32_t depth = GetScreenDepth();
  auto ret =
      MakeRefPtr<Screen>(rect, rect, depth, depth, 0,
                         DesktopToLayoutDeviceScale(), CSSToLayoutDeviceScale(),
                         96.0f, Screen::IsPseudoDisplay::No, Screen::IsHDR::No);
  screenList.AppendElement(ret.forget());
  ScreenManager::Refresh(std::move(screenList));
}

}  // namespace widget
}  // namespace mozilla
