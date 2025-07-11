/*
Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.

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

#include <LocationUtil.h>
#include <log_util.h>
#include <inttypes.h>
#include <loc_misc_utils.h>
#include <gps_extended_c.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

using ::aidl::android::hardware::gnss::ElapsedRealtime;

void convertGnssLocation(const Location& in, GnssLocation& out)
{
    memset(&out, 0, sizeof(GnssLocation));
    if (in.flags & LOCATION_HAS_LAT_LONG_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_LAT_LONG;
        out.latitudeDegrees = in.latitude;
        out.longitudeDegrees = in.longitude;
    }
    if (in.flags & LOCATION_HAS_ALTITUDE_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_ALTITUDE;
        out.altitudeMeters = in.altitude;
    }
    if (in.flags & LOCATION_HAS_SPEED_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_SPEED;
        out.speedMetersPerSec = in.speed;
    }
    if (in.flags & LOCATION_HAS_BEARING_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_BEARING;
        out.bearingDegrees = in.bearing;
    }
    if (in.flags & LOCATION_HAS_ACCURACY_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_HORIZONTAL_ACCURACY;
        out.horizontalAccuracyMeters = in.accuracy;
    }
    if (in.flags & LOCATION_HAS_VERTICAL_ACCURACY_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_VERTICAL_ACCURACY;
        out.verticalAccuracyMeters = in.verticalAccuracy;
    }
    if (in.flags & LOCATION_HAS_SPEED_ACCURACY_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_SPEED_ACCURACY;
        out.speedAccuracyMetersPerSecond = in.speedAccuracy;
    }
    if (in.flags & LOCATION_HAS_BEARING_ACCURACY_BIT) {
        out.gnssLocationFlags |= GnssLocation::HAS_BEARING_ACCURACY;
        out.bearingAccuracyDegrees = in.bearingAccuracy;
    }
    if (in.flags & LOCATION_HAS_ELAPSED_REAL_TIME_BIT) {
        out.elapsedRealtime.flags |= ElapsedRealtime::HAS_TIMESTAMP_NS;
        out.elapsedRealtime.timestampNs = in.elapsedRealTime;
        out.elapsedRealtime.flags |= ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS;
        out.elapsedRealtime.timeUncertaintyNs = in.elapsedRealTimeUnc;
        LOC_LOGd("out.elapsedRealtime.timestampNs=%" PRIi64 ""
                 " out.elapsedRealtime.timeUncertaintyNs=%lf"
                 " out.elapsedRealtime.flags=0x%X",
                 out.elapsedRealtime.timestampNs,
                 out.elapsedRealtime.timeUncertaintyNs, out.elapsedRealtime.flags);
    }

    out.timestampMillis = in.timestamp;
}

void convertGnssLocation(const GnssLocation& in, Location& out)
{
    memset(&out, 0, sizeof(out));
    if (in.gnssLocationFlags & GnssLocation::HAS_LAT_LONG) {
        out.flags |= LOCATION_HAS_LAT_LONG_BIT;
        out.latitude = in.latitudeDegrees;
        out.longitude = in.longitudeDegrees;
    }
    if (in.gnssLocationFlags & GnssLocation::HAS_ALTITUDE) {
        out.flags |= LOCATION_HAS_ALTITUDE_BIT;
        out.altitude = in.altitudeMeters;
    }
    if (in.gnssLocationFlags & GnssLocation::HAS_SPEED) {
        out.flags |= LOCATION_HAS_SPEED_BIT;
        out.speed = in.speedMetersPerSec;
    }
    if (in.gnssLocationFlags & GnssLocation::HAS_BEARING) {
        out.flags |= LOCATION_HAS_BEARING_BIT;
        out.bearing = in.bearingDegrees;
    }
    if (in.gnssLocationFlags & GnssLocation::HAS_HORIZONTAL_ACCURACY) {
        out.flags |= LOCATION_HAS_ACCURACY_BIT;
        out.accuracy = in.horizontalAccuracyMeters;
    }
    if (in.gnssLocationFlags & GnssLocation::HAS_VERTICAL_ACCURACY) {
        out.flags |= LOCATION_HAS_VERTICAL_ACCURACY_BIT;
        out.verticalAccuracy = in.verticalAccuracyMeters;
    }
    if (in.gnssLocationFlags & GnssLocation::HAS_SPEED_ACCURACY) {
        out.flags |= LOCATION_HAS_SPEED_ACCURACY_BIT;
        out.speedAccuracy = in.speedAccuracyMetersPerSecond;
    }
    if (in.gnssLocationFlags & GnssLocation::HAS_BEARING_ACCURACY) {
        out.flags |= LOCATION_HAS_BEARING_ACCURACY_BIT;
        out.bearingAccuracy = in.bearingAccuracyDegrees;
    }

    out.timestamp = static_cast<uint64_t>(in.timestampMillis);
}

void convertGnssConstellationType(const GnssSvType& in, GnssConstellationType& out)
{
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

void convertGnssSvid(const GnssSv& in, int& out)
{
    switch (in.type) {
        case GNSS_SV_TYPE_GPS:
            out = in.svId;
            break;
        case GNSS_SV_TYPE_SBAS:
            out = in.svId;
            break;
        case GNSS_SV_TYPE_GLONASS:
            if (!isGloSlotUnknown(in.svId)) { // OSN is known
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

void convertGnssSvid(const GnssMeasurementsData& in, int16_t& out)
{
    switch (in.svType) {
        case GNSS_SV_TYPE_GPS:
            out = in.svId;
            break;
        case GNSS_SV_TYPE_SBAS:
            out = in.svId;
            break;
        case GNSS_SV_TYPE_GLONASS:
            if (!isGloSlotUnknown(in.svId)) { // OSN is known
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

void convertGnssEphemerisType(const GnssEphemerisType& in, IGnssDebug::SatelliteEphemerisType& out)
{
    switch (in) {
        case GNSS_EPH_TYPE_EPHEMERIS:
            out = IGnssDebug::SatelliteEphemerisType::EPHEMERIS;
            break;
        case GNSS_EPH_TYPE_ALMANAC:
            out = IGnssDebug::SatelliteEphemerisType::ALMANAC_ONLY;
            break;
        case GNSS_EPH_TYPE_UNKNOWN:
        default:
            out = IGnssDebug::SatelliteEphemerisType::NOT_AVAILABLE;
            break;
    }
}

void convertGnssEphemerisSource(const GnssEphemerisSource& in,
        SatellitePvt::SatelliteEphemerisSource& out)
{
    switch (in) {
        case GNSS_EPH_SOURCE_DEMODULATED:
            out = SatellitePvt::SatelliteEphemerisSource::DEMODULATED;
            break;
        case GNSS_EPH_SOURCE_SUPL_PROVIDED:
            out = SatellitePvt::SatelliteEphemerisSource::SERVER_NORMAL;
            break;
        case GNSS_EPH_SOURCE_OTHER_SERVER_PROVIDED:
            out = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM;
            break;
        case GNSS_EPH_SOURCE_LOCAL:
        case GNSS_EPH_SOURCE_UNKNOWN:
        default:
            out = SatellitePvt::SatelliteEphemerisSource::OTHER;
            break;
    }
}

void convertGnssEphemerisHealth(const GnssEphemerisHealth& in,
        IGnssDebug::SatelliteEphemerisHealth& out)
{
    switch (in) {
        case GNSS_EPH_HEALTH_GOOD:
            out = IGnssDebug::SatelliteEphemerisHealth::GOOD;
            break;
        case GNSS_EPH_HEALTH_BAD:
            out = IGnssDebug::SatelliteEphemerisHealth::BAD;
            break;
        case GNSS_EPH_HEALTH_UNKNOWN:
        default:
            out = IGnssDebug::SatelliteEphemerisHealth::UNKNOWN;
            break;
    }
}

void convertSingleSatCorrections(const SingleSatCorrection& in, GnssSingleSatCorrection& out)
{
    out.flags = GNSS_MEAS_CORR_UNKNOWN_BIT;
    if (in.singleSatCorrectionFlags &
            (SingleSatCorrection::SINGLE_SAT_CORRECTION_HAS_SAT_IS_LOS_PROBABILITY)) {
        out.flags |= GNSS_MEAS_CORR_HAS_SAT_IS_LOS_PROBABILITY_BIT;
    }
    if (in.singleSatCorrectionFlags &
            (SingleSatCorrection::SINGLE_SAT_CORRECTION_HAS_COMBINED_EXCESS_PATH_LENGTH)) {
        out.flags |= GNSS_MEAS_CORR_HAS_EXCESS_PATH_LENGTH_BIT;
    }
    if (in.singleSatCorrectionFlags &
            (SingleSatCorrection::SINGLE_SAT_CORRECTION_HAS_COMBINED_EXCESS_PATH_LENGTH_UNC)) {
        out.flags |= GNSS_MEAS_CORR_HAS_EXCESS_PATH_LENGTH_UNC_BIT;
    }
    switch (in.constellation) {
    case (GnssConstellationType::GPS):
        out.svType = GNSS_SV_TYPE_GPS;
        break;
    case (GnssConstellationType::SBAS):
        out.svType = GNSS_SV_TYPE_SBAS;
        break;
    case (GnssConstellationType::GLONASS):
        out.svType = GNSS_SV_TYPE_GLONASS;
        break;
    case (GnssConstellationType::QZSS):
        out.svType = GNSS_SV_TYPE_QZSS;
        break;
    case (GnssConstellationType::BEIDOU):
        out.svType = GNSS_SV_TYPE_BEIDOU;
        break;
    case (GnssConstellationType::GALILEO):
        out.svType = GNSS_SV_TYPE_GALILEO;
        break;
    case (GnssConstellationType::IRNSS):
        out.svType = GNSS_SV_TYPE_NAVIC;
        break;
    case (GnssConstellationType::UNKNOWN):
    default:
        out.svType = GNSS_SV_TYPE_UNKNOWN;
        break;
    }
    out.svId = in.svid;
    out.carrierFrequencyHz = in.carrierFrequencyHz;
    out.probSatIsLos = in.probSatIsLos;
    out.excessPathLengthMeters = in.combinedExcessPathLengthMeters;
    out.excessPathLengthUncertaintyMeters = in.combinedExcessPathLengthUncertaintyMeters;
}

void convertMeasurementCorrections(const MeasurementCorrections& in,
                                   GnssMeasurementCorrections& out)
{
    memset(&out, 0, sizeof(GnssMeasurementCorrections));
    out.latitudeDegrees = in.latitudeDegrees;
    out.longitudeDegrees = in.longitudeDegrees;
    out.altitudeMeters = in.altitudeMeters;
    out.horizontalPositionUncertaintyMeters = in.horizontalPositionUncertaintyMeters;
    out.verticalPositionUncertaintyMeters = in.verticalPositionUncertaintyMeters;
    out.toaGpsNanosecondsOfWeek = in.toaGpsNanosecondsOfWeek;

    for (int i = 0; i < in.satCorrections.size(); i++) {
        GnssSingleSatCorrection gnssSingleSatCorrection = {};

        convertSingleSatCorrections(in.satCorrections[i], gnssSingleSatCorrection);
        out.satCorrections.push_back(gnssSingleSatCorrection);
    }
}

void convertGnssMeasurementsCodeType(
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

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
