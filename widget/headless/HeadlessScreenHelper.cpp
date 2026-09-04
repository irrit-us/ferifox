/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessScreenHelper.h"

#include "FerifoxConfig.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/DOMTypes.h"
#include "nsTArray.h"
#include "prenv.h"

#include <cmath>
#include <limits>

namespace mozilla {
namespace widget {

static Maybe<int32_t> GetScaledDimension(const nsACString& aPath,
                                         double aScale) {
  auto* cfg = FerifoxConfig::GetSingleton();
  if (!cfg) {
    return Nothing();
  }
  auto value = cfg->GetInt32(aPath);
  if (!value || *value <= 0) {
    return Nothing();
  }
  double scaled = std::round(*value * aScale);
  if (scaled < 1.0 || scaled > std::numeric_limits<int32_t>::max()) {
    return Nothing();
  }
  return Some(static_cast<int32_t>(scaled));
}

/* static */
LayoutDeviceIntRect HeadlessScreenHelper::GetScreenRect() {
  double scale = GetScale();
  auto width = GetScaledDimension("screen.width"_ns, scale);
  auto height = GetScaledDimension("screen.height"_ns, scale);
  if (width && height) {
    return LayoutDeviceIntRect(0, 0, *width, *height);
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

/* static */
LayoutDeviceIntRect HeadlessScreenHelper::GetAvailableScreenRect() {
  double scale = GetScale();
  auto width = GetScaledDimension("screen.availWidth"_ns, scale);
  auto height = GetScaledDimension("screen.availHeight"_ns, scale);
  if (width && height) {
    return LayoutDeviceIntRect(0, 0, *width, *height);
  }
  return GetScreenRect();
}

/* static */
double HeadlessScreenHelper::GetScale() {
  if (auto* cfg = FerifoxConfig::GetSingleton()) {
    if (auto scale = cfg->GetDouble("screen.devicePixelRatio"_ns)) {
      if (std::isfinite(*scale) && *scale > 0.0 && *scale <= 10.0) {
        double effectiveScale = StaticPrefs::layout_css_devPixelsPerPx();
        if (std::isfinite(effectiveScale) && effectiveScale > 0.0) {
          return effectiveScale;
        }
      }
    }
  }
  return 1.0;
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
  LayoutDeviceIntRect availRect = GetAvailableScreenRect();
  uint32_t depth = GetScreenDepth();
  double scale = GetScale();
#ifdef XP_WIN
  DesktopToLayoutDeviceScale contentsScale;
#else
  DesktopToLayoutDeviceScale contentsScale(scale);
#endif
  auto ret = MakeRefPtr<Screen>(rect, availRect, depth, depth, 0, contentsScale,
                                CSSToLayoutDeviceScale(scale),
                                static_cast<float>(96.0 * scale),
                                Screen::IsPseudoDisplay::No, Screen::IsHDR::No,
                                80.0f, 1000.0f);
  screenList.AppendElement(ret.forget());
  ScreenManager::Refresh(std::move(screenList));
}

}  // namespace widget
}  // namespace mozilla
