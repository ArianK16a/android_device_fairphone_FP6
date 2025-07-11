/*
Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <aidl/android/hardware/gnss/visibility_control/BnGnssVisibilityControl.h>
#include "Gnss.h"
#include "GnssVisibilityControl.h"

namespace android {
namespace hardware {
namespace gnss {
namespace visibility_control {
namespace aidl {
namespace implementation {
static void convertGnssNfwNotification(const GnssNfwNotification& in,
    IGnssVisibilityControlCallback::NfwNotification& out);

void gnssVisibilityControlServiceDied(void* cookie) {
    LOC_LOGe("IGnssVisibilityControl AIDL service died");
    GnssVisibilityControl* iface = static_cast<GnssVisibilityControl*>(cookie);
    if (iface != nullptr) {
        iface->setCallback(nullptr);
        iface = nullptr;
    }
}

GnssVisibilityControl::GnssVisibilityControl(Gnss* gnss) : mGnss(gnss),
    mDeathRecipient(AIBinder_DeathRecipient_new(&gnssVisibilityControlServiceDied)) {
    LocationControlCallbacks locCtrlCbs;
    memset(&locCtrlCbs, 0, sizeof(locCtrlCbs));
    locCtrlCbs.size = sizeof(LocationControlCallbacks);

    locCtrlCbs.nfwStatusCb = [this](const GnssNfwNotification& notification) {
        statusCb(notification);
    };

    locCtrlCbs.isInEmergencyStatusCb = [this] () {
        return isE911Session();
    };

    mGnss->getLocationControlApi()->updateCallbacks(locCtrlCbs);
}

ScopedAStatus GnssVisibilityControl::enableNfwLocationAccess(
        const std::vector<std::string>& proxyApps) {
    if (nullptr == mGnss || nullptr == mGnss->getLocationControlApi()) {
        LOC_LOGe("Null GNSS interface");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    std::vector<std::string> apps;
    for (auto i = 0; i < proxyApps.size(); i++) {
        apps.push_back((std::string)proxyApps[i]);
    }

    mGnss->getLocationControlApi()->enableNfwLocationAccess(apps);
        return ScopedAStatus::ok();
}
static void convertGnssNfwNotification(const GnssNfwNotification& in,
    IGnssVisibilityControlCallback::NfwNotification& out) {
    memset(&out, 0, sizeof(IGnssVisibilityControlCallback::NfwNotification));
    out.proxyAppPackageName = in.proxyAppPackageName;
    out.protocolStack = (IGnssVisibilityControlCallback::NfwProtocolStack)in.protocolStack;
    out.otherProtocolStackName = in.otherProtocolStackName;
    out.requestor = (IGnssVisibilityControlCallback::NfwRequestor)in.requestor;
    out.requestorId = in.requestorId;
    out.responseType = (IGnssVisibilityControlCallback::NfwResponseType)in.responseType;
    out.inEmergencyMode = in.inEmergencyMode;
    out.isCachedLocation = in.isCachedLocation;
}
void GnssVisibilityControl::statusCb(const GnssNfwNotification& notification) {
    std::unique_lock<std::mutex> lock(mMutex);
    auto gnssVisibilityControlCbIface(mGnssVisibilityControlCbIface);
    lock.unlock();
    if (gnssVisibilityControlCbIface != nullptr) {
        IGnssVisibilityControlCallback::NfwNotification nfwNotification;

        // Convert from one structure to another
        convertGnssNfwNotification(notification, nfwNotification);

        auto r = gnssVisibilityControlCbIface->nfwNotifyCb(nfwNotification);
        if (!r.isOk()) {
            LOC_LOGw("Error invoking NFW status cb");
        }
    } else {
        LOC_LOGw("setCallback has not been called yet");
    }
}

bool GnssVisibilityControl::isE911Session() {
    std::unique_lock<std::mutex> lock(mMutex);
    auto gnssVisibilityControlCbIface(mGnssVisibilityControlCbIface);
    lock.unlock();
    if (gnssVisibilityControlCbIface != nullptr) {
        bool res = false;
        auto r = gnssVisibilityControlCbIface->isInEmergencySession(&res);
        if (!r.isOk()) {
            LOC_LOGw("Error invoking NFW status cb");
            return false;
        } else {
            return (res);
        }
    } else {
        LOC_LOGw("setCallback has not been called yet");
        return false;
    }
}
/**
 * Registers the callback for HAL implementation to use.
 *
 * @param callback Handle to IGnssVisibilityControlCallback interface.
 */
ScopedAStatus GnssVisibilityControl::setCallback(
        const shared_ptr<IGnssVisibilityControlCallback>& callback) {
    if (nullptr == mGnss || nullptr == mGnss->getLocationControlApi()) {
        LOC_LOGe("Null GNSS interface");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    std::unique_lock<std::mutex> lock(mMutex);
    if (mGnssVisibilityControlCbIface != nullptr) {
        AIBinder_unlinkToDeath(mGnssVisibilityControlCbIface->asBinder().get(), mDeathRecipient,
                this);
    }
    mGnssVisibilityControlCbIface = callback;
    if (mGnssVisibilityControlCbIface != nullptr) {
        AIBinder_linkToDeath(mGnssVisibilityControlCbIface->asBinder().get(), mDeathRecipient,
                this);
    }

    return ScopedAStatus::ok();
}
}  // namespace implementation
}  // namespace aidl
}  // namespace visibility_control
}  // namespace gnss
}  // namespace hardware
}  // namespace android
