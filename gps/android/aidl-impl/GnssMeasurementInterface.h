/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Not a Contribution
 */
/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef ANDROID_HARDWARE_GNSS_AIDL_GNSSMEASUREMENTINTERFACE_H
#define ANDROID_HARDWARE_GNSS_AIDL_GNSSMEASUREMENTINTERFACE_H
#include <aidl/android/hardware/gnss/BnGnssMeasurementInterface.h>
#include <aidl/android/hardware/gnss/BnGnssMeasurementCallback.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <LocationAPIClientBase.h>
#include <gps_extended_c.h>

using aidl::android::hardware::gnss::ElapsedRealtime;
using aidl::android::hardware::gnss::GnssClock;
using aidl::android::hardware::gnss::GnssData;
using aidl::android::hardware::gnss::GnssMeasurement;
using aidl::android::hardware::gnss::GnssSignalType;
using aidl::android::hardware::gnss::GnssConstellationType;

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

using ::aidl::android::hardware::gnss::BnGnssMeasurementInterface;
using ::aidl::android::hardware::gnss::IGnssMeasurementCallback;
using ::aidl::android::hardware::gnss::IGnssMeasurementInterface;
using ::std::shared_ptr;
using ::ndk::ScopedAStatus;

struct GnssMeasurementInterface : public BnGnssMeasurementInterface, public LocationAPIClientBase {
public:
    GnssMeasurementInterface();
    ~GnssMeasurementInterface() {}
    ScopedAStatus setCallback(const shared_ptr<IGnssMeasurementCallback>& callback,
            bool enableFullTracking, bool enableCorrVecOutputs) override;
    ScopedAStatus close() override;

    ScopedAStatus setCallbackWithOptions(const shared_ptr<IGnssMeasurementCallback>& callback,
            const IGnssMeasurementInterface::Options& options) override;
    // callbacks we are interested in
    void onGnssMeasurementsCb(
            const GnssMeasurementsNotification &gnssMeasurementsNotification) final;

private:
    shared_ptr<IGnssMeasurementCallback> mGnssMeasurementCbIface = nullptr;
    // Synchronization lock for mGnssMeasurementCbIface
    mutable std::mutex mMutex;
    AIBinder_DeathRecipient* mDeathRecipient;
    bool mTracking;

    static void gnssMeasurementDied(void* cookie);
    void startTracking(GnssPowerMode powerMode = GNSS_POWER_MODE_DEFAULT,
                       uint32_t timeBetweenMeasurement = GPS_DEFAULT_FIX_INTERVAL_MS);
    void convertGnssData(const GnssMeasurementsNotification& in, GnssData& out);
    void convertGnssMeasurement(const GnssMeasurementsData& in, GnssMeasurement& out);
    void convertGnssFlags(const GnssMeasurementsData& in, GnssMeasurement& out);
    static void convertGnssSvId(const GnssMeasurementsData& in, int& out);
    void convertGnssSignalType(const GnssMeasurementsData& in, GnssSignalType& out);
    static void convertGnssConstellationType(const GnssSvType& in, GnssConstellationType& out);
    void convertGnssMeasurementsCodeType(const GnssMeasurementsCodeType& inCodeType,
                                         const char* inOtherCodeTypeName, GnssSignalType& out);
    void convertGnssState(const GnssMeasurementsData& in, GnssMeasurement& out);
    void convertGnssAccumulatedDeltaRangeState(const GnssMeasurementsData& in,
                                               GnssMeasurement& out);
    void convertGnssMultipathIndicator(const GnssMeasurementsData& in, GnssMeasurement& out);
    void convertGnssSatellitePvtFlags(const GnssMeasurementsData& in, GnssMeasurement& out);
    void convertGnssSatellitePvt(const GnssMeasurementsData& in, GnssMeasurement& out);
    void convertGnssClock(const GnssMeasurementsClock& in, GnssClock& out);
    void convertElapsedRealtimeNanos(const GnssMeasurementsNotification& in,
                                     ElapsedRealtime& elapsedRealtime);
    void printGnssData(GnssData& data);
};

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif //ANDROID_HARDWARE_GNSS_AIDL_GNSSMEASUREMENTINTERFACE_H
