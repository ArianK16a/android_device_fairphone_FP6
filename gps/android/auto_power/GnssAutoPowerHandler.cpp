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
#define LOG_TAG "LocSvc_GnssAutoPowerHandler"

#include <log_util.h>
#include <gps_extended_c.h>
#include <utils/Mutex.h>
#include "GnssAutoPowerHandler.h"
#include "android-base/macros.h"
#include <VehicleUtils.h>
#include <chrono>
#include <thread>

using android::Mutex;

// Power state report
constexpr int32_t POWER_STATE_WAIT_FOR_VHAL =
    static_cast<int32_t>(VehicleApPowerStateReport::WAIT_FOR_VHAL);
constexpr int32_t POWER_STATE_DEEP_SLEEP_ENTRY =
    static_cast<int32_t>(VehicleApPowerStateReport::DEEP_SLEEP_ENTRY);
constexpr int32_t POWER_STATE_DEEP_SLEEP_EXIT =
    static_cast<int32_t>(VehicleApPowerStateReport::DEEP_SLEEP_EXIT);
constexpr int32_t POWER_STATE_SHUTDOWN_POSTPONE =
    static_cast<int32_t>(VehicleApPowerStateReport::SHUTDOWN_POSTPONE);
constexpr int32_t POWER_STATE_SHUTDOWN_START =
    static_cast<int32_t>(VehicleApPowerStateReport::SHUTDOWN_START);
constexpr int32_t POWER_STATE_ON =
    static_cast<int32_t>(VehicleApPowerStateReport::ON);
constexpr int32_t POWER_STATE_SHUTDOWN_PREPARE =
    static_cast<int32_t>(VehicleApPowerStateReport::SHUTDOWN_PREPARE);
constexpr int32_t POWER_STATE_SHUTDOWN_CANCELLED =
    static_cast<int32_t>(VehicleApPowerStateReport::SHUTDOWN_CANCELLED);

static GnssAutoPowerHandler* sGnssAutoPowerHandler = nullptr;

/*VHidlDeathRecipient Implementation*/
void GnssAutoPowerHandler::VHidlDeathRecipient::serviceDied(uint64_t cookie, const wp<IBase>& who) {
   mHandler->handleVHidlDeath(cookie, who);
}

void GnssAutoPowerHandler::sendPowerEventToLocCtrlApi(PowerStateType intPowerState) {

    if (NULL == mLocationControlApi)
        mLocationControlApi = LocationControlAPI::getInstance();

    if (NULL != mLocationControlApi)
        mLocationControlApi->powerStateEvent(intPowerState);
}

void GnssAutoPowerHandler::initializeGnssAutoPower() {
    bool retVal = false;

    do {
        retVal = connectToVhal();

        if (true != retVal) {
            LOC_LOGw("Could not connect to VHAL");
        }

        //Sleep for 10ms for VHAL service to come-up before trying again.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while (false == retVal);
}

GnssAutoPowerHandler::GnssAutoPowerHandler():
        mVHidlDeathRecipient(new VHidlDeathRecipient(this)),
        mLocationControlApi(LocationControlAPI::getInstance()) {

    sGnssAutoPowerHandler = this;

    std::thread subscriptionThread ( [this]() {
                                        initializeGnssAutoPower();
                                    });
    subscriptionThread.detach();
}

GnssAutoPowerHandler::~GnssAutoPowerHandler(){

}

bool GnssAutoPowerHandler::subscribeToVhal() {
    bool returnVal = false;
    constexpr int32_t prop = static_cast<int32_t>(VehicleProperty::AP_POWER_STATE_REPORT);

    if (!isPropertySupported(prop)) {
        LOC_LOGW("Vehicle property(%d) is not supported by VHAL.", prop);
        return returnVal;
    }

    VehiclePropValue propValue{
        .prop = prop,
    };

    sp<IVehicle> vhalService;
    {
        Mutex::Autolock lock(mMutex);
        vhalService = mVhalService;
    }

    StatusCode status;
    vhalService->get(propValue, [&status, &propValue](StatusCode s, const
                     VehiclePropValue& value) {
                     status = s;
                     propValue = value;
                    });
    if (status != StatusCode::OK) {
        LOC_LOGW("Failed to get vehicle property(%d) value.", prop);
        return returnVal;
    }

    handleGnssAutoPowerEvent(propValue.value.int32Values[0]);

    // Subscribe to AP_POWER_STATE_REPORT.
    SubscribeOptions reqVhalProperties[] = {
        {.propId = prop, .flags = SubscribeFlags::EVENTS_FROM_ANDROID},
    };
    hidl_vec<SubscribeOptions> options;
    options.setToExternal(reqVhalProperties, arraysize(reqVhalProperties));

    status = vhalService->subscribe(this, options);

    if (status != StatusCode::OK) {
        LOC_LOGW("%s], Failed to subscribe to vehicle property(%d).", __FUNCTION__, prop);
    } else {
        returnVal = true;
    }

    return returnVal;
} //GnssAutoPowerHandler::subscribeToVhal

bool GnssAutoPowerHandler::isPropertySupported(int32_t prop) {

    if (mSupportedProperties.count(prop) > 0) {
        return mSupportedProperties[prop];
    }
    StatusCode status;
    hidl_vec<int32_t> props = {prop};
    sp<IVehicle> vhalService;
    {
        Mutex::Autolock lock(mMutex);
        vhalService = mVhalService;
    }
    vhalService->getPropConfigs(props,
                                [&status](StatusCode s,
                                hidl_vec<VehiclePropConfig>) {
                                        status = s;
                                });
    mSupportedProperties[prop] = (status == StatusCode::OK);
    return mSupportedProperties[prop];

}


bool GnssAutoPowerHandler::connectToVhal(void) {

    bool returnValue = false;

    {
        Mutex::Autolock lock(mMutex);
        if (mVhalService.get() != nullptr) {
            return true;
        }
    }

    // Get a vehicle HAL instance.
    sp<IVehicle> vhalService = IVehicle::tryGetService();
    if (vhalService.get() == nullptr) {
        LOC_LOGe("Unable to connect to VHAL Service.");
        return returnValue;
    }

    auto ret = vhalService->linkToDeath(mVHidlDeathRecipient, /*cookie=*/0x03);
    if (!ret.isOk() || ret == false) {
        LOC_LOGw("Failed to connect to VHAL. VHAL is dead.");
        return returnValue;
    }
    {
        Mutex::Autolock lock(mMutex);
        mVhalService = vhalService;
    }
    LOC_LOGi("Connected to VHAL");
    returnValue = subscribeToVhal();
    return returnValue;
}//initializeGnssAutoPower

void GnssAutoPowerHandler::handleGnssAutoPowerEvent(int32_t powerState) {

    bool retVal = false;
    LOC_LOGd("Power State: %d", powerState);

    switch (powerState) {
        case POWER_STATE_SHUTDOWN_PREPARE:
            LOC_LOGi("Suspend GNSS sessions.");
            sendPowerEventToLocCtrlApi(POWER_STATE_SUSPEND);
            break;
        case POWER_STATE_DEEP_SLEEP_EXIT:
        case POWER_STATE_SHUTDOWN_CANCELLED:
            LOC_LOGi("Resume GNSS Sessions.");
            sendPowerEventToLocCtrlApi(POWER_STATE_RESUME);
            break;
        default:
            break;
    }//switch
} //GnssAutoPowerHandler::handleGnssAutoPowerEvent

void GnssAutoPowerHandler::handleVHidlDeath(uint64_t cookie, const wp<IBase>& who) {

    {
        LOC_LOGE("%s] VHAL service died. cookie: %llu, who: %p",
                    __FUNCTION__, static_cast<unsigned long long>(cookie), &who);

        Mutex::Autolock lock(mMutex);
        if (mVhalService.get() != nullptr) {
            mVhalService->unlinkToDeath(mVHidlDeathRecipient);
            mVhalService = nullptr;
        }
    }

    (void) cookie;
    //TBD: To re-connect to VHAL service

} //handleVHidlDeath


/*Overrides*/

Return<void> GnssAutoPowerHandler::onPropertyEvent(const hidl_vec<VehiclePropValue>& ) {
    return Return<void>();
}

Return<void> GnssAutoPowerHandler::onPropertySet(const VehiclePropValue& propValue) {

    if (propValue.prop == static_cast<int32_t>(VehicleProperty::AP_POWER_STATE_REPORT))
    {
        int32_t powerState = propValue.value.int32Values[0];
        handleGnssAutoPowerEvent(powerState);
    }
    return Return<void>();
}

Return<void> GnssAutoPowerHandler::onPropertySetError(StatusCode status, int32_t propId,
                                                        int32_t areaId) {
    (void)status;
    (void)propId;
    (void)areaId;
    return Return<void>();
}


GnssAutoPowerHandler* GnssAutoPowerHandler::getGnssAutoPowerHandler() {

    if (nullptr == sGnssAutoPowerHandler) {

        GnssAutoPowerHandler  *autoPwrHandler;
        autoPwrHandler = new GnssAutoPowerHandler();
    }

    return sGnssAutoPowerHandler;
}

extern "C" void initGnssAutoPowerHandler(void){

    GnssAutoPowerHandler::getGnssAutoPowerHandler();
}
