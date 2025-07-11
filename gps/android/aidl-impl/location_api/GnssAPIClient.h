/* Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
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
 *     * Neither the name of The Linux Foundation, nor the names of its
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
 *
 */
/*
Changes from Qualcomm Innovation Center are provided under the following license:

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

#ifndef GNSS_API_CLINET_H
#define GNSS_API_CLINET_H

#include <mutex>
#include <LocationAPIClientBase.h>
#include <aidl/android/hardware/gnss/IGnssCallback.h>
#include <aidl/android/hardware/gnss/BnGnss.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

using ::std::shared_ptr;
using ::aidl::android::hardware::gnss::IGnssCallback;
using ::aidl::android::hardware::gnss::IGnss;

class GnssAPIClient : public LocationAPIClientBase {
public:
    GnssAPIClient(const shared_ptr<IGnssCallback>& gpsCb);
    GnssAPIClient(const GnssAPIClient&) = delete;
    virtual ~GnssAPIClient();
    GnssAPIClient& operator=(const GnssAPIClient&) = delete;

    // for GpsInterface
    void gnssUpdateCallbacks(const shared_ptr<IGnssCallback>& gpsCb);
    bool gnssStart();
    bool gnssStop();
    void configSvStatus(bool enable);
    void configNmea(bool enable);

    void requestCapabilities();
    bool gnssSetPositionMode(IGnss::GnssPositionMode mode,
            IGnss::GnssPositionRecurrence recurrence,
            uint32_t minIntervalMs,
            uint32_t preferredAccuracyMeters,
            uint32_t preferredTimeMs,
            GnssPowerMode powerMode = GNSS_POWER_MODE_DEFAULT,
            uint32_t timeBetweenMeasurement = 0);

    // these apis using LocationAPIControlClient
    void gnssConfigurationUpdate(const GnssConfig& gnssConfig);
    void gnssEnable(LocationTechnologyType techType);
    void gnssDisable();
    void gnssDeleteAidingData(IGnss::GnssAidingData aidingDataFlags);

    // callbacks we are interested in
    void onCapabilitiesCb(LocationCapabilitiesMask capabilitiesMask) final;
    void onTrackingCb(const Location& location) final;
    void onGnssSvCb(const GnssSvNotification& gnssSvNotification) final;
    void onGnssNmeaCb(const GnssNmeaNotification& gnssNmeaNotification) final;
    void onEngineLocationsInfoCb(uint32_t count,
            GnssLocationInfoNotification* engineLocationInfoNotification);
    void onGnssSignalTypesCb(const GnssCapabNotification& gnssCapabNotification);

    void onStartTrackingCb(LocationError error) final;
    void onStopTrackingCb(LocationError error) final;

    // sNlpRequestCb and gnssStatusCb will be NULL in case of Android OS
    static std::function<void(bool)> sNlpRequestCb;
    static std::function<void(bool)> sGnssStatusCb;
    static bool sFlpRequestAllowed;

private:
    void setCallbacks();
    void setFlpCallbacks();
    void initLocationOptions();
    void updateCapabilities(LocationCapabilitiesMask capabilitiesMask,
                            bool forceSendCapabilities);
    void updateCallbacksByAccuracy(uint32_t preferredAccuracyMeters);

    std::mutex mMutex;
    bool mTracking;
    TrackingOptions mTrackingOptions;
    LocationAPIControlClient* mControlClient;
    LocationCapabilitiesMask mLocationCapabilitiesMask;
    bool mLocationCapabilitiesCached;
    bool mSvStatusEnabled;
    bool mNmeaEnabled;
    bool mIsNlpActive;
    const shared_ptr<IGnssCallback>& mGnssCbIface;
};

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif // GNSS_API_CLINET_H
