/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Connection.h"

#include "ConnectionMainThread.h"
#include "ConnectionWorker.h"
#include "Constants.h"
#include "FerifoxConfig.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsString.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define CHANGE_EVENT_NAME u"typechange"_ns

namespace mozilla::dom::network {

static Maybe<ConnectionType> ConfiguredConnectionType() {
  auto* cfg = FerifoxConfig::GetSingleton();
  if (!cfg) {
    return Nothing();
  }

  nsAutoString type;
  if (!cfg->GetString("navigator.connection.type"_ns, type)) {
    return Nothing();
  }

  if (type.EqualsLiteral("cellular")) {
    return Some(ConnectionType::Cellular);
  }
  if (type.EqualsLiteral("bluetooth")) {
    return Some(ConnectionType::Bluetooth);
  }
  if (type.EqualsLiteral("ethernet")) {
    return Some(ConnectionType::Ethernet);
  }
  if (type.EqualsLiteral("wifi")) {
    return Some(ConnectionType::Wifi);
  }
  if (type.EqualsLiteral("other")) {
    return Some(ConnectionType::Other);
  }
  if (type.EqualsLiteral("none")) {
    return Some(ConnectionType::None);
  }
  if (type.EqualsLiteral("unknown")) {
    return Some(ConnectionType::Unknown);
  }

  return Nothing();
}

// Don't use |Connection| alone, since that confuses nsTraceRefcnt since
// we're not the only class with that name.
NS_IMPL_ISUPPORTS_INHERITED0(dom::network::Connection, DOMEventTargetHelper)

Connection::Connection(nsPIDOMWindowInner* aWindow,
                       bool aShouldResistFingerprinting)
    : DOMEventTargetHelper(aWindow),
      mShouldResistFingerprinting(aShouldResistFingerprinting),
      mType(static_cast<ConnectionType>(kDefaultType)),
      mIsWifi(kDefaultIsWifi),
      mDHCPGateway(kDefaultDHCPGateway),
      mBeenShutDown(false) {}

Connection::~Connection() {
  NS_ASSERT_OWNINGTHREAD(Connection);
  MOZ_ASSERT(mBeenShutDown);
}

void Connection::Shutdown() {
  NS_ASSERT_OWNINGTHREAD(Connection);

  if (mBeenShutDown) {
    return;
  }

  mBeenShutDown = true;
  ShutdownInternal();
}

JSObject* Connection::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return NetworkInformation_Binding::Wrap(aCx, this, aGivenProto);
}

ConnectionType Connection::Type() const {
  if (auto type = ConfiguredConnectionType()) {
    return *type;
  }

  return mShouldResistFingerprinting
             ? static_cast<ConnectionType>(ConnectionType::Unknown)
             : mType;
}

void Connection::Update(ConnectionType aType, bool aIsWifi,
                        uint32_t aDHCPGateway, bool aNotify) {
  NS_ASSERT_OWNINGTHREAD(Connection);

  ConnectionType previousType = Type();

  mType = aType;
  mIsWifi = aIsWifi;
  mDHCPGateway = aDHCPGateway;

  if (aNotify && previousType != Type()) {
    DispatchTrustedEvent(CHANGE_EVENT_NAME);
  }
}

/* static */
Connection* Connection::CreateForWindow(nsPIDOMWindowInner* aWindow,
                                        bool aShouldResistFingerprinting) {
  MOZ_ASSERT(aWindow);
  return new ConnectionMainThread(aWindow, aShouldResistFingerprinting);
}

/* static */
already_AddRefed<Connection> Connection::CreateForWorker(
    WorkerPrivate* aWorkerPrivate, ErrorResult& aRv) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  return ConnectionWorker::Create(aWorkerPrivate, aRv);
}

}  // namespace mozilla::dom::network
