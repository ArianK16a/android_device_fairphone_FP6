/* Copyright (c) 2013-2017, 2020-2021 The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

/*
Changes from Qualcomm Innovation Center are provided under the following license:

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

#ifndef GPS_EXTENDED_H
#define GPS_EXTENDED_H

/**
 * @file
 * @brief C++ declarations for GPS types
 */

#include <gps_extended_c.h>
#if defined(USE_GLIB) || defined(OFF_TARGET)
#include <string.h>
#endif
#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



struct LocPosMode
{
    LocPositionMode mode;
    LocGpsPositionRecurrence recurrence;
    uint32_t min_interval;
    uint32_t preferred_accuracy;
    uint32_t preferred_time;
    bool share_position;
    char credentials[14];
    char provider[8];
    GnssPowerMode powerMode;
    uint32_t timeBetweenMeasurements;
    LocPosMode(LocPositionMode m, LocGpsPositionRecurrence recr,
               uint32_t gap, uint32_t accu, uint32_t time,
               bool sp, const char* cred, const char* prov,
               GnssPowerMode pMode = GNSS_POWER_MODE_DEFAULT,
               uint32_t tbm = 0) :
        mode(m), recurrence(recr),
        min_interval(gap < GPS_MIN_POSSIBLE_FIX_INTERVAL_MS ?
                     GPS_MIN_POSSIBLE_FIX_INTERVAL_MS : gap),
        preferred_accuracy(accu), preferred_time(time),
        share_position(sp), powerMode(pMode),
        timeBetweenMeasurements(tbm) {
        memset(credentials, 0, sizeof(credentials));
        memset(provider, 0, sizeof(provider));
        if (NULL != cred) {
            memcpy(credentials, cred, sizeof(credentials)-1);
        }
        if (NULL != prov) {
            memcpy(provider, prov, sizeof(provider)-1);
        }
    }

    inline LocPosMode() :
        mode(LOC_POSITION_MODE_MS_BASED),
        recurrence(LOC_GPS_POSITION_RECURRENCE_PERIODIC),
        min_interval(GPS_DEFAULT_FIX_INTERVAL_MS),
        preferred_accuracy(50), preferred_time(120000),
        share_position(true), powerMode(GNSS_POWER_MODE_DEFAULT),
        timeBetweenMeasurements(GPS_DEFAULT_FIX_INTERVAL_MS) {
        memset(credentials, 0, sizeof(credentials));
        memset(provider, 0, sizeof(provider));
    }

    inline bool equals(const LocPosMode &anotherMode) const
    {
        return anotherMode.mode == mode &&
            anotherMode.recurrence == recurrence &&
            anotherMode.min_interval == min_interval &&
            anotherMode.preferred_accuracy == preferred_accuracy &&
            anotherMode.preferred_time == preferred_time &&
            anotherMode.powerMode == powerMode &&
            anotherMode.timeBetweenMeasurements == timeBetweenMeasurements &&
            !strncmp(anotherMode.credentials, credentials, sizeof(credentials)-1) &&
            !strncmp(anotherMode.provider, provider, sizeof(provider)-1);
    }

    void logv() const;
};

/*
* Encapsulates the parameters (client name, preferred subscription ID and preferred APN
* for backhaul connect.
*/
struct BackhaulContext {
    std::string clientName;
    uint16_t prefSub;
    std::string prefApn;
    uint16_t prefIpType;
    inline bool operator ==(const BackhaulContext& i1) const {
        // we do not support multiple request from same client
        return i1.clientName == clientName;
    }

    // Custom hash function for BackhaulContext set.
    struct hash {
        inline size_t operator()(BackhaulContext const& i) const {
            // Index with client name base hash, as we support just one BackhaulRequest
            // with the same client name.
            return (std::hash<std::string>()(i.clientName));
        }
    };
};

/* Engine Debug data Information */

#define GNSS_MAX_SV_INFO_LIST_SIZE 176

typedef struct {
    uint16_t gnssSvId;
    /**<   GNSS SV ID. Range:
      - GPS --     1 to 32
      - GLONASS -- 65 to 96
      - SBAS --    120 to 158 and 183 to 191
      - QZSS --    193 to 197
      - BDS --     201 to 263
      - Galileo -- 301 to 336
      - NavIC --   401 to 414 */

    uint8_t type;
    /**<   Navigation data type */

    uint8_t src;
    /**<   Navigation data source.*/
    int32_t age;
    /**<   Age of navigation data.
      - Units: Seconds */
} GnssNavDataInfo;


typedef struct {
    uint32_t hours;
    /**<   Hours of timestamp */

    uint32_t mins;
    /**<   Minutes of timestamp */

    float secs;
    /**<   Seconds of timestamp */
} GnssTimeInfo;

typedef enum {
    GPS_XTRA_VALID_BIT     = 1 << 0,
    GLONASS_XTRA_VALID_BIT = 1 << 1,
    BDS_XTRA_VALID_BIT     = 1 << 2,
    GAL_XTRA_VALID_BIT     = 1 << 3,
    QZSS_XTRA_VALID_BIT    = 1 << 4,
    NAVIC_XTRA_VALID_BIT   = 1 << 5
} GnssXtraValidMaskBits;

typedef struct {

    uint8_t timeValid;
    /**<Time validity >*/

    uint16_t gpsWeek;
    /**<   Full GPS week */

    uint32_t gpsTowMs;
    /**<   GPS time of week.
      - Units: milliseconds  */

    uint8_t sourceOfTime;
    /**<   Source of the time information*/

    float clkTimeUnc;
    /**<   Single-sided maximum time bias uncertainty.
      - Units: milliseconds */

    float clkFreqBias;
    /**<   Receiver clock frequency bias. \n
      - Units -- ppb */

    float clkFreqUnc;
    /**<   Receiver clock frequency uncertainty. \n
      - Units -- ppb */

    uint8_t xoState;
    /**<   XO calibration. */

    uint32_t rcvrErrRecovery;
    /**<   Error recovery reason. */

    Gnss_LeapSecondInfoStructType leapSecondInfo;
    /**<   Leap second information. */

    std::vector<int32_t> jammerInd;
    /**<   Jammer indicator of each signal. */

    uint64_t jammedSignalsMask;
    /* Jammer Signal Mask */

    GnssTimeInfo epiTime;
    /**<   UTC Time associated with EPI. */

    uint8_t epiValidity;
    /**<   Epi validity >*/

    float epiLat;
    /**<   EPI Latitude. - Units: Radians
        valid if 0th bit set in epiValidity*/

    float epiLon;
    /**<   EPI Longitude. - Units: Radians
        valid if 0th bit set in epiValidity*/

    float epiAlt;
    /**<   EPI Altitude. - Units: Meters
        valid if 1st bit set in epiValidity*/

    float epiHepe;
    /**<   EPI Horizontal Estimated Position Error.
      - Units: Meters
        valid if 0th bit set in epiValidity*/

    float epiAltUnc;
    /**<   EPI Altitude Uncertainty.
      - Units: Meters
        valid if 1st bit set in epiValidity*/

    uint8_t epiSrc;
    /**<   EPI Source
        valid if 2nd bit set in epiValidity*/

    GnssTimeInfo bestPosTime;
    /**<   UTC Time associated with Best Position. */

    float bestPosLat;
    /**<   Best Position Latitude.
      - Units: Radians */

    float bestPosLon;
    /**<   Best Position Longitude.
      - Units: Radians */

    float bestPosAlt;
    /**<   Best Position Altitude.
      - Units: Meters */

    float bestPosHepe;
    /**<   Best Position Horizontal Estimated Position Error.
      - Units: Meters */

    float bestPosAltUnc;
    /**<   Best Position Altitude Uncertainty.
      - Units: Meters */

    GnssTimeInfo xtraInfoTime;
    /**<   UTC time when XTRA debug information was generated. */

    uint8_t xtraValidMask;
    /**<xtra valid mask>*/

    uint32_t gpsXtraAge;
    /**<   Age of GPS XTRA data.
      - Units: Seconds */

    uint32_t gloXtraAge;
    /**<   Age of GLONASS XTRA data.
      - Units: Seconds */

    uint32_t bdsXtraAge;
    /**<   Age of BDS XTRA data.
      - Units: Seconds */

    uint32_t galXtraAge;
    /**<   Age of GAL XTRA data.
      - Units: Seconds */

    uint32_t qzssXtraAge;
    /**<   Age of QZSS XTRA data.
      - Units: Seconds */

    uint32_t navicXtraAge;
    /**<   Age of NAVIC XTRA data.
      - Units: Seconds */

    uint32_t gpsXtraMask;
    /**<   Specifies the GPS SV mask.
      - SV ID mapping: SV 1 maps to bit 0. */

    uint32_t gloXtraMask;
    /**<   Specifies the GLONASS SV mask.
      - SV ID mapping: SV 65 maps to bit 0. */

    uint64_t bdsXtraMask;
    /**<   Specifies the BDS SV mask.
      - SV ID mapping: SV 201 maps to bit 0. */

    uint64_t galXtraMask;
    /**<   Specifies the Galileo SV mask.
      - SV ID mapping: SV 301 maps to bit 0. */

    uint8_t qzssXtraMask;
    /**<   Specifies the QZSS SV mask.
      - SV ID mapping: SV 193 maps to bit 0 */

    uint32_t navicXtraMask;
    /**<   Specifies the NAVIC SV mask.
      - SV ID mapping: SV 401 maps to bits 0. */

    GnssTimeInfo ephInfoTime;
    /**<   UTC time when ephemeris debug information was generated. */

    uint32_t gpsEphMask;
    /**<   Specifies the GPS SV mask.
      - SV ID mapping: SV 1 maps to bit 0. */

    uint32_t gloEphMask;
    /**<   Specifies the GLONASS SV mask.
      - SV ID mapping: SV 65 maps to bit 0. */

    uint64_t bdsEphMask;
    /**<   Specifies the BDS SV mask.
      - SV ID mapping: SV 201 maps to bit 0. */

    uint64_t galEphMask;
    /**<   Specifies the Galileo SV mask.
      - SV ID mapping: SV 301 maps to bit 0. */

    uint8_t qzssEphMask;
    /**<   Specifies the QZSS SV mask.
      - SV ID mapping: SV 193 maps to bit 0 */

    uint32_t navicEphMask;
    /**<   Specifies the NAVIC SV mask.
      - SV ID mapping: SV 401 maps to bits 0. */

    GnssTimeInfo healthInfoTime;
    /**<   UTC time when SV health information was generated. */

    uint32_t gpsHealthUnknownMask;
    /**<   Specifies the GPS SV mask.
      - SV ID mapping: SV 1 maps to bit 0. */

    uint32_t gloHealthUnknownMask;
    /**<   Specifies the GLONASS SV mask.
      - SV ID mapping: SV 65 maps to bit 0. */

    uint64_t bdsHealthUnknownMask;
    /**<   Specifies the BDS SV mask.
      - SV ID mapping: SV 201 maps to bit 0. */

    uint64_t galHealthUnknownMask;
    /**<   Specifies the Galileo SV mask.
      - SV ID mapping: SV 301 maps to bit 0. */

    uint8_t qzssHealthUnknownMask;
    /**<   Specifies the QZSS SV mask.
      - SV ID mapping: SV 193 maps to bit 0 */

    uint32_t navicHealthUnknownMask;
    /**<   Specifies the NAVIC SV mask.
      - SV ID mapping: SV 401 maps to bits 0. */

    uint32_t gpsHealthGoodMask;
    /**<   Specifies the GPS SV mask.
      - SV ID mapping: SV 1 maps to bit 0. */

    uint32_t gloHealthGoodMask;
    /**<   Specifies the GLONASS SV mask.
      - SV ID mapping: SV 65 maps to bit 0. */

    uint64_t bdsHealthGoodMask;
    /**<   Specifies the BDS SV mask.
      - SV ID mapping: SV 201 maps to bit 0. */

    uint64_t galHealthGoodMask;
    /**<   Specifies the Galileo SV mask.
      - SV ID mapping: SV 301 maps to bit 0. */

    uint8_t qzssHealthGoodMask;
    /**<   Specifies the QZSS SV mask.
      - SV ID mapping: SV 193 maps to bit 0 */

    uint32_t navicHealthGoodMask;
    /**<   Specifies the NAVIC SV mask.
      - SV ID mapping: SV 401 maps to bits 0. */

    uint32_t gpsHealthBadMask;
    /**<   Specifies the GPS SV mask.
      - SV ID mapping: SV 1 maps to bit 0. */

    uint32_t gloHealthBadMask;
    /**<   Specifies the GLONASS SV mask.
      - SV ID mapping: SV 65 maps to bit 0. */

    uint64_t bdsHealthBadMask;
    /**<   Specifies the BDS SV mask.
      - SV ID mapping: SV 201 maps to bit 0. */

    uint64_t galHealthBadMask;
    /**<   Specifies the Galileo SV mask.
      - SV ID mapping: SV 301 maps to bit 0. */

    uint8_t qzssHealthBadMask;
    /**<   Specifies the QZSS SV mask.
      - SV ID mapping: SV 193 maps to bit 0 */

    uint32_t navicHealthBadMask;
    /**<   Specifies the NAVIC SV mask.
      - SV ID mapping: SV 401 maps to bits 0. */

    GnssTimeInfo fixInfoTime;
    /**<   UTC time when fix information was generated. */

    uint32_t fixInfoMask;
    /**<   Fix Information Mask*/

    GnssTimeInfo navDataTime;
    /**<   UTC time when navigation data was generated. */

    GnssNavDataInfo navData[GNSS_MAX_SV_INFO_LIST_SIZE];
    /**<   Satellite navigation data. */

    GnssTimeInfo fixStatusTime;
    /**<   UTC time when fix status was generated. */

    uint32_t fixStatusMask;
    /**<   Fix Status Mask */

    uint32_t fixHepeLimit;
    /**<   Session HEPE Limit.
      - Units: Meters */
} GnssEngineDebugDataInfo;

/** Represents gps location extended. */
typedef struct {
    /** set to sizeof(GpsLocationExtended) */
    uint32_t          size;
    /** Contains GpsLocationExtendedFlags bits. */
    uint64_t        flags;
    /** Contains the Altitude wrt mean sea level */
    float           altitudeMeanSeaLevel;
    /** Contains Position Dilusion of Precision. */
    float           pdop;
    /** Contains Horizontal Dilusion of Precision. */
    float           hdop;
    /** Contains Vertical Dilusion of Precision. */
    float           vdop;
    /** Contains Magnetic Deviation. */
    float           magneticDeviation;
    /** vertical uncertainty in meters
     *  confidence level is at 68% */
    float           vert_unc;
    /** horizontal speed uncertainty in m/s
     *  confidence level is at 68% */
    float           speed_unc;
    /** heading uncertainty in degrees (0 to 359.999)
     *  confidence level is at 68% */
    float           bearing_unc;
    /** horizontal reliability. */
    LocReliability  horizontal_reliability;
    /** vertical reliability. */
    LocReliability  vertical_reliability;
    /**  Horizontal Elliptical Uncertainty (Semi-Major Axis)
     *   Confidence level is at 39% */
    float           horUncEllipseSemiMajor;
    /**  Horizontal Elliptical Uncertainty (Semi-Minor Axis)
     *   Confidence level is at 39% */
    float           horUncEllipseSemiMinor;
    /**  Elliptical Horizontal Uncertainty Azimuth */
    float           horUncEllipseOrientAzimuth;

    Gnss_ApTimeStampStructType               timeStamp;
    /** Gnss sv used in position data */
    GnssSvUsedInPosition gnss_sv_used_ids;
    /** Gnss sv used in position data for multiband */
    GnssSvMbUsedInPosition gnss_mb_sv_used_ids;
    /** Nav solution mask to indicate sbas corrections */
    LocNavSolutionMask  navSolutionMask;
    /** Position technology used in computing this fix */
    LocPosTechMask tech_mask;
    /** SV Info source used in computing this fix */
    LocSvInfoSource sv_source;
    /** Body Frame Dynamics: 4wayAcceleration and pitch set with validity */
    GnssLocationPositionDynamics bodyFrameData;
    /** GPS Time */
    GPSTimeStruct gpsTime;
    GnssSystemTime gnssSystemTime;
    /** Dilution of precision associated with this position*/
    LocExtDOP extDOP;
    /** North standard deviation.
        Unit: Meters */
    float northStdDeviation;
    /** East standard deviation.
        Unit: Meters */
    float eastStdDeviation;
    /** North Velocity.
        Unit: Meters/sec */
    float northVelocity;
    /** East Velocity.
        Unit: Meters/sec */
    float eastVelocity;
    /** Up Velocity.
        Unit: Meters/sec */
    float upVelocity;
    /** North Velocity standard deviation.
     *  Unit: Meters/sec.
     *  Confidence level is at 68% */
    float northVelocityStdDeviation;
    /** East Velocity standard deviation.
     *  Unit: Meters/sec
     *  Confidence level is at 68%   */
    float eastVelocityStdDeviation;
    /** Up Velocity standard deviation
     *  Unit: Meters/sec
     *  Confidence level is at 68% */
    float upVelocityStdDeviation;
    /** Estimated clock bias. Unit: Nano seconds */
    float clockbiasMeter;
    /** Estimated clock bias std deviation. Unit: Nano seconds */
    float clockBiasStdDeviationMeter;
    /** Estimated clock drift. Unit: Meters/sec */
    float clockDrift;
    /** Estimated clock drift std deviation. Unit: Meters/sec */
    float clockDriftStdDeviation;
    /** Number of valid reference stations. Range:[0-4] */
    uint8_t numValidRefStations;
    /** Reference station(s) number */
    uint16_t referenceStation[4];
    /** Number of measurements received for use in fix.
        Shall be used as maximum index in-to svUsageInfo[].
        Set to 0, if svUsageInfo reporting is not supported.
        Range: 0-EP_GNSS_MAX_MEAS */
    uint8_t numOfMeasReceived;
    /** Measurement Usage Information */
    GpsMeasUsageInfo measUsageInfo[GNSS_SV_MAX];
    /** Leap Seconds */
    uint8_t leapSeconds;
    /** Time uncertainty in milliseconds,
     *  SPE engine: confidence level is 99%
     *  all other engines: confidence level is not specified */
    float timeUncMs;
    /** Heading Rate is in NED frame.
        Range: 0 to 359.999. 946
        Unit: Degrees per Seconds */
    float headingRateDeg;
    /** Sensor calibration confidence percent. Range: 0 - 100 */
    uint8_t calibrationConfidence;
    DrCalibrationStatusMask calibrationStatus;
    /** location engine type. When the fix. when the type is set to
        LOC_ENGINE_SRC_FUSED, the fix is the propagated/aggregated
        reports from all engines running on the system (e.g.:
        DR/SPE/PPE). To check which location engine contributes to
        the fused output, check for locOutputEngMask. */
    LocOutputEngineType locOutputEngType;
    /** when loc output eng type is set to fused, this field
        indicates the set of engines contribute to the fix. */
    PositioningEngineMask locOutputEngMask;

    /**  DGNSS Correction Source for position report: RTCM, 3GPP
     *   etc. */
    LocDgnssCorrectionSourceType dgnssCorrectionSourceType;

    /**  If DGNSS is used, the SourceID is a 32bit number identifying
     *   the DGNSS source ID */
    uint32_t dgnssCorrectionSourceID;

    /** If DGNSS is used, which constellation was DGNSS used for to
     *  produce the pos report. */
    GnssConstellationTypeMask dgnssConstellationUsage;

    /** If DGNSS is used, DGNSS Reference station ID used for
     *  position report */
    uint16_t dgnssRefStationId;

    /**  If DGNSS is used, DGNSS data age in milli-seconds  */
    uint32_t dgnssDataAgeMsec;

    /** When robust location is enabled, this field
     * will how well the various input data considered for
     * navigation solution conform to expectations.
     * Range: 0 (least conforming) to 1 (most conforming) */
    float conformityIndex;
    GnssLocationPositionDynamicsExt bodyFrameDataExt;
    /** VRR-based latitude/longitude/altitude */
    LLAInfo llaVRPBased;
    /** VRR-based east, north, and up velocity */
    float enuVelocityVRPBased[3];
    DrSolutionStatusMask drSolutionStatusMask;
    /** When this field is valid, it will indicates whether altitude
     *  is assumed or calculated.
     *  false: Altitude is calculated.
     *  true:  Altitude is assumed; there may not be enough
     *         satellites to determine the precise altitude. */
    bool altitudeAssumed;

    /** Integrity risk used for protection level parameters.
     *  Unit of 2.5e-10. Valid range is [1 to (4e9-1)].
     *  Other values means integrity risk is disabled and
     *  GnssLocation::protectAlongTrack,
     *  GnssLocation::protectCrossTrack and
     *  GnssLocation::protectVertical will not be available.
     */
    uint32_t integrityRiskUsed;
    /** Along-track protection level at specified integrity risk, in
     *  unit of meter.
     */
    float    protectAlongTrack;
   /** Cross-track protection level at specified integrity risk, in
     *  unit of meter.
     */
    float    protectCrossTrack;
    /** Vertical component protection level at specified integrity
     *  risk, in unit of meter.
     */
    float    protectVertical;
    /** System Tick at GPS Time */
    uint64_t systemTick;
    /** Uncertainty for System Tick at GPS Time in milliseconds   */
    float systemTickUnc;

    // number of dgnss station id that is valid in dgnssStationId array
    uint32_t  numOfDgnssStationId;
    // List of DGNSS station IDs providing corrections.
    //   Range:
    //   - SBAS --  120 to 158 and 183 to 191.
    //   - Monitoring station -- 1000-2023 (Station ID biased by 1000).
    //   - Other values reserved.
    uint16_t dgnssStationId[DGNSS_STATION_ID_MAX];

    /** Uncertainty for the GNSS leap second.
     *  Units -- Seconds */
    uint8_t leapSecondsUnc;
    /** helper function to check sanity of accurate time */
    bool isReportTimeAccurate() const {
        return ((gnssSystemTime.hasAccurateGpsTime() == true) &&
            (flags & GPS_LOCATION_EXTENDED_HAS_SYSTEM_TICK) &&
            (systemTick != 0) &&
            (flags & GPS_LOCATION_EXTENDED_HAS_SYSTEM_TICK_UNC) &&
            (systemTickUnc != 0.0f));
    }

} GpsLocationExtended;

// struct that contains complete position info from engine
typedef struct {
    UlpLocation location;
    GpsLocationExtended locationExtended;
    enum loc_sess_status sessionStatus;
} EngineLocationInfo;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GPS_EXTENDED_H */
