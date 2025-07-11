/*
Copyright (c) 2022, 2023 Qualcomm Innovation Center, Inc. All rights reserved.

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
#include <aidl/android/hardware/gnss/measurement_corrections/IMeasurementCorrectionsCallback.h>
#include <aidl/android/hardware/gnss/measurement_corrections/MeasurementCorrections.h>
#include "Gnss.h"
#include "MeasurementCorrectionsInterface.h"
#include <LocationUtil.h>

namespace android {
namespace hardware {
namespace gnss {
namespace measurement_corrections {
namespace aidl {
namespace implementation {
using ::aidl::android::hardware::gnss::GnssConstellationType;

void measurementCorrectionsInterfaceDied(void* cookie) {
    LOC_LOGe("IGnssAntennaInfo AIDL service died");
    MeasurementCorrectionsInterface* iface = static_cast<MeasurementCorrectionsInterface*>(cookie);
    if (iface != nullptr) {
        iface->setCallback(nullptr);
        iface = nullptr;
    }
}
MeasurementCorrectionsInterface::MeasurementCorrectionsInterface(Gnss* gnss) : mGnss(gnss),
    mDeathRecipient(AIBinder_DeathRecipient_new(&measurementCorrectionsInterfaceDied)) {
}

MeasurementCorrectionsInterface::~MeasurementCorrectionsInterface() {
}

void MeasurementCorrectionsInterface::setCapabilitiesCb(
    GnssMeasurementCorrectionsCapabilitiesMask capabilities) {
    std::unique_lock<std::mutex> lock(mMutex);
    auto measCorrCbIface(mMeasurementCorrectionsCbIface);
    lock.unlock();
    if (measCorrCbIface != nullptr) {
        uint32_t measCorrCapabilities = 0;

        // Convert from one enum to another
        if (capabilities & GNSS_MEAS_CORR_LOS_SATS) {
            measCorrCapabilities |=
                    IMeasurementCorrectionsCallback::CAPABILITY_LOS_SATS;
        }
        if (capabilities & GNSS_MEAS_CORR_EXCESS_PATH_LENGTH) {
            measCorrCapabilities |=
                    IMeasurementCorrectionsCallback::CAPABILITY_EXCESS_PATH_LENGTH;
        }
        if (capabilities & GNSS_MEAS_CORR_REFLECTING_PLANE) {
            measCorrCapabilities |=
                    IMeasurementCorrectionsCallback::CAPABILITY_REFLECTING_PLANE;
        }

        auto r = measCorrCbIface->setCapabilitiesCb(measCorrCapabilities);
        if (!r.isOk()) {
            LOC_LOGw("Error invoking setCapabilitiesCb");
        }
    } else {
        LOC_LOGw("setCallback has not been called yet");
    }
}


ScopedAStatus MeasurementCorrectionsInterface::setCorrections(
        const MeasurementCorrections& corrections) {
    GnssMeasurementCorrections gnssMeasurementCorrections = {};

    gnss::aidl::implementation::convertMeasurementCorrections(corrections,
            gnssMeasurementCorrections);

    gnssMeasurementCorrections.hasEnvironmentBearing = corrections.hasEnvironmentBearing;
    gnssMeasurementCorrections.environmentBearingDegrees =
            corrections.environmentBearingDegrees;
    gnssMeasurementCorrections.environmentBearingUncertaintyDegrees =
            corrections.environmentBearingUncertaintyDegrees;

    for (int i = 0; i < corrections.satCorrections.size(); i++) {
        GnssSingleSatCorrection gnssSingleSatCorrection = {};

        gnss::aidl::implementation::convertSingleSatCorrections(corrections.satCorrections[i],
                gnssSingleSatCorrection);
        switch (corrections.satCorrections[i].constellation) {
        case (GnssConstellationType::GPS):
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_GPS;
            break;
        case (GnssConstellationType::SBAS):
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_SBAS;
            break;
        case (GnssConstellationType::GLONASS):
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_GLONASS;
            break;
        case (GnssConstellationType::QZSS):
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_QZSS;
            break;
        case (GnssConstellationType::BEIDOU):
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_BEIDOU;
            break;
        case (GnssConstellationType::GALILEO):
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_GALILEO;
            break;
        case (GnssConstellationType::IRNSS):
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_NAVIC;
            break;
        case (GnssConstellationType::UNKNOWN):
        default:
            gnssSingleSatCorrection.svType = GNSS_SV_TYPE_UNKNOWN;
            break;
        }
        gnssMeasurementCorrections.satCorrections.push_back(gnssSingleSatCorrection);
    }

    mGnss->getLocationControlApi()->measCorrSetCorrections(gnssMeasurementCorrections);
   return ScopedAStatus::ok();
}

ScopedAStatus MeasurementCorrectionsInterface::setCallback(
        const shared_ptr<IMeasurementCorrectionsCallback>& callback) {
    if (nullptr == mGnss || nullptr == mGnss->getLocationControlApi()) {
        LOC_LOGe("Null GNSS interface");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    std::unique_lock<std::mutex> lock(mMutex);
    if (mMeasurementCorrectionsCbIface != nullptr) {
        AIBinder_unlinkToDeath(mMeasurementCorrectionsCbIface->asBinder().get(), mDeathRecipient,
                this);
    }
    mMeasurementCorrectionsCbIface = callback;
    if (mMeasurementCorrectionsCbIface != nullptr) {
        AIBinder_linkToDeath(mMeasurementCorrectionsCbIface->asBinder().get(), mDeathRecipient,
                this);
    }
    lock.unlock();

    LocationControlCallbacks locCtrlCbs;
    memset(&locCtrlCbs, 0, sizeof(locCtrlCbs));
    locCtrlCbs.size = sizeof(LocationControlCallbacks);

    locCtrlCbs.measCorrSetCapabilitiesCb =
            [this] (GnssMeasurementCorrectionsCapabilitiesMask capabilities) {
            setCapabilitiesCb(capabilities);
    };
    mGnss->getLocationControlApi()->updateCallbacks(locCtrlCbs);
    return ScopedAStatus::ok();}

}
}  // namespace aidl
}  // namespace measurement_corrections
}  // namespace gnss
}  // namespace hardware
}  // namespace android
