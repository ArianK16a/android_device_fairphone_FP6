/*
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
#include <aidl/android/hardware/gnss/GnssConstellationType.h>
#include <aidl/android/hardware/gnss/IGnssDebug.h>
#include "Gnss.h"
#include "GnssDebug.h"
#include <LocationUtil.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
#define GNSS_DEBUG_UNKNOWN_HORIZONTAL_ACCURACY_METERS (20000000)
#define GNSS_DEBUG_UNKNOWN_VERTICAL_ACCURACY_METERS   (20000)
#define GNSS_DEBUG_UNKNOWN_SPEED_ACCURACY_PER_SEC     (500)
#define GNSS_DEBUG_UNKNOWN_BEARING_ACCURACY_DEG       (180)

#define GNSS_DEBUG_UNKNOWN_UTC_TIME            (1483228800000LL) // 1/1/2017 00:00 GMT
#define GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MIN    (999) // 999 ns
#define GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MAX    (1.57783680E17) // 5 years in ns
#define GNSS_DEBUG_UNKNOWN_FREQ_UNC_NS_PER_SEC (2.0e5)  // ppm
GnssDebug::GnssDebug(Gnss* gnss) : mGnss(gnss) {}
  GnssDebug::~GnssDebug() {}

  ScopedAStatus GnssDebug::getDebugData(IGnssDebug::DebugData* _aidl_return) {
      LOC_LOGd("]: ");

    IGnssDebug::DebugData data = { };

    if (nullptr == mGnss) {
        LOC_LOGe("GnssDebug - Null GNSS interface");
        *_aidl_return = data;
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    // get debug report snapshot via hal interface
    GnssDebugReport reports = { };
    mGnss->getApi().locAPIGetDebugReport(reports);

    // location block
    if (reports.mLocation.mValid) {
        data.position.valid = true;
        data.position.latitudeDegrees = reports.mLocation.mLocation.latitude;
        data.position.longitudeDegrees = reports.mLocation.mLocation.longitude;
        data.position.altitudeMeters = reports.mLocation.mLocation.altitude;

        data.position.speedMetersPerSec =
            (double)(reports.mLocation.mLocation.speed);
        data.position.bearingDegrees =
            (double)(reports.mLocation.mLocation.bearing);
        data.position.horizontalAccuracyMeters =
            (double)(reports.mLocation.mLocation.accuracy);
        data.position.verticalAccuracyMeters =
            reports.mLocation.verticalAccuracyMeters;
        data.position.speedAccuracyMetersPerSecond =
            reports.mLocation.speedAccuracyMetersPerSecond;
        data.position.bearingAccuracyDegrees =
            reports.mLocation.bearingAccuracyDegrees;

        timeval tv_now, tv_report;
        tv_report.tv_sec  = reports.mLocation.mUtcReported.tv_sec;
        tv_report.tv_usec = reports.mLocation.mUtcReported.tv_nsec / 1000ULL;
        gettimeofday(&tv_now, NULL);
        data.position.ageSeconds =
            (tv_now.tv_sec - tv_report.tv_sec) +
            (float)((tv_now.tv_usec - tv_report.tv_usec)) / 1000000;
    }
    else {
        data.position.valid = false;
    }

    if (data.position.horizontalAccuracyMeters <= 0 ||
        data.position.horizontalAccuracyMeters > GNSS_DEBUG_UNKNOWN_HORIZONTAL_ACCURACY_METERS) {
        data.position.horizontalAccuracyMeters = GNSS_DEBUG_UNKNOWN_HORIZONTAL_ACCURACY_METERS;
    }
    if (data.position.verticalAccuracyMeters <= 0 ||
        data.position.verticalAccuracyMeters > GNSS_DEBUG_UNKNOWN_VERTICAL_ACCURACY_METERS) {
        data.position.verticalAccuracyMeters = GNSS_DEBUG_UNKNOWN_VERTICAL_ACCURACY_METERS;
    }
    if (data.position.speedAccuracyMetersPerSecond <= 0 ||
        data.position.speedAccuracyMetersPerSecond > GNSS_DEBUG_UNKNOWN_SPEED_ACCURACY_PER_SEC) {
        data.position.speedAccuracyMetersPerSecond = GNSS_DEBUG_UNKNOWN_SPEED_ACCURACY_PER_SEC;
    }
    if (data.position.bearingAccuracyDegrees <= 0 ||
        data.position.bearingAccuracyDegrees > GNSS_DEBUG_UNKNOWN_BEARING_ACCURACY_DEG) {
        data.position.bearingAccuracyDegrees = GNSS_DEBUG_UNKNOWN_BEARING_ACCURACY_DEG;
    }

    // time block
    if (reports.mTime.mValid) {
        data.time.timeEstimateMs = reports.mTime.timeEstimate;
        data.time.timeUncertaintyNs = reports.mTime.timeUncertaintyNs;
        data.time.frequencyUncertaintyNsPerSec =
            reports.mTime.frequencyUncertaintyNsPerSec;
    }

    if (data.time.timeEstimateMs < GNSS_DEBUG_UNKNOWN_UTC_TIME) {
        data.time.timeEstimateMs = GNSS_DEBUG_UNKNOWN_UTC_TIME;
    }
    if (data.time.timeUncertaintyNs <= 0) {
        data.time.timeUncertaintyNs = (float)GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MIN;
    }
    else if (data.time.timeUncertaintyNs > GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MAX) {
        data.time.timeUncertaintyNs = (float)GNSS_DEBUG_UNKNOWN_UTC_TIME_UNC_MAX;
    }
    if (data.time.frequencyUncertaintyNsPerSec <= 0 ||
        data.time.frequencyUncertaintyNsPerSec > (float)GNSS_DEBUG_UNKNOWN_FREQ_UNC_NS_PER_SEC) {
        data.time.frequencyUncertaintyNsPerSec = (float)GNSS_DEBUG_UNKNOWN_FREQ_UNC_NS_PER_SEC;
    }

    // satellite data block
    IGnssDebug::SatelliteData s = { };
    std::vector<IGnssDebug::SatelliteData> s_array;

    for (uint32_t i=0; i<reports.mSatelliteInfo.size(); i++) {
        memset(&s, 0, sizeof(s));
        s.svid = reports.mSatelliteInfo[i].svid;
        convertGnssConstellationType(
            reports.mSatelliteInfo[i].constellation, s.constellation);
        convertGnssEphemerisType(
            reports.mSatelliteInfo[i].mEphemerisType, s.ephemerisType);
        convertGnssEphemerisSource(
            reports.mSatelliteInfo[i].mEphemerisSource, s.ephemerisSource);
        convertGnssEphemerisHealth(
            reports.mSatelliteInfo[i].mEphemerisHealth, s.ephemerisHealth);

        s.ephemerisAgeSeconds =
            reports.mSatelliteInfo[i].ephemerisAgeSeconds;
        s.serverPredictionIsAvailable =
            reports.mSatelliteInfo[i].serverPredictionIsAvailable;
        s.serverPredictionAgeSeconds =
            reports.mSatelliteInfo[i].serverPredictionAgeSeconds;

        s_array.push_back(s);
    }
    data.satelliteDataArray = s_array;

        *_aidl_return = data;
      return ScopedAStatus::ok();
  }
}
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
