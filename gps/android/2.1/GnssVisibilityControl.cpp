/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <android/hardware/gnss/visibility_control/1.0/IGnssVisibilityControl.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include "GnssVisibilityControl.h"
#include <location_interface.h>

namespace android {
namespace hardware {
namespace gnss {
namespace visibility_control {
namespace V1_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

static void convertGnssNfwNotification(GnssNfwNotification& in,
    IGnssVisibilityControlCallback::NfwNotification& out);

GnssVisibilityControl::GnssVisibilityControl(Gnss* gnss) : mGnss(gnss) {
    LocationControlCallbacks locCtrlCbs;
    memset(&locCtrlCbs, 0, sizeof(locCtrlCbs));
    locCtrlCbs.size = sizeof(LocationControlCallbacks);

    locCtrlCbs.nfwStatusCb = [this](const GnssNfwNotification& notification) {
        nfwStatusCb(notification);
    };

    locCtrlCbs.isInEmergencyStatusCb = [this] () {
        return isInEmergencySession();
    };

    mGnss->getLocationControlApi()->updateCallbacks(locCtrlCbs);

}
GnssVisibilityControl::~GnssVisibilityControl() {
}

static void convertGnssNfwNotification(GnssNfwNotification& in,
    IGnssVisibilityControlCallback::NfwNotification& out)
{
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

void GnssVisibilityControl::nfwStatusCb(const GnssNfwNotification& notification) {
    std::unique_lock<std::mutex> lock(mMutex);
    auto gnssVisibilityControlCbIface(mGnssVisibilityControlCbIface);
    lock.unlock();

    if (gnssVisibilityControlCbIface != nullptr) {
        IGnssVisibilityControlCallback::NfwNotification nfwNotification;

        // Convert from one structure to another
        convertGnssNfwNotification(notification, nfwNotification);

        auto r = gnssVisibilityControlCbIface->nfwNotifyCb(nfwNotification);
        if (!r.isOk()) {
            LOC_LOGw("Error invoking NFW status cb %s", r.description().c_str());
        }
    } else {
        LOC_LOGw("setCallback has not been called yet");
    }
}

bool GnssVisibilityControl::isInEmergencySession() {
    std::unique_lock<std::mutex> lock(mMutex);
    auto gnssVisibilityControlCbIface(mGnssVisibilityControlCbIface);
    lock.unlock();

    if (gnssVisibilityControlCbIface != nullptr) {
        auto r = gnssVisibilityControlCbIface->isInEmergencySession();
        if (!r.isOk()) {
            LOC_LOGw("Error invoking NFW status cb %s", r.description().c_str());
            return false;
        } else {
            return (r);
        }
    } else {
        LOC_LOGw("setCallback has not been called yet");
        return false;
    }
}

// Methods from ::android::hardware::gnss::visibility_control::V1_0::IGnssVisibilityControl follow.
Return<bool> GnssVisibilityControl::enableNfwLocationAccess(const hidl_vec<::android::hardware::hidl_string>& proxyApps) {

    if (nullptr == mGnss || nullptr == mGnss->getLocationControlApi()) {
        LOC_LOGe("Null GNSS interface");
        return false;
    }

    std::vector<std::string> apps;
    for (auto i = 0; i < proxyApps.size(); i++) {
        apps.push_back((std::string)proxyApps[i]);
    }

    mGnss->getLocationControlApi()->enableNfwLocationAccess(apps);
    return true;
}
/**
 * Registers the callback for HAL implementation to use.
 *
 * @param callback Handle to IGnssVisibilityControlCallback interface.
 */
Return<bool> GnssVisibilityControl::setCallback(const ::android::sp<::android::hardware::gnss::visibility_control::V1_0::IGnssVisibilityControlCallback>& callback) {

    std::unique_lock<std::mutex> lock(mMutex);
    mGnssVisibilityControlCbIface = callback;
    lock.unlock();
    return true;
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace visibility_control
}  // namespace gnss
}  // namespace hardware
}  // namespace android
