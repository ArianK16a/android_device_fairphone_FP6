/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Not a Contribution
 */
/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2_0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2_0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
Changes from Qualcomm Innovation Center are provided under the following license:

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

#ifndef ANDROID_HARDWARE_GNSS_AIDL_GNSS_H
#define ANDROID_HARDWARE_GNSS_AIDL_GNSS_H

#include "location_api/GnssAPIClient.h"
#include <android/binder_auto_utils.h>
#include <aidl/android/hardware/gnss/BnGnss.h>
namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

using ::aidl::android::hardware::gnss::GnssConstellationType;
using ::aidl::android::hardware::gnss::BnGnss;
using ::aidl::android::hardware::gnss::IGnssCallback;
using ::aidl::android::hardware::gnss::IGnssPowerIndication;
using ::aidl::android::hardware::gnss::IGnssMeasurementInterface;
using ::std::shared_ptr;
using ::ndk::ScopedAStatus;
using ::aidl::android::hardware::gnss::GnssLocation;
using ::aidl::android::hardware::gnss::IGnssPsds;
using ::aidl::android::hardware::gnss::IGnssConfiguration;
using ::aidl::android::hardware::gnss::IGnssBatching;
using ::aidl::android::hardware::gnss::IGnssGeofence;
using ::aidl::android::hardware::gnss::IGnssNavigationMessageInterface;
using ::aidl::android::hardware::gnss::IAGnss;
using ::aidl::android::hardware::gnss::IAGnssRil;
using ::aidl::android::hardware::gnss::IGnssDebug;
using ::aidl::android::hardware::gnss::IGnssAntennaInfo;
using ::aidl::android::hardware::gnss::visibility_control::IGnssVisibilityControl;
using ::aidl::android::hardware::gnss::measurement_corrections::IMeasurementCorrectionsInterface;
struct Gnss : public BnGnss {
    Gnss();
    ~Gnss();

    ScopedAStatus setCallback(const shared_ptr<IGnssCallback>& callback) override;
    ScopedAStatus close() override;
    ScopedAStatus getExtensionPsds(shared_ptr<IGnssPsds>* _aidl_return) {
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }
    ScopedAStatus getExtensionGnssConfiguration(
            shared_ptr<IGnssConfiguration>* _aidl_return) override;
    ScopedAStatus getExtensionGnssPowerIndication(
            shared_ptr<IGnssPowerIndication>* _aidl_return) override;
    ScopedAStatus getExtensionGnssMeasurement(
            shared_ptr<IGnssMeasurementInterface>* _aidl_return) override;

    ScopedAStatus getExtensionGnssBatching(shared_ptr<IGnssBatching>* _aidl_return) override;
    ScopedAStatus getExtensionGnssGeofence(shared_ptr<IGnssGeofence>* _aidl_return) override;
    ScopedAStatus getExtensionGnssNavigationMessage(
            shared_ptr<IGnssNavigationMessageInterface>* _aidl_return) {
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }
    ScopedAStatus getExtensionAGnss(shared_ptr<IAGnss>* _aidl_return) override;
    ScopedAStatus getExtensionAGnssRil(shared_ptr<IAGnssRil>* _aidl_return) override;
    ScopedAStatus getExtensionGnssDebug(shared_ptr<IGnssDebug>* _aidl_return) override;
    ScopedAStatus getExtensionGnssVisibilityControl(
            shared_ptr<IGnssVisibilityControl>* _aidl_return) override;
    ScopedAStatus start() override;
    ScopedAStatus stop() override;
    ScopedAStatus injectTime(int64_t timeMs, int64_t timeReferenceMs,
            int32_t uncertaintyMs) override;
    ScopedAStatus injectLocation(const GnssLocation& location) override;
    ScopedAStatus injectBestLocation(const GnssLocation& gnssLocation) override;
    ScopedAStatus deleteAidingData(IGnss::GnssAidingData aidingDataFlags) override;
    ScopedAStatus setPositionMode(const IGnss::PositionModeOptions& options) override;
    ScopedAStatus getExtensionGnssAntennaInfo(shared_ptr<IGnssAntennaInfo>* _aidl_return) override;
    ScopedAStatus getExtensionMeasurementCorrections(
            shared_ptr<IMeasurementCorrectionsInterface>* _aidl_return) override;
    ScopedAStatus startSvStatus() override;
    ScopedAStatus stopSvStatus() override;
    ScopedAStatus startNmea() override;
    ScopedAStatus stopNmea() override;

    // These methods are not part of the IGnss base class.
    inline GnssAPIClient& getApi() { return mApi; }
    ScopedAStatus updateConfiguration(GnssConfig& gnssConfig);
    ILocationControlAPI* getLocationControlApi();
    void handleAidlClientSsr();

    // ILocationControlAPI callbacks
    void onCtrlResponseCb(LocationError error, uint32_t id) {}
    void onCtrlCollectiveResponseCb(size_t count, LocationError* errors, uint32_t* ids) {}
    // Callback for ODCPI request
    void odcpiRequestCb(const OdcpiRequestInfo& request);
    inline bool isGnssCbValid() {return mGnssCallback != nullptr; };
private:
    GnssAPIClient mApi;
    shared_ptr<IGnssConfiguration> mGnssConfiguration = nullptr;
    shared_ptr<IGnssPowerIndication> mGnssPowerIndication = nullptr;
    shared_ptr<IGnssMeasurementInterface> mGnssMeasurementInterface = nullptr;
    shared_ptr<IGnssBatching> mGnssBatching = nullptr;
    shared_ptr<IGnssGeofence> mGnssGeofence = nullptr;
    shared_ptr<IAGnss> mAGnss = nullptr;
    shared_ptr<IAGnssRil> mAGnssRil = nullptr;
    shared_ptr<IGnssDebug> mGnssDebug = nullptr;
    shared_ptr<IGnssVisibilityControl> mGnssVisibCtrl = nullptr;
    shared_ptr<IGnssAntennaInfo> mGnssAntennaInfo = nullptr;
    shared_ptr<IMeasurementCorrectionsInterface> mGnssMeasCorr = nullptr;

    shared_ptr<IGnssCallback> mGnssCallback = nullptr;
    ILocationControlAPI* mLocationControlApi = nullptr;
    AIBinder_DeathRecipient *mDeathRecipient = nullptr;
    std::mutex mMutex;
};

extern "C" void registerGnssStatusCallback(std::function<void(bool)> gnssCb,
        std::function<void(bool)> nlpCb);
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_GNSS_AIDL_GNSS_H
