/*
Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

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
#include <aidl/android/hardware/gnss/IGnssGeofenceCallback.h>
#include "GnssGeofence.h"

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
void gnssGeofenceDied(void* cookie) {
    LOC_LOGe("IGnssGeofence AIDL service died");
    GnssGeofence* iface = static_cast<GnssGeofence*>(cookie);
    if (iface != nullptr) {
        iface->removeAllGeofences();
        iface->setCallback(nullptr);
        iface = nullptr;
    }
}
GnssGeofence::GnssGeofence(): mApi(nullptr),
    mDeathRecipient(AIBinder_DeathRecipient_new(&gnssGeofenceDied)) {}
GnssGeofence::~GnssGeofence() {}

ScopedAStatus GnssGeofence::setCallback(const shared_ptr<IGnssGeofenceCallback>& callback) {
    if (mApi != nullptr) {
        mApi->upcateCallback(callback);
    } else {
        mApi = new GeofenceAPIClient(callback);
    }
    if (mApi == nullptr) {
        LOC_LOGe("]: failed to create mApi");
    }
    mMutex.lock();
    if (mGnssGeofencingCbIface != nullptr) {
        AIBinder_unlinkToDeath(mGnssGeofencingCbIface->asBinder().get(), mDeathRecipient, this);
    }
    mGnssGeofencingCbIface = callback;
    if (mGnssGeofencingCbIface != nullptr) {
        AIBinder_linkToDeath(mGnssGeofencingCbIface->asBinder().get(), mDeathRecipient, this);
    }
    mMutex.unlock();
    return ScopedAStatus::ok();
}

ScopedAStatus GnssGeofence::addGeofence(int32_t geofenceId, double latitudeDegrees,
        double longitudeDegrees, double radiusMeters, int32_t lastTransition,
        int32_t monitorTransitions, int32_t notificationResponsivenessMs, int32_t unknownTimerMs) {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr");
    } else {
        mApi->geofenceAdd(
                geofenceId,
                latitudeDegrees,
                longitudeDegrees,
                radiusMeters,
                static_cast<int32_t>(lastTransition),
                monitorTransitions,
                notificationResponsivenessMs,
                unknownTimerMs);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus GnssGeofence::pauseGeofence(int32_t geofenceId) {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr");
    } else {
        mApi->geofencePause(geofenceId);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus GnssGeofence::resumeGeofence(int32_t geofenceId, int32_t monitorTransitions) {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr");
    } else {
        mApi->geofenceResume(geofenceId, monitorTransitions);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus GnssGeofence::removeGeofence(int32_t geofenceId) {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr");
    } else {
        mApi->geofenceRemove(geofenceId);
    }
    return ScopedAStatus::ok();
}

void GnssGeofence::removeAllGeofences()  {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr, do nothing");
    } else {
        mApi->geofenceRemoveAll();
    }
}
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
