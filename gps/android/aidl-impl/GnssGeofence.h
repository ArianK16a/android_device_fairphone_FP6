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
#ifndef ANDROID_HARDWARE_GNSS_AIDL_GNSSGEOFENCE_H
#define ANDROID_HARDWARE_GNSS_AIDL_GNSSGEOFENCE_H
#include <aidl/android/hardware/gnss/IGnssGeofenceCallback.h>
#include <aidl/android/hardware/gnss/BnGnssGeofence.h>
#include <GeofenceAPIClient.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
using ::aidl::android::hardware::gnss::IGnssGeofenceCallback;
using ::aidl::android::hardware::gnss::BnGnssGeofence;
using ::std::shared_ptr;
using ::ndk::ScopedAStatus;
class GnssGeofence : public BnGnssGeofence {
public:
  GnssGeofence();
  virtual ~GnssGeofence();

    ScopedAStatus setCallback(const shared_ptr<IGnssGeofenceCallback>& callback) override;
    ScopedAStatus addGeofence(int32_t geofenceId, double latitudeDegrees, double longitudeDegrees,
            double radiusMeters, int32_t lastTransition, int32_t monitorTransitions,
            int32_t notificationResponsivenessMs, int32_t unknownTimerMs) override;
    ScopedAStatus pauseGeofence(int32_t geofenceId) override;
    ScopedAStatus resumeGeofence(int32_t geofenceId, int32_t monitorTransitions) override;
    ScopedAStatus removeGeofence(int32_t geofenceId) override;
    void removeAllGeofences();
private:
    shared_ptr<IGnssGeofenceCallback> mGnssGeofencingCbIface = nullptr;
    GeofenceAPIClient* mApi = nullptr;
    AIBinder_DeathRecipient *mDeathRecipient = nullptr;
    std::mutex mMutex;
};
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif //ANDROID_HARDWARE_GNSS_AIDL_GNSSGEOFENCE_H
