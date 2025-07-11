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


#define LOG_TAG "GnssAidl"
#define LOG_NDEBUG 0

#include "Gnss.h"
#include <log_util.h>
#include "loc_misc_utils.h"
#include "LocationUtil.h"
#include "GnssConfiguration.h"
#include "AGnssRil.h"
#include "AGnss.h"
#include "GnssGeofence.h"
#include "GnssDebug.h"
#include "GnssAntennaInfo.h"
#include "GnssVisibilityControl.h"
#include "GnssBatching.h"
#include "GnssPowerIndication.h"
#include "GnssMeasurementInterface.h"
#include "MeasurementCorrectionsInterface.h"
#include "battery_listener.h"

#define MAX_GNSS_ACCURACY_ALLOWED 10000
namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
using measurement_corrections::aidl::implementation::MeasurementCorrectionsInterface;
using ::android::hardware::gnss::visibility_control::aidl::implementation::GnssVisibilityControl;

static Gnss* sGnss;
void gnssServiceDied(void* cookie) {
    LOC_LOGe("IGnssCallback AIDL service died");
    Gnss* iface = static_cast<Gnss*>(cookie);
    if (iface != nullptr) {
        iface->handleAidlClientSsr();
    }
}
ScopedAStatus Gnss::setCallback(const shared_ptr<IGnssCallback>& callback) {
    if (callback == nullptr) {
        LOC_LOGe("Null callback ignored");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    mMutex.lock();
    if (mGnssCallback != nullptr) {
        AIBinder_unlinkToDeath(mGnssCallback->asBinder().get(), mDeathRecipient, this);
    }
    mGnssCallback = callback;

    if (mGnssCallback != nullptr) {
        AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient, this);
    }
    mMutex.unlock();

    mApi.gnssUpdateCallbacks(callback);
    mApi.gnssEnable(LOCATION_TECHNOLOGY_TYPE_GNSS);
    mApi.requestCapabilities();

    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::close() {
    mApi.gnssStop();
    mApi.gnssDisable();
    return ScopedAStatus::ok();
}

void location_on_battery_status_changed(bool charging) {
    LOC_LOGd("battery status changed to %s charging", charging ? "" : "not");
    if ((sGnss != nullptr) && (sGnss->getLocationControlApi() != nullptr)) {
        sGnss->getLocationControlApi()->updateBatteryStatus(charging);
    }
}

Gnss::Gnss(): mApi(mGnssCallback), mGnssCallback(nullptr),
    mDeathRecipient(AIBinder_DeathRecipient_new(&gnssServiceDied)) {
    ENTRY_LOG_CALLFLOW();
    if (sGnss == nullptr) {
        sGnss = this;
    }
    // register health client to listen on battery change
    loc_extn_battery_properties_listener_init(location_on_battery_status_changed);
    getLocationControlApi();
}

Gnss::~Gnss() {
    ENTRY_LOG_CALLFLOW();
    handleAidlClientSsr();
    mApi.destroy();
    sGnss = nullptr;
}

void Gnss::handleAidlClientSsr() {
    if (mGnssCallback != nullptr) {
        AIBinder_unlinkToDeath(mGnssCallback->asBinder().get(), mDeathRecipient, this);
        mGnssCallback = nullptr;
    }
    mApi.gnssStop();
}

ILocationControlAPI* Gnss::getLocationControlApi() {
    if (mLocationControlApi == nullptr) {

        LocationControlCallbacks locCtrlCbs;
        memset(&locCtrlCbs, 0, sizeof(locCtrlCbs));
        locCtrlCbs.size = sizeof(LocationControlCallbacks);

        locCtrlCbs.odcpiReqCb =
                [this](const OdcpiRequestInfo& odcpiRequest) {
            odcpiRequestCb(odcpiRequest);
        };

        mLocationControlApi = LocationControlAPI::getInstance(locCtrlCbs);
        mLocationControlApi->updateCallbacks(locCtrlCbs);
    }

    return mLocationControlApi;
}


ScopedAStatus Gnss::updateConfiguration(GnssConfig& gnssConfig) {
    ENTRY_LOG_CALLFLOW();
    mApi.gnssConfigurationUpdate(gnssConfig);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssBatching(shared_ptr<IGnssBatching>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssBatching == nullptr) {
        mGnssBatching = SharedRefBase::make<GnssBatching>();
    }
    *_aidl_return = mGnssBatching;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionGnssGeofence(shared_ptr<IGnssGeofence>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssGeofence == nullptr) {
        mGnssGeofence = SharedRefBase::make<GnssGeofence>();
    }
    *_aidl_return = mGnssGeofence;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionAGnss(shared_ptr<IAGnss>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mAGnss == nullptr) {
        mAGnss = SharedRefBase::make<AGnss>(this);
    }
    *_aidl_return = mAGnss;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionAGnssRil(shared_ptr<IAGnssRil>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mAGnssRil == nullptr) {
        mAGnssRil = SharedRefBase::make<AGnssRil>(this);
    }
    *_aidl_return = mAGnssRil;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionGnssDebug(shared_ptr<IGnssDebug>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssDebug == nullptr) {
        mGnssDebug = SharedRefBase::make<GnssDebug>(this);
    }
    *_aidl_return = mGnssDebug;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionGnssVisibilityControl(
        shared_ptr<IGnssVisibilityControl>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssVisibCtrl == nullptr) {
        mGnssVisibCtrl = SharedRefBase::make<GnssVisibilityControl>(this);
    }
    *_aidl_return = mGnssVisibCtrl;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::start() {
    ENTRY_LOG_CALLFLOW();
    mApi.gnssStart();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stop()  {
    ENTRY_LOG_CALLFLOW();
    mApi.gnssStop();
    return ScopedAStatus::ok();
 }
ScopedAStatus Gnss::startSvStatus() {
    ENTRY_LOG_CALLFLOW();
    mApi.configSvStatus(true);
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::stopSvStatus() {
    ENTRY_LOG_CALLFLOW();
    mApi.configSvStatus(false);
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::startNmea() {
    ENTRY_LOG_CALLFLOW();
    mApi.configNmea(true);
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::stopNmea() {
    ENTRY_LOG_CALLFLOW();
    mApi.configNmea(false);
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::injectTime(int64_t timeMs, int64_t timeReferenceMs,
            int32_t uncertaintyMs) {
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::injectLocation(const GnssLocation& location) {
    ENTRY_LOG_CALLFLOW();
    ILocationControlAPI* pCtrlApi = getLocationControlApi();
    if (pCtrlApi != nullptr) {
        pCtrlApi->injectLocation(location.latitudeDegrees, location.longitudeDegrees,
                location.horizontalAccuracyMeters);
    }
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::injectBestLocation(const GnssLocation& gnssLocation) {
    ENTRY_LOG_CALLFLOW();
    ILocationControlAPI* pCtrlApi = getLocationControlApi();
    if (pCtrlApi != nullptr) {
        Location location = {};
        convertGnssLocation(gnssLocation, location);
        location.techMask |= LOCATION_TECHNOLOGY_HYBRID_BIT;
        pCtrlApi->odcpiInject(location);
    }
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::deleteAidingData(IGnss::GnssAidingData aidingDataFlags) {
    ENTRY_LOG_CALLFLOW();
    mApi.gnssDeleteAidingData(aidingDataFlags);
    return ScopedAStatus::ok();
}

void Gnss::odcpiRequestCb(const OdcpiRequestInfo& request) {
    ENTRY_LOG_CALLFLOW();
    if (ODCPI_REQUEST_TYPE_STOP == request.type) {
        return;
    }
    mMutex.lock();
    auto gnssCb = mGnssCallback;
    mMutex.unlock();
    if (gnssCb != nullptr) {
        // For emergency mode, request DBH (Device based hybrid) location
        // Mark Independent from GNSS flag to false.
        if (ODCPI_REQUEST_TYPE_START == request.type) {
            LOC_LOGd("gnssRequestLocationCb isUserEmergency = %d", request.isEmergencyMode);
            auto r = gnssCb->gnssRequestLocationCb(!request.isEmergencyMode,
                                                                 request.isEmergencyMode);
            if (!r.isOk()) {
                LOC_LOGe("Error invoking gnssRequestLocationCb");
            }
        } else {
            LOC_LOGv("Unsupported ODCPI request type: %d", request.type);
        }
    } else {
        LOC_LOGe("ODCPI request not supported.");
    }
}
ScopedAStatus Gnss::setPositionMode(const IGnss::PositionModeOptions& options) {
    ENTRY_LOG_CALLFLOW();
    GnssPowerMode powerMode = options.lowPowerMode? GNSS_POWER_MODE_M4 : GNSS_POWER_MODE_M2;
    mApi.gnssSetPositionMode(options.mode, options.recurrence, options.minIntervalMs,
            options.preferredAccuracyMeters, options.preferredTimeMs, powerMode,
            options.minIntervalMs);
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionGnssAntennaInfo(shared_ptr<IGnssAntennaInfo>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssAntennaInfo == nullptr) {
        mGnssAntennaInfo = SharedRefBase::make<GnssAntennaInfo>(this);
    }
    *_aidl_return = mGnssAntennaInfo;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionMeasurementCorrections(
        shared_ptr<IMeasurementCorrectionsInterface>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssMeasCorr == nullptr) {
        mGnssMeasCorr = SharedRefBase::make<MeasurementCorrectionsInterface>(this);
    }
    *_aidl_return = mGnssMeasCorr;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionGnssConfiguration(
        shared_ptr<IGnssConfiguration>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssConfiguration == nullptr) {
        mGnssConfiguration = SharedRefBase::make<GnssConfiguration>(this);
    }
    *_aidl_return = mGnssConfiguration;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssPowerIndication(
        shared_ptr<IGnssPowerIndication>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssPowerIndication == nullptr) {
        mGnssPowerIndication = SharedRefBase::make<GnssPowerIndication>();
    }
    *_aidl_return = mGnssPowerIndication;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::getExtensionGnssMeasurement(
        shared_ptr<IGnssMeasurementInterface>* _aidl_return) {
    ENTRY_LOG_CALLFLOW();
    if (mGnssMeasurementInterface == nullptr) {
        mGnssMeasurementInterface = SharedRefBase::make<GnssMeasurementInterface>();
    }
    *_aidl_return = mGnssMeasurementInterface;
    return ScopedAStatus::ok();
}

// Method that will register gnssStatusCallback,
// only if the host FW is non-AFW and native NLP library
// is available.
void registerGnssStatusCallback(std::function<void(bool)> gnssCb, std::function<void(bool)> nlpCb) {
    LOC_LOGd("registerGnssStatusCallback");
    if (nullptr != gnssCb && nullptr != nlpCb && sGnss != nullptr) {
        GnssAPIClient::sGnssStatusCb = gnssCb;
        GnssAPIClient::sNlpRequestCb = nlpCb;
        GnssAPIClient::sFlpRequestAllowed = true;
        gnssCb(sGnss->isGnssCbValid());
    } else {
        LOC_LOGe("Failed to register!!!");
    }
}

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
