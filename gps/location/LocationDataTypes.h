/* Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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

#ifndef LOCATIONDATATYPES_H
#define LOCATIONDATATYPES_H

#include <vector>
#include <stdint.h>
#include <functional>
#include <list>
#include <string.h>
#include <string>
#include <time.h>

#define GNSS_NI_REQUESTOR_MAX  (256)
#define GNSS_NI_MESSAGE_ID_MAX (2048)
#define GNSS_SV_MAX            (176)
#define GNSS_MEASUREMENTS_MAX  (144)
#define GNSS_BANDS_MAX         (32)
#define DGNSS_STATION_ID_MAX   (3)
#define GNSS_UTC_TIME_OFFSET   (3657)

#define GNSS_BUGREPORT_GPS_MIN    (1)
#define GNSS_BUGREPORT_SBAS_MIN   (120)
#define GNSS_BUGREPORT_GLO_MIN    (1)
#define GNSS_BUGREPORT_QZSS_MIN   (193)
#define GNSS_BUGREPORT_BDS_MIN    (1)
#define GNSS_BUGREPORT_GAL_MIN    (1)
#define GNSS_BUGREPORT_NAVIC_MIN  (1)

#define GNSS_MAX_NAME_LENGTH    (8)
#define XTRA_STATS_DL_REASON_CODE_MAX_LEN (64)

#define UNKNOWN_GPS_WEEK_NUM    (65535)
#define REAL_TIME_ESTIMATOR_TIME_UNC_THRESHOLD_MSEC (20.0f)

/**  Maximum number of satellites in an ephemeris report.  */
#define GNSS_EPHEMERIS_LIST_MAX_SIZE_V02 32

enum LocationError {
    LOCATION_ERROR_SUCCESS = 0,
    LOCATION_ERROR_GENERAL_FAILURE,
    LOCATION_ERROR_CALLBACK_MISSING,
    LOCATION_ERROR_INVALID_PARAMETER,
    LOCATION_ERROR_ID_EXISTS,
    LOCATION_ERROR_ID_UNKNOWN,
    LOCATION_ERROR_ALREADY_STARTED,
    LOCATION_ERROR_GEOFENCES_AT_MAX,
    LOCATION_ERROR_NOT_SUPPORTED,
    LOCATION_ERROR_TIMEOUT,
    LOCATION_ERROR_SYSTEM_NOT_READY,
    LOCATION_ERROR_EXCLUSIVE_SESSION_IN_PROGRESS,
    LOCATION_ERROR_TZ_LOCKED,
};

// Flags to indicate which values are valid in a Location
typedef uint32_t LocationFlagsMask;
enum LocationFlagsBits {
    LOCATION_HAS_LAT_LONG_BIT          = (1<<0), // location has valid latitude and longitude
    LOCATION_HAS_ALTITUDE_BIT          = (1<<1), // location has valid altitude
    LOCATION_HAS_SPEED_BIT             = (1<<2), // location has valid speed
    LOCATION_HAS_BEARING_BIT           = (1<<3), // location has valid bearing
    LOCATION_HAS_ACCURACY_BIT          = (1<<4), // location has valid accuracy
    LOCATION_HAS_VERTICAL_ACCURACY_BIT = (1<<5), // location has valid vertical accuracy
    LOCATION_HAS_SPEED_ACCURACY_BIT    = (1<<6), // location has valid speed accuracy
    LOCATION_HAS_BEARING_ACCURACY_BIT  = (1<<7), // location has valid bearing accuracy
    LOCATION_HAS_SPOOF_MASK_BIT        = (1<<8), // location has valid spoof mask
    LOCATION_HAS_ELAPSED_REAL_TIME_BIT = (1<<9), // location has valid elapsed real time
    LOCATION_HAS_CONFORMITY_INDEX_BIT  = (1<<10), // location has valid conformity index
    LOCATION_HAS_QUALITY_TYPE_BIT      = (1<<11), // location has valid quality type
    LOCATION_HAS_TECH_MASK_BIT         = (1<<12), // location has valid tech mask
    LOCATION_HAS_TIME_UNC_BIT          = (1<<13), // location has timeUncMs
    LOCATION_HAS_SYSTEM_TICK_BIT       = (1<<14), // location has system Tick for qtimer tick count
    LOCATION_HAS_GPTP_TIME_BIT         = (1<<15), // location has valid GPTP time
    LOCATION_HAS_GPTP_TIME_UNC_BIT     = (1<<16), // location has valid GPTP time Uncertainity
    LOCATION_HAS_SESSION_STATUS_BIT    = (1<<17), // location has session status
};

typedef uint16_t LocationTechnologyMask;
// mask indicating location calculations including...
enum LocationTechnologyBits{
    LOCATION_TECHNOLOGY_GNSS_BIT                     = (1<<0), // using GNSS
    LOCATION_TECHNOLOGY_CELL_BIT                     = (1<<1), // using Cell
    LOCATION_TECHNOLOGY_WIFI_BIT                     = (1<<2), // using WiFi
    LOCATION_TECHNOLOGY_SENSORS_BIT                  = (1<<3), // using Sensors
    LOCATION_TECHNOLOGY_REFERENCE_LOCATION_BIT       = (1<<4), // using reference location
    LOCATION_TECHNOLOGY_INJECTED_COARSE_POSITION_BIT = (1<<5), // using CPI
    LOCATION_TECHNOLOGY_AFLT_BIT                     = (1<<6), // AFLT
    LOCATION_TECHNOLOGY_HYBRID_BIT                   = (1<<7), // HYBRID
    LOCATION_TECHNOLOGY_PPE_BIT                      = (1<<8), // PPE
    LOCATION_TECHNOLOGY_VEH_BIT                      = (1<<9), // using vehicular data
    LOCATION_TECHNOLOGY_VIS_BIT                      = (1<<10), // using visual data
    LOCATION_TECHNOLOGY_DGNSS_BIT                    = (1<<11), // DGNSS
    LOCATION_TECHNOLOGY_HYBRID_ALE_BIT               = (1<<12), // HYBRID using ALE POS
    LOCATION_TECHNOLOGY_PDR_BIT                      = (1<<13), // PED mode
    LOCATION_TECHNOLOGY_PROPAGATED_BIT               = (1<<14), //using cached measures
};

typedef uint32_t LocationSpoofMask;
enum LocationSpoofBits {
    LOCATION_POSTION_SPOOFED             = (1<<0), // location position spoofed
    LOCATION_TIME_SPOOFED                = (1<<1), // location time spoofed
    LOCATION_NAVIGATION_DATA_SPOOFED     = (1<<2), // location navigation data spoofed
};

enum LocationReliability {
    LOCATION_RELIABILITY_NOT_SET = 0,
    LOCATION_RELIABILITY_VERY_LOW,
    LOCATION_RELIABILITY_LOW,
    LOCATION_RELIABILITY_MEDIUM,
    LOCATION_RELIABILITY_HIGH,
};

typedef uint32_t GnssLocationNavSolutionMask;
enum GnssLocationNavSolutionBits {
    // SBAS ionospheric correction is used
    LOCATION_SBAS_CORRECTION_IONO_BIT  = (1<<0),
    // SBAS fast correction is used
    LOCATION_SBAS_CORRECTION_FAST_BIT  = (1<<1),
    // SBAS long-tem correction is used
    LOCATION_SBAS_CORRECTION_LONG_BIT  = (1<<2),
    // SBAS integrity information is used
    LOCATION_SBAS_INTEGRITY_BIT        = (1<<3),
    // Position Report is DGNSS corrected
    LOCATION_NAV_CORRECTION_DGNSS_BIT  = (1<<4),
     // Position Report is RTK corrected
    LOCATION_NAV_CORRECTION_RTK_BIT    = (1<<5),
    // Position Report is PPP corrected
    LOCATION_NAV_CORRECTION_PPP_BIT    = (1<<6),
    // Posiiton Report is RTF fixed corrected
    LOCATION_NAV_CORRECTION_RTK_FIXED_BIT  = (1<<7),
    // Position report is computed with only SBAS corrected SVs.
    LOCATION_NAV_CORRECTION_ONLY_SBAS_CORRECTED_SV_USED_BIT = (1<<8)
};

typedef uint32_t GnssLocationPosDataMask;
enum GnssLocationPosDataBits {
    LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT  = (1<<0), // Navigation data has Forward Acceleration
    LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT   = (1<<1), // Navigation data has Sideward Acceleration
    LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT  = (1<<2), // Navigation data has Vertical Acceleration
    LOCATION_NAV_DATA_HAS_YAW_RATE_BIT    = (1<<3), // Navigation data has Heading Rate
    LOCATION_NAV_DATA_HAS_PITCH_BIT       = (1<<4),  // Navigation data has Body pitch
    // Navigation data has Forward Acceleration uncertainty
    LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT = (1<<5),
    // Navigation data has Sideward Acceleration uncertainty
    LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT  = (1<<6),
    // Navigation data has Vertical Acceleration uncertainty
    LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT = (1<<7),
    // Navigation data has Heading Rate uncertainty
    LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT   = (1<<8),
    // Navigation data has Body pitch uncertainty
    LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT      = (1<<9)
};

typedef uint32_t GnssLocationPosDataMaskExt;
enum GnssLocationPosDataBitsExt {
    // Navigation data has pitch rate
    LOCATION_NAV_DATA_HAS_PITCH_RATE_BIT     = (1<<0),
    // Navigation data has body pitch rate uncertainty
    LOCATION_NAV_DATA_HAS_PITCH_RATE_UNC_BIT = (1<<1),
    // Navigation data has body roll
    LOCATION_NAV_DATA_HAS_ROLL_BIT           = (1<<2),
    // Navigation data has body roll uncertainty
    LOCATION_NAV_DATA_HAS_ROLL_UNC_BIT       = (1<<3),
    // Navigation data has body rate roll
    LOCATION_NAV_DATA_HAS_ROLL_RATE_BIT      = (1<<4),
    // Navigation data has body roll rate uncertainty
    LOCATION_NAV_DATA_HAS_ROLL_RATE_UNC_BIT  = (1<<5),
    // Navigation data has body yaw
    LOCATION_NAV_DATA_HAS_YAW_BIT            = (1<<6),
    // Navigation data has body roll uncertainty
    LOCATION_NAV_DATA_HAS_YAW_UNC_BIT        = (1<<7)
};

typedef uint64_t GnssLocationInfoFlagMask;
#define LDT_GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT (1<<0)  // altitude mean sea level
#define LDT_GNSS_LOCATION_INFO_DOP_BIT (1<<1)  // pdop, hdop and vdop
#define LDT_GNSS_LOCATION_INFO_MAGNETIC_DEVIATION_BIT (1<<2)  // magnetic deviation
#define LDT_GNSS_LOCATION_INFO_HOR_RELIABILITY_BIT (1<<3)  // horizontal reliability
#define LDT_GNSS_LOCATION_INFO_VER_RELIABILITY_BIT (1<<4)  // vertical reliability
#define LDT_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MAJOR_BIT (1<<5)  // elipsode semi major
#define LDT_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MINOR_BIT (1<<6)  // elipsode semi minor
#define LDT_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_AZIMUTH_BIT (1<<7)  // accuracy elipsode azimuth
#define LDT_GNSS_LOCATION_INFO_GNSS_SV_USED_DATA_BIT (1<<8)  // svUsedInPosition
                                                                   //       numOfMeasReceived
                                                                   //       and measUsageInfo
#define LDT_GNSS_LOCATION_INFO_NAV_SOLUTION_MASK_BIT (1<<9)  // navSolutionMask
#define LDT_GNSS_LOCATION_INFO_SV_SOURCE_INFO_BIT (1<<10) // LocSvInfoSource
#define LDT_GNSS_LOCATION_INFO_POS_DYNAMICS_DATA_BIT (1<<11) // position dynamics data &
                                                                   //       Position Dynamics Ext
#define LDT_GNSS_LOCATION_INFO_EXT_DOP_BIT (1<<12) // gdop tdop
#define LDT_GNSS_LOCATION_INFO_NORTH_STD_DEV_BIT (1<<13) // North standard deviation
#define LDT_GNSS_LOCATION_INFO_EAST_STD_DEV_BIT (1<<14) // East standard deviation
#define LDT_GNSS_LOCATION_INFO_NORTH_VEL_BIT (1<<15) // North Velocity
#define LDT_GNSS_LOCATION_INFO_EAST_VEL_BIT (1<<16) // East Velocity
#define LDT_GNSS_LOCATION_INFO_UP_VEL_BIT (1<<17) // Up Velocity
#define LDT_GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT (1<<18) // North Velocity Uncertainty
#define LDT_GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT (1<<19) // East Velocity Uncertainty
#define LDT_GNSS_LOCATION_INFO_UP_VEL_UNC_BIT (1<<20) // Up Velocity Uncertainty
#define LDT_GNSS_LOCATION_INFO_LEAP_SECONDS_BIT (1<<21) // leap seconds
#define LDT_GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT (1<<22) // number of SV used in position
#define LDT_GNSS_LOCATION_INFO_CALIBRATION_CONFIDENCE_BIT (1<<23) // sensor cal confidence
#define LDT_GNSS_LOCATION_INFO_CALIBRATION_STATUS_BIT (1<<24) // sensor cal status
#define LDT_GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT (1<<25) // output engine type
#define LDT_GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT (1<<26) // output engine mask
#define LDT_GNSS_LOCATION_INFO_CONFORMITY_INDEX_BIT (1<<27) // conformity index
#define LDT_GNSS_LOCATION_INFO_LLA_VRP_BASED_BIT (1<<28) // VRP-based lat/long/alt
#define LDT_GNSS_LOCATION_INFO_ENU_VELOCITY_VRP_BASED_BIT (1<<29) // VRP-based east/north/up vel
#define LDT_GNSS_LOCATION_INFO_DR_SOLUTION_STATUS_MASK_BIT (1ULL<<30) // Valid DR solution status
#define LDT_GNSS_LOCATION_INFO_ALTITUDE_ASSUMED_BIT (1ULL<<31) // Valid altitude assumed
#define LDT_GNSS_LOCATION_INFO_SESSION_STATUS_BIT (1ULL<<32) // session status
#define LDT_GNSS_LOCATION_INFO_INTEGRITY_RISK_USED_BIT (1ULL<<33) // integrity risk used
#define LDT_GNSS_LOCATION_INFO_PROTECT_ALONG_TRACK_BIT (1ULL<<34) // along-track protection level
#define LDT_GNSS_LOCATION_INFO_PROTECT_CROSS_TRACK_BIT (1ULL<<35) // Cross-track protection level
#define LDT_GNSS_LOCATION_INFO_PROTECT_VERTICAL_BIT (1ULL<<36) // vertical protection level
#define LDT_GNSS_LOCATION_INFO_DGNSS_STATION_ID_BIT (1ULL<<37) // dgnss station id
#define LDT_GNSS_LOCATION_INFO_LEAP_SECONDS_UNC_BIT (1ULL<<40) // Leap Second Uncertainity

enum GeofenceBreachType {
    GEOFENCE_BREACH_ENTER = 0,
    GEOFENCE_BREACH_EXIT,
    GEOFENCE_BREACH_DWELL_IN,
    GEOFENCE_BREACH_DWELL_OUT,
    GEOFENCE_BREACH_UNKNOWN,
};

typedef uint16_t GeofenceBreachTypeMask;
enum GeofenceBreachTypeBits {
    GEOFENCE_BREACH_ENTER_BIT     = (1<<0),
    GEOFENCE_BREACH_EXIT_BIT      = (1<<1),
    GEOFENCE_BREACH_DWELL_IN_BIT  = (1<<2),
    GEOFENCE_BREACH_DWELL_OUT_BIT = (1<<3),
};

enum GeofenceStatusAvailable {
    GEOFENCE_STATUS_AVAILABILE_NO = 0,
    GEOFENCE_STATUS_AVAILABILE_YES,
};

enum GeofenceConfidence {
    GEOFENCE_CONFIDENCE_LOW = 1,
    GEOFENCE_CONFIDENCE_MEDIUM,
    GEOFENCE_CONFIDENCE_HIGH,
};

// Set of masks for Modem and QWES capabilities.
typedef uint64_t LocationCapabilitiesMask;
// supports startTracking API with minInterval param
#define   LOCATION_CAPABILITIES_TIME_BASED_TRACKING_BIT           (1<<0)
// supports startBatching API with minInterval param
#define   LOCATION_CAPABILITIES_TIME_BASED_BATCHING_BIT           (1<<1)
// supports startTracking API with minDistance param
#define  LOCATION_CAPABILITIES_DISTANCE_BASED_TRACKING_BIT        (1<<2)
// supports startBatching API with minDistance param
#define   LOCATION_CAPABILITIES_DISTANCE_BASED_BATCHING_BIT       (1<<3)
// supports addGeofences API
#define   LOCATION_CAPABILITIES_GEOFENCE_BIT                      (1<<4)
// supports GnssMeasurementsCallback
#define   LOCATION_CAPABILITIES_GNSS_MEASUREMENTS_BIT             (1<<5)
// supports startTracking/startBatching API with LocationOptions.mode of MSB (Ms Based)
#define   LOCATION_CAPABILITIES_GNSS_MSB_BIT                      (1<<6)
// supports startTracking/startBatching API with LocationOptions.mode of MSA (MS Assisted)
#define   LOCATION_CAPABILITIES_GNSS_MSA_BIT                      (1<<7)
// supports debug nmea sentences in the debugNmeaCallback
#define   LOCATION_CAPABILITIES_DEBUG_DATA_BIT                    (1<<8)
// support outdoor trip batching
#define   LOCATION_CAPABILITIES_OUTDOOR_TRIP_BATCHING_BIT         (1<<9)
// support constellation enablement
#define   LOCATION_CAPABILITIES_CONSTELLATION_ENABLEMENT_BIT      (1<<10)
// support agpm
#define   LOCATION_CAPABILITIES_AGPM_BIT                          (1<<11)
// support location privacy
#define   LOCATION_CAPABILITIES_PRIVACY_BIT                       (1<<12)
// support measurement corrections
#define   LOCATION_CAPABILITIES_MEASUREMENTS_CORRECTION_BIT       (1<<13)
// support Robust Location
#define   LOCATION_CAPABILITIES_CONFORMITY_INDEX_BIT              (1<<14)
// support precise location edgnss
#define   LOCATION_CAPABILITIES_EDGNSS_BIT                        (1<<15)
// Modem supports Carrier Phase for Precise Positioning
// Measurement Engine (PPME).
#define   LOCATION_CAPABILITIES_QWES_CARRIER_PHASE_BIT            (1<<16)
// Modem supports SV Polynomial for tightly coupled external
// DR support. This is a Standalone Feature.
#define   LOCATION_CAPABILITIES_QWES_SV_POLYNOMIAL_BIT            (1<<17)
// Modem supports SV Ephemeris for tightly coupled external
// PPE engines. This is a Standalone Feature.
#define   LOCATION_CAPABILITIES_QWES_SV_EPHEMERIS_BIT            (1<<18)
// Modem supports GNSS Single Frequency feature. This is a
// Standalone Feature.
#define   LOCATION_CAPABILITIES_QWES_GNSS_SINGLE_FREQUENCY       (1<<19)
// Modem supports GNSS Multi Frequency feature. Multi Frequency
// enables Single frequency also.
#define   LOCATION_CAPABILITIES_QWES_GNSS_MULTI_FREQUENCY        (1<<20)
// This mask indicates VPe license bundle is enabled. VEPP
// bundle include Carrier Phase and SV Polynomial features.
#define   LOCATION_CAPABILITIES_QWES_VPE                         (1<<21)
// This mask indicates support for CV2X Location basic features.
// This bundle includes features for GTS Time & Freq, C-TUNC
// (Constrained Time uncertainity.
#define   LOCATION_CAPABILITIES_QWES_CV2X_LOCATION_BASIC         (1<<22)
// This mask indicates support for CV2X Location premium features.
// This bundle includes features for CV2X Location Basic features,
// QDR3 feature, and PACE. (Position Assisted Clock Estimator.
#define   LOCATION_CAPABILITIES_QWES_CV2X_LOCATION_PREMIUM       (1<<23)
// This mask indicates that PPE (Precise Positioning Engine)
// library is enabled or Precise Positioning Framework (PPF)
// is available. This bundle includes features for Carrier
// Phase and SV Ephermeris.
#define   LOCATION_CAPABILITIES_QWES_PPE                         (1<<24)
// This mask indicates QDR2_C license bundle is enabled. This
// bundle includes features for SV Polynomial.
#define   LOCATION_CAPABILITIES_QWES_QDR2                        (1<<25)
// This mask indicates QDR3_C license bundle is enabled. This
// bundle includes features for SV Polynomial.
#define   LOCATION_CAPABILITIES_QWES_QDR3                        (1<<26)
// This mask indicates DGNSS license bundle is enabled.
#define   LOCATION_CAPABILITIES_QWES_DGNSS                       (1<<27)
// This mask indicates Antenna info is enabled.
#define   LOCATION_CAPABILITIES_ANTENNA_INFO                     (1<<28)
// This mask indicates qppe or qfe library is presented.
#define   LOCATION_CAPABILITIES_PRECISE_LIB_PRESENT              (1<<29)
// This mask indicates wifi RSSI positioning is
// enabled by QWES license.
#define   LOCATION_CAPABILITIES_QWES_WIFI_RSSI_POSITIONING       (1ULL<<30)
// This mask indicates wifi RTT positioning is
// enabled by QWES license.
#define   LOCATION_CAPABILITIES_QWES_WIFI_RTT_POSITIONING        (1ULL<<31)
// This mask indicates wifi RSSI positioning is supported.
#define   LOCATION_CAPABILITIES_WIFI_RSSI_POSITIONING                 (1ULL<<32)
// This mask indicates wifi RTT positioning is supported.
#define   LOCATION_CAPABILITIES_WIFI_RTT_POSITIONING                  (1ULL<<33)
// support GNSS bands
#define   LOCATION_CAPABILITIES_GNSS_BANDS_BIT                        (1ULL<<34)
// This mask indicates modem 3GPP source is available.
#define   LOCATION_CAPABILITIES_MODEM_3GPP_AVAIL                      (1ULL<<35)
// This mask indicates PR ML inference is present
#define   LOCATION_CAPABILITIES_NLOS_ML20                             (1ULL<<36)
// This mask indicates if NHz is enableD
#define   LOCATION_CAPABILITIES_QWES_GNSS_NHZ                         (1ULL<<37)
// This mask indicates wwan standard positioning is
// enabled by QWES license.
#define   LOCATION_CAPABILITIES_QWES_WWAN_STANDARD_POSITIONING        (1ULL<<38)
// This mask indicates wwan premium positioning is
// enabled by QWES license.
#define   LOCATION_CAPABILITIES_QWES_WWAN_PREMIUM_POSITIONING         (1ULL<<39)

typedef uint8_t LocationQwesFeatureType;
enum LocationQwesFeatureTypes {
    // Modem supports Carrier Phase for Precise Positioning
    // Measurement Engine (PPME).
    LOCATION_QWES_FEATURE_TYPE_CARRIER_PHASE                 = 1,
    // Modem supports SV Polynomial for tightly coupled external
    // DR support. This is a Standalone Feature.
    LOCATION_QWES_FEATURE_TYPE_SV_POLYNOMIAL                 = 2,
    // Modem supports SV Ephemeris for tightly coupled external
    // PPE support. This is a Standalone Feature.
    LOCATION_QWES_FEATURE_TYPE_SV_EPH                        = 3,
    // Modem supports GNSS Single Frequency feature. This is a
    // Standalone Feature.
    LOCATION_QWES_FEATURE_TYPE_GNSS_SINGLE_FREQUENCY         = 4,
    // Modem supports GNSS Multi Frequency feature. Multi Frequency
    // enables Single frequency also.
    LOCATION_QWES_FEATURE_TYPE_GNSS_MULTI_FREQUENCY          = 5,
    // This indicates Time and Frequency status.
    LOCATION_QWES_FEATURE_TYPE_TIME_FREQUENCY                = 6,
    // This indicates Time Uncertainty  status.
    LOCATION_QWES_FEATURE_TYPE_TIME_UNCERTAINTY              = 7,
    // This indicates Clock Estimate status.
    LOCATION_QWES_FEATURE_TYPE_CLOCK_ESTIMATE                = 8,
    // This mask indicates that PPE (Precise Positioning Engine)
    // library is enabled or Precise Positioning Framework (PPF)
    // is available. This bundle includes features for Carrier
    // Phase and SV Ephermeris.
    LOCATION_QWES_FEATURE_TYPE_PPE                           = 9,
    // This indicates QDR2_C license bundle is enabled. This
    // bundle includes features for SV Polynomial.
    LOCATION_QWES_FEATURE_TYPE_QDR2                          = 10,
    // This indicates QDR3_C license bundle is enabled. This
    // bundle includes features for SV Polynomial.
    LOCATION_QWES_FEATURE_TYPE_QDR3                          = 11,
    // This indicates VEPP license bundle is enabled. VEPP
    // bundle include Carrier Phase and SV Polynomial features.
    LOCATION_QWES_FEATURE_TYPE_VPE                           = 12,
    // This indicates DGNSS license is enabled.
    LOCATION_QWES_FEATURE_TYPE_DGNSS                         = 13,
    // This indicates DLP feature is enabled by QESDK APP
    // license
    LOCATION_QWES_FEATURE_TYPE_DLP_QESDK                     = 14,
    // This indicates MLP feature is enabled by QESDK APP
    // license
    LOCATION_QWES_FEATURE_TYPE_MLP_QESDK                     = 15,
    // This indicates EP can do SSR2OSR correction data
    // parseing
    LOCATION_FEATURE_TYPE_CORR_DATA_PARSER                   = 16,
    // This indicates PR meas ML infernece is enabled
    LOCATION_QWES_FEATURE_NLOS_ML20                          = 17,
    // This indicates wifi RSSI positioning is
    // enabled by QWES license.
    LOCATION_QWES_FEATURE_TYPE_RSSI_POSITIONING              = 18,
    // This indicates wifi RTT positioning is
    // enabled by QWES license.
    LOCATION_QWES_FEATURE_TYPE_RTT_POSITIONING               = 19,
    // This indicates if NHz feature is supported
    LOCATION_QWES_FEATURE_STATUS_GNSS_NHZ                    = 20,
    // This indicates wwan standard positioning is
    // enabled by QWES license.
    LOCATION_QWES_FEATURE_TYPE_WWAN_STANDARD_POSITIONING     = 21,
    // This indicates wwan premium positioning is
    // enabled by QWES license.
    LOCATION_QWES_FEATURE_TYPE_WWAN_PREMIUM_POSITIONING      = 22,
    // Max value
    LOCATION_QWES_FEATURE_TYPE_MAX                           = 23
};

typedef uint64_t LocationHwCapabilitiesMask;
enum LocationHwCapabilitiesBits {
    // This indicates wifi HW has RSSI capability.
    LOCATION_WIFI_CAPABILITY_RSSI = (1<<0),
    // This indicates wifi HW has RTT capability.
    LOCATION_WIFI_CAPABILITY_RTT  = (1<<1)
};

enum LocationTechnologyType {
    LOCATION_TECHNOLOGY_TYPE_GNSS = 0,
};

// Configures how GPS is locked when GPS is disabled (through GnssDisable)
/*
There are several interpretations for GnssConfigGpsLock as follows:
1. S behavior. This is identified by mSupportNfwControl being 1 and
    isFeatureSupported(LOC_SUPPORTED_FEATURE_MULTIPLE_ATTRIBUTION_APPS)
    In this case the bits GNSS_CONFIG_GPS_NFW_XXX below come into play,
    a 1 will lock the corresponding NFW client while a 0 will unlock it.
2. Q behavior. This is identified by mSupportNfwControl being 1. In this case
    ContextBase::mGps_conf.GPS_LOCK is a "state", meaning it should reflect the
    NV value. Therefore we will set the NV to ContextBase::mGps_conf.GPS_LOCK
    GNSS_CONFIG_GPS_LOCK_NI bit will be 0 (enabled) if either GNSS_CONFIG_GPS_NFW_XXX
    is 1 (enabled).
3. P behavior. This is identified by mSupportNfwControl being 0. In this case
    ContextBase::mGps_conf.GPS_LOCK is a "configuration", meaning it should hold
    the "mask" for NI.
*/
enum {
    // gps is not locked when GPS is disabled (GnssDisable)
    GNSS_CONFIG_GPS_LOCK_NONE               = 0x00000000,
    // gps mobile originated (MO) is locked when GPS is disabled
    GNSS_CONFIG_GPS_LOCK_MO                 = 0x00000001,
    GNSS_CONFIG_GPS_LOCK_NFW_IMS            = 0x00000002,
    GNSS_CONFIG_GPS_LOCK_NFW_SIM            = 0x00000004,
    GNSS_CONFIG_GPS_LOCK_NFW_MDT            = 0x00000008,
    GNSS_CONFIG_GPS_LOCK_NFW_TLOC           = 0x00000010,
    GNSS_CONFIG_GPS_LOCK_NFW_RLOC           = 0x00000020,
    GNSS_CONFIG_GPS_LOCK_NFW_V2X            = 0x00000040,
    GNSS_CONFIG_GPS_LOCK_NFW_R1             = 0x00000080,
    GNSS_CONFIG_GPS_LOCK_NFW_R2             = 0x00000100,
    GNSS_CONFIG_GPS_LOCK_NFW_R3             = 0x00000200,
    GNSS_CONFIG_GPS_LOCK_NFW_SUPL           = 0x00000400,
    GNSS_CONFIG_GPS_LOCK_NFW_CP             = 0X00000800,
    GNSS_CONFIG_GPS_LOCK_NFW_ALL            =
            (((GNSS_CONFIG_GPS_LOCK_NFW_CP << 1) - 1) & ~GNSS_CONFIG_GPS_LOCK_MO),
    GNSS_CONFIG_GPS_LOCK_MO_AND_NI          =
            (GNSS_CONFIG_GPS_LOCK_MO | GNSS_CONFIG_GPS_LOCK_NFW_ALL),
};
typedef uint32_t GnssConfigGpsLock;

// SUPL version
enum GnssConfigSuplVersion {
    GNSS_CONFIG_SUPL_VERSION_1_0_0 = 1,
    GNSS_CONFIG_SUPL_VERSION_2_0_0,
    GNSS_CONFIG_SUPL_VERSION_2_0_2,
    GNSS_CONFIG_SUPL_VERSION_2_0_4,
};

// LTE Positioning Profile
typedef uint16_t GnssConfigLppProfileMask;
enum GnssConfigLppProfileBits {
    GNSS_CONFIG_LPP_PROFILE_RRLP_ON_LTE = 0,                         // RRLP on LTE (Default)
    GNSS_CONFIG_LPP_PROFILE_USER_PLANE_BIT                 = (1<<0), // LPP User Plane (UP) on LTE
    GNSS_CONFIG_LPP_PROFILE_CONTROL_PLANE_BIT              = (1<<1), // LPP_Control_Plane (CP)
    GNSS_CONFIG_LPP_PROFILE_USER_PLANE_OVER_NR5G_SA_BIT    = (1<<2), // LPP User Plane (UP) on LTE
    GNSS_CONFIG_LPP_PROFILE_CONTROL_PLANE_OVER_NR5G_SA_BIT = (1<<3), // LPP_Control_Plane (CP)
};

// Technology for LPPe Control Plane
typedef uint16_t GnssConfigLppeControlPlaneMask;
enum GnssConfigLppeControlPlaneBits {
    GNSS_CONFIG_LPPE_CONTROL_PLANE_DBH_BIT                  = (1<<0), // DBH
    GNSS_CONFIG_LPPE_CONTROL_PLANE_WLAN_AP_MEASUREMENTS_BIT = (1<<1), // WLAN_AP_MEASUREMENTS
    GNSS_CONFIG_LPPE_CONTROL_PLANE_SRN_AP_MEASUREMENTS_BIT = (1<<2),
                                                            // SRN_AP_MEASUREMENTS, Not Supported
    GNSS_CONFIG_LPPE_CONTROL_PLANE_SENSOR_BARO_MEASUREMENTS_BIT = (1<<3),
                                                             // SENSOR_BARO_MEASUREMENTS
    GNSS_CONFIG_LPPE_CONTROL_PLANE_NON_E911_BIT = (1<<4), // NON_E911
    GNSS_CONFIG_LPPE_CONTROL_PLANE_CIV_ADDRESS_BIT          = (1<<5), // CIV_ADDRESS
};

// Technology for LPPe User Plane
typedef uint16_t GnssConfigLppeUserPlaneMask;
enum GnssConfigLppeUserPlaneBits {
    GNSS_CONFIG_LPPE_USER_PLANE_DBH_BIT                  = (1<<0), // DBH
    GNSS_CONFIG_LPPE_USER_PLANE_WLAN_AP_MEASUREMENTS_BIT = (1<<1), // WLAN_AP_MEASUREMENTS
    GNSS_CONFIG_LPPE_USER_PLANE_SRN_AP_MEASUREMENTS_BIT = (1<<2),
                                                            // SRN_AP_MEASUREMENTS, Not Supported
    GNSS_CONFIG_LPPE_USER_PLANE_SENSOR_BARO_MEASUREMENTS_BIT = (1<<3),
                                                            // SENSOR_BARO_MEASUREMENTS
    GNSS_CONFIG_LPPE_USER_PLANE_NON_E911_BIT = (1<<4), // NON_E911
    GNSS_CONFIG_LPPE_USER_PLANE_CIV_ADDRESS_BIT           = (1<<5), // CIV_ADDRESS
};

// Positioning Protocol on A-GLONASS system
typedef uint16_t GnssConfigAGlonassPositionProtocolMask;
enum GnssConfigAGlonassPositionProtocolBits {
    GNSS_CONFIG_RRC_CONTROL_PLANE_BIT = (1<<0),  // RRC Control Plane
    GNSS_CONFIG_RRLP_USER_PLANE_BIT   = (1<<1),  // RRLP User Plane
    GNSS_CONFIG_LLP_USER_PLANE_BIT    = (1<<2),  // LPP User Plane
    GNSS_CONFIG_LLP_CONTROL_PLANE_BIT = (1<<3),  // LPP Control Plane
};

enum GnssConfigEmergencyPdnForEmergencySupl {
    GNSS_CONFIG_EMERGENCY_PDN_FOR_EMERGENCY_SUPL_NO = 0,
    GNSS_CONFIG_EMERGENCY_PDN_FOR_EMERGENCY_SUPL_YES,
};

enum GnssConfigSuplEmergencyServices {
    GNSS_CONFIG_SUPL_EMERGENCY_SERVICES_NO = 0,
    GNSS_CONFIG_SUPL_EMERGENCY_SERVICES_YES,
};

typedef uint16_t GnssConfigSuplModeMask;
enum GnssConfigSuplModeBits {
    GNSS_CONFIG_SUPL_MODE_MSB_BIT = (1<<0),
    GNSS_CONFIG_SUPL_MODE_MSA_BIT = (1<<1),
};

typedef uint32_t GnssConfigFlagsMask;
enum GnssConfigFlagsBits {
    GNSS_CONFIG_FLAGS_GPS_LOCK_VALID_BIT                   = (1<<0),
    GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT               = (1<<1),
    GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT        = (1<<2),
    GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT                = (1<<3),
    GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT         = (1<<4),
    GNSS_CONFIG_FLAGS_LPPE_USER_PLANE_VALID_BIT            = (1<<5),
    GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT = (1<<6),
    GNSS_CONFIG_FLAGS_EM_PDN_FOR_EM_SUPL_VALID_BIT         = (1<<7),
    GNSS_CONFIG_FLAGS_SUPL_EM_SERVICES_BIT                 = (1<<8),
    GNSS_CONFIG_FLAGS_SUPL_MODE_BIT                        = (1<<9),
    GNSS_CONFIG_FLAGS_BLACKLISTED_SV_IDS_BIT               = (1<<10),
    GNSS_CONFIG_FLAGS_EMERGENCY_EXTENSION_SECONDS_BIT      = (1<<11),
    GNSS_CONFIG_FLAGS_ROBUST_LOCATION_BIT                  = (1<<12),
    GNSS_CONFIG_FLAGS_MIN_GPS_WEEK_BIT                     = (1<<13),
    GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT                 = (1<<14),
    GNSS_CONFIG_FLAGS_CONSTELLATION_SECONDARY_BAND_BIT     = (1<<15),
    GNSS_CONFIG_FLAGS_XTRA_STATUS_BIT                      = (1<<16),
};

enum GnssNiEncodingType {
    GNSS_NI_ENCODING_TYPE_NONE = 0,
    GNSS_NI_ENCODING_TYPE_GSM_DEFAULT,
    GNSS_NI_ENCODING_TYPE_UTF8,
    GNSS_NI_ENCODING_TYPE_UCS2,
};

enum GnssNiType {
    GNSS_NI_TYPE_VOICE = 0,
    GNSS_NI_TYPE_SUPL,
    GNSS_NI_TYPE_CONTROL_PLANE,
    GNSS_NI_TYPE_EMERGENCY_SUPL
};

typedef uint16_t GnssNiOptionsMask;
enum GnssNiOptionsBits {
    GNSS_NI_OPTIONS_NOTIFICATION_BIT     = (1<<0),
    GNSS_NI_OPTIONS_VERIFICATION_BIT     = (1<<1),
    GNSS_NI_OPTIONS_PRIVACY_OVERRIDE_BIT = (1<<2),
};

enum GnssNiResponse {
    GNSS_NI_RESPONSE_ACCEPT = 1,
    GNSS_NI_RESPONSE_DENY,
    GNSS_NI_RESPONSE_NO_RESPONSE,
    GNSS_NI_RESPONSE_IGNORE,
};

enum GnssSvType {
    GNSS_SV_TYPE_UNKNOWN = 0,
    GNSS_SV_TYPE_GPS,
    GNSS_SV_TYPE_SBAS,
    GNSS_SV_TYPE_GLONASS,
    GNSS_SV_TYPE_QZSS,
    GNSS_SV_TYPE_BEIDOU,
    GNSS_SV_TYPE_GALILEO,
    GNSS_SV_TYPE_NAVIC,
};

enum GnssEphemerisType {
    GNSS_EPH_TYPE_UNKNOWN = 0,
    GNSS_EPH_TYPE_EPHEMERIS,
    GNSS_EPH_TYPE_ALMANAC,
};

enum GnssEphemerisSource {
    GNSS_EPH_SOURCE_UNKNOWN = 0,
    GNSS_EPH_SOURCE_DEMODULATED,
    GNSS_EPH_SOURCE_SUPL_PROVIDED,
    GNSS_EPH_SOURCE_OTHER_SERVER_PROVIDED,
    GNSS_EPH_SOURCE_LOCAL,
};

enum GnssEphemerisHealth {
    GNSS_EPH_HEALTH_UNKNOWN = 0,
    GNSS_EPH_HEALTH_GOOD,
    GNSS_EPH_HEALTH_BAD,
};

typedef uint16_t GnssSvOptionsMask;
enum GnssSvOptionsBits {
    GNSS_SV_OPTIONS_HAS_EPHEMER_BIT             = (1<<0),
    GNSS_SV_OPTIONS_HAS_ALMANAC_BIT             = (1<<1),
    // Bit indicates whether this SV is used in SPE fix.
    GNSS_SV_OPTIONS_USED_IN_FIX_BIT             = (1<<2),
    GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT   = (1<<3),
    GNSS_SV_OPTIONS_HAS_GNSS_SIGNAL_TYPE_BIT    = (1<<4),
    GNSS_SV_OPTIONS_HAS_BASEBAND_CARRIER_TO_NOISE_BIT = (1<<5),
    GNSS_SV_OPTIONS_HAS_ELEVATION_BIT           = (1<<6),
    GNSS_SV_OPTIONS_HAS_AZIMUTH_BIT             = (1<<7),
};

enum GnssAssistanceType {
    GNSS_ASSISTANCE_TYPE_SUPL = 0,
    GNSS_ASSISTANCE_TYPE_C2K,
    GNSS_ASSISTANCE_TYPE_SUPL_EIMS,
    GNSS_ASSISTANCE_TYPE_SUPL_IMS,
};

enum GnssSuplMode {
    GNSS_SUPL_MODE_STANDALONE = 0,
    GNSS_SUPL_MODE_MSB,
    GNSS_SUPL_MODE_MSA,
};

enum BatchingMode {
    BATCHING_MODE_ROUTINE = 0,   // positions are reported when batched positions memory is full
    BATCHING_MODE_TRIP,          // positions are reported when a certain distance is covered
    BATCHING_MODE_NO_AUTO_REPORT // no report of positions automatically, instead queried on demand
};

enum BatchingStatus {
    BATCHING_STATUS_TRIP_COMPLETED = 0,
    BATCHING_STATUS_POSITION_AVAILABE,
    BATCHING_STATUS_POSITION_UNAVAILABLE
};

typedef uint16_t GnssMeasurementsAdrStateMask;
enum GnssMeasurementsAdrStateBits {
    GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_UNKNOWN                 = 0,
    GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT               = (1<<0),
    GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT               = (1<<1),
    GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT          = (1<<2),
    GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_HALF_CYCLE_RESOLVED_BIT = (1<<3),
};

enum GnssMeasurementsCodeType {
    GNSS_MEASUREMENTS_CODE_TYPE_A       = 0,
    GNSS_MEASUREMENTS_CODE_TYPE_B       = 1,
    GNSS_MEASUREMENTS_CODE_TYPE_C       = 2,
    GNSS_MEASUREMENTS_CODE_TYPE_I       = 3,
    GNSS_MEASUREMENTS_CODE_TYPE_L       = 4,
    GNSS_MEASUREMENTS_CODE_TYPE_M       = 5,
    GNSS_MEASUREMENTS_CODE_TYPE_P       = 6,
    GNSS_MEASUREMENTS_CODE_TYPE_Q       = 7,
    GNSS_MEASUREMENTS_CODE_TYPE_S       = 8,
    GNSS_MEASUREMENTS_CODE_TYPE_W       = 9,
    GNSS_MEASUREMENTS_CODE_TYPE_X       = 10,
    GNSS_MEASUREMENTS_CODE_TYPE_Y       = 11,
    GNSS_MEASUREMENTS_CODE_TYPE_Z       = 12,
    GNSS_MEASUREMENTS_CODE_TYPE_N       = 13,
    GNSS_MEASUREMENTS_CODE_TYPE_OTHER   = 255,
};

typedef uint32_t GnssMeasurementsDataFlagsMask;
enum GnssMeasurementsDataFlagsBits {
    GNSS_MEASUREMENTS_DATA_SV_ID_BIT                        = (1<<0),
    GNSS_MEASUREMENTS_DATA_SV_TYPE_BIT                      = (1<<1),
    GNSS_MEASUREMENTS_DATA_STATE_BIT                        = (1<<2),
    GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT             = (1<<3),
    GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_UNCERTAINTY_BIT = (1<<4),
    GNSS_MEASUREMENTS_DATA_CARRIER_TO_NOISE_BIT             = (1<<5),
    GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_BIT             = (1<<6),
    GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_UNCERTAINTY_BIT = (1<<7),
    GNSS_MEASUREMENTS_DATA_ADR_STATE_BIT                    = (1<<8),
    GNSS_MEASUREMENTS_DATA_ADR_BIT                          = (1<<9),
    GNSS_MEASUREMENTS_DATA_ADR_UNCERTAINTY_BIT              = (1<<10),
    GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT            = (1<<11),
    GNSS_MEASUREMENTS_DATA_CARRIER_CYCLES_BIT               = (1<<12),
    GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT                = (1<<13),
    GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT    = (1<<14),
    GNSS_MEASUREMENTS_DATA_MULTIPATH_INDICATOR_BIT          = (1<<15),
    GNSS_MEASUREMENTS_DATA_SIGNAL_TO_NOISE_RATIO_BIT        = (1<<16),
    GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT       = (1<<17),
    GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT                     = (1<<18),
    GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT         = (1<<19),
    GNSS_MEASUREMENTS_DATA_SATELLITE_ISB_BIT                = (1<<20),
    GNSS_MEASUREMENTS_DATA_SATELLITE_ISB_UNCERTAINTY_BIT    = (1<<21),
    GNSS_MEASUREMENTS_DATA_CYCLE_SLIP_COUNT_BIT             = (1<<22),
    GNSS_MEASUREMENTS_DATA_SATELLITE_PVT_BIT                = (1<<23),
    GNSS_MEASUREMENTS_DATA_CORRELATION_VECTOR_BIT           = (1<<24),
    GNSS_MEASUREMENTS_DATA_GNSS_SIGNAL_TYPE_BIT             = (1<<25),
    GNSS_MEASUREMENTS_DATA_GLO_FREQUENCY_BIT                = (1<<26),
    GNSS_MEASUREMENTS_DATA_BASEBAND_CARRIER_TO_NOISE_BIT    = (1<<27),
};

typedef uint32_t GnssMeasurementsStateMask;
enum GnssMeasurementsStateBits {
    GNSS_MEASUREMENTS_STATE_UNKNOWN_BIT               = 0,
    GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT             = (1<<0),
    GNSS_MEASUREMENTS_STATE_BIT_SYNC_BIT              = (1<<1),
    GNSS_MEASUREMENTS_STATE_SUBFRAME_SYNC_BIT         = (1<<2),
    GNSS_MEASUREMENTS_STATE_TOW_DECODED_BIT           = (1<<3),
    GNSS_MEASUREMENTS_STATE_MSEC_AMBIGUOUS_BIT        = (1<<4),
    GNSS_MEASUREMENTS_STATE_SYMBOL_SYNC_BIT           = (1<<5),
    GNSS_MEASUREMENTS_STATE_GLO_STRING_SYNC_BIT       = (1<<6),
    GNSS_MEASUREMENTS_STATE_GLO_TOD_DECODED_BIT       = (1<<7),
    GNSS_MEASUREMENTS_STATE_BDS_D2_BIT_SYNC_BIT       = (1<<8),
    GNSS_MEASUREMENTS_STATE_BDS_D2_SUBFRAME_SYNC_BIT  = (1<<9),
    GNSS_MEASUREMENTS_STATE_GAL_E1BC_CODE_LOCK_BIT    = (1<<10),
    GNSS_MEASUREMENTS_STATE_GAL_E1C_2ND_CODE_LOCK_BIT = (1<<11),
    GNSS_MEASUREMENTS_STATE_GAL_E1B_PAGE_SYNC_BIT     = (1<<12),
    GNSS_MEASUREMENTS_STATE_SBAS_SYNC_BIT             = (1<<13),
    GNSS_MEASUREMENTS_STATE_TOW_KNOWN_BIT             = (1<<14),
    GNSS_MEASUREMENTS_STATE_GLO_TOD_KNOWN_BIT         = (1<<15),
    GNSS_MEASUREMENTS_STATE_2ND_CODE_LOCK_BIT         = (1<<16),
};

typedef uint16_t GnssSingleSatCorrectionMask;
enum GnssSingleSatCorrectionBits {
    GNSS_MEAS_CORR_UNKNOWN_BIT                     = 0,
    GNSS_MEAS_CORR_HAS_SAT_IS_LOS_PROBABILITY_BIT  = (1 << 0),
    GNSS_MEAS_CORR_HAS_EXCESS_PATH_LENGTH_BIT      = (1 << 1),
    GNSS_MEAS_CORR_HAS_EXCESS_PATH_LENGTH_UNC_BIT  = (1 << 2),
    GNSS_MEAS_CORR_HAS_REFLECTING_PLANE_BIT        = (1 << 3),
};

enum GnssMeasurementsMultipathIndicator {
    GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN = 0,
    GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_PRESENT,
    GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_NOT_PRESENT,
};

typedef uint32_t GnssMeasurementsClockFlagsMask;
enum GnssMeasurementsClockFlagsBits {
    GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT                  = (1<<0),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_BIT                         = (1<<1),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_UNCERTAINTY_BIT             = (1<<2),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT                    = (1<<3),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT                         = (1<<4),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT             = (1<<5),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT                        = (1<<6),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT            = (1<<7),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_HW_CLOCK_DISCONTINUITY_COUNT_BIT = (1<<8),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_ELAPSED_REAL_TIME_BIT            = (1<<9),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_ELAPSED_REAL_TIME_UNC_BIT        = (1<<10),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_ELAPSED_GPTP_TIME_BIT            = (1<<11),
    GNSS_MEASUREMENTS_CLOCK_FLAGS_ELAPSED_GPTP_TIME_UNC_BIT        = (1<<12),
};

typedef uint32_t GnssAidingDataSvMask;
enum GnssAidingDataSvBits {
    GNSS_AIDING_DATA_SV_EPHEMERIS_BIT    = (1<<0), // ephemeris
    GNSS_AIDING_DATA_SV_ALMANAC_BIT      = (1<<1), // almanac
    GNSS_AIDING_DATA_SV_HEALTH_BIT       = (1<<2), // health
    GNSS_AIDING_DATA_SV_DIRECTION_BIT    = (1<<3), // direction
    GNSS_AIDING_DATA_SV_STEER_BIT        = (1<<4), // steer
    GNSS_AIDING_DATA_SV_ALMANAC_CORR_BIT = (1<<5), // almanac correction
    GNSS_AIDING_DATA_SV_BLACKLIST_BIT    = (1<<6), // blacklist SVs
    GNSS_AIDING_DATA_SV_SA_DATA_BIT      = (1<<7), // sensitivity assistance data
    GNSS_AIDING_DATA_SV_NO_EXIST_BIT     = (1<<8), // SV does not exist
    GNSS_AIDING_DATA_SV_IONOSPHERE_BIT   = (1<<9), // ionosphere correction
    GNSS_AIDING_DATA_SV_TIME_BIT         = (1<<10), // reset satellite time
    GNSS_AIDING_DATA_SV_MB_DATA          = (1<<11), // delete multiband data
    GNSS_AIDING_DATA_SV_POLY_BIT         = (1<<12), // poly
};

typedef uint32_t GnssAidingDataSvTypeMask;
enum GnssAidingDataSvTypeBits {
    GNSS_AIDING_DATA_SV_TYPE_GPS_BIT      = (1<<0),
    GNSS_AIDING_DATA_SV_TYPE_GLONASS_BIT  = (1<<1),
    GNSS_AIDING_DATA_SV_TYPE_QZSS_BIT     = (1<<2),
    GNSS_AIDING_DATA_SV_TYPE_BEIDOU_BIT   = (1<<3),
    GNSS_AIDING_DATA_SV_TYPE_GALILEO_BIT  = (1<<4),
    GNSS_AIDING_DATA_SV_TYPE_NAVIC_BIT    = (1<<5),
    GNSS_AIDING_DATA_SV_TYPE_MAX          = (1<<6),
};
#define GNSS_AIDING_DATA_SV_TYPE_MASK_ALL (GNSS_AIDING_DATA_SV_TYPE_MAX-1)

/* Gnss constellation type mask */
typedef uint16_t GnssConstellationTypeMask;
enum GnssConstellationTypeBits {
    GNSS_CONSTELLATION_TYPE_GPS_BIT      = (1<<0),
    GNSS_CONSTELLATION_TYPE_GLONASS_BIT  = (1<<1),
    GNSS_CONSTELLATION_TYPE_QZSS_BIT     = (1<<2),
    GNSS_CONSTELLATION_TYPE_BEIDOU_BIT   = (1<<3),
    GNSS_CONSTELLATION_TYPE_GALILEO_BIT  = (1<<4),
    GNSS_CONSTELLATION_TYPE_SBAS_BIT     = (1<<5),
    GNSS_CONSTELLATION_TYPE_NAVIC_BIT    = (1<<6),
};

#define GNSS_CONSTELLATION_TYPE_MASK_ALL\
        (GNSS_CONSTELLATION_TYPE_GPS_BIT     | GNSS_CONSTELLATION_TYPE_GLONASS_BIT |\
         GNSS_CONSTELLATION_TYPE_QZSS_BIT    | GNSS_CONSTELLATION_TYPE_BEIDOU_BIT  |\
         GNSS_CONSTELLATION_TYPE_GALILEO_BIT | GNSS_CONSTELLATION_TYPE_SBAS_BIT    |\
         GNSS_CONSTELLATION_TYPE_NAVIC_BIT)

/** GNSS Signal Type and RF Band */
typedef uint32_t GnssSignalTypeMask;
enum GnssSignalTypeBits {
    /** GPS L1CA Signal */
    GNSS_SIGNAL_GPS_L1CA            = (1<<0),
    /** GPS L1C Signal */
    GNSS_SIGNAL_GPS_L1C             = (1<<1),
    /** GPS L2 RF Band */
    GNSS_SIGNAL_GPS_L2              = (1<<2),
    /** GPS L5 RF Band */
    GNSS_SIGNAL_GPS_L5              = (1<<3),
    /** GLONASS G1 (L1OF) RF Band */
    GNSS_SIGNAL_GLONASS_G1          = (1<<4),
    /** GLONASS G2 (L2OF) RF Band */
    GNSS_SIGNAL_GLONASS_G2          = (1<<5),
    /** GALILEO E1 RF Band */
    GNSS_SIGNAL_GALILEO_E1          = (1<<6),
    /** GALILEO E5A RF Band */
    GNSS_SIGNAL_GALILEO_E5A         = (1<<7),
    /** GALILEO E5B RF Band */
    GNSS_SIGNAL_GALILEO_E5B         = (1<<8),
    /** BEIDOU B1 RF Band */
    GNSS_SIGNAL_BEIDOU_B1           = (1<<9),
    /** BEIDOU B2 RF Band */
    GNSS_SIGNAL_BEIDOU_B2           = (1<<10),
    /** QZSS L1CA RF Band */
    GNSS_SIGNAL_QZSS_L1CA           = (1<<11),
    /** QZSS L1S RF Band */
    GNSS_SIGNAL_QZSS_L1S            = (1<<12),
    /** QZSS L2 RF Band */
    GNSS_SIGNAL_QZSS_L2             = (1<<13),
    /** QZSS L5 RF Band */
    GNSS_SIGNAL_QZSS_L5             = (1<<14),
    /** SBAS L1 RF Band */
    GNSS_SIGNAL_SBAS_L1             = (1<<15),
    /** BEIDOU B1I RF Band */
    GNSS_SIGNAL_BEIDOU_B1I          = (1<<16),
    /** BEIDOU B1C RF Band */
    GNSS_SIGNAL_BEIDOU_B1C          = (1<<17),
    /** BEIDOU B2I RF Band */
    GNSS_SIGNAL_BEIDOU_B2I          = (1<<18),
    /** BEIDOU B2AI RF Band */
    GNSS_SIGNAL_BEIDOU_B2AI         = (1<<19),
    /** NAVIC L5 RF Band */
    GNSS_SIGNAL_NAVIC_L5            = (1<<20),
    /** BEIDOU B2A_Q RF Band */
    GNSS_SIGNAL_BEIDOU_B2AQ         = (1<<21),
    /** BEIDOU B2B_I RF Band */
    GNSS_SIGNAL_BEIDOU_B2BI         = (1<<22),
    /** BEIDOU B2B_Q RF Band */
    GNSS_SIGNAL_BEIDOU_B2BQ         = (1<<23),
    /** NAVIC L1 RF Band */
    GNSS_SIGNAL_NAVIC_L1            = (1<<24),
};

#define GNSS_SIGNAL_TYPE_MASK_ALL\
    (GNSS_SIGNAL_GPS_L1CA | GNSS_SIGNAL_GPS_L1C | GNSS_SIGNAL_GPS_L2 |\
     GNSS_SIGNAL_GPS_L5| GNSS_SIGNAL_GLONASS_G1 | GNSS_SIGNAL_GLONASS_G2 |\
     GNSS_SIGNAL_GALILEO_E1 | GNSS_SIGNAL_GALILEO_E5A | GNSS_SIGNAL_GALILEO_E5B |\
     GNSS_SIGNAL_BEIDOU_B1I | GNSS_SIGNAL_BEIDOU_B1C | GNSS_SIGNAL_BEIDOU_B2I|\
     GNSS_SIGNAL_BEIDOU_B2AI | GNSS_SIGNAL_QZSS_L1CA | GNSS_SIGNAL_QZSS_L1S |\
     GNSS_SIGNAL_QZSS_L2| GNSS_SIGNAL_QZSS_L5 | GNSS_SIGNAL_SBAS_L1 |\
     GNSS_SIGNAL_NAVIC_L5 | GNSS_SIGNAL_BEIDOU_B2AQ | GNSS_SIGNAL_BEIDOU_B2BI |\
     GNSS_SIGNAL_BEIDOU_B2BQ | GNSS_SIGNAL_NAVIC_L1)

enum Gnss_LocSvSystemEnumType {
    GNSS_LOC_SV_SYSTEM_UNKNOWN                = 0,
    /** unknown sv system. */
    GNSS_LOC_SV_SYSTEM_MIN                    = 1,
    /**< Min enum of valid SV system. */
    GNSS_LOC_SV_SYSTEM_GPS                    = 1,
    /**< GPS satellite. */
    GNSS_LOC_SV_SYSTEM_GALILEO                = 2,
    /**< GALILEO satellite. */
    GNSS_LOC_SV_SYSTEM_SBAS                   = 3,
    /**< SBAS satellite. */
    GNSS_LOC_SV_SYSTEM_GLONASS                = 4,
    /**< GLONASS satellite. */
    GNSS_LOC_SV_SYSTEM_BDS                    = 5,
    /**< BDS satellite. */
    GNSS_LOC_SV_SYSTEM_QZSS                   = 6,
    /**< QZSS satellite. */
    GNSS_LOC_SV_SYSTEM_NAVIC                  = 7,
    /**< NAVIC satellite. */
    GNSS_LOC_SV_SYSTEM_MAX                    = 7,
    /**< Max enum of valid SV system. */
};

enum Gnss_LocSignalEnumType {
    GNSS_LOC_SIGNAL_TYPE_GPS_L1CA = 0,          /**<  GPS L1CA Signal  */
    GNSS_LOC_SIGNAL_TYPE_GPS_L1C = 1,           /**<  GPS L1C Signal  */
    GNSS_LOC_SIGNAL_TYPE_GPS_L2C_L = 2,         /**<  GPS L2C_L RF Band  */
    GNSS_LOC_SIGNAL_TYPE_GPS_L5_Q = 3,          /**<  GPS L5_Q RF Band  */
    GNSS_LOC_SIGNAL_TYPE_GLONASS_G1 = 4,        /**<  GLONASS G1 (L1OF) RF Band  */
    GNSS_LOC_SIGNAL_TYPE_GLONASS_G2 = 5,        /**<  GLONASS G2 (L2OF) RF Band  */
    GNSS_LOC_SIGNAL_TYPE_GALILEO_E1_C = 6,      /**<  GALILEO E1_C RF Band  */
    GNSS_LOC_SIGNAL_TYPE_GALILEO_E5A_Q = 7,     /**<  GALILEO E5A_Q RF Band  */
    GNSS_LOC_SIGNAL_TYPE_GALILEO_E5B_Q = 8,     /**<  GALILEO E5B_Q RF Band  */
    GNSS_LOC_SIGNAL_TYPE_BEIDOU_B1_I = 9,       /**<  BEIDOU B1_I RF Band  */
    GNSS_LOC_SIGNAL_TYPE_BEIDOU_B1C = 10,       /**<  BEIDOU B1C RF Band  */
    GNSS_LOC_SIGNAL_TYPE_BEIDOU_B2_I = 11,      /**<  BEIDOU B2_I RF Band  */
    GNSS_LOC_SIGNAL_TYPE_BEIDOU_B2A_I = 12,     /**<  BEIDOU B2A_I RF Band  */
    GNSS_LOC_SIGNAL_TYPE_QZSS_L1CA = 13,        /**<  QZSS L1CA RF Band  */
    GNSS_LOC_SIGNAL_TYPE_QZSS_L1S = 14,         /**<  QZSS L1S RF Band  */
    GNSS_LOC_SIGNAL_TYPE_QZSS_L2C_L = 15,       /**<  QZSS L2C_L RF Band  */
    GNSS_LOC_SIGNAL_TYPE_QZSS_L5_Q = 16,        /**<  QZSS L5_Q RF Band  */
    GNSS_LOC_SIGNAL_TYPE_SBAS_L1_CA = 17,       /**<  SBAS L1_CA RF Band  */
    GNSS_LOC_SIGNAL_TYPE_NAVIC_L5 = 18,         /**<  NAVIC L5 RF Band */
    GNSS_LOC_SIGNAL_TYPE_BEIDOU_B2A_Q = 19,     /**<  BEIDOU B2A_Q RF Band  */
    GNSS_LOC_SIGNAL_TYPE_BEIDOU_B2B_I = 20,     /**<  BeiDou B2B_I RF band (data) */
    GNSS_LOC_SIGNAL_TYPE_BEIDOU_B2B_Q = 21,     /**< BeiDou B2B_Q RF band (Pilot)*/
    GNSS_LOC_SIGNAL_TYPE_NAVIC_L1 = 22,         /**<  NAVIC L1 RF Band */
    GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES = 23    /**<  Maximum number of signal types */
};

typedef uint32_t PositioningEngineMask;
enum PositioningEngineBits {
    STANDARD_POSITIONING_ENGINE = (1 << 0),
    DEAD_RECKONING_ENGINE       = (1 << 1),
    PRECISE_POSITIONING_ENGINE  = (1 << 2),
    VP_POSITIONING_ENGINE  = (1 << 3)
};
#define POSITION_ENGINE_MASK_ALL \
        (STANDARD_POSITIONING_ENGINE|DEAD_RECKONING_ENGINE| \
        PRECISE_POSITIONING_ENGINE|VP_POSITIONING_ENGINE)

/** Specify the position engine running state. <br/> */
enum LocEngineRunState {
    /** Request the position engine to be put into resume state.
     *  <br/> */
    LOC_ENGINE_RUN_STATE_PAUSE   = 1,
    /** Request the position engine to be put into resume state.
     *  <br/> */
    LOC_ENGINE_RUN_STATE_RESUME   = 2,
    /** Request the selected position engine to be put into pause state
     *  while retaining of any useful state data. This engine run state
     *  is currently applicable to QDR engine only. It is strongly advised
     *  to link this state to a vehicle state in which the vehicle is expected
     *  to be stationary at the time of invocation of API and subsequently, until
     *  state is changed to Running. For QDR, transition out of PAUSE_RETAIN happens
     *  when either the state is changed to RESUME state via same command OR when the
     *  device taken through suspend/resume or reboot power-state cycles. <br/> */
    LOC_ENGINE_RUN_STATE_PAUSE_RETAIN = 3,

};

typedef uint64_t GnssDataMask;
enum GnssDataBits {
    // Jammer Indicator is available
    GNSS_LOC_DATA_JAMMER_IND_BIT    = (1ULL << 0),
    // AGC is available
    GNSS_LOC_DATA_AGC_BIT           = (1ULL << 1),
    // AGC status for L1 band is available.
    GNSS_LOC_DATA_AGC_STATUS_L1_BIT = (1ULL << 2),
    // AGC status for L2 band is available.
    GNSS_LOC_DATA_AGC_STATUS_L2_BIT = (1ULL << 3),
    // AGC status for L5 band is available.
    GNSS_LOC_DATA_AGC_STATUS_L5_BIT = (1ULL << 4),
};

/** Indicate RF Automatic Gain Control Status <br/>   */
enum AgcStatus {
    /**< AGC status is unknown <br/> */
    AGC_STATUS_UNKNOWN                              = 0,
    /**< AGC status is No saturation <br/> */
    AGC_STATUS_NO_SATURATION                        = 1,
    /**< AGC status is Front end gain maximum saturation <br/> */
    AGC_STATUS_FRONT_END_GAIN_MAXIMUM_SATURATION    = 2,
    /**< AGC status is Front end gain minimum saturation <br/> */
    AGC_STATUS_FRONT_END_GAIN_MINIMUM_SATURATION    = 3,
};

typedef uint32_t GnssSystemTimeStructTypeFlags;
enum GnssSystemTimeTypeBits {
    GNSS_SYSTEM_TIME_WEEK_VALID             = (1 << 0),
    GNSS_SYSTEM_TIME_WEEK_MS_VALID          = (1 << 1),
    GNSS_SYSTEM_CLK_TIME_BIAS_VALID         = (1 << 2),
    GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID     = (1 << 3),
    GNSS_SYSTEM_REF_FCOUNT_VALID            = (1 << 4),
    GNSS_SYSTEM_NUM_CLOCK_RESETS_VALID      = (1 << 5)
};

typedef uint32_t GnssGloTimeStructTypeFlags;
enum GnssGloTimeTypeBits {
    GNSS_CLO_DAYS_VALID                     = (1 << 0),
    GNSS_GLO_MSEC_VALID                     = (1 << 1),
    GNSS_GLO_CLK_TIME_BIAS_VALID            = (1 << 2),
    GNSS_GLO_CLK_TIME_BIAS_UNC_VALID        = (1 << 3),
    GNSS_GLO_REF_FCOUNT_VALID               = (1 << 4),
    GNSS_GLO_NUM_CLOCK_RESETS_VALID         = (1 << 5),
    GNSS_GLO_FOUR_YEAR_VALID                = (1 << 6)
};

struct GnssAidingDataSv {
    GnssAidingDataSvMask svMask;         // bitwise OR of GnssAidingDataSvBits
    GnssAidingDataSvTypeMask svTypeMask; // bitwise OR of GnssAidingDataSvTypeBits
};

typedef uint32_t GnssAidingDataCommonMask;
enum GnssAidingDataCommonBits {
    GNSS_AIDING_DATA_COMMON_POSITION_BIT      = (1<<0), // position estimate
    GNSS_AIDING_DATA_COMMON_TIME_BIT          = (1<<1), // reset all clock values
    GNSS_AIDING_DATA_COMMON_UTC_BIT           = (1<<2), // UTC estimate
    GNSS_AIDING_DATA_COMMON_RTI_BIT           = (1<<3), // RTI
    GNSS_AIDING_DATA_COMMON_FREQ_BIAS_EST_BIT = (1<<4), // frequency bias estimate
    GNSS_AIDING_DATA_COMMON_CELLDB_BIT        = (1<<5), // all celldb info
};

struct GnssAidingDataCommon {
    GnssAidingDataCommonMask mask; // bitwise OR of GnssAidingDataCommonBits
};

typedef uint32_t DrEngineAidingDataMask;
enum DrEngineAidingDataBits {
    DR_ENGINE_AIDING_DATA_CALIBRATION_BIT = (1<<0), // Calibration data for DRE engine
};

struct GnssAidingData {
    bool deleteAll;              // if true, delete all aiding data and ignore other params
    GnssAidingDataSv sv;         // SV specific aiding data
    GnssAidingDataCommon common; // common aiding data
    DrEngineAidingDataMask dreAidingDataMask;// aiding data mask for dr engine
    PositioningEngineMask posEngineMask;     // engines to perform the delete operation on.
};

typedef uint32_t DrCalibrationStatusMask;
enum DrCalibrationStatusBits {
    // Indicate that roll calibration is needed. Need to take more turns on level ground
    DR_ROLL_CALIBRATION_NEEDED  = (1<<0),
    // Indicate that pitch calibration is needed. Need to take more turns on level ground
    DR_PITCH_CALIBRATION_NEEDED = (1<<1),
    // Indicate that yaw calibration is needed. Need to accelerate in a straight line
    DR_YAW_CALIBRATION_NEEDED   = (1<<2),
    // Indicate that odo calibration is needed. Need to accelerate in a straight line
    DR_ODO_CALIBRATION_NEEDED   = (1<<3),
    // Indicate that gyro calibration is needed. Need to take more turns on level ground
    DR_GYRO_CALIBRATION_NEEDED  = (1<<4),
    // Lot more turns on level ground needed
    DR_TURN_CALIBRATION_LOW     = (1<<5),
    // Some more turns on level ground needed
    DR_TURN_CALIBRATION_MEDIUM  = (1<<6),
    // Sufficient turns on level ground observed
    DR_TURN_CALIBRATION_HIGH  =   (1<<7),
    // Lot more accelerations in straight line needed
    DR_LINEAR_ACCEL_CALIBRATION_LOW  = (1<<8),
    // Some more accelerations in straight line needed
    DR_LINEAR_ACCEL_CALIBRATION_MEDIUM  =  (1<<9),
    // Sufficient acceleration events in straight line observed
    DR_LINEAR_ACCEL_CALIBRATION_HIGH  =    (1<<10),
    // Lot more motion in straight line needed
    DR_LINEAR_MOTION_CALIBRATION_LOW  =    (1<<11),
    // Some more motion in straight line needed
    DR_LINEAR_MOTION_CALIBRATION_MEDIUM  = (1<<12),
    // Sufficient motion events in straight line observed
    DR_LINEAR_MOTION_CALIBRATION_HIGH  =   (1<<13),
    // Lot more stationary events on level ground needed
    DR_STATIC_CALIBRATION_LOW  =           (1<<14),
    // Some more stationary events on level ground needed
    DR_STATIC_CALIBRATION_MEDIUM  =        (1<<15),
    // Sufficient stationary events on level ground observed
    DR_STATIC_CALIBRATION_HIGH  =          (1<<16)
};

enum LocationQualityType {
    /**< Position calculated by standard alone postion engine. */
    LOCATION_STANDALONE_QUALITY_TYPE = 0,
    /**< Position calculated by using DGNSS technology. */
    LOCATION_DGNSS_QUALITY_TYPE = 1,
    /**< Position accuracy in RTK or PPP float performance. */
    LOCATION_FLOAT_QUALITY_TYPE = 2,
    /**< Position accuracy in RTK or PPP fixed performance. */
    LOCATION_FIXED_QUALITY_TYPE = 3,
};

enum loc_sess_status {
    LOC_SESS_SUCCESS,
    LOC_SESS_INTERMEDIATE,
    LOC_SESS_FAILURE
};

struct Location {
    uint32_t size;           // set to sizeof(Location)
    LocationFlagsMask flags; // bitwise OR of LocationFlagsBits to mark which params are valid
    loc_sess_status sessionStatus; // location session status
    uint64_t timestamp;      // UTC timestamp for location fix, milliseconds since January 1, 1970
    double latitude;         // in degrees
    double longitude;        // in degrees
    double altitude;         // in meters above the WGS 84 reference ellipsoid
    float speed;             // horizontal speed, in meters per second
    float bearing;           // in degrees; range [0, 360)
    float accuracy;          // horizontal acuracy, in meters
                             // confidence level is at 68%
    float verticalAccuracy;  // in meters
                             // confidence level is at 68%
    float speedAccuracy;     // horizontal speed unc, in meters/second
                             // confidence level is at 68%
    float bearingAccuracy;   // in degrees (0 to 359.999)
                             // confidence level is at 68%
    float conformityIndex;   // in range [0, 1]
    LocationTechnologyMask techMask;
    LocationSpoofMask spoofMask;
    uint64_t elapsedRealTime;    // in ns
    uint64_t elapsedRealTimeUnc; // in ns
    LocationQualityType qualityType; // position quality
    float timeUncMs;             // Time uncertainty in milliseconds
                                 // SPE report: confidence level is 99%
                                 // Other engine report: confidence not unspecified
    uint64_t systemTick;        // System Tick at GPS Time
    uint64_t elapsedgPTPTime;    // GPTP time field in ns
    uint64_t elapsedgPTPTimeUnc; // GPTP time Unc
};

enum LocReqEngineTypeMask {
    LOC_REQ_ENGINE_FUSED_BIT = (1<<0),
    LOC_REQ_ENGINE_SPE_BIT   = (1<<1),
    LOC_REQ_ENGINE_PPE_BIT   = (1<<2),
    LOC_REQ_ENGINE_VPE_BIT   = (1<<3)
};

enum LocOutputEngineType {
    LOC_OUTPUT_ENGINE_FUSED   = 0,
    /** This is the GNSS fix from modem */
    LOC_OUTPUT_ENGINE_SPE     = 1,
    /** This is the GNSS fix with correction PPP/RTK correction */
    LOC_OUTPUT_ENGINE_PPE     = 2,
    LOC_OUTPUT_ENGINE_VPE = 3,
    LOC_OUTPUT_ENGINE_COUNT,
};

enum FixQualityLevel {
    QUALITY_HIGH_ACCU_FIX_ONLY = 0,       /* Only allow valid fix with high accuracy */
    QUALITY_ANY_VALID_FIX,                /* Allow fix with any accuracy, like intermediate fix */
    QUALITY_ANY_OR_FAILED_FIX,            /* Allow fix of any type, even failed fix */
};

struct LocationOptions {
    uint32_t size;          // set to sizeof(LocationOptions)
    uint32_t minInterval; // in milliseconds
    uint32_t minDistance; // in meters. if minDistance > 0, gnssSvCallback/gnssNmeaCallback/
                          // gnssMeasurementsCallback may not be called
    GnssSuplMode mode;    // Standalone/MS-Based/MS-Assisted
    // behavior when this field is 0:
    //  if engine hub is running, this will be fused fix,
    //  if engine hub is not running, this will be SPE fix
    LocReqEngineTypeMask locReqEngTypeMask;
    FixQualityLevel qualityLevelAccepted; /* Send through position reports with which accuracy. */

    inline LocationOptions() :
            size(0), minInterval(0), minDistance(0), mode(GNSS_SUPL_MODE_STANDALONE),
            locReqEngTypeMask((LocReqEngineTypeMask)0),
            qualityLevelAccepted(QUALITY_HIGH_ACCU_FIX_ONLY) {}
};

enum GnssPowerMode {
    GNSS_POWER_MODE_M1 = 1,  /* Improved Accuracy Mode */
    GNSS_POWER_MODE_M2,      /* Normal Mode */
    GNSS_POWER_MODE_M3,      /* Background Mode */
    GNSS_POWER_MODE_M4,      /* Background Mode */
    GNSS_POWER_MODE_M5,      /* Background Mode */
    GNSS_POWER_MODE_DEFAULT = GNSS_POWER_MODE_M2
};

enum SpecialReqType {
    SPECIAL_REQ_INVALID = 0,
    SPECIAL_REQ_SHORT_CODE,   /* Short code */
};

struct TrackingOptions : LocationOptions {
    GnssPowerMode powerMode; /* Power Mode to be used for time based tracking
                                sessions */
    uint32_t tbm;  /* Time interval between measurements specified in millis.
                      Applicable to background power modes */
    SpecialReqType specialReq; /* Special Request type */

    inline TrackingOptions() :
            LocationOptions(), powerMode(GNSS_POWER_MODE_DEFAULT), tbm(0),
            specialReq(SPECIAL_REQ_INVALID){}
    inline TrackingOptions(const LocationOptions& options) :
            LocationOptions(options), powerMode(GNSS_POWER_MODE_DEFAULT), tbm(0),
            specialReq(SPECIAL_REQ_INVALID){}
    inline bool equalsInTimeBasedRequest(const TrackingOptions& other) const {
        return minInterval == other.minInterval && powerMode == other.powerMode &&
                tbm == other.tbm && qualityLevelAccepted == other.qualityLevelAccepted;
    }
    inline bool multiplexWithForTimeBasedRequest(const TrackingOptions& other) {
        bool updated = false;
        if (other.minInterval < minInterval) {
            updated = true;
            minInterval = other.minInterval;
        }
        if (other.powerMode < powerMode) {
            updated = true;
            powerMode = other.powerMode;
        }
        if (other.tbm < tbm) {
            updated = true;
            tbm = other.tbm;
        }
        if (other.qualityLevelAccepted > qualityLevelAccepted) {
            qualityLevelAccepted = other.qualityLevelAccepted;
        }
        return updated;
    }
    inline void setLocationOptions(const LocationOptions& options) {
        size = sizeof(TrackingOptions);
        minInterval = options.minInterval;
        minDistance = options.minDistance;
        mode = options.mode;
        locReqEngTypeMask = options.locReqEngTypeMask;
        qualityLevelAccepted = options.qualityLevelAccepted;
    }
    inline LocationOptions getLocationOptions() {
        LocationOptions locOption;
        locOption.size = sizeof(locOption);
        locOption.minDistance = minDistance;
        locOption.minInterval = minInterval;
        locOption.mode = mode;
        locOption.locReqEngTypeMask = locReqEngTypeMask;
        locOption.qualityLevelAccepted = qualityLevelAccepted;
        return locOption;
    }
};

struct BatchingOptions : LocationOptions {
    BatchingMode batchingMode;

    inline BatchingOptions() :
            LocationOptions(), batchingMode(BATCHING_MODE_ROUTINE) {}
    inline BatchingOptions(uint32_t s, BatchingMode m) :
            LocationOptions(), batchingMode(m) { LocationOptions::size = s; }
    inline BatchingOptions(const LocationOptions& options) :
            LocationOptions(options), batchingMode(BATCHING_MODE_ROUTINE) {}
    inline void setLocationOptions(const LocationOptions& options) {
        minInterval = options.minInterval;
        minDistance = options.minDistance;
        mode = options.mode;
    }
};

struct BatchingStatusInfo {
    uint32_t size;
    BatchingStatus batchingStatus;
};

struct GeofenceOption {
    uint32_t size;                          // set to sizeof(GeofenceOption)
    GeofenceBreachTypeMask breachTypeMask;  // bitwise OR of GeofenceBreachTypeBits
    uint32_t responsiveness;                // in milliseconds
    uint32_t dwellTime;                     // in seconds
    GeofenceConfidence confidence;          // confidence of breach event
};

struct GeofenceInfo {
    uint32_t size;    // set to sizeof(GeofenceInfo)
    double latitude;  // in degrees
    double longitude; // in degrees
    double radius;    // in meters
};

struct GeofenceBreachNotification {
    uint32_t size;             // set to sizeof(GeofenceBreachNotification)
    uint32_t count;            // number of ids in array
    uint32_t* ids;           // array of ids that have breached
    Location location;       // location associated with breach
    GeofenceBreachType type; // type of breach
    uint64_t timestamp;      // timestamp of breach
};

struct GeofenceStatusNotification {
    uint32_t size;                       // set to sizeof(GeofenceBreachNotification)
    GeofenceStatusAvailable available; // GEOFENCE_STATUS_AVAILABILE_NO/_YES
    LocationTechnologyType techType;   // GNSS
};

struct GnssLocationSvUsedInPosition {
    uint64_t gpsSvUsedIdsMask;
    uint64_t gloSvUsedIdsMask;
    uint64_t galSvUsedIdsMask;
    uint64_t bdsSvUsedIdsMask;
    uint64_t qzssSvUsedIdsMask;
    uint64_t navicSvUsedIdsMask;
};

struct GnssMeasUsageInfo {
    /** GnssSignalType mask */
    GnssSignalTypeMask gnssSignalType;
   /** Specifies GNSS Constellation Type */
    Gnss_LocSvSystemEnumType gnssConstellation;
    /** Unique SV Identifier.
     *  For SV Range of supported constellation, please refer to
     *  the comment section of svId in GnssSv.
     *  For GLONASS:  When slot-number to SV ID mapping is unknown, set as 255.
     */
    uint16_t gnssSvId;
};

/** @struct
    Body Frame parameters
*/
struct GnssLocationPositionDynamics {
    GnssLocationPosDataMask bodyFrameDataMask; // Contains Body frame LocPosDataMask bits
    float longAccel;                           // Forward Acceleration in body frame (m/s2)
    float latAccel;                            // Sideward Acceleration in body frame (m/s2)
    float vertAccel;                           // Vertical Acceleration in body frame (m/s2)
    float yawRate;                             // Heading Rate (Radians/second)
    float pitch;                               // Body pitch (Radians)
    float longAccelUnc;   // Uncertainty of Forward Acceleration in body frame
                          // Confidence level is at 68%
    float latAccelUnc;    // Uncertainty of Side-ward Acceleration in body frame
                          // Confidence level is at 68%
    float vertAccelUnc;   // Uncertainty of Vertical Acceleration in body frame
                          // Confidence level is at 68%
    float yawRateUnc;     // Uncertainty of Heading Rate
                          // Confidence level is at 68%
    float pitchUnc;       // Uncertainty of Body pitch
                          // Confidence level is at 68%
};

struct GnssLocationPositionDynamicsExt {
    GnssLocationPosDataMaskExt bodyFrameDataMask; // Contains Ext Body frame LocPosDataMask bits
    float pitchRate;      // Body pitch rate (Radians/second)
    float pitchRateUnc;   // Uncertainty of pitch rate (Radians/second)
                          // Confidence level is at 68%
    float roll;           // Roll of body frame. Clockwise positive. (radian
    float rollUnc;        // Uncertainty of Roll (radian)
                          // Confidence level is at 68%
    float rollRate;       // Roll rate of body frame. Clockwise positive. (radian/second)
    float rollRateUnc;    // Uncertainty of Roll rate (radian/second)
                          // Confidence level is at 68%
    float yaw;            // Yaw of body frame. Clockwise positive (radian)
    float yawUnc;         // Uncertainty of Yaw (radian)
                          // Confidence level is at 68%
};

struct GnssSystemTimeStructType {
    /** Validity mask for below fields */
    GnssSystemTimeStructTypeFlags validityMask;
    /** Extended week number at reference tick.
    Unit: Week.
    Set to 65535 if week number is unknown.
    For GPS:
      Calculated from midnight, Jan. 6, 1980.
      OTA decoded 10 bit GPS week is extended to map between:
      [NV6264 to (NV6264 + 1023)].
      NV6264: Minimum GPS week number configuration.
      Default value of NV6264: 1738
    For BDS:
      Calculated from 00:00:00 on January 1, 2006 of Coordinated Universal Time (UTC).
    For GAL:
      Calculated from 00:00 UT on Sunday August 22, 1999 (midnight between August 21 and August 22).
   */
    uint16_t systemWeek;
    /** Time in to the current week at reference tick.
       Unit: Millisecond. Range: 0 to 604799999.
       Check for systemClkTimeUncMs before use */
    uint32_t systemMsec;
    /** System clock time bias (sub-millisecond)
        Units: Millisecond
        Note: System time (TOW Millisecond) = systemMsec - systemClkTimeBias.
        Check for systemClkTimeUncMs before use. */
    float systemClkTimeBias;
    /** Single sided maximum time bias uncertainty
        Units: Millisecond */
    float systemClkTimeUncMs;
    /** FCount (free running HW timer) value. Don't use for relative time purpose
         due to possible discontinuities.
         Unit: Millisecond */
    uint32_t refFCount;
    /** Number of clock resets/discontinuities detected,
        affecting the local hardware counter value. */
    uint32_t numClockResets;

    inline bool hasAccurateTime() const {
        bool retVal = false;
        if ((validityMask & GNSS_SYSTEM_TIME_WEEK_VALID) &&
                // 65535 GPS week from modem means unknown
                (systemWeek != UNKNOWN_GPS_WEEK_NUM) &&
                (validityMask & GNSS_SYSTEM_TIME_WEEK_MS_VALID) &&
                (validityMask & GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID) &&
                (systemClkTimeUncMs != 0.0f) &&
                (systemClkTimeUncMs < REAL_TIME_ESTIMATOR_TIME_UNC_THRESHOLD_MSEC)) {
            retVal = true;
        }
        return retVal;
    }
};

struct GnssGloTimeStructType {
    /** GLONASS day number in four years. Refer to GLONASS ICD.
        Applicable only for GLONASS and shall be ignored for other constellations.
        If unknown shall be set to 65535 */
    uint16_t gloDays;
    /** Validity mask for below fields */
    GnssGloTimeStructTypeFlags validityMask;
    /** GLONASS time of day in Millisecond. Refer to GLONASS ICD.
        Units: Millisecond
        Check for gloClkTimeUncMs before use */
    uint32_t gloMsec;
    /** GLONASS clock time bias (sub-millisecond)
        Units: Millisecond
        Note: GLO time (TOD Millisecond) = gloMsec - gloClkTimeBias.
        Check for gloClkTimeUncMs before use. */
    float gloClkTimeBias;
    /** Single sided maximum time bias uncertainty
        Units: Millisecond */
    float gloClkTimeUncMs;
    /** FCount (free running HW timer) value. Don't use for relative time purpose
        due to possible discontinuities.
        Unit: Millisecond */
    uint32_t  refFCount;
    /** Number of clock resets/discontinuities detected,
        affecting the local hardware counter value. */
    uint32_t numClockResets;
    /** GLONASS four year number from 1996. Refer to GLONASS ICD.
        Applicable only for GLONASS and shall be ignored for other constellations.
        If unknown shall be set to 255 */
    uint8_t gloFourYear;
};

typedef union {
    GnssSystemTimeStructType gpsSystemTime;
    GnssSystemTimeStructType galSystemTime;
    GnssSystemTimeStructType bdsSystemTime;
    GnssSystemTimeStructType qzssSystemTime;
    GnssGloTimeStructType    gloSystemTime;
    GnssSystemTimeStructType navicSystemTime;
} SystemTimeStructUnion;

    /** Time applicability of PVT report */
struct GnssSystemTime {
    /** Specifies GNSS system time reported. Mandatory field */
    Gnss_LocSvSystemEnumType gnssSystemTimeSrc;
    /** Reporting of GPS system time is recommended.
      If GPS time is unknown & other satellite system time is known,
      it should be reported.
      Mandatory field
     */
    SystemTimeStructUnion u;

    inline bool hasAccurateGpsTime() const {
        bool retVal = false;
        if ((gnssSystemTimeSrc == GNSS_LOC_SV_SYSTEM_GPS) &&
                (u.gpsSystemTime.hasAccurateTime() == true)) {
            retVal = true;
        }
        return retVal;
    }

};

typedef uint32_t DrSolutionStatusMask;
#define VEHICLE_SENSOR_SPEED_INPUT_DETECTED    (1<<0)
#define VEHICLE_SENSOR_SPEED_INPUT_USED        (1<<1)
#define DRE_WARNING_UNCALIBRATED               (1<<2)
#define DRE_WARNING_GNSS_QUALITY_INSUFFICIENT  (1<<3)
#define DRE_WARNING_FERRY_DETECTED             (1<<4)
#define DRE_ERROR_6DOF_SENSOR_UNAVAILABLE      (1<<5)
#define DRE_ERROR_VEHICLE_SPEED_UNAVAILABLE    (1<<6)
#define DRE_ERROR_GNSS_EPH_UNAVAILABLE         (1<<7)
#define DRE_ERROR_GNSS_MEAS_UNAVAILABLE        (1<<8)
#define DRE_WARNING_INIT_POSITION_INVALID      (1<<9)
#define DRE_WARNING_INIT_POSITION_UNRELIABLE   (1<<10)
#define DRE_WARNING_POSITON_UNRELIABLE         (1<<11)
#define DRE_ERROR_GENERIC                      (1<<12)
#define DRE_WARNING_SENSOR_TEMP_OUT_OF_RANGE   (1<<13)
#define DRE_WARNING_USER_DYNAMICS_INSUFFICIENT (1<<14)
#define DRE_WARNING_FACTORY_DATA_INCONSISTENT  (1<<15)

struct LLAInfo {
    double latitude;  // in degree
    double longitude; // in degree
    float altitude;  // altitude wrt to ellipsoid
};

struct GnssLocationInfoNotification {
    uint32_t size;                      // set to sizeof(GnssLocationInfo)
    Location location;                  // basic locaiton info, latitude, longitude, and etc
    GnssLocationInfoFlagMask flags;     // bitwise OR of GnssLocationInfoBits for param validity
    float altitudeMeanSeaLevel;         // altitude wrt mean sea level
    float pdop;                         // position dilusion of precision
    float hdop;                         // horizontal dilusion of precision
    float vdop;                         // vertical dilusion of precision
    float gdop;                         // geometric  dilution of precision
    float tdop;                         // time dilution of precision
    float magneticDeviation;            // magnetic deviation
    LocationReliability horReliability; // horizontal reliability
    LocationReliability verReliability; // vertical reliability
    float horUncEllipseSemiMajor;       // horizontal elliptical accuracy semi-major axis
                                        // Confidence level is at 39%
    float horUncEllipseSemiMinor;       // horizontal elliptical accuracy semi-minor axis
                                        // Confidence level is at 39%
    float horUncEllipseOrientAzimuth;   // horizontal elliptical accuracy azimuth
    float northStdDeviation;            // North standard deviation Unit: Meters
                                        // Confidence level is at 68%
    float eastStdDeviation;             // East standard deviation. Unit: Meters
                                        // Confidence level is at 68%
    float northVelocity;                // North Velocity.Unit: Meters/sec
    float eastVelocity;                 // East Velocity Unit Meters/sec
    float upVelocity;                   // Up Velocity. Unit Meters/sec
    float northVelocityStdDeviation;    // Confidence level is at 68%
    float eastVelocityStdDeviation;     // Confidence level is at 68%
    float upVelocityStdDeviation;       // Confidence level is at 68%
    uint16_t numSvUsedInPosition;
    GnssLocationSvUsedInPosition svUsedInPosition;// Gnss sv used in position data
    GnssLocationNavSolutionMask navSolutionMask;  // Nav solution mask to indicate sbas corrections
    GnssLocationPositionDynamics bodyFrameData;   // Body Frame Dynamics: 4wayAcceleration and
                                                  // pitch set with validity
    GnssSystemTime gnssSystemTime;            // GNSS System Time
    uint8_t numOfMeasReceived; // Number of measurements received for use in fix.
    GnssMeasUsageInfo measUsageInfo[GNSS_SV_MAX]; // GNSS Measurement Usage info
    uint8_t leapSeconds;                          // leap second
    uint8_t calibrationConfidence;                // Sensor calibration confidence percent,
                                                  // in range of [0, 100]
    DrCalibrationStatusMask calibrationStatus;    // Sensor calibration status
    // location engine type. When the fix. when the type is set to
    // LOC_ENGINE_SRC_FUSED, the fix is the propagated/aggregated
    // reports from all engines running on the system (e.g.:
    // DR/SPE/PPE/VPE). To check which location engine contributes to
    // the fused output, check for locOutputEngMask.
    LocOutputEngineType locOutputEngType;
    // when loc output eng type is set to fused, this field
    // indicates the set of engines contribute to the fix.
    PositioningEngineMask locOutputEngMask;
    // When robust location is enabled, this field
    // will how well the various input data considered for
    // navigation solution conform to expectations.
    // Range: 0 (least conforming) to 1 (most conforming)
    float conformityIndex;
    GnssLocationPositionDynamicsExt bodyFrameDataExt;   // Additional Body Frame Dynamics
    // VRR-based latitude/longitude/altitude
    LLAInfo llaVRPBased;
    // VRR-based east, north, and up velocity
    float enuVelocityVRPBased[3];
    DrSolutionStatusMask drSolutionStatusMask;
    // true: altitude is assumed, false: altitude is calculated
    bool altitudeAssumed;
    // location session status
    loc_sess_status sessionStatus;
    // integrity risk used for protection level parameters.
    uint32_t integrityRiskUsed;
    // along-track protection level
    float    protectAlongTrack;
    // cross-track protection level
    float    protectCrossTrack;
    // vertical component protection level
    float    protectVertical;
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
};

// Indicate the API that is called to generate the location report
enum LocReportTriggerType {
    LOC_REPORT_TRIGGER_UNSPECIFIED               = 0,
    LOC_REPORT_TRIGGER_SIMPLE_TRACKING_SESSION   = 1,
    LOC_REPORT_TRIGGER_DETAILED_TRACKING_SESSION = 2,
    LOC_REPORT_TRIGGER_ENGINE_TRACKING_SESSION   = 3,
    LOC_REPORT_TRIGGER_SINGLE_TERRESTRIAL_FIX    = 4,
    LOC_REPORT_TRIGGER_SINGLE_FIX                = 5,
    LOC_REPORT_TRIGGER_TRIP_BATCHING_SESSION     = 6,
    LOC_REPORT_TRIGGER_ROUTINE_BATCHING_SESSION  = 7,
    LOC_REPORT_TRIGGER_GEOFENCE_SESSION          = 8,
};

struct DiagLocationInfoExt {
    LocationCapabilitiesMask capMask;
    uint64_t sessionStartBootTimestampNs;
    LocReportTriggerType reportTriggerType;
    inline DiagLocationInfoExt(LocationCapabilitiesMask inCapMask,
                               uint64_t inSessionStartBootTimestampNs,
                               LocReportTriggerType inReportTriggerType) :
            capMask(inCapMask), sessionStartBootTimestampNs(inSessionStartBootTimestampNs),
            reportTriggerType(inReportTriggerType) {}
};

struct GnssNiNotification {
    uint32_t size;                           // set to sizeof(GnssNiNotification)
    GnssNiType type;                       // type of NI (Voice, SUPL, Control Plane)
    GnssNiOptionsMask options;             // bitwise OR of GnssNiOptionsBits
    uint32_t timeout;                      // time (seconds) to wait for user input
    GnssNiResponse timeoutResponse;        // the response that should be sent when timeout expires
    char requestor[GNSS_NI_REQUESTOR_MAX]; // the requestor that is making the request
    GnssNiEncodingType requestorEncoding;  // the encoding type for requestor
    char message[GNSS_NI_MESSAGE_ID_MAX];  // the message to show user
    GnssNiEncodingType messageEncoding;    // the encoding type for message
    char extras[GNSS_NI_MESSAGE_ID_MAX];
};

// carrier frequency of the signal tracked
#define GPS_L1CA_CARRIER_FREQUENCY      (1575420000.0)
#define GPS_L1C_CARRIER_FREQUENCY       (1575420000.0)
#define GPS_L2C_L_CARRIER_FREQUENCY     (1227600000.0)
#define GPS_L5_Q_CARRIER_FREQUENCY      (1176450000.0)
#define GLONASS_G1_CARRIER_FREQUENCY    (1602000000.0)
#define GLONASS_G2_CARRIER_FREQUENCY    (1246000000.0)
#define GALILEO_E1_C_CARRIER_FREQUENCY  (1575420000.0)
#define GALILEO_E5A_Q_CARRIER_FREQUENCY (1176450000.0)
#define GALILEO_E5B_Q_CARRIER_FREQUENCY (1207140000.0)
#define BEIDOU_B1_I_CARRIER_FREQUENCY   (1561098000.0)
#define BEIDOU_B1C_CARRIER_FREQUENCY    (1575420000.0)
#define BEIDOU_B2_I_CARRIER_FREQUENCY   (1207140000.0)
#define BEIDOU_B2A_I_CARRIER_FREQUENCY  (1176450000.0)
#define BEIDOU_B2A_Q_CARRIER_FREQUENCY  (1176450000.0)
#define BEIDOU_B2B_I_CARRIER_FREQUENCY  (1207140000.0)
#define BEIDOU_B2B_Q_CARRIER_FREQUENCY  (1207140000.0)
#define QZSS_L1CA_CARRIER_FREQUENCY     (1575420000.0)
#define QZSS_L1S_CARRIER_FREQUENCY      (1575420000.0)
#define QZSS_L2C_L_CARRIER_FREQUENCY    (1227600000.0)
#define QZSS_L5_Q_CARRIER_FREQUENCY     (1176450000.0)
#define SBAS_L1_CA_CARRIER_FREQUENCY    (1575420000.0)
#define NAVIC_L5_CARRIER_FREQUENCY      (1176450000.0)
#define NAVIC_L1_CARRIER_FREQUENCY      (1575420000.0)

struct GnssSv {
    uint32_t size;       // set to sizeof(GnssSv)
    // Unique SV Identifier.
    // SV Range for supported constellation is specified as below:
    //    - For GPS:     1 to 32
    //    - For GLONASS: 65 to 96
    //    - For SBAS:    120 to 158 and 183 to 191
    //    - For QZSS:    193 to 197
    //    - For BDS:     201 to 263
    //    - For GAL:     301 to 336
    //    - For NAVIC:   401 to 420
    uint16_t svId;
    GnssSvType type;   // type of SV (GPS, SBAS, GLONASS, QZSS, BEIDOU, GALILEO, NAVIC)
    float cN0Dbhz;     // signal strength
    float elevation;   // elevation of SV (in degrees)
    float azimuth;     // azimuth of SV (in degrees)
    GnssSvOptionsMask gnssSvOptionsMask; // Bitwise OR of GnssSvOptionsBits
    double carrierFrequencyHz; // carrier frequency of the signal tracked
    GnssSignalTypeMask gnssSignalTypeMask; // Specifies GNSS signal type
    double basebandCarrierToNoiseDbHz; // baseband signal strength
    uint16_t  gloFrequency; // GLONASS Frequency channel number
};

struct GnssConfigSetAssistanceServer {
    uint32_t size;             // set to sizeof(GnssConfigSetAssistanceServer)
    GnssAssistanceType type; // SUPL or C2K
    const char* hostName;    // null terminated string
    uint32_t port;           // port of server

    inline bool equals(const GnssConfigSetAssistanceServer& config) {
        if (config.type == type && config.port == port &&
               ((NULL == config.hostName && NULL == hostName) ||
                (NULL != config.hostName && NULL != hostName &&
                     0 == strcmp(config.hostName, hostName)))) {
            return true;
        }
        return false;
    }
};

typedef uint32_t GnssSatellitePvtFlagsMask;
enum GnssSatellitePvtFlagsBits {
    GNSS_SATELLITE_PVT_POSITION_VELOCITY_CLOCK_INFO_BIT = (1 << 0),
    GNSS_SATELLITE_PVT_IONO_BIT = (1 << 1),
    GNSS_SATELLITE_PVT_TROPO_BIT = (1 << 2),
};

struct GnssSatellitePositionEcef {
    double posXMeters;
    double posYMeters;
    double posZMeters;
    double ureMeters;
};

struct GnssSatelliteVelocityEcef {
    double velXMps;
    double velYMps;
    double velZMps;
    double ureRateMps;
};

struct GnssSatelliteClockInfo {
    double satHardwareCodeBiasMeters;
    double satTimeCorrectionMeters;
    double satClkDriftMps;
};

enum GnssEphemerisSourceExt {
    GNSS_EPHEMERIS_SOURCE_EXT_INVALID = 0,
    GNSS_EPHEMERIS_SOURCE_EXT_DEMODULATED,
    GNSS_EPHEMERIS_SOURCE_EXT_SERVER_NORMAL,
    GNSS_EPHEMERIS_SOURCE_EXT_SERVER_LONG_TERM,
    GNSS_EPHEMERIS_SOURCE_EXT_OTHER,
};

struct GnssSatellitePvt {
    GnssSatellitePvtFlagsMask flags;
    GnssSatellitePositionEcef satPosEcef;
    GnssSatelliteVelocityEcef satVelEcef;
    GnssSatelliteClockInfo satClockInfo;
    double  ionoDelayMeters;
    double  tropoDelayMeters;
    int64_t TOC;
    int     IODC;
    int64_t TOE;
    int     IODE;
    GnssEphemerisSourceExt ephemerisSource;
};

struct GnssMeasurementsData {
    // set to sizeof(GnssMeasurementsData)
    uint32_t size;
    // bitwise OR of GnssMeasurementsDataFlagsBits
    GnssMeasurementsDataFlagsMask flags;
    // Unique SV Identifier
    // For SV Range of supported constellation,
    // please refer to the comment section of svId in GnssSv.
    int16_t svId;
    GnssSvType svType;
    double timeOffsetNs;
    GnssMeasurementsStateMask stateMask;       // bitwise OR of GnssMeasurementsStateBits
    // valid when GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT is set
    // total time is: receivedSvTimeNs + receivedSvTimeSubNs
    int64_t receivedSvTimeNs;
    // valid when GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT is set
    // total time is: receivedSvTimeNs + receivedSvTimeSubNs
    float  receivedSvTimeSubNs;
    int64_t  receivedSvTimeUncertaintyNs;
    double carrierToNoiseDbHz;
    double pseudorangeRateMps;
    double pseudorangeRateUncertaintyMps;
    GnssMeasurementsAdrStateMask adrStateMask; // bitwise OR of GnssMeasurementsAdrStateBits
    double adrMeters;
    double adrUncertaintyMeters;
    double carrierFrequencyHz;
    int64_t carrierCycles;
    double carrierPhase;
    double carrierPhaseUncertainty;
    GnssMeasurementsMultipathIndicator multipathIndicator;
    double signalToNoiseRatioDb;
    double agcLevelDb;
    GnssMeasurementsCodeType codeType;
    char otherCodeTypeName[GNSS_MAX_NAME_LENGTH];
    double basebandCarrierToNoiseDbHz;
    GnssSignalTypeMask gnssSignalType;
    double fullInterSignalBiasNs;
    double fullInterSignalBiasUncertaintyNs;
    double satelliteInterSignalBiasNs;
    double satelliteInterSignalBiasUncertaintyNs;
    int16_t gloFrequency;
    uint8_t cycleSlipCount;
    GnssSatellitePvt satellitePvt;
};

struct GnssMeasurementsSignalType {
    GnssSvType svType;
    double carrierFrequencyHz;
    GnssMeasurementsCodeType codeType;
    char otherCodeTypeName[GNSS_MAX_NAME_LENGTH];
};

struct GnssReflectingPlane {
    uint32_t size;                          // set to sizeof(GnssReflectingPlane)
    double latitudeDegrees;
    double longitudeDegrees;
    double altitudeMeters;
    double azimuthDegrees;
};

struct GnssSingleSatCorrection {
    uint32_t size;                          // set to sizeof(GnssSingleSatCorrection)
    GnssSingleSatCorrectionMask flags;
    GnssSvType svType;
    uint16_t svId;
    double carrierFrequencyHz;
    float probSatIsLos;
    float excessPathLengthMeters;
    float excessPathLengthUncertaintyMeters;
    GnssReflectingPlane reflectingPlane;
};

struct GnssMeasurementCorrections {
    uint32_t size;                          // set to sizeof(GnssMeasurementCorrections)
    double latitudeDegrees;
    double longitudeDegrees;
    double altitudeMeters;
    double horizontalPositionUncertaintyMeters;
    double verticalPositionUncertaintyMeters;
    uint64_t toaGpsNanosecondsOfWeek;
    std::vector<GnssSingleSatCorrection> satCorrections;
    bool hasEnvironmentBearing;
    float environmentBearingDegrees;
    float environmentBearingUncertaintyDegrees;
};

struct GnssMeasurementsClock {
    uint32_t size;                          // set to sizeof(GnssMeasurementsClock)
    GnssMeasurementsClockFlagsMask flags; // bitwise OR of GnssMeasurementsClockFlagsBits
    int16_t leapSecond;
    int64_t timeNs;
    double timeUncertaintyNs;
    int64_t fullBiasNs;
    double biasNs;
    double biasUncertaintyNs;
    double driftNsps;
    double driftUncertaintyNsps;
    uint32_t hwClockDiscontinuityCount;
    GnssMeasurementsSignalType referenceSignalTypeForIsb;
    uint64_t elapsedRealTime;    // in ns
    uint64_t elapsedRealTimeUnc; // in ns
    uint64_t elapsedgPTPTime;    // in ns
    uint64_t elapsedgPTPTimeUnc; // in ns
};

struct GnssSvNotification {
    uint32_t size;                 // set to sizeof(GnssSvNotification)
    uint32_t count;                // number of SVs in the GnssSv array
    bool gnssSignalTypeMaskValid;
    GnssSv gnssSvs[GNSS_SV_MAX]; // information on a number of SVs
};

struct GnssNmeaNotification {
    uint32_t size;         // set to sizeof(GnssNmeaNotification)
    uint64_t timestamp;  // timestamp
    LocOutputEngineType locOutputEngType; // engine type
    const char* nmea;    // nmea text
    uint32_t length;       // length of the nmea text
    bool isSvNmea;         //  is NMEA from SV report or not
};

struct GnssDataNotification {
    uint32_t size;                 // set to sizeof(GnssDataNotification)
    GnssDataMask  gnssDataMask[GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES];  // bitwise OR of GnssDataBits
    double        jammerInd[GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES];     // Jammer Indication
    double        agc[GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES];           // Automatic gain control
    AgcStatus     agcStatusL1; // RF Automatic gain control status for L1 band.
    AgcStatus     agcStatusL2; // RF Automatic gain control status for L2 band.
    AgcStatus     agcStatusL5; // RF Automatic gain control status for L5 band.
};

struct GnssMeasurementsAgc {
    double      agcLevelDb;
    GnssSvType  svType;
    double      carrierFrequencyHz;
};

struct GnssMeasurementsNotification {
    uint32_t size;         // set to sizeof(GnssMeasurementsNotification)
    bool isNhz;            // NHz indicator
    uint32_t count;        // number of items in GnssMeasurements array
    GnssMeasurementsData measurements[GNSS_MEASUREMENTS_MAX];
    GnssMeasurementsClock clock; // clock
    AgcStatus     agcStatusL1; // RF Automatic gain control status for L1 band.
    AgcStatus     agcStatusL2; // RF Automatic gain control status for L2 band.
    AgcStatus     agcStatusL5; // RF Automatic gain control status for L5 band.
    bool isFullTracking;
    uint32_t agcCount;     // number of items in GnssMeasurementsAgc array
    GnssMeasurementsAgc gnssAgc[GNSS_BANDS_MAX];
};

struct GnssCapabNotification {
    uint32_t size;              // set to sizeof(GnssCapabilitiesNotification)
    uint32_t count;             // number of SVs in the gnssSignalType array
    GnssMeasurementsSignalType  gnssSignalType[GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES];
    GnssSignalTypeMask gnssSupportedSignals; // GNSS Supported Signals
};

typedef uint32_t GnssSvId;

struct GnssSvIdSource{
    uint32_t size;              // set to sizeof(GnssSvIdSource)
    GnssSvType constellation;   // constellation for the sv to blacklist
    GnssSvId svId;              // Unique SV Identifier,
                                // For SV Range of supported constellation,
                                // please refer to the comment section of svId in GnssSv.
};
inline bool operator ==(GnssSvIdSource const& left, GnssSvIdSource const& right) {
    return left.size == right.size &&
            left.constellation == right.constellation && left.svId == right.svId;
}

#define GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK ((uint64_t)0xFFFFFFFFFFFFFFFF)
struct GnssSvIdConfig {
    uint32_t size; // set to sizeof(GnssSvIdConfig)

    // GPS - SV 1 maps to bit 0
#define GNSS_SV_CONFIG_GPS_INITIAL_SV_ID 1
    uint64_t gpsBlacklistSvMask;

    // GLONASS - SV 65 maps to bit 0
#define GNSS_SV_CONFIG_GLO_INITIAL_SV_ID 65
    uint64_t gloBlacklistSvMask;

    // BEIDOU - SV 201 maps to bit 0
#define GNSS_SV_CONFIG_BDS_INITIAL_SV_ID 201
    uint64_t bdsBlacklistSvMask;

    // QZSS - SV 193 maps to bit 0
#define GNSS_SV_CONFIG_QZSS_INITIAL_SV_ID 193
    uint64_t qzssBlacklistSvMask;

    // GAL - SV 301 maps to bit 0
#define GNSS_SV_CONFIG_GAL_INITIAL_SV_ID 301
    uint64_t galBlacklistSvMask;

    // SBAS - SV 120 to 158, maps to 0 to 38
    //        SV 183 to 191, maps to 39 to 47
#define GNSS_SV_CONFIG_SBAS_INITIAL_SV_ID     120
#define GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH 39
#define GNSS_SV_CONFIG_SBAS_INITIAL2_SV_ID    183
    uint64_t sbasBlacklistSvMask;

    //Navic - SV 401 maps to bit 0
#define GNSS_SV_CONFIG_NAVIC_INITIAL_SV_ID 401
    uint64_t navicBlacklistSvMask;

    inline bool equals(const GnssSvIdConfig& inConfig) {
        if ((inConfig.size == size) &&
                (inConfig.gpsBlacklistSvMask == gpsBlacklistSvMask) &&
                (inConfig.gloBlacklistSvMask == gloBlacklistSvMask) &&
                (inConfig.bdsBlacklistSvMask == bdsBlacklistSvMask) &&
                (inConfig.qzssBlacklistSvMask == qzssBlacklistSvMask) &&
                (inConfig.galBlacklistSvMask == galBlacklistSvMask) &&
                (inConfig.sbasBlacklistSvMask == sbasBlacklistSvMask) &&
                (inConfig.navicBlacklistSvMask == navicBlacklistSvMask)) {
            return true;
        } else {
            return false;
        }
    }
};

// Specify the valid mask for robust location configure
// defined in GnssConfigRobustLocation.
enum GnssConfigRobustLocationValidMask {
    // GnssConfigRobustLocation has valid enabled field.
    GNSS_CONFIG_ROBUST_LOCATION_ENABLED_VALID_BIT          = (1<<0),
    // GnssConfigRobustLocation has valid enabledForE911 field.
    GNSS_CONFIG_ROBUST_LOCATION_ENABLED_FOR_E911_VALID_BIT = (1<<1),
    // GnssConfigRobustLocation has valid version field.
    GNSS_CONFIG_ROBUST_LOCATION_VERSION_VALID_BIT          = (1<<2),
};

struct GnssConfigRobustLocationVersion {
    // Major version number
    uint8_t major;
    // Minor version number
    uint16_t minor;
    inline bool equals(const GnssConfigRobustLocationVersion& version) const {
        return (version.major == major && version.minor == minor);
    }
};

// specify the robust location configuration used by modem GNSS engine
struct GnssConfigRobustLocation {
   GnssConfigRobustLocationValidMask validMask;
   bool enabled;
   bool enabledForE911;
   GnssConfigRobustLocationVersion version;

   inline bool equals(const GnssConfigRobustLocation& config) const {
        if (config.validMask == validMask &&
            config.enabled == enabled &&
            config.enabledForE911 == enabledForE911 &&
            config.version.equals(version)) {
            return true;
        }
        return false;
    }
};

/* Mask indicating enabled or disabled constellations and
   secondary frequency.*/
typedef uint64_t GnssSvTypesMask;
enum GnssSvTypesMaskBits {
    GNSS_SV_TYPES_MASK_GLO_BIT   = (1<<0),
    GNSS_SV_TYPES_MASK_BDS_BIT   = (1<<1),
    GNSS_SV_TYPES_MASK_QZSS_BIT  = (1<<2),
    GNSS_SV_TYPES_MASK_GAL_BIT   = (1<<3),
    GNSS_SV_TYPES_MASK_NAVIC_BIT = (1<<4),
    GNSS_SV_TYPES_MASK_GPS_BIT   = (1<<5),
};
#define GNSS_SV_TYPES_MASK_ALL \
    (GNSS_SV_TYPES_MASK_GPS_BIT|GNSS_SV_TYPES_MASK_GLO_BIT|GNSS_SV_TYPES_MASK_BDS_BIT|\
     GNSS_SV_TYPES_MASK_QZSS_BIT|GNSS_SV_TYPES_MASK_GAL_BIT|GNSS_SV_TYPES_MASK_NAVIC_BIT)

/* This SV Type config is injected directly to GNSS Adapter
 * bypassing Location API */
struct GnssSvTypeConfig{
    uint32_t size; // set to sizeof(GnssSvTypeConfig)
    // Enabled Constellations
    GnssSvTypesMask enabledSvTypesMask;
    // Disabled Constellations
    GnssSvTypesMask blacklistedSvTypesMask;

    inline bool equals (const GnssSvTypeConfig& inConfig) const {
        return ((inConfig.size == size) &&
                (inConfig.enabledSvTypesMask == enabledSvTypesMask) &&
                (inConfig.blacklistedSvTypesMask == blacklistedSvTypesMask));
    }
};

// Specify the caller who set SV Type config
enum GnssSvTypeConfigSource {
    SV_TYPE_CONFIG_FROM_API = 0,
    SV_TYPE_CONFIG_FROM_XTRA,
    SV_TYPE_CONFIG_MAX_SOURCE
};

/** Specify the XTRA assistance data status. */
enum XtraDataStatus {
    /** If XTRA feature is disabled or if XTRA feature is enabled,
     *  but XTRA daemon has not yet retrieved the assistance data
     *  status from modem on early stage of device bootup, xtra data
     *  status will be unknown.   */
    XTRA_DATA_STATUS_UNKNOWN = 0,
    /** If XTRA feature is enabled, but XTRA data is not present
     *  on the device. */
    XTRA_DATA_STATUS_NOT_AVAIL = 1,
    /** If XTRA feature is enabled, XTRA data has been downloaded
     *  but it is no longer valid. */
    XTRA_DATA_STATUS_NOT_VALID = 2,
    /** If XTRA feature is enabled, XTRA data has been downloaded
     *  and is currently valid. */
    XTRA_DATA_STATUS_VALID = 3,
};

struct XtraStatus {
    /** XTRA assistance data and NTP time download is enabled or
     *  disabled. */
    bool featureEnabled;
    /** XTRA assistance data status. If XTRA assistance data
     *  download is not enabled, this field will be set to
     *  XTRA_DATA_STATUS_UNKNOWN. */
    XtraDataStatus xtraDataStatus;
    /** Number of hours that xtra assistance data will remain valid.
     *  This field will be valid when xtraDataStatus is set to
     *  XTRA_DATA_STATUS_VALID.
     *  For all other XtraDataStatus, this field will be set to
     *  0. */
    uint32_t xtraValidForHours;
    std::string lastDownloadReasonCode;

    inline bool equals (const XtraStatus& inXtraStatus) const {
        if (inXtraStatus.featureEnabled != featureEnabled) {
            return false;
        } else if (featureEnabled == false) {
            return true;
        } else if ((inXtraStatus.xtraDataStatus == xtraDataStatus) &&
                   (inXtraStatus.xtraValidForHours == xtraValidForHours) &&
                   (0 == inXtraStatus.lastDownloadReasonCode.compare(lastDownloadReasonCode))) {
            return true;
        } else {
            return false;
        }
    }
};

struct GnssConfig {
    uint32_t size;  // set to sizeof(GnssConfig)
    GnssConfigFlagsMask flags; // bitwise OR of GnssConfigFlagsBits to mark which params are valid
    GnssConfigGpsLock gpsLock;
    GnssConfigSuplVersion suplVersion;
    GnssConfigSetAssistanceServer assistanceServer;
    GnssConfigLppProfileMask lppProfileMask;
    GnssConfigLppeControlPlaneMask lppeControlPlaneMask;
    GnssConfigLppeUserPlaneMask lppeUserPlaneMask;
    GnssConfigAGlonassPositionProtocolMask aGlonassPositionProtocolMask;
    GnssConfigEmergencyPdnForEmergencySupl emergencyPdnForEmergencySupl;
    GnssConfigSuplEmergencyServices suplEmergencyServices;
    GnssConfigSuplModeMask suplModeMask; //bitwise OR of GnssConfigSuplModeBits
    std::vector<GnssSvIdSource> blacklistedSvIds;
    uint32_t emergencyExtensionSeconds;
    GnssConfigRobustLocation robustLocationConfig;
    uint16_t minGpsWeek;
    uint8_t minSvElevation;
    GnssSvTypeConfig secondaryBandConfig;
    XtraStatus xtraStatus;

    inline bool equals(const GnssConfig& config) {
        if (flags == config.flags &&
                gpsLock == config.gpsLock &&
                suplVersion == config.suplVersion &&
                assistanceServer.equals(config.assistanceServer) &&
                lppProfileMask == config.lppProfileMask &&
                lppeControlPlaneMask == config.lppeControlPlaneMask &&
                lppeUserPlaneMask == config.lppeUserPlaneMask &&
                aGlonassPositionProtocolMask == config.aGlonassPositionProtocolMask &&
                emergencyPdnForEmergencySupl == config.emergencyPdnForEmergencySupl &&
                suplEmergencyServices == config.suplEmergencyServices &&
                suplModeMask == config.suplModeMask  &&
                blacklistedSvIds == config.blacklistedSvIds &&
                emergencyExtensionSeconds == config.emergencyExtensionSeconds &&
                robustLocationConfig.equals(config.robustLocationConfig) &&
                minGpsWeek == config.minGpsWeek &&
                minSvElevation == config.minSvElevation &&
                secondaryBandConfig.equals(config.secondaryBandConfig)) {
            return true;
        }
        return false;
    }
};

struct GnssDebugLocation {
    uint32_t size;                        // set to sizeof
    bool                                mValid;
    Location                            mLocation;
    double                              verticalAccuracyMeters;
    double                              speedAccuracyMetersPerSecond;
    double                              bearingAccuracyDegrees;
    timespec                            mUtcReported;
};

struct GnssDebugTime {
    uint32_t size;                        // set to sizeof
    bool                                mValid;
    int64_t                             timeEstimate;
    float                               timeUncertaintyNs;
    float                               frequencyUncertaintyNsPerSec;
};

struct GnssDebugSatelliteInfo {
    // set to sizeof
    uint32_t size;
    // Unique SV Identifier
    // For SV Range of supported constellation,
    // please refer to the comment section of svId in GnssSv.
    uint32_t                            svid;
    GnssSvType                          constellation;
    GnssEphemerisType                   mEphemerisType;
    GnssEphemerisSource                 mEphemerisSource;
    GnssEphemerisHealth                 mEphemerisHealth;
    float                               ephemerisAgeSeconds;
    bool                                serverPredictionIsAvailable;
    float                               serverPredictionAgeSeconds;
};

struct GnssDebugReport {
    uint32_t size;                        // set to sizeof
    GnssDebugLocation                   mLocation;
    GnssDebugTime                       mTime;
    std::vector<GnssDebugSatelliteInfo> mSatelliteInfo;
};

typedef uint32_t LeapSecondSysInfoMask;
enum LeapSecondSysInfoDataBits {
    // current leap second info is available. This info will only
    // be available if the leap second change info is not available.
    //
    // If leap second change info is avaiable, to figure out
    // the current leap second info, compare current gps time with
    // the gps timestamp of leap second change to know whether to choose
    // leapSecondBefore or leapSecondAfter as current leap second.
    LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT = (1ULL << 0),
    // the last known leap change event is available.
    // The info can be available on two scenario:
    // 1: this leap second change event has been scheduled and yet to happen
    // 2: this leap second change event has already happened and next
    //    leap second change event has not yet been scheduled.
    LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT = (1ULL << 1),
};

struct LeapSecondChangeInfo {
    // GPS timestamp that corrresponds to the last known
    // leap second change event.
    //
    // The info can be available on two scenario:
    // 1: this leap second change event has been scheduled and yet to happen
    // 2: this leap second change event has already happened and next
    //    leap second change event has not yet been scheduled.
    GnssSystemTimeStructType gpsTimestampLsChange;
    // Number of leap seconds prior to the leap second change event
    // that corresponds to the timestamp at gpsTimestampLsChange.
    uint8_t leapSecondsBeforeChange;
    // Number of leap seconds after the leap second change event
    // that corresponds to the timestamp at gpsTimestampLsChange.
    uint8_t leapSecondsAfterChange;
};

struct LeapSecondSystemInfo {
    LeapSecondSysInfoMask leapSecondInfoMask;
    uint8_t               leapSecondCurrent;
    LeapSecondChangeInfo  leapSecondChangeInfo;
};

typedef uint32_t LocationSystemInfoMask;
enum LocationSystemInfoDataBits {
    // contains current leap second or leap second change info
    LOCATION_SYS_INFO_LEAP_SECOND = (1ULL << 0),
};

struct LocationSystemInfo {
    LocationSystemInfoMask systemInfoMask;
    LeapSecondSystemInfo   leapSecondSysInfo;
};

// Disaster crisis report type from GNSS engine
enum GnssDcReportType {
    GNSS_DC_REPORT_TYPE_UNDEFINED = 0,
    // Disaster Prevention information provided by Japan Meteolorogical Agency
    QZSS_JMA_DISASTER_PREVENTION_INFO = 43,
    // Disaster Prevention information provided by other organizations
    QZSS_NON_JMA_DISASTER_PREVENTION_INFO = 44,
};

// Disaster crisis report from GNSS engine
struct GnssDcReportInfo {
    // dc report type, as defined in standard
    GnssDcReportType     dcReportType;
    // number of valid bits that client should make use in dcReportData
    uint32_t             numValidBits;
    // dc report data, packed into uint8_t
    std::vector<uint8_t> dcReportData;
};

// Specify the set of terrestrial technologies
enum TerrestrialTechMask {
    TERRESTRIAL_TECH_GTP_WWAN = 1 << 0,
};

// Specify parameters related to lever arm
struct LeverArmParams {
    // Offset along the vehicle forward axis
    float forwardOffsetMeters;
    // Offset along the vehicle starboard axis
    float sidewaysOffsetMeters;
    // Offset along the vehicle up axis
    float upOffsetMeters;
};

typedef uint32_t LeverArmTypeMask;

enum LeverArmTypeBits {
    // Lever arm regarding the VRP (Vehicle Reference Point) w.r.t
    // the origin (at the GPS Antenna)
    LEVER_ARM_TYPE_GNSS_TO_VRP_BIT = (1<<0),
    // Lever arm regarding GNSS Antenna w.r.t the origin at the IMU
    // e.g.: inertial measurement unit for DR (dead reckoning
    // engine)
    LEVER_ARM_TYPE_DR_IMU_TO_GNSS_BIT = (1<<1),
    // Lever arm regarding GNSS Antenna w.r.t the origin at the
    // IMU (inertial measurement unit) for VEPP (vision enhanced
    // precise positioning engine)
    LEVER_ARM_TYPE_VEPP_IMU_TO_GNSS_BIT = (1<<2)
};

struct LeverArmConfigInfo {
    // Valid mask for the types of lever arm parameters provided
    LeverArmTypeMask leverArmValidMask;
    // Lever arm regarding the VRP (Vehicle Reference Point) w.r.t the origin
    // (at the GPS Antenna)
    LeverArmParams   gnssToVRP;
    // Lever arm parameters regarding GNSS Antenna w.r.t the origin at the IMU
    // (inertial measurement unit) for DR (dead reckoning engine)
    LeverArmParams   drImuToGnss;
    // Lever arm regarding GNSS Antenna w.r.t the origin at the IMU
    // (inertial measurement unit) for VEPP (vision enhanced precise position engine)
    LeverArmParams   veppImuToGnss;
};

// Specify vehicle body-to-Sensor mount parameters to be used
// by dead reckoning positioning engine.
struct BodyToSensorMountParams {
    // The misalignment of the sensor board along the
    // horizontal plane of the vehicle chassis measured looking
    // from the vehicle to forward direction. In unit of degree.
    float rollOffset;
    // The misalignment along the horizontal plane of the vehicle
    // chassis measured looking from the vehicle to the right
    // side. Positive pitch indicates vehicle is inclined such
    // that forward wheels are at higher elevation than rear
    // wheels. In unit of degree.
    float yawOffset;
    // The angle between the vehicle forward direction and the
    // sensor axis as seen from the top of the vehicle, and
    // measured in counterclockwise direction. In unit of degree.
    float pitchOffset;
    // Single uncertainty number that may be the largest of the
    // roll, pitch and yaw offset uncertainties.
    float offsetUnc;
};

typedef uint64_t DeadReckoningEngineConfigValidMask;
// Specify the valid mask for the configuration paramters of
// dead reckoning position engine.
enum DeadReckoningEngineConfigValidBit {
    // DeadReckoningEngineConfig has valid
    // DeadReckoningEngineConfig::DeadReckoningEngineConfig.
    BODY_TO_SENSOR_MOUNT_PARAMS_BIT    = (1<<0),
    // DeadReckoningEngineConfig has valid
    //  DeadReckoningEngineConfig::vehicleSpeedScaleFactor.
    VEHICLE_SPEED_SCALE_FACTOR_BIT     = (1<<1),
    // DeadReckoningEngineConfig has valid
    //  DeadReckoningEngineConfig::vehicleSpeedScaleFactorUnc.
    VEHICLE_SPEED_SCALE_FACTOR_UNC_BIT = (1<<2),
    // DeadReckoningEngineConfig has valid
    //  DeadReckoningEngineConfig::gyroScaleFactor.
    GYRO_SCALE_FACTOR_BIT              = (1<<3),
    // DeadReckoningEngineConfig has valid
    // DeadReckoningEngineConfig::gyroScaleFactorUnc.
    GYRO_SCALE_FACTOR_UNC_BIT          = (1<<4),
};

// Specify the configuration parameters for the dead reckoning
//  position engine
struct DeadReckoningEngineConfig{
    // Specify the valid fields in the config.
    DeadReckoningEngineConfigValidMask validMask;
    // Body to sensor mount parameters for use by dead reckoning
    //  positioning engine
    BodyToSensorMountParams bodyToSensorMountParams;

    // Vehicle Speed Scale Factor configuration input for the dead
    // reckoning positioning engine. The multiplicative scale
    // factor is applied to received Vehicle Speed value (in m/s)
    // to obtain the true Vehicle Speed.
    //
    // Range is [0.9 to 1.1].
    //
    // Note: The scale factor is specific to a given vehicle
    // make & model.
    float vehicleSpeedScaleFactor;
    // Vehicle Speed Scale Factor Uncertainty (68% confidence)
    // configuration input for the dead reckoning positioning
    // engine.
    //
    // Range is [0.0 to 0.1].
    //
    // Note: The scale factor unc is specific to a given vehicle
    // make & model.
    float vehicleSpeedScaleFactorUnc;

    // Gyroscope Scale Factor configuration input for the dead
    // reckoning positioning engine. The multiplicative scale
    // factor is applied to received gyroscope value to obtain the
    // true value.
    //
    // Range is [0.9 to 1.1].
    //
    // Note: The scale factor is specific to the Gyroscope sensor
    // and typically derived from either sensor data-sheet or
    // from actual calibration.
    float gyroScaleFactor;

    // Gyroscope Scale Factor uncertainty (68% confidence)
    // configuration input for the dead reckoning positioning
    // engine.
    //
    // Range is [0.0 to 0.1].
    // engine.
    //
    // Note: The scale factor unc is specific to the make & model
    // of Gyroscope sensor and typically derived from either
    // sensor data-sheet or from actual calibration.
    float gyroScaleFactorUnc;
};

/*  Specify the NMEA sentence types that are generated by GNSS
 *  stack on HLOS.
 *
 *  Please note that this setting is only applicable if
 *  NMEA_PROVIDER in gps.conf is set to 0 to use HLOS
 *  generated NMEA. */
enum GnssNmeaTypesMask {
    NMEA_TYPE_NONE     = (0x0),
    // GGA NMEA sentence.
    NMEA_TYPE_GGA      = (1<<0),
    // RMC NMEA sentence.
    NMEA_TYPE_RMC      = (1<<1),
    // GSA NMEA sentence.
    NMEA_TYPE_GSA      = (1<<2),
    // VTG NMEA sentence.
    NMEA_TYPE_VTG      = (1<<3),
    // GNS NMEA sentence.
    NMEA_TYPE_GNS      = (1<<4),
    // DTM NMEA sentence.
    NMEA_TYPE_DTM      = (1<<5),
    // GPGSV NMEA sentence for SVs from GPS constellation.
    NMEA_TYPE_GPGSV    = (1<<6),
    // GLGSV NMEA sentence for SVs from GLONASS constellation.
    NMEA_TYPE_GLGSV    = (1<<7),
    // GAGSV NMEA sentence for SVs from GALILEO constellation
    NMEA_TYPE_GAGSV    = (1<<8),
    // GQGSV NMEA sentence for SVs from QZSS constellation.
    NMEA_TYPE_GQGSV    = (1<<9),
    // GBGSV NMEA sentence for SVs from BEIDOU constellation.
    NMEA_TYPE_GBGSV    = (1<<10),
    // GIGSV NMEA sentence for SVs from NAVIC constellation.
    NMEA_TYPE_GIGSV    = (1<<11),
    // All HLOS supported NMEA  sentences.
    NMEA_TYPE_ALL        = 0xffffffff,
};

/*  Specify the Geodetic datum for NMEA sentence types that
 *  are generated by GNSS stack on HLOS.
 *
 *  Please note that this setting is only applicable if
 *  NMEA_PROVIDER in gps.conf is set to 0 to use HLOS
 *  generated NMEA. */
enum GnssGeodeticDatumType {
    // Geodetic datum defined in World Geodetic System 1984 (WGS84) format.
    GEODETIC_TYPE_WGS_84 = 0,
    // Geodetic datum defined for use in the GLONASS system.
    GEODETIC_TYPE_PZ_90 = 1,
};

/* ODCPI Request Info */
enum OdcpiRequestType {
    ODCPI_REQUEST_TYPE_START,
    ODCPI_REQUEST_TYPE_STOP
};

/* ODCPI callback priorities*/
enum OdcpiPrioritytype {
    //ODCPI callback registered by AFW via IGNSS AIDL has LOW priority
    ODCPI_HANDLER_PRIORITY_LOW,
    ODCPI_HANDLER_PRIORITY_DEFAULT = ODCPI_HANDLER_PRIORITY_LOW,
    //ODCPI callback registered by IzatProvider on LE/KaiOS has medium priority
    ODCPI_HANDLER_PRIORITY_MEDIUM,
    //Non emergency ODCPI callback registered by IzatManager for RTT position injection
    //has high priority
    ODCPI_HANDLER_PRIORITY_HIGH
};

struct OdcpiRequestInfo {
    uint32_t size;
    OdcpiRequestType type;
    uint32_t tbfMillis;
    bool isEmergencyMode;
    bool isCivicAddressRequired;
};

/** AGPS type */
enum AGpsType {
    AGPS_TYPE_INVALID = - 1,
    AGPS_TYPE_ANY = 0,
    AGPS_TYPE_SUPL,
    AGPS_TYPE_C2K,
    AGPS_TYPE_WWAN_ANY,
    AGPS_TYPE_WIFI,
    AGPS_TYPE_SUPL_ES
};

enum SubId {
    DEFAULT_SUB = 0,
    PRIMARY_SUB = 1,
    SECONDARY_SUB =  2,
    TERTIARY_SUB =  3
};

typedef uint32_t AGpsTypeMask;
enum AGpsTypeBits {
    AGPS_ATL_TYPE_SUPL    = 1 << 0,
    AGPS_ATL_TYPE_SUPL_ES = 1 << 1,
    AGPS_ATL_TYPE_WWAN    = 1 << 2,
};

enum AgpsCbPriority {
    AGPS_CB_PRIORITY_NONE = 0,
    AGPS_CB_PRIORITY_LOW = 1,
    AGPS_CB_PRIORITY_MED = 2,
    AGPS_CB_PRIORITY_HIGH = 3,
};

/** AGPS status event values. */
enum AGpsStatusValue {
    AGPS_REQUEST_AGPS_DATA_CONN  = 1,
    /** GPS releases the AGPS data connection. */
    AGPS_RELEASE_AGPS_DATA_CONN,
    /** AGPS data connection initiated */
    AGPS_DATA_CONNECTED,
    /** AGPS data connection completed */
    AGPS_DATA_CONN_DONE,
    /** AGPS data connection failed */
    AGPS_DATA_CONN_FAILED
};

typedef uint32_t ApnTypeMask;
enum ApnTypeBits {
    /**<  Denotes APN type for Default/Internet traffic  */
    APN_TYPE_DEFAULT_BIT = (1 << 0),
    /**<  Denotes  APN type for IP Multimedia Subsystem  */
    APN_TYPE_IMS_BIT = (1 << 1),
    /**<  Denotes APN type for Multimedia Messaging Service  */
    APN_TYPE_MMS_BIT = (1 << 2),
    /**<  Denotes APN type for Dial Up Network  */
    APN_TYPE_DUN_BIT = (1 << 3),
    /**<  Denotes APN type for Secure User Plane Location  */
    APN_TYPE_SUPL_BIT = (1 << 4),
    /**<  Denotes APN type for High Priority Mobile Data  */
    APN_TYPE_HIPRI_BIT = (1 << 5),
    /**<  Denotes APN type for over the air administration  */
    APN_TYPE_FOTA_BIT = (1 << 6),
    /**<  Denotes APN type for Carrier Branded Services  */
    APN_TYPE_CBS_BIT = (1 << 7),
    /**<  Denotes APN type for Initial Attach  */
    APN_TYPE_IA_BIT = (1 << 8),
    /**<  Denotes APN type for emergency  */
    APN_TYPE_EMERGENCY_BIT  = (1 << 9)
};


/*
 * Represents the status of AGNSS augmented to support IPv4.
 */
struct AGnssExtStatusIpV4 {
    AGpsType         type;
    ApnTypeMask      apnTypeMask;
    AGpsStatusValue  status;
    /*
     * 32-bit IPv4 address.
     */
    uint32_t          ipV4Addr;
    SubId             subId;
};

struct GnssCoordinate {
    uint32_t size;                        // set to sizeof
    double x;
    double xUncertainty;
    double y;
    double yUncertainty;
    double z;
    double zUncertainty;
};


struct GnssAntennaInformation{
    uint32_t size;                        // set to sizeof
    double carrierFrequencyMHz;
    GnssCoordinate phaseCenterOffsetCoordinateMillimeters;
    std::vector<std::vector<double>> phaseCenterVariationCorrectionMillimeters;
    std::vector<std::vector<double>> phaseCenterVariationCorrectionUncertaintyMillimeters;
    std::vector<std::vector<double>> signalGainCorrectionDbi;
    std::vector<std::vector<double>> signalGainCorrectionUncertaintyDbi;
};

struct GnssNtripConnectionParams {
    uint32_t size;                        // set to sizeof
    bool requiresNmeaLocation;
    std::string hostNameOrIp;    // null terminated string
    std::string mountPoint;      // null terminated string
    std::string username;        // null terminated string
    std::string password;        // null terminated string
    uint32_t port;
    bool useSSL;
    uint32_t nmeaUpdateInterval; // unit: second
};

/*
* Represents the the Nfw Notification structure
*/
#define GNSS_MAX_NFW_APP_STRING_LEN 64
#define GNSS_MAX_NFW_STRING_LEN  20

enum GnssNfwProtocolStack {
    GNSS_NFW_CTRL_PLANE = 0,
    GNSS_NFW_SUPL = 1,
    GNSS_NFW_IMS = 10,
    GNSS_NFW_SIM = 11,
    GNSS_NFW_OTHER_PROTOCOL_STACK = 100
};

enum GnssNfwRequestor {
    GNSS_NFW_CARRIER = 0,
    GNSS_NFW_OEM = 10,
    GNSS_NFW_MODEM_CHIPSET_VENDOR = 11,
    GNSS_NFW_GNSS_CHIPSET_VENDOR = 12,
    GNSS_NFW_OTHER_CHIPSET_VENDOR = 13,
    GNSS_NFW_AUTOMOBILE_CLIENT = 20,
    GNSS_NFW_OTHER_REQUESTOR = 100
};

enum GnssNfwResponseType {
    GNSS_NFW_REJECTED = 0,
    GNSS_NFW_ACCEPTED_NO_LOCATION_PROVIDED = 1,
    GNSS_NFW_ACCEPTED_LOCATION_PROVIDED = 2,
};

struct GnssNfwNotification {
    char                    proxyAppPackageName[GNSS_MAX_NFW_APP_STRING_LEN];
    GnssNfwProtocolStack    protocolStack;
    char                    otherProtocolStackName[GNSS_MAX_NFW_STRING_LEN];
    GnssNfwRequestor        requestor;
    char                    requestorId[GNSS_MAX_NFW_STRING_LEN];
    GnssNfwResponseType     responseType;
    bool                    inEmergencyMode;
    bool                    isCachedLocation;
};

typedef uint16_t GnssMeasurementCorrectionsCapabilitiesMask;
enum GnssMeasurementCorrectionsCapabilities {
    GNSS_MEAS_CORR_LOS_SATS            = 1 << 0,
    GNSS_MEAS_CORR_EXCESS_PATH_LENGTH  = 1 << 1,
    GNSS_MEAS_CORR_REFLECTING_PLANE    = 1 << 2,
};

 /*  Specify the valid fields in GnssEnergyConsumedInfo. */
enum GnssEnergyConsumedInfoMask {
    /** GnssEnergyConsumedInfo has valid
     GnssEnergyConsumedInfo::totalEnergyConsumedSinceFirstBoot. */
    ENERGY_CONSUMED_SINCE_FIRST_BOOT_BIT = (1<<0),
};

/*  Specify the info regarding energy consumed by GNSS engine. */
struct GnssEnergyConsumedInfo {
    /** Bitwise OR of GnssEnergyConsumedInfoMask to
     specify the valid fields in GnssEnergyConsumedInfo.*/
    GnssEnergyConsumedInfoMask flags;

    /** Energy consumed by the modem GNSS engine since device first
     ever bootup, in unit of 0.1 milli watt seconds.
     A value of 0xffffffffffffffff indicates an invalid reading.*/
    uint64_t totalEnergyConsumedSinceFirstBoot;
};

/** Specify the logcat debug level. Currently, only XTRA
 *  daemon will support the runtime configure of debug log
 *  level. */
enum DebugLogLevel {
    /** No debug message will be outputed. */
    DEBUG_LOG_LEVEL_NONE = 0,
    /** Only error level debug messages will get logged. */
    DEBUG_LOG_LEVEL_ERROR = 1,
    /** Only warning/error level debug messages will get logged. */
    DEBUG_LOG_LEVEL_WARNING = 2,
    /** Only info/wanring/error level debug messages will get
     *  logged. */
    DEBUG_LOG_LEVEL_INFO = 3,
    /** Only debug/info/wanring/error level debug messages will
     *  get logged. */
    DEBUG_LOG_LEVEL_DEBUG = 4,
    /** Verbose/debug/info/wanring/error level debug messages will
     *  get logged. */
    DEBUG_LOG_LEVEL_VERBOSE = 5,
};

/** Xtra feature configuration parameters */
struct XtraConfigParams {
    /** Number of minutes between periodic, consecutive successful
     *  XTRA assistance data downloads. The configured value is in
     *  unit of 1 minute and will be capped at a maximum of 168
     *  hours and minimum of 48 hours. 0 means to use modem
     *  default download. */
    uint32_t xtraDownloadIntervalMinute;
    /** Connection timeout when connecting backend for both xtra
     *  assistance data download and NTP time downlaod. The
     *  configured value is in in unit of 1 second and should be
     *  capped at a maximum of 300 secs (not indefinite) and minimum
     *  of 3 secs. */
    uint32_t xtraDownloadTimeoutSec;
    /** Interval to wait before retrying xtra assistance data
     *  download in case of device error. The configured value is in
     *  unit of 1 minute and should be capped with a maximum of 1
     *  day and a minimum of 3 minutes. Please note that this
     *  parameter does not apply to NTP time download. */
    uint32_t xtraDownloadRetryIntervalMinute;
    /** Total number of allowed retry attempts for assistance data
     *  download in case of device error.
     *  The configured value is in unit of 1 retry and max number of
     *  allowed retry is 6 per download interval.
     *  Please note that this parameter does not apply to NTP
     *  time download. */
    uint32_t xtraDownloadRetryAttempts;
    /** Path to the certificate authority (CA) repository that need
     *  to be used for XTRA assistance data download. Max of 128
     *  bytes, including null-terminating byte will be supported.
     *  Please note that paraemter does not apply to NTP time
     *  download. */
    char xtraCaPath[128];
    /** URLs from which XTRA assistance data will be fetched. Up to
     *  three URLs can be configured when this API is used. The URLs
     *  shall include the port number to be used for download. Max
     *  of 128 bytes, including null-terminating byte will be
     *  supported. Valid xtra server URLs should start with
     *  "https://". */
    uint32_t xtraServerURLsCount;
    char xtraServerURLs[3][128];
    /** URLs for NTP server to fetch current time. Up to three URLs
     *  can be configured when this API is used. The URLs shall
     *  include the port number to be used for download. Max of 128
     *  bytes, including null-terminating byte will be supported.
     *  Example of valid ntp server URL is:
     *  ntp.contiserver.com:123. */
    uint32_t ntpServerURLsCount;
    char ntpServerURLs[3][128];
    /** Enable or disable XTRA integrity download. */
    bool xtraIntegrityDownloadEnable;
    /** XTRA integrity download interval, only applicable if XTRA
     *  integrity download is enabled. */
    uint32_t xtraIntegrityDownloadIntervalMinute;
    /** Level of debug log messages that will be logged. */
    DebugLogLevel xtraDaemonDebugLogLevel;
    /** URL of NTS KE Server. if provided, shall be complete and
     *  shall include the port number. Max of 128 bytes,
     *  including null-terminating byte will be supported.
     *  Valid NTS KE server URL should start with "https://".
     *  If not specified, then device will use
     *  default URL of https://nts.xtracloud.net:4460. */
    char ntsKeServerURL[128];
    /** To indicate if Diag logging to be enabled for XTRA */
    uint32_t xtraDaemonDiagLoggingStatus;
};

enum XtraStatusUpdateType {
    XTRA_STATUS_UPDATE_UNDEFINED = 0,
    /** XTRA status update due to invoke getXtraStatus(). */
    XTRA_STATUS_UPDATE_UPON_QUERY = 1,
    /** XTRA status update due to first invokation of
     *  registerXtraStatusUpdate(). */
    XTRA_STATUS_UPDATE_UPON_REGISTRATION = 2,
    /** XTRA status update due to calling configXtra() to
     *  enable/disable XTRA feature. */
    XTRA_STATUS_UPDATE_UPON_ENABLEMENT = 3,
    /** XTRA status update due to status change in xtra assistance
     *  data status, e.g.: from unknown to known during device
     *  bootup, or when XTRA data gets downloaded. */
    XTRA_STATUS_UPDATE_UPON_STATUS_CHANGE = 4,
};

/**************** EPH REPORT *************/
/** Specifies the Source of ephemeris and action to be performed on
 *  receipt of the ephemeris (Update/Delete) Action shall be
 *  performed on GnssEphSource specified */
enum GnssEphAction {
    /**<Update ephemeris. Source of ephemeris is unknown  */
    GNSS_EPH_ACTION_UPDATE_SRC_UNKNOWN_V02 = 0,
    /**<Update ephemeris. Source of ephemeris is OTA  */
    GNSS_EPH_ACTION_UPDATE_SRC_OTA_V02     = 1,
    /**<Update ephemeris. Source of ephemeris is Network  */
    GNSS_EPH_ACTION_UPDATE_SRC_NETWORK_V02 = 2,
    /**<Max value for update ephemeris action. DO NOT USE  */
    GNSS_EPH_ACTION_UPDATE_MAX_V02         = 999,
    /**<Delete previous ephemeris from unknown source  */
    GNSS_EPH_ACTION_DELETE_SRC_UNKNOWN_V02 = 1000,
    /**<Delete previous ephemeris from network  */
    GNSS_EPH_ACTION_DELETE_SRC_NETWORK_V02 = 1001,
    /**<Delete previous ephemeris from OTA  */
    GNSS_EPH_ACTION_DELETE_SRC_OTA_V02     = 1002,
    /**<Max value for delete ephemeris action. DO NOT USE  */
    GNSS_EPH_ACTION_DELETE_MAX_V02         = 1999,
};

enum GalEphSignalSource {
    /** GALILEO signal is unknown  */
    GAL_EPH_SIGNAL_SRC_UNKNOWN_V02 = 0,
    /** GALILEO signal is E1B  */
    GAL_EPH_SIGNAL_SRC_E1B_V02     = 1,
    /** GALILEO signal is E5A  */
    GAL_EPH_SIGNAL_SRC_E5A_V02     = 2,
    /** GALILEO signal is E5B  */
    GAL_EPH_SIGNAL_SRC_E5B_V02     = 3,
};

struct GnssEphCommon {
    uint16_t gnssSvId;
    /** Unique SV Identifier.
     *  For SV Range of supported constellation, please refer to the
     *  comment section of gnssSvId in GpsMeasUsageInfo.
     */

    GnssEphAction updateAction;
    /**<   Specifies the action and source of ephemeris. \n
    - Type: int32 enum */

    uint16_t IODE;
    /** Issue of data ephemeris used (unit-less).
     *  GPS: IODE 8 bits.
     *  BDS: AODE 5 bits.
     *  GAL: SIS IOD 10 bits.
     *  - Type: uint16
     *  - Units: Unit-less
     */

    double aSqrt;
    /** Square root of semi-major axis.
     *  - Type: double
     *  - Units: Square Root of Meters
     */

    double deltaN;
    /** Mean motion difference from computed value.
     * - Type: double
     * - Units: Radians/Second
     */

    double m0;
    /** Mean anomaly at reference time.
     * - Type: double
     * - Units: Radians
     */

    double eccentricity;
    /**  Eccentricity .
     * - Type: double
     * - Units: Unit-less
     */

    double omega0;
    /** Longitude of ascending node of orbital plane at the weekly epoch.
     * - Type: double
     * - Units: Radians */

    double i0;
    /** Inclination angle at reference time.
     * - Type: double
     * - Units: Radians */

    double omega;
    /** Argument of Perigee.
     * - Type: double
     * - Units: Radians */

    double omegaDot;
    /** Rate of change of right ascension.
     * - Type: double
     * - Units: Radians/Second */

    double iDot;
    /** Rate of change of inclination angle.
     * - Type: double
     * - Units: Radians/Second */

    double cUc;
    /** Amplitude of the cosine harmonic correction term to the argument of latitude.
     * - Type: double
     * - Units: Radians */

    double cUs;
    /** Amplitude of the sine harmonic correction term to the argument of latitude. \n
     * - Type: double
     * - Units: Radians */

    double cRc;
    /** Amplitude of the cosine harmonic correction term to the orbit radius.
     * - Type: double
     * - Units: Meters */

    double cRs;
    /** Amplitude of the sine harmonic correction term to the orbit radius.
     * - Type: double
     * - Units: Meters */

    double cIc;
    /** Amplitude of the cosine harmonic correction term to the angle of inclination.
     * - Type: double
     * - Units: Radians */

    double cIs;
    /** Amplitude of the sine harmonic correction term to the angle of inclination.
     * - Type: double
     * - Units: Radians */

    uint32_t toe;
    /** Reference time of ephemeris.
     * - Type: uint32
     * - Units: Seconds */

    uint32_t toc;
    /** Clock data reference time of week.
     * - Type: uint32
     * - Units: Seconds */

    double af0;
    /** Clock bias correction coefficient.
     * - Type: double
     * - Units: Seconds */

    double af1;
    /** Clock drift coefficient.
     * - Type: double
     * - Units: Seconds/Second */

    double af2;
    /** Clock drift rate correction coefficient.
     * - Type: double
     * - Units: Seconds/Seconds^2 */

};

/* GPS Navigation Model Info */
struct GpsEphemeris {
    GnssEphCommon commonEphemerisData;
    /** Common ephemeris data.   */

    uint8_t signalHealth;
    /**  Signal health.
     *    Bit 0 : L5 Signal Health.
     *    Bit 1 : L2 Signal Health.
     *    Bit 2 : L1 Signal Health.
     *    - Type: uint8
     *    - Values: 3 bit mask of signal health, where set bit indicates unhealthy signal */

    uint8_t URAI;
    /**  User Range Accuracy Index.
     *    - Type: uint8
     *    - Units: Unit-less */

    uint8_t codeL2;
    /** Indicates which codes are commanded ON for the L2 channel (2-bits).
     *   - Type: uint8
     *   Valid Values:
     *   - 00 : Reserved
     *   - 01 : P code ON
     *   - 10 : C/A code ON */

    uint8_t dataFlagL2P;
    /**  L2 P-code indication flag.
     *    - Type: uint8
     *    - Value 1 indicates that the Nav data stream was
     *   commanded OFF on the P-code of the L2 channel. */

    double tgd;
    /** Time of group delay.
     *    - Type: double
     *    - Units: Seconds */

    uint8_t fitInterval;
    /** Indicates the curve-fit interval used by the CS.
     *    - Type: uint8
     *    Valid Values:
     *    - 0 : Four hours
     *    - 1 : Greater than four hours */

    uint16_t IODC;
    /** Issue of Data, Clock.
     *    - Type: uint16
     *    - Units: Unit-less */
};

/* GLONASS Navigation Model Info */
struct GlonassEphemeris {

    uint16_t gnssSvId;
    /** GNSS SV ID.
     *  - Type: uint16
     *  - Range: 65 to 96 if known.
     * When the slot number to SV ID mapping is unknown, set to 255 */

    GnssEphAction updateAction;
    /**<   Specifies the action and source of ephemeris. \n
    - Type: int32 enum */

    uint8_t bnHealth;
    /** SV health flags.
     *   - Type: uint8
     *   - Valid Values:
     *   - 0 : Healthy
     *   - 1 : Unhealthy */

    uint8_t lnHealth;
    /**  Ln SV health flags. GLONASS-M.
     *  - Type: uint8
     *  Valid Values:
     *  - 0 : Healthy
     *  - 1 : Unhealthy */

    uint8_t tb;
    /**  Index of a time interval within current day according to UTC(SU) + 03 hours 00 min.
     *  - Type: uint8
     *  - Units: Unit-less */

    uint8_t ft;
    /**  SV accuracy index.
     *  - Type: uint8
     *  - Units: Unit-less */

    uint8_t gloM;
    /** GLONASS-M flag.
     *  - Type: uint8
     *  Valid Values:
     *  - 0 : GLONASS
     *  - 1 : GLONASS-M */

    uint8_t enAge;
    /** Characterizes "Age" of current information.
     *  - Type: uint8
     *  - Units: Days */

    uint8_t gloFrequency;
    /** GLONASS frequency number + 8.
     *  - Type: uint8
     *  - Range: 1 to 14
     */

    uint8_t p1;
    /** Time interval between two adjacent values of tb parameter.
     *  - Type: uint8
     *  - Units: Minutes */

    uint8_t p2;
    /** Flag of oddness ("1") or evenness ("0") of the value of tb
     *  for intervals 30 or 60 minutes.
     *  - Type: uint8 */

    float deltaTau;
    /** Time difference between navigation RF signal transmitted in L2 sub-band
     *  and aviation RF signal transmitted in L1 sub-band.
     *  - Type: floating point
     *  - Units: Seconds */

    double position[3];
    /** Satellite XYZ position.
     *  - Type: array of doubles
     *  - Units: Meters */

    double velocity[3];
    /** Satellite XYZ velocity.
     *  - Type: array of doubles
     *  - Units: Meters/Second */

    double acceleration[3];
    /** Satellite XYZ sola-luni acceleration.
     *  - Type: array of doubles
     *  - Units: Meters/Second^2 */

    float tauN;
    /** Satellite clock correction relative to GLONASS time.
     * - Type: floating point
     * - Units: Seconds */

    float gamma;
    /** Relative deviation of predicted carrier frequency value
     *  from nominal value at the instant tb.
     *  - Type: floating point
     *  - Units: Unit-less */

    double toe;
    /** Complete ephemeris time, including N4, NT and Tb.
     *  [(N4-1)*1461 + (NT-1)]*86400 + tb*900
     *  - Type: double
     *  - Units: Seconds */

    uint16_t nt;
    /** Current date, calendar number of day within four-year interval.
     *  Starting from the 1-st of January in a leap year.
     *  - Type: uint16
     *  - Units: Days */
};

/* BDS Navigation Model Info */
struct BdsEphemeris {

    GnssEphCommon commonEphemerisData;
    /** Common ephemeris data.   */

    uint8_t svHealth;
    /** Satellite health information applied to both B1 and B2 (SatH1).
     *  - Type: uint8
     *  Valid Values:
     *  - 0 : Healthy
     *  - 1 : Unhealthy */

    uint8_t AODC;
    /** Age of data clock.
     *  - Type: uint8
     *  - Units: Hours */

    double tgd1;
    /**  Equipment group delay differential on B1 signal.
     *  - Type: double
     *  - Units: Nano-Seconds */

    double tgd2;
    /** Equipment group delay differential on B2 signal.
     *  - Type: double
     *  - Units: Nano-Seconds */

    uint8_t URAI;
    /** User range accuracy index (4-bits).
     *  - Type: uint8
     *  - Units: Unit-less */
};

/* GALIELO Navigation Model Info */
struct GalileoEphemeris {

    GnssEphCommon commonEphemerisData;
    /** Common ephemeris data.   */

    GalEphSignalSource dataSourceSignal;
    /** Galileo Signal Source.
     *  Valid Values: \n
     * - GAL_EPH_SIGNAL_SRC_UNKNOWN (0) --  GALILEO signal is unknown
     * - GAL_EPH_SIGNAL_SRC_E1B (1) --  GALILEO signal is E1B
     * - GAL_EPH_SIGNAL_SRC_E5A (2) --  GALILEO signal is E5A
     * - GAL_EPH_SIGNAL_SRC_E5B (3) --  GALILEO signal is E5B  */

    uint8_t sisIndex;
    /** Signal-in-space index for dual frequency E1-E5b/E5a depending on dataSignalSource.
     *  - Type: uint8
     *  - Units: Unit-less */

    double bgdE1E5a;
    /** E1-E5a Broadcast group delay from F/Nav (E5A).
     *  - Type: double
     *  - Units: Seconds */

    double bgdE1E5b;
    /** E1-E5b Broadcast group delay from I/Nav (E1B or E5B).
     *  For E1B or E5B signal, both bgdE1E5a and bgdE1E5b are valid.
     *  For E5A signal, only bgdE1E5a is valid.
     *  Signal source identified using dataSignalSource.
     *  - Type: double
     *  - Units: Seconds */

    uint8_t svHealth;
    /** SV health status of signal identified by dataSourceSignal.
     * - Type: uint8
     * Valid Values:
     * - 0 : Healthy
     * - 1 : Unhealthy */
};

/* NAVIC Navigation Model Info */
struct NavicEphemeris {

    /** Common ephemeris data. */
    GnssEphCommon commonEphemerisData;
    /** Week number since the NavIC system time start epoch (August 22, 1999) */
    uint32_t weekNum;
    /** Issue of Data, Clock
     *   Mandatory Field */
    uint32_t iodec;
    /** Health status of navigation data on L5 SPS signal.
     *   0=OK, 1=bad */
    uint8_t l5Health;
    /** Health status of navigation data on S SPS signal.
     *   0=OK, 1=bad */
    uint8_t sHealth;
    /** Inclination angle at reference time
     *   Unit: radian
     *   Mandatory Field */
    double inclinationAngleRad;
    /** User Range Accuracy Index(4bit)
     *   Mandatory Field */
    uint8_t urai;
    /** Time of Group delay
     *   Unit: second
     *   Mandatory Field */
    double  tgd;
};

/** GPS Navigation model for each SV */
struct GpsEphemerisResponse {
    uint16_t numOfEphemeris;
    GpsEphemeris gpsEphemerisData[GNSS_EPHEMERIS_LIST_MAX_SIZE_V02];
};

/** GLONASS Navigation model for each SV */
struct GlonassEphemerisResponse {
    uint16_t numOfEphemeris;
    GlonassEphemeris gloEphemerisData[GNSS_EPHEMERIS_LIST_MAX_SIZE_V02];
};

/** BDS Navigation model for each SV */
struct BdsEphemerisResponse {
    uint16_t numOfEphemeris;
    BdsEphemeris bdsEphemerisData[GNSS_EPHEMERIS_LIST_MAX_SIZE_V02];
};

/** GALILEO Navigation model for each SV */
struct GalileoEphemerisResponse {
    uint16_t numOfEphemeris;
    GalileoEphemeris galEphemerisData[GNSS_EPHEMERIS_LIST_MAX_SIZE_V02];
};

/** QZSS Navigation model for each SV */
struct QzssEphemerisResponse {
    uint16_t numOfEphemeris;
    GpsEphemeris qzssEphemerisData[GNSS_EPHEMERIS_LIST_MAX_SIZE_V02];
};
/** NAVIC Navigation model for each SV */
struct NavicEphemerisResponse {
    uint16_t numOfEphemeris;
    NavicEphemeris navicEphemerisData[GNSS_EPHEMERIS_LIST_MAX_SIZE_V02];
};

struct GnssSvEphemerisReport {
    /** Indicates GNSS Constellation Type
     *   Mandatory field */
    Gnss_LocSvSystemEnumType gnssConstellation;

    /** GPS System Time of the ephemeris report */
    bool isSystemTimeValid;
    GnssSystemTimeStructType systemTime;

    union {
       /** GPS Ephemeris */
       GpsEphemerisResponse gpsEphemeris;
       /** GLONASS Ephemeris */
       GlonassEphemerisResponse glonassEphemeris;
       /** BDS Ephemeris */
       BdsEphemerisResponse bdsEphemeris;
       /** GALILEO Ephemeris */
       GalileoEphemerisResponse galileoEphemeris;
       /** QZSS Ephemeris */
       QzssEphemerisResponse qzssEphemeris;
       /** NAVIC Ephemeris */
       NavicEphemerisResponse navicEphemeris;
    } ephInfo;
};

/* Provides the capabilities of the system
   capabilities callback is called once soon after createInstance is called */
typedef std::function<void(
    LocationCapabilitiesMask capabilitiesMask // bitwise OR of LocationCapabilitiesBits
)> capabilitiesCallback;

/* Used by tracking, batching, and miscellanous APIs
   responseCallback is called for every Tracking, Batching API, and Miscellanous API */
typedef std::function<void(
    LocationError err, // if not SUCCESS, then id is not valid
    uint32_t id        // id to be associated to the request
)> responseCallback;

/* Used by APIs that gets more than one LocationError in it's response
   collectiveResponseCallback is called for every geofence API call.
   ids array and LocationError array are only valid until collectiveResponseCallback returns. */
typedef std::function<void(
    uint32_t count, // number of locations in arrays
    LocationError* errs, // array of LocationError associated to the request
    uint32_t* ids // array of ids to be associated to the request
)> collectiveResponseCallback;

/* Used for startTracking API, optional can be NULL
   trackingCallback is called when delivering a location in a tracking session
   broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
    const Location& location
)> trackingCallback;

/* Used for startBatching API, optional can be NULL
   batchingCallback is called when delivering locations in a batching session.
   broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
    uint32_t count,      // number of locations in array
    Location* location, // array of locations
    const BatchingOptions& batchingOptions // Batching options
)> batchingCallback;

typedef std::function<void(
    const BatchingStatusInfo& batchingStatus, // batch status
    std::list<uint32_t> & listOfCompletedTrips
)> batchingStatusCallback;

/* Gives GNSS Location information, optional can be NULL
    gnssLocationInfoCallback is called only during a tracking session
    broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
    const GnssLocationInfoNotification& gnssLocationInfoNotification
)> gnssLocationInfoCallback;

/* Gives default combined location information from all engines and
   location information individually from selected engines.
   This callback is only used when there are multiple engines
   running in the system.

   optional can be NULL

   engineLocationsInfoCallback is called only during a tracking session
   broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
    uint32_t count,
    GnssLocationInfoNotification* engineLocationInfoNotification
)> engineLocationsInfoCallback;

/* Used for addGeofences API, optional can be NULL
   geofenceBreachCallback is called when any number of geofences have a state change */
typedef std::function<void(
    const GeofenceBreachNotification& geofenceBreachNotification
)> geofenceBreachCallback;

/* Used for addGeofences API, optional can be NULL
       geofenceStatusCallback is called when any number of geofences have a status change */
typedef std::function<void(
    const GeofenceStatusNotification& geofenceStatusNotification
)> geofenceStatusCallback;

/* Network Initiated request, optional can be NULL
   This callback should be responded to by calling gnssNiResponse */
typedef std::function<void(
    uint32_t id, // id that should be used to respond by calling gnssNiResponse
    const GnssNiNotification& gnssNiNotification
)> gnssNiCallback;

/* Gives GNSS SV information, optional can be NULL
    gnssSvCallback is called only during a tracking session
    broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
    const GnssSvNotification& gnssSvNotification
)> gnssSvCallback;

/* Gives GNSS NMEA data, optional can be NULL
    gnssNmeaCallback is called only during a tracking session
    broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
   const GnssNmeaNotification& gnssNmeaNotification
)> gnssNmeaCallback;

/* Gives GNSS data, optional can be NULL
    gnssDataCallback is called only during a tracking session
    broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
    const GnssDataNotification& gnssDataNotification
)> gnssDataCallback;

/* Gives GNSS Measurements information, optional can be NULL
    gnssMeasurementsCallback is called only during a tracking session
    broadcasted to all clients, no matter if a session has started by client */
typedef std::function<void(
    const GnssMeasurementsNotification& gnssMeasurementsNotification
)> gnssMeasurementsCallback;

/* Provides the current GNSS configuration to the client */
typedef std::function<void(
    uint32_t session_id,
    const GnssConfig& config
)> gnssConfigCallback;

/* LocationSystemInfoCb is for receiving rare occuring location
   system information update. optional, can be NULL.
*/
typedef std::function<void(
    const LocationSystemInfo& locationSystemInfo
)> locationSystemInfoCallback;

/* LocationSystemInfoCb is for receiving rare occuring location
   system information update. optional, can be NULL.
*/
typedef std::function<void(
   const GnssDcReportInfo& dcReportInfo
)> gnssDcReportCallback;

/* Informs the framework of the list of GnssSignalTypes the GNSS HAL implementation
   supports, optional can be NULL
 */
typedef std::function<void(
    const GnssCapabNotification& gnssCapabNotification
)> gnssSignalTypesCallback;

typedef std::function<void(
)> locationApiDestroyCompleteCallback;

typedef uint16_t LocationAdapterTypeMask;
typedef enum {
    LOCATION_ADAPTER_GNSS_TYPE_BIT      = (1<<0), // adapter type is GNSS
    LOCATION_ADAPTER_BATCHING_TYPE_BIT  = (1<<1), // adapter type is BATCHING
    LOCATION_ADAPTER_GEOFENCE_TYPE_BIT  = (1<<2)  // adapter type is geo fence
} LocationAdapterTypeBits;

typedef std::function <void(
    AGnssExtStatusIpV4 status
)> agnssStatusIpV4Callback;


/* Callback to send ODCPI request to framework */
typedef std::function<void(
    const OdcpiRequestInfo& request
)> odcpiRequestCallback;

/*
* Callback with Measurement corrections information.
*/
typedef std::function<void(
    GnssMeasurementCorrectionsCapabilitiesMask capabilities
)> measCorrSetCapabilitiesCallback;

/*
* Callback with Antenna information.
*/
struct AntennaInfoCallback {
    AntennaInfoCallback() = default;
    virtual ~AntennaInfoCallback() = default;
    virtual void operator()(std::vector<GnssAntennaInformation>& gnssAntennaInformations) = 0;
};

/*
* Callback with NFW information.
*/
typedef std::function<void(
    const GnssNfwNotification& notification
)> nfwStatusCallback;

typedef std::function<bool(
)> isInEmergencySessionCallback;
typedef std::function<void(uint32_t session_id, XtraStatus xtraStatus)> xtraStatusCallback;

/**
* Callback used to retrieve energy consumed by modem GNSS
  engine as defined in GnssEnergyConsumedInfo.
*/
typedef std::function<void(
    const GnssEnergyConsumedInfo& gnssEneryConsumed
)> gnssEnergyConsumedCallback;

/** Callback to receive OTA ephemeris data reported by MODEM */
typedef std::function<void(
    const GnssSvEphemerisReport& svEphemeris
)> gnssSvEphemerisCallback;

struct LocationCallbacks {
    uint32_t size; // set to sizeof(LocationCallbacks)
    capabilitiesCallback capabilitiesCb;                // mandatory
    responseCallback responseCb;                        // mandatory
    collectiveResponseCallback collectiveResponseCb;    // mandatory
    trackingCallback trackingCb;                        // optional
    batchingCallback batchingCb;                        // optional
    geofenceBreachCallback geofenceBreachCb;            // optional
    geofenceStatusCallback geofenceStatusCb;            // optional
    gnssLocationInfoCallback gnssLocationInfoCb;        // optional
    gnssNiCallback gnssNiCb;                            // optional
    gnssSvCallback gnssSvCb;                            // optional
    gnssNmeaCallback gnssNmeaCb;                        // optional
    gnssDataCallback gnssDataCb;                        // optional
    gnssMeasurementsCallback gnssMeasurementsCb;        // optional
    gnssMeasurementsCallback gnssNHzMeasurementsCb;     // optional
    batchingStatusCallback batchingStatusCb;            // optional
    locationSystemInfoCallback locationSystemInfoCb;    // optional
    engineLocationsInfoCallback engineLocationsInfoCb;  // optional
    gnssDcReportCallback gnssDcReportCb;                // optional
    gnssSignalTypesCallback gnssSignalTypesCb;          // optional
    gnssNmeaCallback engineNmeaCb;                      // optional
    gnssSvEphemerisCallback svEphemerisCb;          // optional
};

struct LocationControlCallbacks {
    size_t size; // set to sizeof(LocationControlCallbacks)
    responseCallback responseCb;                     // mandatory
    collectiveResponseCallback collectiveResponseCb; // mandatory
    gnssConfigCallback gnssConfigCb;                 // optional
    odcpiRequestCallback odcpiReqCb;                 //optional
    measCorrSetCapabilitiesCallback measCorrSetCapabilitiesCb; // optional
    agnssStatusIpV4Callback   agpsStatusIpV4Cb;      //optional
    nfwStatusCallback nfwStatusCb;                   // optional
    isInEmergencySessionCallback isInEmergencyStatusCb; // optional
    xtraStatusCallback xtraStatusCb;                  // optional
};


struct GnssLatencyInfo {
    uint64_t meQtimer1;
    uint64_t meQtimer2;
    uint64_t meQtimer3;
    uint64_t peQtimer1;
    uint64_t peQtimer2;
    uint64_t peQtimer3;
    uint64_t smQtimer1;
    uint64_t smQtimer2;
    uint64_t smQtimer3;
    uint64_t locMwQtimer;
    uint64_t hlosQtimer1;
    uint64_t hlosQtimer2;
    uint64_t hlosQtimer3;
    uint64_t hlosQtimer4;
    uint64_t hlosQtimer5;
};

struct GnssCivicAddress {
    uint32_t size;
    std::string adminArea;
    std::string countryCode;
    std::string countryName;
    std::string featureName;
    bool hasLatitude;
    double latitude;
    bool hasLongitude;
    double longitude;
    std::string locale;
    std::string locality;
    std::string phone;
    std::string postalCode;
    std::string premises;
    std::string subAdminArea;
    std::string subLocality;
    std::string thoroughfare;
    std::string subThoroughfare;
    std::string url;
};

enum PowerStateType {
    POWER_STATE_UNKNOWN = 0,
    POWER_STATE_SUSPEND = 1,
    POWER_STATE_RESUME  = 2,
    POWER_STATE_SHUTDOWN = 3,
    POWER_STATE_DEEP_SLEEP_ENTRY = 4,
    POWER_STATE_DEEP_SLEEP_EXIT = 5
};

typedef uint64_t NetworkHandle;
#define NETWORK_HANDLE_UNKNOWN  ~0
#define MAX_NETWORK_HANDLES 10

enum OdcpiCallbackTypeMaskBits {
    NON_EMERGENCY_ODCPI = (1<<0),
    EMERGENCY_ODCPI =     (1<<1)
};

typedef uint16_t OdcpiCallbackTypeMask;

enum ModemGnssQesdkFeatureBits {
    MODEM_QESDK_FEATURE_CARRIER_PHASE     = (1<<0),
    MODEM_QESDK_FEATURE_SV_POLYNOMIALS    = (1<<1),
    MODEM_QESDK_FEATURE_DGNSS             = (1<<2),
    MODEM_QESDK_FEATURE_ROBUST_LOCATION   = (1<<3)
};

typedef uint64_t ModemGnssQesdkFeatureMask;

/* enum OSNMA New Public Key Type (NPKT) */
enum mgpOsnmaNpktEnumTypeVal {
    MGP_OSNMA_NPKT_RESERVED0    = 0, /* reserved 0 */
    MGP_OSNMA_NPKT_ECDSA_P_256  = 1, /* 1: ECDSA P-256, key length shall be 264 */
    MGP_OSNMA_NPKT_RESERVED2    = 2, /* reserved 2 */
    MGP_OSNMA_NPKT_ECDSA_P_521  = 3, /* 3: ECDSA P-521, key length shall be 536 */
    MGP_OSNMA_NPKT_ALERT        = 4  /* OSNMA Alert Message (OAM) */
};
typedef uint8_t mgpOsnmaNpktEnumType;

/* Tree Node structure */
struct mgpOsnmaTreeNodeT {
    uint8_t uj; /* the height of the node in the Merkle Tree */
    uint8_t ui; /* the position of the node in the Merkle Tree level */
    uint16_t wLengthInBits; /*  the length in bits of the hash in the x_ji element;
                         shall be 256 */
    uint8_t uHash[32]; /* Hash of Merkle tree nodes */
};

/* public key structure */
struct mgpOsnmaPublicKeyT {
    uint8_t uFlag; /* 1: valid, 0: invalid */
    mgpOsnmaNpktEnumType eNpkt; /* Public key type */
    uint8_t  uNpkId; /* public key ID */
    uint16_t wKeyLen; /* in bits */
    uint8_t  uKey[67]; /* max key length is 536 = 8 * 67 */
    mgpOsnmaTreeNodeT zNodes[4]; /* required Merkle tree nodes at level 0, 1, 2, 3
                                 zNodes[0] is at level 0;
                                 zNodes[3] is at level 3 */
};

/* Hash Function (HF) */
enum mgpOsnmaHfEnumTypeVal {
    MGP_OSNMA_HF_SHA_256   = 0, /* 0: SHA-256 */
    MGP_OSNMA_HF_RESERVED1 = 1, /* 1: reserved */
    MGP_OSNMA_HF_SHA3_256  = 2, /* 2: SHA3-256 */
    MGP_OSNMA_HF_RESERVED3 = 3, /* 3: reserved */
};
typedef uint8_t mgpOsnmaHfEnumType;

/* Merkle Tree Nodes */
struct mgpOsnmaMerkleTreeT {
    uint8_t uFlag; /* 1: valid; 0: invalid */
    mgpOsnmaHfEnumType eHfType;
    mgpOsnmaTreeNodeT zRootNode; /* Root Node */
};

struct mgpOsnmaPublicKeyAndMerkleTreeStruct {
    mgpOsnmaPublicKeyT   zPublicKey;  /* public key */
    mgpOsnmaMerkleTreeT  zMerkleTree; /* Merkle Tree Nodes */
};

typedef void* QDgnssListenerHDL;

typedef std::function<void(
    bool    sessionActive
)> QDgnssSessionActiveCb;

typedef uint16_t QDgnss3GppSourceBitMask;
#define QDGNSS_3GPP_SOURCE_UNKNOWN          0X00
#define QDGNSS_3GPP_EP_PARSER_AVAIL         0X01
#define QDGNSS_3GPP_SOURCE_AVAIL            0X02
#define QDGNSS_3GPP_SOURCE_ACTIVE           0X04

typedef std::function<void(
    QDgnss3GppSourceBitMask    modem3GppSourceMask
)> QDgnssModem3GppAvailCb;

#endif /* LOCATIONDATATYPES_H */
