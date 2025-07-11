/*
* Copyright (c) 2021 The Linux Foundation. All rights reserved.

* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of The Linux Foundation nor the names of its
*      contributors may be used to endorse or promote products derived
*      from this software without specific prior written permission.
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
#ifndef GNSSAUTOPOWERHANDLER_H
#define GNSSAUTOPOWERHANDLER_H

#include <utils/StrongPointer.h>
#include <utils/RefBase.h>
#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include <unordered_map>
#include <dlfcn.h>
#include <ILocationAPI.h>


using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_death_recipient;
using ::android::sp;
using ::android::wp;
using ::android::hidl::base::V1_0::IBase;
using ::android::hardware::automotive::vehicle::V2_0::IVehicleCallback;
using ::android::hardware::hidl_vec;
using ::android::hardware::automotive::vehicle::V2_0::VehiclePropValue;
using ::android::hardware::automotive::vehicle::V2_0::StatusCode;
using ::android::hardware::automotive::vehicle::V2_0::IVehicle;
using ::android::hardware::automotive::vehicle::V2_0::SubscribeFlags;
using ::android::hardware::automotive::vehicle::V2_0::SubscribeOptions;
using ::android::hardware::automotive::vehicle::V2_0::VehicleApPowerStateReport;
using ::android::hardware::automotive::vehicle::V2_0::VehiclePropConfig;
using ::android::hardware::automotive::vehicle::V2_0::VehicleProperty;
using ::android::hardware::automotive::vehicle::V2_0::VehiclePropValue;

class GnssAutoPowerHandler : public IVehicleCallback {

public:
    GnssAutoPowerHandler();
    ~GnssAutoPowerHandler();

    /* Overrides */
    Return<void> onPropertyEvent(const hidl_vec<VehiclePropValue>& propValues) override;
    Return<void> onPropertySet(const VehiclePropValue& propValue) override;
    Return<void> onPropertySetError(StatusCode errorCode, int32_t propId,
                                        int32_t areaId) override;

    void handleVHidlDeath(uint64_t cookie, const wp<IBase>& who);

    static GnssAutoPowerHandler* getGnssAutoPowerHandler();
    void sendPowerEventToLocCtrlApi(PowerStateType intPowerState);
    bool connectToVhal();
    void initializeGnssAutoPower();
    void handleGnssAutoPowerEvent(int32_t powerState);
    bool subscribeToVhal();
    bool isPropertySupported(int32_t prop);
private:
    struct VHidlDeathRecipient : public hidl_death_recipient {
        public:
            explicit VHidlDeathRecipient(const android::sp<GnssAutoPowerHandler>& handler)
                : mHandler(handler) {
            }
            ~VHidlDeathRecipient() = default;
            void serviceDied(uint64_t cookie,
                               const wp<IBase>& who) override;
        private:
            sp<GnssAutoPowerHandler> mHandler;
    }; //VHidlDeathRecipient

    android::Mutex mMutex;
    std::unordered_map<int32_t, bool> mSupportedProperties;
    sp<VHidlDeathRecipient> mVHidlDeathRecipient;
    sp<IVehicle> mVhalService GUARDED_BY(mMutex);
    ILocationControlAPI    *mLocationControlApi;

}; //class PowerStateHandler

#endif //GNSSAUTOPOWERHANDLER_H
