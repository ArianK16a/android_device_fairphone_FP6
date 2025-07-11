/*
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

#ifndef LOCATION_UTIL_H
#define LOCATION_UTIL_H

#include <LocationAPI.h>
#include <aidl/android/hardware/gnss/IGnssDebug.h>
#include <aidl/android/hardware/gnss/measurement_corrections/SingleSatCorrection.h>
#include <aidl/android/hardware/gnss/measurement_corrections/MeasurementCorrections.h>
#include <aidl/android/hardware/gnss/GnssLocation.h>
#include <aidl/android/hardware/gnss/SatellitePvt.h>
#include <aidl/android/hardware/gnss/GnssConstellationType.h>
#include <aidl/android/hardware/gnss/GnssSignalType.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
using ::aidl::android::hardware::gnss::measurement_corrections::SingleSatCorrection;
using ::aidl::android::hardware::gnss::measurement_corrections::MeasurementCorrections;
using ::aidl::android::hardware::gnss::IGnssDebug;
using ::aidl::android::hardware::gnss::GnssLocation;
using ::aidl::android::hardware::gnss::GnssConstellationType;
using ::aidl::android::hardware::gnss::SatellitePvt;
using ::aidl::android::hardware::gnss::GnssSignalType;

void convertGnssLocation(const GnssLocation& in, Location& out);
void convertGnssLocation(const Location& in, GnssLocation& out);
void convertGnssConstellationType(const GnssSvType& in, GnssConstellationType& out);
void convertGnssSvid(const GnssSv& in, int& out);
void convertGnssSvid(const GnssMeasurementsData& in, int16_t& out);
void convertGnssEphemerisType(const GnssEphemerisType& in, IGnssDebug::SatelliteEphemerisType& out);
void convertGnssEphemerisSource(const GnssEphemerisSource& in,
        SatellitePvt::SatelliteEphemerisSource& out);
void convertGnssEphemerisHealth(const GnssEphemerisHealth& in,
        IGnssDebug::SatelliteEphemerisHealth& out);
void convertSingleSatCorrections(const SingleSatCorrection& in, GnssSingleSatCorrection& out);
void convertMeasurementCorrections(const MeasurementCorrections& in,
                                   GnssMeasurementCorrections& out);
void convertGnssMeasurementsCodeType(const GnssMeasurementsCodeType& inCodeType,
                                     const char* inOtherCodeTypeName, GnssSignalType& out);
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif // LOCATION_UTIL_H
