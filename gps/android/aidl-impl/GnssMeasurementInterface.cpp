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

#define LOG_TAG "GnssMeasurementInterfaceAidl"

#include <log_util.h>
#include "GnssMeasurementInterface.h"
#include <android/binder_auto_utils.h>
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <inttypes.h>
#include <loc_misc_utils.h>

using aidl::android::hardware::gnss::ElapsedRealtime;
using aidl::android::hardware::gnss::GnssClock;
using aidl::android::hardware::gnss::GnssData;
using aidl::android::hardware::gnss::GnssMeasurement;
using aidl::android::hardware::gnss::GnssSignalType;
using aidl::android::hardware::gnss::GnssConstellationType;
using aidl::android::hardware::gnss::GnssMultipathIndicator;
using aidl::android::hardware::gnss::SatellitePvt;

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

GnssMeasurementInterface::GnssMeasurementInterface() :
    mDeathRecipient(AIBinder_DeathRecipient_new(GnssMeasurementInterface::gnssMeasurementDied)),
    mTracking(false) {
}

ScopedAStatus GnssMeasurementInterface::setCallback(
        const shared_ptr<IGnssMeasurementCallback>& callback,
        bool enableFullTracking, bool enableCorrVecOutputs) {

    IGnssMeasurementInterface::Options options;

    options.enableFullTracking = enableFullTracking;
    options.enableCorrVecOutputs = enableCorrVecOutputs;
    options.intervalMs = 1000;

    return setCallbackWithOptions(callback, options);
}

ScopedAStatus GnssMeasurementInterface::setCallbackWithOptions(
        const shared_ptr<IGnssMeasurementCallback>& callback,
        const IGnssMeasurementInterface::Options& options) {
    LOC_LOGd("setCallback: enableFullTracking: %d enableCorrVecOutputs: %d intervalMs: %d",
             (int)options.enableFullTracking,
             (int)options.enableCorrVecOutputs,
             (int)options.intervalMs);
    std::unique_lock<std::mutex> lock(mMutex);

    if (nullptr == callback) {
        LOC_LOGe("callback is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient, this);
    mGnssMeasurementCbIface = callback;

    lock.unlock();
    startTracking(options.enableFullTracking ? GNSS_POWER_MODE_M1 : GNSS_POWER_MODE_M2,
                  options.intervalMs);
    return ScopedAStatus::ok();
}

ScopedAStatus GnssMeasurementInterface::close()  {
    LOC_LOGd("()");
    if (nullptr != mGnssMeasurementCbIface) {
        AIBinder_unlinkToDeath(mGnssMeasurementCbIface->asBinder().get(), mDeathRecipient, this);
        mGnssMeasurementCbIface = nullptr;
    }
    std::unique_lock<std::mutex> lock(mMutex);
    mTracking = false;
    lock.unlock();
    locAPIStopTracking();

    // Clear measurement callback
    LocationCallbacks locationCallbacks;
    memset(&locationCallbacks, 0, sizeof(LocationCallbacks));
    locationCallbacks.size = sizeof(LocationCallbacks);
    locAPISetCallbacks(locationCallbacks);

    return ScopedAStatus::ok();
}

void GnssMeasurementInterface::onGnssMeasurementsCb(
        const GnssMeasurementsNotification &gnssMeasurementsNotification) {

    std::unique_lock<std::mutex> lock(mMutex);
    LOC_LOGv("(count: %u active: %d)", gnssMeasurementsNotification.count, mTracking);
    if (mTracking) {
        auto gnssMeasurementCbIface = mGnssMeasurementCbIface;
        if (gnssMeasurementCbIface != nullptr) {
            GnssData gnssData = {};
            convertGnssData(gnssMeasurementsNotification, gnssData);
            printGnssData(gnssData);
            lock.unlock();
            gnssMeasurementCbIface->gnssMeasurementCb(gnssData);
        }
    }
}

void GnssMeasurementInterface::gnssMeasurementDied(void* cookie) {

    LOC_LOGe("IGnssMeasurementCallback service died");
    GnssMeasurementInterface* iface = static_cast<GnssMeasurementInterface*>(cookie);
    //clean up, i.e.  iface->close();
    if (iface != nullptr) {
        iface->close();
    }
}

void GnssMeasurementInterface::startTracking(
        GnssPowerMode powerMode, uint32_t timeBetweenMeasurement) {

    LocationCallbacks locationCallbacks;
    memset(&locationCallbacks, 0, sizeof(LocationCallbacks));
    locationCallbacks.size = sizeof(LocationCallbacks);

    locationCallbacks.trackingCb = nullptr;
    locationCallbacks.batchingCb = nullptr;
    locationCallbacks.geofenceBreachCb = nullptr;
    locationCallbacks.geofenceStatusCb = nullptr;
    locationCallbacks.gnssLocationInfoCb = nullptr;
    locationCallbacks.gnssNiCb = nullptr;
    locationCallbacks.gnssSvCb = nullptr;
    locationCallbacks.gnssNmeaCb = nullptr;

    locationCallbacks.gnssMeasurementsCb = nullptr;
    if (nullptr != mGnssMeasurementCbIface) {
        locationCallbacks.gnssMeasurementsCb =
            [this](const GnssMeasurementsNotification &gnssMeasurementsNotification) {
            onGnssMeasurementsCb(gnssMeasurementsNotification);
        };
    }

    locAPISetCallbacks(locationCallbacks);

    TrackingOptions options;
    options.size = sizeof(TrackingOptions);
    options.minInterval = timeBetweenMeasurement;
    options.mode = GNSS_SUPL_MODE_STANDALONE;
    options.powerMode = powerMode;
    options.tbm = timeBetweenMeasurement;

    std::unique_lock<std::mutex> lock(mMutex);
    mTracking = true;
    lock.unlock();
    LOC_LOGd("start tracking session");
    locAPIStartTracking(options);
}

void GnssMeasurementInterface::convertGnssData(
        const GnssMeasurementsNotification& in, GnssData& out) {

    out.measurements.resize(in.count);
    for (size_t i = 0; i < in.count; i++) {
        out.measurements[i].flags = 0;
        convertGnssMeasurement(in.measurements[i], out.measurements[i]);
    }
    convertGnssClock(in.clock, out.clock);
    convertElapsedRealtimeNanos(in, out.elapsedRealtime);

    if (in.agcCount > 0) {
        GnssConstellationType constellation;
        convertGnssConstellationType(in.gnssAgc[0].svType, constellation);

        GnssData::GnssAgc gnssAgc0 = {
            .agcLevelDb = in.gnssAgc[0].agcLevelDb,
            .constellation = constellation,
            .carrierFrequencyHz = (int64_t)in.gnssAgc[0].carrierFrequencyHz,
        };
        out.gnssAgcs = std::vector({ gnssAgc0 });

        for (size_t i = 1; i < in.agcCount; i++) {
            gnssAgc0.agcLevelDb = in.gnssAgc[i].agcLevelDb;
            convertGnssConstellationType(in.gnssAgc[i].svType, constellation);
            gnssAgc0.constellation = constellation;
            gnssAgc0.carrierFrequencyHz = (int64_t)in.gnssAgc[i].carrierFrequencyHz;
            out.gnssAgcs.push_back(gnssAgc0);
        }
    }
    out.isFullTracking = in.isFullTracking;
}

void GnssMeasurementInterface::convertGnssMeasurement(
        const GnssMeasurementsData& in, GnssMeasurement& out) {
    // flags
    convertGnssFlags(in, out);
    // svid
    convertGnssSvId(in, out.svid);
    // signalType
    convertGnssSignalType(in, out.signalType);
    // timeOffsetNs
    out.timeOffsetNs = in.timeOffsetNs;
    // state
    convertGnssState(in, out);
    // receivedSvTimeInNs
    out.receivedSvTimeInNs = in.receivedSvTimeNs;
    // receivedSvTimeUncertaintyInNs
    out.receivedSvTimeUncertaintyInNs = in.receivedSvTimeUncertaintyNs;
    // antennaCN0DbHz
    out.antennaCN0DbHz = in.carrierToNoiseDbHz;
    // basebandCN0DbHz
    out.basebandCN0DbHz = in.basebandCarrierToNoiseDbHz;
    // pseudorangeRateMps
    out.pseudorangeRateMps = in.pseudorangeRateMps;
    // pseudorangeRateUncertaintyMps
    out.pseudorangeRateUncertaintyMps = in.pseudorangeRateUncertaintyMps;
    // accumulatedDeltaRangeState
    convertGnssAccumulatedDeltaRangeState(in, out);
    // accumulatedDeltaRangeM
    out.accumulatedDeltaRangeM = in.adrMeters;
    // accumulatedDeltaRangeUncertaintyM
    out.accumulatedDeltaRangeUncertaintyM = in.adrUncertaintyMeters;
    // carrierCycles
    out.carrierCycles = in.carrierCycles;
    // carrierPhase
    out.carrierPhase = in.carrierPhase;
    // carrierPhaseUncertainty
    out.carrierPhaseUncertainty = in.carrierPhaseUncertainty;
    // multipathIndicator
    convertGnssMultipathIndicator(in, out);
    // snrDb
    out.snrDb = in.signalToNoiseRatioDb;
    // agcLevelDb
    out.agcLevelDb = in.agcLevelDb;
    // fullInterSignalBiasNs
    out.fullInterSignalBiasNs = in.fullInterSignalBiasNs;
    // fullInterSignalBiasUncertaintyNs
    out.fullInterSignalBiasUncertaintyNs = in.fullInterSignalBiasUncertaintyNs;
    // satelliteInterSignalBiasNs
    out.satelliteInterSignalBiasNs = in.satelliteInterSignalBiasNs;
    // satelliteInterSignalBiasUncertaintyNs
    out.satelliteInterSignalBiasUncertaintyNs = in.satelliteInterSignalBiasUncertaintyNs;
    // satellitePvt
    convertGnssSatellitePvt(in, out);
    // correlationVectors
    /* This is not supported, the corresponding flag is not set */
}

void GnssMeasurementInterface::convertGnssFlags(
        const GnssMeasurementsData& in, GnssMeasurement& out) {

    if (in.flags & GNSS_MEASUREMENTS_DATA_SIGNAL_TO_NOISE_RATIO_BIT)
        out.flags |= out.HAS_SNR;
    if (in.flags & GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT)
        out.flags |= out.HAS_CARRIER_FREQUENCY;
    if (in.flags & GNSS_MEASUREMENTS_DATA_CARRIER_CYCLES_BIT)
        out.flags |= out.HAS_CARRIER_CYCLES;
    if (in.flags & GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT)
        out.flags |= out.HAS_CARRIER_PHASE;
    if (in.flags & GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT)
        out.flags |= out.HAS_CARRIER_PHASE_UNCERTAINTY;
    if (in.flags & GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT)
        out.flags |= out.HAS_AUTOMATIC_GAIN_CONTROL;
    if (in.flags & GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT)
        out.flags |= out.HAS_FULL_ISB;
    if (in.flags & GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT)
        out.flags |= out.HAS_FULL_ISB_UNCERTAINTY;
    if (in.flags & GNSS_MEASUREMENTS_DATA_SATELLITE_ISB_BIT)
        out.flags |= out.HAS_SATELLITE_ISB;
    if (in.flags & GNSS_MEASUREMENTS_DATA_SATELLITE_ISB_UNCERTAINTY_BIT)
        out.flags |= out.HAS_SATELLITE_ISB_UNCERTAINTY;
    if (in.flags & GNSS_MEASUREMENTS_DATA_SATELLITE_PVT_BIT)
        out.flags |= out.HAS_SATELLITE_PVT;
    if (in.flags & GNSS_MEASUREMENTS_DATA_CORRELATION_VECTOR_BIT)
        out.flags |= out.HAS_CORRELATION_VECTOR;
}

void GnssMeasurementInterface::convertGnssSvId(const GnssMeasurementsData& in, int& out) {

    switch (in.svType) {
    case GNSS_SV_TYPE_GPS:
        out = in.svId;
        break;
    case GNSS_SV_TYPE_SBAS:
        out = in.svId;
        break;
    case GNSS_SV_TYPE_GLONASS:
        if (in.svId != 255) { // OSN is known
            out = in.svId - GLO_SV_PRN_MIN + 1;
        } else { // OSN is not known, report FCN
            out = in.gloFrequency + 92;
        }
        break;
    case GNSS_SV_TYPE_QZSS:
        out = in.svId;
        break;
    case GNSS_SV_TYPE_BEIDOU:
        out = in.svId - BDS_SV_PRN_MIN + 1;
        break;
    case GNSS_SV_TYPE_GALILEO:
        out = in.svId - GAL_SV_PRN_MIN + 1;
        break;
    case GNSS_SV_TYPE_NAVIC:
        out = in.svId - NAVIC_SV_PRN_MIN + 1;
        break;
    default:
        out = in.svId;
        break;
    }
}

void GnssMeasurementInterface::convertGnssSignalType(
        const GnssMeasurementsData& in, GnssSignalType& out) {

    convertGnssConstellationType(in.svType, out.constellation);
    out.carrierFrequencyHz = in.carrierFrequencyHz;
    convertGnssMeasurementsCodeType(in.codeType, in.otherCodeTypeName, out);
}

void GnssMeasurementInterface::convertGnssConstellationType(
        const GnssSvType& in, GnssConstellationType& out) {

    switch (in) {
        case GNSS_SV_TYPE_GPS:
            out = GnssConstellationType::GPS;
            break;
        case GNSS_SV_TYPE_SBAS:
            out = GnssConstellationType::SBAS;
            break;
        case GNSS_SV_TYPE_GLONASS:
            out = GnssConstellationType::GLONASS;
            break;
        case GNSS_SV_TYPE_QZSS:
            out = GnssConstellationType::QZSS;
            break;
        case GNSS_SV_TYPE_BEIDOU:
            out = GnssConstellationType::BEIDOU;
            break;
        case GNSS_SV_TYPE_GALILEO:
            out = GnssConstellationType::GALILEO;
            break;
        case GNSS_SV_TYPE_NAVIC:
            out = GnssConstellationType::IRNSS;
            break;
        case GNSS_SV_TYPE_UNKNOWN:
        default:
            out = GnssConstellationType::UNKNOWN;
            break;
    }
}

void GnssMeasurementInterface::convertGnssMeasurementsCodeType(
        const GnssMeasurementsCodeType& inCodeType,
        const char* inOtherCodeTypeName, GnssSignalType& out) {

    switch (inCodeType) {
    case GNSS_MEASUREMENTS_CODE_TYPE_A:
        out.codeType = out.CODE_TYPE_A;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_B:
        out.codeType = out.CODE_TYPE_B;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_C:
        out.codeType = out.CODE_TYPE_C;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_I:
        out.codeType = out.CODE_TYPE_I;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_L:
        out.codeType = out.CODE_TYPE_L;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_M:
        out.codeType = out.CODE_TYPE_M;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_N:
        out.codeType = out.CODE_TYPE_N;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_P:
        out.codeType = out.CODE_TYPE_P;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_Q:
        out.codeType = out.CODE_TYPE_Q;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_S:
        out.codeType = out.CODE_TYPE_S;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_W:
        out.codeType = out.CODE_TYPE_W;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_X:
        out.codeType = out.CODE_TYPE_X;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_Y:
        out.codeType = out.CODE_TYPE_Y;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_Z:
        out.codeType = out.CODE_TYPE_Z;
        break;
    case GNSS_MEASUREMENTS_CODE_TYPE_OTHER:
    default:
        out.codeType = inOtherCodeTypeName;
        break;
    }
}

void GnssMeasurementInterface::convertGnssState(
        const GnssMeasurementsData& in, GnssMeasurement& out) {

    if (in.stateMask & GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT)
        out.state |= out.STATE_CODE_LOCK;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_BIT_SYNC_BIT)
        out.state |= out.STATE_BIT_SYNC;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_SUBFRAME_SYNC_BIT)
        out.state |= out.STATE_SUBFRAME_SYNC;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_TOW_DECODED_BIT)
        out.state |= out.STATE_TOW_DECODED;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_MSEC_AMBIGUOUS_BIT)
        out.state |= out.STATE_MSEC_AMBIGUOUS;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_SYMBOL_SYNC_BIT)
        out.state |= out.STATE_SYMBOL_SYNC;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_GLO_STRING_SYNC_BIT)
        out.state |= out.STATE_GLO_STRING_SYNC;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_GLO_TOD_DECODED_BIT)
        out.state |= out.STATE_GLO_TOD_DECODED;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_BDS_D2_BIT_SYNC_BIT)
        out.state |= out.STATE_BDS_D2_BIT_SYNC;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_BDS_D2_SUBFRAME_SYNC_BIT)
        out.state |= out.STATE_BDS_D2_SUBFRAME_SYNC;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_GAL_E1BC_CODE_LOCK_BIT)
        out.state |= out.STATE_GAL_E1BC_CODE_LOCK;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_GAL_E1C_2ND_CODE_LOCK_BIT)
        out.state |= out.STATE_GAL_E1C_2ND_CODE_LOCK;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_GAL_E1B_PAGE_SYNC_BIT)
        out.state |= out.STATE_GAL_E1B_PAGE_SYNC;
    if (in.stateMask &  GNSS_MEASUREMENTS_STATE_SBAS_SYNC_BIT)
        out.state |= out.STATE_SBAS_SYNC;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_TOW_KNOWN_BIT)
        out.state |= out.STATE_TOW_KNOWN;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_GLO_TOD_KNOWN_BIT)
        out.state |= out.STATE_GLO_TOD_KNOWN;
    if (in.stateMask & GNSS_MEASUREMENTS_STATE_2ND_CODE_LOCK_BIT)
        out.state |= out.STATE_2ND_CODE_LOCK;
}

void GnssMeasurementInterface::convertGnssAccumulatedDeltaRangeState(
        const GnssMeasurementsData& in, GnssMeasurement& out) {

    if (in.adrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT)
        out.accumulatedDeltaRangeState |= out.ADR_STATE_VALID;
    if (in.adrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT)
        out.accumulatedDeltaRangeState |= out.ADR_STATE_RESET;
    if (in.adrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT)
        out.accumulatedDeltaRangeState |= out.ADR_STATE_CYCLE_SLIP;
    if (in.adrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_HALF_CYCLE_RESOLVED_BIT)
        out.accumulatedDeltaRangeState |= out.ADR_STATE_HALF_CYCLE_RESOLVED;
}

void GnssMeasurementInterface::convertGnssMultipathIndicator(
        const GnssMeasurementsData& in, GnssMeasurement& out) {

    switch (in.multipathIndicator) {
    case GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_PRESENT:
        out.multipathIndicator = GnssMultipathIndicator::PRESENT;
        break;
    case GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_NOT_PRESENT:
        out.multipathIndicator = GnssMultipathIndicator::NOT_PRESENT;
        break;
    case GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN:
    default:
        out.multipathIndicator = GnssMultipathIndicator::UNKNOWN;
        break;
    }
}

void GnssMeasurementInterface::convertGnssSatellitePvtFlags(const GnssMeasurementsData& in,
                                                            GnssMeasurement& out) {

    if (in.satellitePvt.flags & GNSS_SATELLITE_PVT_POSITION_VELOCITY_CLOCK_INFO_BIT)
        out.satellitePvt.flags |= out.satellitePvt.HAS_POSITION_VELOCITY_CLOCK_INFO;
    if (in.satellitePvt.flags & GNSS_SATELLITE_PVT_IONO_BIT)
        out.satellitePvt.flags |= out.satellitePvt.HAS_IONO;
    if (in.satellitePvt.flags & GNSS_SATELLITE_PVT_TROPO_BIT)
        out.satellitePvt.flags |= out.satellitePvt.HAS_TROPO;
}

void GnssMeasurementInterface::convertGnssSatellitePvt(
        const GnssMeasurementsData& in, GnssMeasurement& out) {

    // flags
    convertGnssSatellitePvtFlags(in, out);

    // satPosEcef
    out.satellitePvt.satPosEcef.posXMeters = in.satellitePvt.satPosEcef.posXMeters;
    out.satellitePvt.satPosEcef.posYMeters = in.satellitePvt.satPosEcef.posYMeters;
    out.satellitePvt.satPosEcef.posZMeters = in.satellitePvt.satPosEcef.posZMeters;
    out.satellitePvt.satPosEcef.ureMeters = in.satellitePvt.satPosEcef.ureMeters;
    // satVelEcef
    out.satellitePvt.satVelEcef.velXMps = in.satellitePvt.satVelEcef.velXMps;
    out.satellitePvt.satVelEcef.velYMps = in.satellitePvt.satVelEcef.velYMps;
    out.satellitePvt.satVelEcef.velZMps = in.satellitePvt.satVelEcef.velZMps;
    out.satellitePvt.satVelEcef.ureRateMps = in.satellitePvt.satVelEcef.ureRateMps;
    // satClockInfo
    out.satellitePvt.satClockInfo.satHardwareCodeBiasMeters =
            in.satellitePvt.satClockInfo.satHardwareCodeBiasMeters;
    out.satellitePvt.satClockInfo.satTimeCorrectionMeters =
            in.satellitePvt.satClockInfo.satTimeCorrectionMeters;
    out.satellitePvt.satClockInfo.satClkDriftMps = in.satellitePvt.satClockInfo.satClkDriftMps;
    // ionoDelayMeters
    out.satellitePvt.ionoDelayMeters = in.satellitePvt.ionoDelayMeters;
    // tropoDelayMeters
    out.satellitePvt.tropoDelayMeters = in.satellitePvt.tropoDelayMeters;

    // timeOfClockSeconds
    out.satellitePvt.timeOfClockSeconds = in.satellitePvt.TOC;
    // issueOfDataClock
    out.satellitePvt.issueOfDataClock = in.satellitePvt.IODC;
    // timeOfEphemerisSeconds
    out.satellitePvt.timeOfEphemerisSeconds = in.satellitePvt.TOE;
    // issueOfDataEphemeris
    out.satellitePvt.issueOfDataEphemeris = in.satellitePvt.IODE;
    // ephemerisSource
    switch (in.satellitePvt.ephemerisSource) {
    case GNSS_EPHEMERIS_SOURCE_EXT_DEMODULATED:
        out.satellitePvt.ephemerisSource = SatellitePvt::SatelliteEphemerisSource::DEMODULATED;
        break;
    case GNSS_EPHEMERIS_SOURCE_EXT_SERVER_NORMAL:
        out.satellitePvt.ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_NORMAL;
        break;
    case GNSS_EPHEMERIS_SOURCE_EXT_SERVER_LONG_TERM:
        out.satellitePvt.ephemerisSource =
                    SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM;
        break;
    case GNSS_EPHEMERIS_SOURCE_EXT_OTHER:
    case GNSS_EPHEMERIS_SOURCE_EXT_INVALID:
    default:
        out.satellitePvt.ephemerisSource = SatellitePvt::SatelliteEphemerisSource::OTHER;
        break;
    }
}

void GnssMeasurementInterface::convertGnssClock(
        const GnssMeasurementsClock& in, GnssClock& out) {

    // gnssClockFlags
    if (in.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT)
        out.gnssClockFlags |= out.HAS_LEAP_SECOND;
    if (in.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_UNCERTAINTY_BIT)
        out.gnssClockFlags |= out.HAS_TIME_UNCERTAINTY;
    if (in.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT)
        out.gnssClockFlags |= out.HAS_FULL_BIAS;
    if (in.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT)
        out.gnssClockFlags |= out.HAS_BIAS;
    if (in.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT)
        out.gnssClockFlags |= out.HAS_BIAS_UNCERTAINTY;
    if (in.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT)
        out.gnssClockFlags |= out.HAS_DRIFT;
    if (in.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT)
        out.gnssClockFlags |= out.HAS_DRIFT_UNCERTAINTY;
    // leapSecond
    out.leapSecond = in.leapSecond;
    // timeNs
    out.timeNs = in.timeNs;
    // timeUncertaintyNs
    out.timeUncertaintyNs = in.timeUncertaintyNs;
    // fullBiasNs
    out.fullBiasNs = in.fullBiasNs;
    // biasNs
    out.biasNs = in.biasNs;
    // biasUncertaintyNs
    out.biasUncertaintyNs = in.biasUncertaintyNs;
    // driftNsps
    out.driftNsps = in.driftNsps;
    // driftUncertaintyNsps
    out.driftUncertaintyNsps = in.driftUncertaintyNsps;
    // hwClockDiscontinuityCount
    out.hwClockDiscontinuityCount = in.hwClockDiscontinuityCount;
    // referenceSignalTypeForIsb
    convertGnssConstellationType(in.referenceSignalTypeForIsb.svType,
            out.referenceSignalTypeForIsb.constellation);
    out.referenceSignalTypeForIsb.carrierFrequencyHz =
            in.referenceSignalTypeForIsb.carrierFrequencyHz;
    convertGnssMeasurementsCodeType(in.referenceSignalTypeForIsb.codeType,
            in.referenceSignalTypeForIsb.otherCodeTypeName,
            out.referenceSignalTypeForIsb);
}

void GnssMeasurementInterface::convertElapsedRealtimeNanos(
        const GnssMeasurementsNotification& in, ElapsedRealtime& elapsedRealtime) {

    if (in.clock.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_ELAPSED_REAL_TIME_BIT) {
        elapsedRealtime.flags |= elapsedRealtime.HAS_TIMESTAMP_NS;
        elapsedRealtime.timestampNs = in.clock.elapsedRealTime;
        elapsedRealtime.flags |= elapsedRealtime.HAS_TIME_UNCERTAINTY_NS;
        elapsedRealtime.timeUncertaintyNs = in.clock.elapsedRealTimeUnc;
        LOC_LOGa("elapsedRealtime.timestampNs=%" PRIi64 ""
                 " elapsedRealtime.timeUncertaintyNs=%lf elapsedRealtime.flags=0x%X",
                 elapsedRealtime.timestampNs,
                 elapsedRealtime.timeUncertaintyNs, elapsedRealtime.flags);
    }
}

void GnssMeasurementInterface::printGnssData(GnssData& data) {
    LOC_LOGa(" Measurements Info for %zu satellites", data.measurements.size());
    for (size_t i = 0; i < data.measurements.size(); i++) {
        LOC_LOGa("%zu : flags: 0x%08x,"
                 " svid: %d,"
                 " signalType.constellation: %u,"
                 " signalType.carrierFrequencyHz: %.2f,"
                 " signalType.codeType: %s,"
                 " timeOffsetNs: %.2f,"
                 " state: 0x%08x,"
                 " receivedSvTimeInNs: %" PRIu64
                 " receivedSvTimeUncertaintyInNs: %" PRIu64
                 " antennaCN0DbHz: %.2f,"
                 " basebandCN0DbHz: %.2f,"
                 " pseudorangeRateMps : %.2f,"
                 " pseudorangeRateUncertaintyMps : %.2f,\n"
                 "      accumulatedDeltaRangeState: 0x%08x,"
                 " accumulatedDeltaRangeM: %.2f, "
                 " accumulatedDeltaRangeUncertaintyM : %.2f, "
                 " carrierCycles: %" PRIu64
                 " carrierPhase: %.2f,"
                 " carrierPhaseUncertainty: %.2f,"
                 " multipathIndicator: %u,"
                 " snrDb: %.2f,"
                 " agcLevelDb: %.2f,"
                 " fullInterSignalBiasNs: %.2f,"
                 " fullInterSignalBiasUncertaintyNs: %.2f,"
                 " satelliteInterSignalBiasNs: %.2f,"
                 " satelliteInterSignalBiasUncertaintyNs: %.2f",
                 i + 1,
                 data.measurements[i].flags,
                 data.measurements[i].svid,
                 data.measurements[i].signalType.constellation,
                 data.measurements[i].signalType.carrierFrequencyHz,
                 data.measurements[i].signalType.codeType.c_str(),
                 data.measurements[i].timeOffsetNs,
                 data.measurements[i].state,
                 data.measurements[i].receivedSvTimeInNs,
                 data.measurements[i].receivedSvTimeUncertaintyInNs,
                 data.measurements[i].antennaCN0DbHz,
                 data.measurements[i].basebandCN0DbHz,
                 data.measurements[i].pseudorangeRateMps,
                 data.measurements[i].pseudorangeRateUncertaintyMps,
                 data.measurements[i].accumulatedDeltaRangeState,
                 data.measurements[i].accumulatedDeltaRangeM,
                 data.measurements[i].accumulatedDeltaRangeUncertaintyM,
                 data.measurements[i].carrierCycles,
                 data.measurements[i].carrierPhase,
                 data.measurements[i].carrierPhaseUncertainty,
                 data.measurements[i].multipathIndicator,
                 data.measurements[i].snrDb,
                 data.measurements[i].agcLevelDb,
                 data.measurements[i].fullInterSignalBiasNs,
                 data.measurements[i].fullInterSignalBiasUncertaintyNs,
                 data.measurements[i].satelliteInterSignalBiasNs,
                 data.measurements[i].satelliteInterSignalBiasUncertaintyNs
            );
        LOC_LOGa("      satellitePvt.flags: 0x%04x,"
                 " satellitePvt.satPosEcef.posXMeters: %.2f,"
                 " satellitePvt.satPosEcef.posYMeters: %.2f,"
                 " satellitePvt.satPosEcef.posZMeters: %.2f,"
                 " satellitePvt.satPosEcef.ureMeters: %.2f,"
                 " satellitePvt.satVelEcef.velXMps: %.2f,"
                 " satellitePvt.satVelEcef.velYMps: %.2f,"
                 " satellitePvt.satVelEcef.velZMps: %.2f,"
                 " satellitePvt.satVelEcef.ureRateMps: %.2f,"
                 " satellitePvt.satClockInfo.satHardwareCodeBiasMeters: %.2f,"
                 " satellitePvt.satClockInfo.satTimeCorrectionMeters: %.2f,"
                 " satellitePvt.satClockInfo.satClkDriftMps: %.2f,"
                 " satellitePvt.ionoDelayMeters: %.2f,"
                 " satellitePvt.tropoDelayMeters: %.2f"
                 " satellitePvt.timeOfClockSeconds: %" PRIi64 ""
                 " satellitePvt.issueOfDataClock: %d"
                 " satellitePvt.timeOfEphemerisSeconds: %" PRIi64 ""
                 " satellitePvt.issueOfDataEphemeris: %d"
                 " satellitePvt.ephemerisSource: %d",
                 data.measurements[i].satellitePvt.flags,
                 data.measurements[i].satellitePvt.satPosEcef.posXMeters,
                 data.measurements[i].satellitePvt.satPosEcef.posYMeters,
                 data.measurements[i].satellitePvt.satPosEcef.posZMeters,
                 data.measurements[i].satellitePvt.satPosEcef.ureMeters,
                 data.measurements[i].satellitePvt.satVelEcef.velXMps,
                 data.measurements[i].satellitePvt.satVelEcef.velYMps,
                 data.measurements[i].satellitePvt.satVelEcef.velZMps,
                 data.measurements[i].satellitePvt.satVelEcef.ureRateMps,
                 data.measurements[i].satellitePvt.satClockInfo.satHardwareCodeBiasMeters,
                 data.measurements[i].satellitePvt.satClockInfo.satTimeCorrectionMeters,
                 data.measurements[i].satellitePvt.satClockInfo.satClkDriftMps,
                 data.measurements[i].satellitePvt.ionoDelayMeters,
                 data.measurements[i].satellitePvt.tropoDelayMeters,
                 data.measurements[i].satellitePvt.timeOfClockSeconds,
                 data.measurements[i].satellitePvt.issueOfDataClock,
                 data.measurements[i].satellitePvt.timeOfEphemerisSeconds,
                 data.measurements[i].satellitePvt.issueOfDataEphemeris,
                 data.measurements[i].satellitePvt.ephemerisSource
            );
    }
    LOC_LOGa(" Clocks Info "
             " gnssClockFlags: 0x%04x,"
             " leapSecond: %d,"
             " timeNs: %" PRId64
             " timeUncertaintyNs: %.2f,"
             " fullBiasNs: %" PRId64
             " biasNs: %.2f,"
             " biasUncertaintyNs: %.2f,"
             " driftNsps: %.2f,"
             " driftUncertaintyNsps: %.2f,"
             " hwClockDiscontinuityCount: %u,"
             " referenceSignalTypeForIsb.constellation: %u,"
             " referenceSignalTypeForIsb.carrierFrequencyHz: %.2f,"
             " referenceSignalTypeForIsb.codeType: %s",
             data.clock.gnssClockFlags,
             data.clock.leapSecond,
             data.clock.timeNs,
             data.clock.timeUncertaintyNs,
             data.clock.fullBiasNs,
             data.clock.biasNs,
             data.clock.biasUncertaintyNs,
             data.clock.driftNsps,
             data.clock.driftUncertaintyNsps,
             data.clock.hwClockDiscontinuityCount,
             data.clock.referenceSignalTypeForIsb.constellation,
             data.clock.referenceSignalTypeForIsb.carrierFrequencyHz,
             data.clock.referenceSignalTypeForIsb.codeType.c_str());
    LOC_LOGa(" ElapsedRealtime "
             " flags: 0x%08x,"
             " timestampNs: %" PRId64", "
             " timeUncertaintyNs: %.2f",
             data.elapsedRealtime.flags,
             data.elapsedRealtime.timestampNs,
             data.elapsedRealtime.timeUncertaintyNs);
    for (size_t i = 0; i < data.gnssAgcs.size(); i++) {
        LOC_LOGa("%zu : "
                 " gnssAgcs.agcLevelDb: %.2f,"
                 " gnssAgcs.constellation: %u,"
                 " gnssAgcs.carrierFrequencyHz: %" PRIi64 "",
                 i,
                 data.gnssAgcs[i].agcLevelDb,
                 data.gnssAgcs[i].constellation,
                 data.gnssAgcs[i].carrierFrequencyHz);
    }
}
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
