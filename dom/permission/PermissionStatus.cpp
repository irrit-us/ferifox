/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PermissionStatus.h"

#include "PermissionStatusSink.h"
#include "PermissionUtils.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/FerifoxConfig.h"
#include "mozilla/Permission.h"
#include "mozilla/Services.h"
#include "nsGlobalWindowInner.h"
#include "nsIPermissionManager.h"

namespace mozilla::dom {

namespace {

Maybe<PermissionState> StringToPermissionState(const nsAString& aValue) {
  if (aValue.EqualsLiteral("granted")) {
    return Some(PermissionState::Granted);
  }
  if (aValue.EqualsLiteral("denied")) {
    return Some(PermissionState::Denied);
  }
  if (aValue.EqualsLiteral("prompt")) {
    return Some(PermissionState::Prompt);
  }
  return Nothing();
}

Maybe<PermissionState> GetFerifoxPermissionState(PermissionName aName) {
  auto* cfg = FerifoxConfig::GetSingleton();

  nsAutoCString path("permissions."_ns);
  path.Append(GetEnumString(aName));

  nsAutoString value;
  if (!cfg->GetString(path, value)) {
    return Nothing();
  }
  return StringToPermissionState(value);
}

}  // namespace

PermissionStatus::PermissionStatus(nsIGlobalObject* aGlobal,
                                   PermissionName aName)
    : DOMEventTargetHelper(aGlobal), mName(aName) {
  KeepAliveIfHasListenersFor(nsGkAtoms::onchange);
}

// https://w3c.github.io/permissions/#onchange-attribute and
// https://w3c.github.io/permissions/#query-method
RefPtr<PermissionStatus::SimplePromise> PermissionStatus::Init() {
  mSink = CreateSink();
  MOZ_ASSERT(mSink);

  return mSink->Init()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr(this)](
          const PermissionStatusSink::InternalPermissionStatesPromise::
              ResolveOrRejectValue& aResult) {
        if (aResult.IsResolve()) {
          PermissionStatusSink::InternalPermissionStates states =
              aResult.ResolveValue();
          self->mState = self->ComputeStateFromAction(states.mBrowser);
          self->mSystemState = states.mSystem;
          self->ApplyFerifoxState();
          return SimplePromise::CreateAndResolve(NS_OK, __func__);
        }

        return SimplePromise::CreateAndReject(aResult.RejectValue(), __func__);
      });
}

PermissionStatus::~PermissionStatus() {
  if (mSink) {
    mSink->Disentangle();
    mSink = nullptr;
  }
}

JSObject* PermissionStatus::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return PermissionStatus_Binding::Wrap(aCx, this, aGivenProto);
}

PermissionState PermissionStatus::State() const {
  if (mFerifoxState) {
    return *mFerifoxState;
  }
  if (mState == PermissionState::Granted &&
      mSystemState != PermissionState::Granted) {
    return mSystemState;
  }
  return mState;
}

nsLiteralCString PermissionStatus::GetPermissionType() const {
  return PermissionNameToType(mName);
}

// https://w3c.github.io/permissions/#dfn-permissionstatus-update-steps
void PermissionStatus::PermissionChanged(uint32_t aAction) {
  const PermissionState oldEffective = State();
  mState = ComputeStateFromAction(aAction);
  const PermissionState newEffective = State();

  if (oldEffective == newEffective) {
    return;
  }

  // Step 4: Queue a task on the permissions task source to fire an
  // event named change at status.
  RefPtr<AsyncEventDispatcher> eventDispatcher =
      new AsyncEventDispatcher(this, u"change"_ns, CanBubble::eNo);
  eventDispatcher->PostDOMEvent();
}

void PermissionStatus::SystemPermissionChanged(
    PermissionState aNewSystemState) {
  const PermissionState oldEffective = State();
  mSystemState = aNewSystemState;
  const PermissionState newEffective = State();

  if (oldEffective == newEffective) {
    return;
  }

  RefPtr<AsyncEventDispatcher> eventDispatcher =
      new AsyncEventDispatcher(this, u"change"_ns, CanBubble::eNo);
  eventDispatcher->PostDOMEvent();
}

void PermissionStatus::DisconnectFromOwner() {
  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onchange);

  if (mSink) {
    mSink->Disentangle();
    mSink = nullptr;
  }

  DOMEventTargetHelper::DisconnectFromOwner();
}

void PermissionStatus::GetType(nsACString& aName) const {
  aName.Assign(GetPermissionType());
}

already_AddRefed<PermissionStatusSink> PermissionStatus::CreateSink() {
  RefPtr<PermissionStatusSink> sink =
      new PermissionStatusSink(this, mName, GetPermissionType());
  return sink.forget();
}

void PermissionStatus::ApplyFerifoxState() {
  mFerifoxState = GetFerifoxPermissionState(mName);
}

PermissionState PermissionStatus::ComputeStateFromAction(uint32_t aAction) {
  nsCOMPtr<nsIGlobalObject> global = GetRelevantGlobal();
  if (NS_WARN_IF(!global)) {
    return PermissionState::Denied;
  }

  return ActionToPermissionState(aAction, mName, global);
}

}  // namespace mozilla::dom
