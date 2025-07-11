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

#ifndef GEOFENCE_API_CLINET_H
#define GEOFENCE_API_CLINET_H

#include <mutex>
#include <LocationAPIClientBase.h>
#include <aidl/android/hardware/gnss/IGnssGeofenceCallback.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
using ::aidl::android::hardware::gnss::IGnssGeofenceCallback;
using ::std::shared_ptr;


class GeofenceAPIClient : public LocationAPIClientBase
{
public:
    GeofenceAPIClient(const shared_ptr<IGnssGeofenceCallback>& callback);
    void upcateCallback(const shared_ptr<IGnssGeofenceCallback>& callback);
    void geofenceAdd(uint32_t geofence_id, double latitude, double longitude,
            double radius_meters, int32_t last_transition, int32_t monitor_transitions,
            uint32_t notification_responsiveness_ms, uint32_t unknown_timer_ms);
    void geofencePause(uint32_t geofence_id);
    void geofenceResume(uint32_t geofence_id, int32_t monitor_transitions);
    void geofenceRemove(uint32_t geofence_id);
    void geofenceRemoveAll();

    // callbacks
    void onGeofenceBreachCb(const GeofenceBreachNotification& geofenceBreachNotification) final;
    void onGeofenceStatusCb(const GeofenceStatusNotification& geofenceStatusNotification) final;
    void onAddGeofencesCb(size_t count, LocationError* errors, uint32_t* ids) final;
    void onRemoveGeofencesCb(size_t count, LocationError* errors, uint32_t* ids) final;
    void onPauseGeofencesCb(size_t count, LocationError* errors, uint32_t* ids) final;
    void onResumeGeofencesCb(size_t count, LocationError* errors, uint32_t* ids) final;

private:
    virtual ~GeofenceAPIClient() = default;
    std::mutex mMutex;
    shared_ptr<IGnssGeofenceCallback> mGnssGeofencingCbIface;
};

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif // GEOFENCE_API_CLINET_H
