/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMimeTypeArray.h"

#include "mozilla/StaticPrefs_pdfjs.h"
#include "mozilla/dom/MimeTypeArrayBinding.h"
#include "mozilla/dom/MimeTypeBinding.h"
#include "nsContentUtils.h"
#include "nsPIDOMWindowInlines.h"
#include "nsPluginArray.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsMimeTypeArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsMimeTypeArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsMimeTypeArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeTypeArray, mWindow, mMimeTypes[0],
                                      mMimeTypes[1])

nsMimeTypeArray::nsMimeTypeArray(
    nsPIDOMWindowInner* aWindow,
    const mozilla::Array<RefPtr<nsMimeType>, 2>& aMimeTypes)
    : mWindow(aWindow), mMimeTypes(aMimeTypes) {}

nsMimeTypeArray::~nsMimeTypeArray() = default;

JSObject* nsMimeTypeArray::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return MimeTypeArray_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* nsMimeTypeArray::GetParentObject() const {
  MOZ_ASSERT(mWindow);
  return mWindow;
}

nsMimeType* nsMimeTypeArray::IndexedGetter(uint32_t aIndex, bool& aFound) {
  if (aIndex < EffectiveLength()) {
    aFound = true;
    return mMimeTypes[aIndex];
  }

  aFound = false;
  return nullptr;
}

nsMimeType* nsMimeTypeArray::NamedGetter(const nsAString& aName, bool& aFound) {
  uint32_t length = EffectiveLength();
  for (uint32_t i = 0; i < length; ++i) {
    if (mMimeTypes[i]->Name().Equals(aName)) {
      aFound = true;
      return mMimeTypes[i];
    }
  }

  aFound = false;
  return nullptr;
}

void nsMimeTypeArray::GetSupportedNames(nsTArray<nsString>& retval) {
  uint32_t length = EffectiveLength();
  for (uint32_t i = 0; i < length; ++i) {
    retval.AppendElement(mMimeTypes[i]->Name());
  }
}

uint32_t nsMimeTypeArray::Length() { return EffectiveLength(); }

uint32_t nsMimeTypeArray::EffectiveLength() {
  if (ForceNoPlugins()) {
    return 0;
  }
  return mMimeTypes.size();
}

bool nsMimeTypeArray::ForceNoPlugins() {
  return StaticPrefs::pdfjs_disabled() &&
         !nsContentUtils::ShouldResistFingerprinting(
             mWindow ? mWindow->GetDocShell() : nullptr, RFPTarget::PdfjsSpoof);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsMimeType, mPluginElement)

nsMimeType::nsMimeType(nsPluginElement* aPluginElement, const nsAString& aName)
    : mPluginElement(aPluginElement), mName(aName) {
  MOZ_ASSERT(aPluginElement);
}

nsMimeType::~nsMimeType() = default;

JSObject* nsMimeType::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return MimeType_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsPluginElement> nsMimeType::EnabledPlugin() const {
  return do_AddRef(mPluginElement);
}
