/* Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 *     * Neither the name of The Linux Foundation, nor the names of its
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
 *
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

#define LOG_NDEBUG 0
#define LOG_TAG "LocSvc_GnssAdapter"

#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <math.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <GnssAdapter.h>
#include <LocGlinkBase.h>
#include <string>
#include <sstream>
#include <loc_log.h>
#include <loc_nmea.h>
#include <Agps.h>
#include <SystemStatus.h>
#include <vector>
#include <loc_misc_utils.h>
#include <gps_extended_c.h>
#include <sys/stat.h>
#include <thread>
#ifdef USE_GLIB
#include "XmlFileParser.h"
#endif

#define RAD2DEG    (180.0 / M_PI)
#define DEG2RAD    (M_PI / 180.0)
#define PROCESS_NAME_ENGINE_SERVICE "engine-service"
#define PROCESS_NAME_SAP_MAP        "hmacdaemon"
#define MIN_TRACKING_INTERVAL (100) // 100 msec
#define NHZ_ENABLED_MIN_TRACKING_INTERVAL (100) // 100 msec
#define NHZ_NOT_ENABLED_MIN_TRACKING_INTERVAL (1000) // 1 sec
#define BILLION_NSEC (1000000000ULL)
#define NMEA_MIN_THRESHOLD_MSEC (99)
#define NMEA_MAX_THRESHOLD_MSEC (975)

#define DGNSS_RANGE_UPDATE_TIME_10MIN_IN_SEC  600
#define GPS_LOCATION_DESIRED_FLAGS (LOC_GPS_LOCATION_HAS_LAT_LONG | LOC_GPS_LOCATION_HAS_ACCURACY)

using namespace loc_core;

static int loadEngHubForExternalEngine = 0;
static int loadLocSlatePUNCModel = 0;
static int sUseZppInDBH = 0;
static loc_param_s_type izatConfParamTable[] = {
    {"LOAD_ENGHUB_FOR_EXTERNAL_ENGINE", &loadEngHubForExternalEngine, nullptr, 'n'},
    {"USE_ZPP_IN_DBH", &sUseZppInDBH, nullptr, 'n'}
};

static loc_param_s_type izatConfLocGlinkParamTable[] = {
    {"LOAD_LOC_SLATE_PUNC_MODEL", &loadLocSlatePUNCModel, nullptr, 'n'}
};

/* Method to fetch status cb from loc_net_iface library */
typedef AgpsCbInfo& (*LocAgpsGetAgpsCbInfo)(LocAgpsOpenResultCb openResultCb,
        LocAgpsCloseResultCb closeResultCb, void* userDataPtr);

static void agpsOpenResultCb (bool isSuccess, AGpsExtType agpsType, const char* apn,
        AGpsBearerType bearerType, void* userDataPtr);
static void agpsCloseResultCb (bool isSuccess, AGpsExtType agpsType, void* userDataPtr);

typedef const CdfwInterface* (*getCdfwInterface)();

typedef void getPdnTypeFromWds(const std::string& apnName, std::function<void(int)> pdnCb);

GnssReportLoggerUtil::GnssReportLoggerUtil() : mLogLatency(nullptr) {
    int loadDiagIfaceLib = 1;
    const loc_param_s_type gps_conf_params[] = {
        {"LOC_DIAGIFACE_ENABLED", &loadDiagIfaceLib, nullptr, 'n'}
    };
    UTIL_READ_CONF(LOC_PATH_GPS_CONF, gps_conf_params);
    LOC_LOGi("Loc_DiagIface_enabled: %d", loadDiagIfaceLib);
    if (0 != loadDiagIfaceLib) {
        const char* libname = "liblocdiagiface.so";
        void* libHandle = nullptr;
        mLogLatency = (LogGnssLatency)dlGetSymFromLib(libHandle, libname, "LogGnssLatency");
        if (nullptr == mLogLatency) {
            LOC_LOGw("DiagIface mLogLatency is null");
        }
    }
}

inline bool GnssReportLoggerUtil::isLogEnabled() {
    return (mLogLatency != nullptr);
}

inline void GnssReportLoggerUtil::log(const GnssLatencyInfo& gnssLatencyMeasInfo) {
    if (mLogLatency != nullptr) {
        mLogLatency(gnssLatencyMeasInfo);
    }
}

GnssAdapter::GnssAdapter() :
    LocAdapterBase(0,
                   LocContext::getLocContext(LocContext::mLocationHalName),
                   true, nullptr, true),
    mEngHubProxy(new EngineHubProxyBase()),
    mLocGlinkProxy(new LocGlinkBase()),
    mNHzNeeded(false),
    mSPEAlreadyRunningAtHighestInterval(false),
    mLocPositionMode(),
    mGnssSvIdUsedInPosition(),
    mGnssSvIdUsedInPosAvail(false),
    mGnssMbSvIdUsedInPosition{},
    mGnssMbSvIdUsedInPosAvail(false),
    mControlCallbacks(),
    mAfwControlId(0),
    mNmeaMask(0),
    mGnssSvIdConfig(),
    mGnssSeconaryBandConfig(),
    mGnssSvTypeConfigCb(nullptr),
    mLocConfigInfo{},
    mNiData(),
    mAgpsManager(),
    mQDgnssListenerHDL(nullptr),
    mCdfwInterface(nullptr),
    mDGnssNeedReport(false),
    mDGnssDataUsage(false),
    mOdcpiStateMask(0),
    mCallbackPriority(OdcpiPrioritytype::ODCPI_HANDLER_PRIORITY_LOW),
    mOdcpiTimer(this),
    mOdcpiRequest(),
    mSystemStatus(SystemStatus::getInstance(mMsgTask)),
    mServerUrl(":"),
    mXtraObserver(this, mSystemStatus->getOsObserver(), mMsgTask),
    mMpXtraEnabled(true),
    mLocSystemInfo{},
    mSystemPowerState(POWER_STATE_UNKNOWN),
    mPowerConnectState(POWER_CONNECT_UNKNOWN),
    mBlockCPIInfo{},
    mEsStatusCb(nullptr),
    mEngHubLoadSuccessful(false),
    mEngServiceInfo{},
    mPowerOn(false),
    mNativeAgpsHandler(mSystemStatus->getOsObserver(), *this),
    mGnssEnergyConsumedCb(nullptr),
    mPowerStateCb(nullptr),
    mSupportNfwControl(true),
    mIsMeasCorrInterfaceOpen(false),
    mLastDeleteAidingDataTime(0),
    mDgnssState(0),
    mSendNmeaConsent(false),
    mDgnssLastNmeaBootTimeMilli(0),
    mPowerIndicationCb(nullptr),
    mGnssPowerStatisticsInit(false),
    mBootReferenceEnergy(0),
    mPowerElapsedRealTimeCal(30000000),
    mPositionElapsedRealTimeCal(30000000),
    mAddressRequestCb(nullptr),
    mHmacConfig(HMAC_CONFIG_UNKNOWN),
    mGnssCapabNotification{},
    mAppHash(""),
    m3GppSourceMask(QDGNSS_3GPP_SOURCE_UNKNOWN),
    mNmeaReqEngTypeMask(LOC_REQ_ENGINE_FUSED_BIT),
    mResponseTimer(this, (LocationError)0, (uint32_t)0)
{
    LOC_LOGd("Constructor %p", this);
    mLocPositionMode.mode = LOC_POSITION_MODE_INVALID;
    mControlCallbacks.size = sizeof(LocationControlCallbacks);
    memset(mGnssSvTypeConfigs, 0, sizeof(mGnssSvTypeConfigs));

    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setclock(&condAttr, CLOCK_REALTIME);
    pthread_cond_init(&mNiData.session.tCond, &condAttr);
    pthread_cond_init(&mNiData.sessionEs.tCond, &condAttr);
    pthread_condattr_destroy(&condAttr);

    /* Set ATL open/close callbacks */
    AgpsAtlOpenStatusCb atlOpenStatusCb =
            [this](int handle, int isSuccess, char* apn, uint32_t apnLen,
                    AGpsBearerType bearerType, AGpsExtType agpsType,
                    LocApnTypeMask mask) {

                mLocApi->atlOpenStatus(
                        handle, isSuccess, apn, apnLen, bearerType, agpsType, mask);
            };
    AgpsAtlCloseStatusCb atlCloseStatusCb =
            [this](int handle, int isSuccess) {

                mLocApi->atlCloseStatus(handle, isSuccess);
            };
    mAgpsManager.registerATLCallbacks(atlOpenStatusCb, atlCloseStatusCb);

    // init default nmea setting based on gps.conf
    uint32_t DATUM_TYPE = 0;
    const loc_param_s_type nmea_conf_params[] = {
        {"DATUM_TYPE", &DATUM_TYPE, NULL, 'n'},
    };
    UTIL_READ_CONF(LOC_PATH_GPS_CONF, nmea_conf_params);
    GnssGeodeticDatumType nmea_datum_type =
            (DATUM_TYPE == 1) ? GEODETIC_TYPE_PZ_90 : GEODETIC_TYPE_WGS_84;
    loc_nmea_config_output_types(NMEA_TYPE_ALL, nmea_datum_type);

    readConfigCommand();
    initDefaultAgpsCommand();
    initCDFWServiceCommand();
    initEngHubProxyCommand();
    initLocGlinkCommand();
    testLaunchQppeBringUp();
    mXtraObserver.init();
    // at last step, let us inform adapater base that we are done
    // with initialization, e.g.: ready to process handleEngineUpEvent
    doneInit();

}

void
GnssAdapter::setControlCallbacksCommand(LocationControlCallbacks& controlCallbacks)
{
    struct MsgSetControlCallbacks : public LocMsg {
        GnssAdapter& mAdapter;
        const LocationControlCallbacks mControlCallbacks;
        inline MsgSetControlCallbacks(GnssAdapter& adapter,
                                      LocationControlCallbacks& controlCallbacks) :
            LocMsg(),
            mAdapter(adapter),
            mControlCallbacks(controlCallbacks) {}
        inline virtual void proc() const {
            if (mControlCallbacks.responseCb != NULL) {
                mAdapter.mControlCallbacks.responseCb = mControlCallbacks.responseCb;
            }
            if (mControlCallbacks.collectiveResponseCb != NULL) {
                mAdapter.mControlCallbacks.collectiveResponseCb =
                        mControlCallbacks.collectiveResponseCb;
            }
            if (mControlCallbacks.gnssConfigCb != NULL) {
                mAdapter.mControlCallbacks.gnssConfigCb = mControlCallbacks.gnssConfigCb;
            }
            if (mControlCallbacks.odcpiReqCb != NULL) {
                mAdapter.mControlCallbacks.odcpiReqCb = mControlCallbacks.odcpiReqCb;
            }
            if (mControlCallbacks.xtraStatusCb != NULL) {
                mAdapter.mControlCallbacks.xtraStatusCb = mControlCallbacks.xtraStatusCb;
            }
        }
    };

    sendMsg(new MsgSetControlCallbacks(*this, controlCallbacks));
}

void
GnssAdapter::convertOptions(LocPosMode& out, const TrackingOptions& trackingOptions)
{
    switch (trackingOptions.mode) {
    case GNSS_SUPL_MODE_MSB:
        out.mode = LOC_POSITION_MODE_MS_BASED;
        break;
    case GNSS_SUPL_MODE_MSA:
        out.mode = LOC_POSITION_MODE_MS_ASSISTED;
        break;
    default:
        out.mode = LOC_POSITION_MODE_STANDALONE;
        break;
    }
    out.share_position = true;
    out.min_interval = trackingOptions.minInterval;
    out.powerMode = trackingOptions.powerMode;
    out.timeBetweenMeasurements = trackingOptions.tbm;
}

bool
GnssAdapter::checkAndSetSPEToRunforNHz(TrackingOptions & out) {

    // first check if NHz meas is needed a all, if not, just return false
    // if a NHz capable engine is subscribed for NHz measurement or NHz positions,
    // always run the SPE only session at 100ms TBF.
    // If SPE session is already set to highest interval, no need to start it again.

    bool isSPERunningAtHighestInterval = false;

    if (!mNHzNeeded) {
        LOC_LOGd("No nHz session needed.");
    } else if (mSPEAlreadyRunningAtHighestInterval) {
        LOC_LOGd("SPE is already running at highest interval.");
        isSPERunningAtHighestInterval = true;
    } else if (out.minInterval > MIN_TRACKING_INTERVAL) {
        out.minInterval = MIN_TRACKING_INTERVAL;
        LOC_LOGd("nHz session is needed, starting SPE only session at 100ms TBF.");
        mSPEAlreadyRunningAtHighestInterval = true;
    }

    return isSPERunningAtHighestInterval;
}


void
GnssAdapter::convertLocation(Location& out, const UlpLocation& ulpLocation,
                             const GpsLocationExtended& locationExtended,
                             loc_sess_status status)
{
    memset(&out, 0, sizeof(Location));
    out.size = sizeof(Location);

    out.flags |= LOCATION_HAS_SESSION_STATUS_BIT;
    out.sessionStatus = status;

    if (LOC_GPS_LOCATION_HAS_LAT_LONG & ulpLocation.gpsLocation.flags) {
        out.flags |= LOCATION_HAS_LAT_LONG_BIT;
        out.latitude = ulpLocation.gpsLocation.latitude;
        out.longitude = ulpLocation.gpsLocation.longitude;
    }
    if (LOC_GPS_LOCATION_HAS_ALTITUDE & ulpLocation.gpsLocation.flags) {
        out.flags |= LOCATION_HAS_ALTITUDE_BIT;
        out.altitude = ulpLocation.gpsLocation.altitude;
    }
    if (LOC_GPS_LOCATION_HAS_SPEED & ulpLocation.gpsLocation.flags) {
        out.flags |= LOCATION_HAS_SPEED_BIT;
        out.speed = ulpLocation.gpsLocation.speed;
    }
    if (LOC_GPS_LOCATION_HAS_BEARING & ulpLocation.gpsLocation.flags) {
        out.flags |= LOCATION_HAS_BEARING_BIT;
        out.bearing = ulpLocation.gpsLocation.bearing;
    }
    if (LOC_GPS_LOCATION_HAS_ACCURACY & ulpLocation.gpsLocation.flags) {
        out.flags |= LOCATION_HAS_ACCURACY_BIT;
        out.accuracy = ulpLocation.gpsLocation.accuracy;
    }
    if (GPS_LOCATION_EXTENDED_HAS_VERT_UNC & locationExtended.flags) {
        out.flags |= LOCATION_HAS_VERTICAL_ACCURACY_BIT;
        out.verticalAccuracy = locationExtended.vert_unc;
    }
    if (GPS_LOCATION_EXTENDED_HAS_SPEED_UNC & locationExtended.flags) {
        out.flags |= LOCATION_HAS_SPEED_ACCURACY_BIT;
        out.speedAccuracy = locationExtended.speed_unc;
    }
    if (GPS_LOCATION_EXTENDED_HAS_BEARING_UNC & locationExtended.flags) {
        out.flags |= LOCATION_HAS_BEARING_ACCURACY_BIT;
        out.bearingAccuracy = locationExtended.bearing_unc;
    }
    if (GPS_LOCATION_EXTENDED_HAS_CONFORMITY_INDEX & locationExtended.flags) {
        out.flags |= LOCATION_HAS_CONFORMITY_INDEX_BIT;
        out.conformityIndex = locationExtended.conformityIndex;
    }
    out.timestamp = ulpLocation.gpsLocation.timestamp;
    if (GPS_LOCATION_EXTENDED_HAS_TIME_UNC & locationExtended.flags) {
        out.flags |= LOCATION_HAS_TIME_UNC_BIT;
        out.timeUncMs = locationExtended.timeUncMs;
    }
    if (GPS_LOCATION_EXTENDED_HAS_POS_TECH_MASK & locationExtended.flags) {
        out.flags |= LOCATION_HAS_TECH_MASK_BIT;
    }
    if (LOC_POS_TECH_MASK_SATELLITE & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_GNSS_BIT;
    }
    if (LOC_POS_TECH_MASK_CELLID & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_CELL_BIT;
    }
    if (LOC_POS_TECH_MASK_WIFI & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_WIFI_BIT;
    }
    if (LOC_POS_TECH_MASK_SENSORS & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_SENSORS_BIT;
    }
    if (LOC_POS_TECH_MASK_REFERENCE_LOCATION & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_REFERENCE_LOCATION_BIT;
    }
    if (LOC_POS_TECH_MASK_INJECTED_COARSE_POSITION & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_INJECTED_COARSE_POSITION_BIT;
    }
    if (LOC_POS_TECH_MASK_AFLT & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_AFLT_BIT;
    }
    if (LOC_POS_TECH_MASK_HYBRID & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_HYBRID_BIT;
    }
    if (LOC_POS_TECH_MASK_PPE & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_PPE_BIT;
    }
    if (LOC_POS_TECH_MASK_VEH & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_VEH_BIT;
    }
    if (LOC_POS_TECH_MASK_VIS & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_VIS_BIT;
    }
    if (LOC_NAV_MASK_DGNSS_CORRECTION & locationExtended.navSolutionMask) {
        out.techMask |= LOCATION_TECHNOLOGY_DGNSS_BIT;
    }
    if (LOC_POS_TECH_MASK_PDR & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_PDR_BIT;
    }
    if (LOC_POS_TECH_MASK_PROPAGATED & locationExtended.tech_mask) {
        out.techMask |= LOCATION_TECHNOLOGY_PROPAGATED_BIT;
    }

    if (LOC_GPS_LOCATION_HAS_SPOOF_MASK & ulpLocation.gpsLocation.flags) {
        out.flags |= LOCATION_HAS_SPOOF_MASK_BIT;
        out.spoofMask = ulpLocation.gpsLocation.spoof_mask;
    }
    out.qualityType = LOCATION_STANDALONE_QUALITY_TYPE;
    if (GPS_LOCATION_EXTENDED_HAS_NAV_SOLUTION_MASK & locationExtended.flags) {
        out.flags |= LOCATION_HAS_QUALITY_TYPE_BIT;
        if ((LOC_NAV_MASK_RTK_FIXED_CORRECTION & locationExtended.navSolutionMask) &&
                (LOC_NAV_MASK_RTK_CORRECTION & locationExtended.navSolutionMask)) {
            out.qualityType = LOCATION_FIXED_QUALITY_TYPE;
        } else if (LOC_NAV_MASK_RTK_CORRECTION & locationExtended.navSolutionMask) {
            out.qualityType = LOCATION_FLOAT_QUALITY_TYPE;
        } else if (LOC_NAV_MASK_PPP_CORRECTION & locationExtended.navSolutionMask) {
            //If HEPE<5cm, we shall claim 'FIXED'; otherwise, 'FLOAT'
            out.qualityType = LOCATION_FLOAT_QUALITY_TYPE;
            if ((LOC_GPS_LOCATION_HAS_ACCURACY & ulpLocation.gpsLocation.flags) &&
                    (ulpLocation.gpsLocation.accuracy < 0.05)) {
                out.qualityType = LOCATION_FIXED_QUALITY_TYPE;
            }
        } else if (LOC_NAV_MASK_DGNSS_CORRECTION & locationExtended.navSolutionMask) {
            out.qualityType = LOCATION_DGNSS_QUALITY_TYPE;
        }
    }

    if ((GPS_LOCATION_EXTENDED_HAS_SYSTEM_TICK & locationExtended.flags) &&
        (LOC_OUTPUT_ENGINE_SPE == locationExtended.locOutputEngType)) {
        out.flags |= LOCATION_HAS_SYSTEM_TICK_BIT;
        out.systemTick = locationExtended.systemTick;
    }
}

void GnssAdapter::fillElapsedRealTime(const GpsLocationExtended& locationExtended,
                                      GnssLocationInfoNotification& out) {
    if (locationExtended.flags & GPS_LOCATION_EXTENDED_HAS_GPS_TIME) {
        int64_t elapsedTimeNs = 0;
        float elapsedTimeUncMsec = 0.0;
        uint64_t elapsedgPTPTimeNsec = 0;
        bool gptpTimeValid = false;
        if (mPositionElapsedRealTimeCal.fillAdditionalTimestamps(
                locationExtended, elapsedTimeNs, elapsedTimeUncMsec,
                elapsedgPTPTimeNsec, gptpTimeValid)) {
            out.location.flags |= LOCATION_HAS_ELAPSED_REAL_TIME_BIT;
            out.location.elapsedRealTime = elapsedTimeNs;
            out.location.elapsedRealTimeUnc = (int64_t) (elapsedTimeUncMsec * 1000000);

            if (gptpTimeValid) {
                out.location.flags |= LOCATION_HAS_GPTP_TIME_BIT;
                out.location.elapsedgPTPTime = elapsedgPTPTimeNsec;
                out.location.flags |= LOCATION_HAS_GPTP_TIME_UNC_BIT;
                out.location.elapsedgPTPTimeUnc = 0;
            }
        }
#ifndef FEATURE_AUTOMOTIVE
        else if ((out.location.timestamp > 0) &&
                 (locationExtended.gpsTime.gpsWeek != UNKNOWN_GPS_WEEK_NUM)) {
            int64_t locationTimeNanos = (int64_t)out.location.timestamp * 1000000;
            bool isCurDataTimeTrustable =
                    (out.location.timestamp % mLocPositionMode.min_interval == 0);
            int64_t elapsedRealTime = mPositionElapsedRealTimeCal.getElapsedRealtimeEstimateNanos(
                    locationTimeNanos, isCurDataTimeTrustable,
                    (int64_t)mLocPositionMode.min_interval * 1000000);

            if (elapsedRealTime != -1) {
                out.location.flags |= LOCATION_HAS_ELAPSED_REAL_TIME_BIT;
                out.location.elapsedRealTime = elapsedRealTime;
                out.location.elapsedRealTimeUnc =
                        mPositionElapsedRealTimeCal.getElapsedRealtimeUncNanos();
            }
        }
#endif //FEATURE_AUTOMOTIVE
    }
#ifndef FEATURE_AUTOMOTIVE
    if (!(out.location.flags & LOCATION_HAS_ELAPSED_REAL_TIME_BIT)) {
        out.location.elapsedRealTime = getBootTimeMilliSec() * 1000000;
        out.location.elapsedRealTimeUnc = mPositionElapsedRealTimeCal.getElapsedRealtimeUncNanos();
        out.location.flags |= LOCATION_HAS_ELAPSED_REAL_TIME_BIT;
    }
#endif //FEATURE_AUTOMOTIVE
}

/* This is utility routine that computes number of SV used
   in the fix from the svUsedIdsMask.
 */
#define MAX_SV_CNT_SUPPORTED_IN_ONE_CONSTELLATION 64
uint16_t GnssAdapter::getNumSvUsed(uint64_t svUsedIdsMask,
                                   int totalSvCntInThisConstellation)
{
    if (totalSvCntInThisConstellation > MAX_SV_CNT_SUPPORTED_IN_ONE_CONSTELLATION) {
        LOC_LOGe ("error: total SV count in this constellation %d exceeded limit of %d",
                  totalSvCntInThisConstellation, MAX_SV_CNT_SUPPORTED_IN_ONE_CONSTELLATION);
        return 0;
    }

    uint16_t numSvUsed = 0;
    uint64_t mask = 0x1;
    for (int i = 0; i < totalSvCntInThisConstellation; i++) {
        if (svUsedIdsMask & mask) {
            numSvUsed++;
        }
        mask <<= 1;
    }

    return numSvUsed;
}

void
GnssAdapter::convertLocationInfo(GnssLocationInfoNotification& out,
                                 const GpsLocationExtended& locationExtended,
                                 enum loc_sess_status status)
{
    memset(&out, 0, sizeof(GnssLocationInfoNotification));
    out.size = sizeof(GnssLocationInfoNotification);
    if (GPS_LOCATION_EXTENDED_HAS_ALTITUDE_MEAN_SEA_LEVEL & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT;
        out.altitudeMeanSeaLevel = locationExtended.altitudeMeanSeaLevel;
    }
    if (GPS_LOCATION_EXTENDED_HAS_EXT_DOP & locationExtended.flags) {
        out.flags |= (LDT_GNSS_LOCATION_INFO_DOP_BIT|LDT_GNSS_LOCATION_INFO_EXT_DOP_BIT);
        out.pdop = locationExtended.extDOP.PDOP;
        out.hdop = locationExtended.extDOP.HDOP;
        out.vdop = locationExtended.extDOP.VDOP;
        out.gdop = locationExtended.extDOP.GDOP;
        out.tdop = locationExtended.extDOP.TDOP;
    } else if (GPS_LOCATION_EXTENDED_HAS_DOP & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_DOP_BIT;
        out.pdop = locationExtended.pdop;
        out.hdop = locationExtended.hdop;
        out.vdop = locationExtended.vdop;
    }
    if (GPS_LOCATION_EXTENDED_HAS_MAG_DEV & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_MAGNETIC_DEVIATION_BIT;
        out.magneticDeviation = locationExtended.magneticDeviation;
    }
    if (GPS_LOCATION_EXTENDED_HAS_HOR_RELIABILITY & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_HOR_RELIABILITY_BIT;
        switch (locationExtended.horizontal_reliability) {
            case LOC_RELIABILITY_VERY_LOW:
                out.horReliability = LOCATION_RELIABILITY_VERY_LOW;
                break;
            case LOC_RELIABILITY_LOW:
                out.horReliability = LOCATION_RELIABILITY_LOW;
                break;
            case LOC_RELIABILITY_MEDIUM:
                out.horReliability = LOCATION_RELIABILITY_MEDIUM;
                break;
            case LOC_RELIABILITY_HIGH:
                out.horReliability = LOCATION_RELIABILITY_HIGH;
                break;
            default:
                out.horReliability = LOCATION_RELIABILITY_NOT_SET;
                break;
        }
    }
    if (GPS_LOCATION_EXTENDED_HAS_VERT_RELIABILITY & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_VER_RELIABILITY_BIT;
        switch (locationExtended.vertical_reliability) {
            case LOC_RELIABILITY_VERY_LOW:
                out.verReliability = LOCATION_RELIABILITY_VERY_LOW;
                break;
            case LOC_RELIABILITY_LOW:
                out.verReliability = LOCATION_RELIABILITY_LOW;
                break;
            case LOC_RELIABILITY_MEDIUM:
                out.verReliability = LOCATION_RELIABILITY_MEDIUM;
                break;
            case LOC_RELIABILITY_HIGH:
                out.verReliability = LOCATION_RELIABILITY_HIGH;
                break;
            default:
                out.verReliability = LOCATION_RELIABILITY_NOT_SET;
                break;
        }
    }
    if (GPS_LOCATION_EXTENDED_HAS_HOR_ELIP_UNC_MAJOR & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MAJOR_BIT;
        out.horUncEllipseSemiMajor = locationExtended.horUncEllipseSemiMajor;
    }
    if (GPS_LOCATION_EXTENDED_HAS_HOR_ELIP_UNC_MINOR & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MINOR_BIT;
        out.horUncEllipseSemiMinor = locationExtended.horUncEllipseSemiMinor;
    }
    if (GPS_LOCATION_EXTENDED_HAS_HOR_ELIP_UNC_AZIMUTH & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_AZIMUTH_BIT;
        out.horUncEllipseOrientAzimuth = locationExtended.horUncEllipseOrientAzimuth;
    }
    if (GPS_LOCATION_EXTENDED_HAS_NORTH_STD_DEV & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_NORTH_STD_DEV_BIT;
        out.northStdDeviation = locationExtended.northStdDeviation;
    }
    if (GPS_LOCATION_EXTENDED_HAS_EAST_STD_DEV & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_EAST_STD_DEV_BIT;
        out.eastStdDeviation = locationExtended.eastStdDeviation;
    }
    if (GPS_LOCATION_EXTENDED_HAS_NORTH_VEL & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_NORTH_VEL_BIT;
        out.northVelocity = locationExtended.northVelocity;
    }
    if (GPS_LOCATION_EXTENDED_HAS_NORTH_VEL_UNC & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT;
        out.northVelocityStdDeviation = locationExtended.northVelocityStdDeviation;
    }
    if (GPS_LOCATION_EXTENDED_HAS_EAST_VEL & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_EAST_VEL_BIT;
        out.eastVelocity = locationExtended.eastVelocity;
    }
    if (GPS_LOCATION_EXTENDED_HAS_EAST_VEL_UNC & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT;
        out.eastVelocityStdDeviation = locationExtended.eastVelocityStdDeviation;
    }
    if (GPS_LOCATION_EXTENDED_HAS_UP_VEL & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_UP_VEL_BIT;
        out.upVelocity = locationExtended.upVelocity;
    }
    if (GPS_LOCATION_EXTENDED_HAS_UP_VEL_UNC & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_UP_VEL_UNC_BIT;
        out.upVelocityStdDeviation = locationExtended.upVelocityStdDeviation;
    }
    if (GPS_LOCATION_EXTENDED_HAS_GNSS_SV_USED_DATA & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_GNSS_SV_USED_DATA_BIT;
        out.svUsedInPosition.gpsSvUsedIdsMask =
                locationExtended.gnss_sv_used_ids.gps_sv_used_ids_mask;
        out.svUsedInPosition.gloSvUsedIdsMask =
                locationExtended.gnss_sv_used_ids.glo_sv_used_ids_mask;
        out.svUsedInPosition.galSvUsedIdsMask =
                locationExtended.gnss_sv_used_ids.gal_sv_used_ids_mask;
        out.svUsedInPosition.bdsSvUsedIdsMask =
                locationExtended.gnss_sv_used_ids.bds_sv_used_ids_mask;
        out.svUsedInPosition.qzssSvUsedIdsMask =
                locationExtended.gnss_sv_used_ids.qzss_sv_used_ids_mask;
        out.svUsedInPosition.navicSvUsedIdsMask =
                locationExtended.gnss_sv_used_ids.navic_sv_used_ids_mask;

        out.flags |= LDT_GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT;
        out.numSvUsedInPosition = getNumSvUsed(out.svUsedInPosition.gpsSvUsedIdsMask,
                                               GPS_SV_PRN_MAX - GPS_SV_PRN_MIN + 1);
        out.numSvUsedInPosition += getNumSvUsed(out.svUsedInPosition.gloSvUsedIdsMask,
                                                GLO_SV_PRN_MAX - GLO_SV_PRN_MIN + 1);
        out.numSvUsedInPosition += getNumSvUsed(out.svUsedInPosition.qzssSvUsedIdsMask,
                                                QZSS_SV_PRN_MAX - QZSS_SV_PRN_MIN + 1);
        out.numSvUsedInPosition += getNumSvUsed(out.svUsedInPosition.bdsSvUsedIdsMask,
                                                BDS_SV_PRN_MAX - BDS_SV_PRN_MIN + 1);
        out.numSvUsedInPosition += getNumSvUsed(out.svUsedInPosition.galSvUsedIdsMask,
                                                GAL_SV_PRN_MAX - GAL_SV_PRN_MIN + 1);
        out.numSvUsedInPosition += getNumSvUsed(out.svUsedInPosition.navicSvUsedIdsMask,
                                                NAVIC_SV_PRN_MAX - NAVIC_SV_PRN_MIN + 1);

        out.numOfMeasReceived = locationExtended.numOfMeasReceived;
        for (int idx =0; idx < locationExtended.numOfMeasReceived; idx++) {
            out.measUsageInfo[idx].gnssSignalType =
                    locationExtended.measUsageInfo[idx].gnssSignalType;
            out.measUsageInfo[idx].gnssSvId =
                    locationExtended.measUsageInfo[idx].gnssSvId;
            out.measUsageInfo[idx].gnssConstellation =
                    locationExtended.measUsageInfo[idx].gnssConstellation;
        }
    }
    if (GPS_LOCATION_EXTENDED_HAS_NAV_SOLUTION_MASK & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_NAV_SOLUTION_MASK_BIT;
        out.navSolutionMask = locationExtended.navSolutionMask;
    }
    if (GPS_LOCATION_EXTENDED_HAS_POS_DYNAMICS_DATA & locationExtended.flags) {
        out.flags |= GPS_LOCATION_EXTENDED_HAS_POS_DYNAMICS_DATA;
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_YAW_RATE_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_YAW_RATE_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
            LOCATION_NAV_DATA_HAS_PITCH_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_PITCH_BIT;
        }

        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT;
        }
        if (locationExtended.bodyFrameData.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT) {
            out.bodyFrameData.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT;
        }

        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_PITCH_RATE_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_PITCH_RATE_BIT;
        }
        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_PITCH_RATE_UNC_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_PITCH_RATE_UNC_BIT;
        }
        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_ROLL_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_ROLL_BIT;
        }
        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_ROLL_UNC_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_ROLL_UNC_BIT;
        }
        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_ROLL_RATE_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_ROLL_RATE_BIT;
        }
        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_ROLL_RATE_UNC_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_ROLL_RATE_UNC_BIT;
        }
        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_YAW_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_YAW_BIT;
        }
        if (locationExtended.bodyFrameDataExt.bodyFrameDataMask &
                LOCATION_NAV_DATA_HAS_YAW_UNC_BIT) {
            out.bodyFrameDataExt.bodyFrameDataMask |= LOCATION_NAV_DATA_HAS_YAW_UNC_BIT;
        }

        out.bodyFrameData.longAccel = locationExtended.bodyFrameData.longAccel;
        out.bodyFrameData.latAccel = locationExtended.bodyFrameData.latAccel;
        out.bodyFrameData.vertAccel = locationExtended.bodyFrameData.vertAccel;
        out.bodyFrameData.yawRate = locationExtended.bodyFrameData.yawRate;
        out.bodyFrameData.pitch = locationExtended.bodyFrameData.pitch;
        out.bodyFrameData.longAccelUnc = locationExtended.bodyFrameData.longAccelUnc;
        out.bodyFrameData.latAccelUnc  = locationExtended.bodyFrameData.latAccelUnc;
        out.bodyFrameData.vertAccelUnc = locationExtended.bodyFrameData.vertAccelUnc;
        out.bodyFrameData.yawRateUnc   = locationExtended.bodyFrameData.yawRateUnc;
        out.bodyFrameData.pitchUnc     = locationExtended.bodyFrameData.pitchUnc;

        out.bodyFrameDataExt.pitchRate    = locationExtended.bodyFrameDataExt.pitchRate;
        out.bodyFrameDataExt.pitchRateUnc = locationExtended.bodyFrameDataExt.pitchRateUnc;
        out.bodyFrameDataExt.roll         = locationExtended.bodyFrameDataExt.roll;
        out.bodyFrameDataExt.rollUnc      = locationExtended.bodyFrameDataExt.rollUnc;
        out.bodyFrameDataExt.rollRate     = locationExtended.bodyFrameDataExt.rollRate;
        out.bodyFrameDataExt.rollRateUnc  = locationExtended.bodyFrameDataExt.rollRateUnc;
        out.bodyFrameDataExt.yaw          = locationExtended.bodyFrameDataExt.yaw;
        out.bodyFrameDataExt.yawUnc       = locationExtended.bodyFrameDataExt.yawUnc;
    }

    // Validity of this structure is established from the timeSrc of the GnssSystemTime structure.
    out.gnssSystemTime = locationExtended.gnssSystemTime;

    if (GPS_LOCATION_EXTENDED_HAS_LEAP_SECONDS & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_LEAP_SECONDS_BIT;
        out.leapSeconds = locationExtended.leapSeconds;
    }

    if (GPS_LOCATION_EXTENDED_HAS_CALIBRATION_CONFIDENCE & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_CALIBRATION_CONFIDENCE_BIT;
        out.calibrationConfidence = locationExtended.calibrationConfidence;
    }

    if (GPS_LOCATION_EXTENDED_HAS_CALIBRATION_STATUS & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_CALIBRATION_STATUS_BIT;
        out.calibrationStatus = locationExtended.calibrationStatus;
    }

    if (GPS_LOCATION_EXTENDED_HAS_OUTPUT_ENG_TYPE & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT;
        out.locOutputEngType = locationExtended.locOutputEngType;
    }

    if (GPS_LOCATION_EXTENDED_HAS_OUTPUT_ENG_MASK & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT;
        out.locOutputEngMask = locationExtended.locOutputEngMask;
    }

    if (GPS_LOCATION_EXTENDED_HAS_CONFORMITY_INDEX & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_CONFORMITY_INDEX_BIT;
        out.conformityIndex = locationExtended.conformityIndex;
    }

    if (GPS_LOCATION_EXTENDED_HAS_LLA_VRP_BASED & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_LLA_VRP_BASED_BIT;
        out.llaVRPBased = locationExtended.llaVRPBased;
    }

    if (GPS_LOCATION_EXTENDED_HAS_ENU_VELOCITY_LLA_VRP_BASED & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_ENU_VELOCITY_VRP_BASED_BIT;
        // copy over east, north and up vrp based velocity
        out.enuVelocityVRPBased[0] = locationExtended.enuVelocityVRPBased[0];
        out.enuVelocityVRPBased[1] = locationExtended.enuVelocityVRPBased[1];
        out.enuVelocityVRPBased[2] = locationExtended.enuVelocityVRPBased[2];
    }

    if (GPS_LOCATION_EXTENDED_HAS_DR_SOLUTION_STATUS_MASK & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_DR_SOLUTION_STATUS_MASK_BIT;
        out.drSolutionStatusMask = locationExtended.drSolutionStatusMask;
    }

    if (GPS_LOCATION_EXTENDED_HAS_ALTITUDE_ASSUMED & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_ALTITUDE_ASSUMED_BIT;
        out.altitudeAssumed = locationExtended.altitudeAssumed;
    }

    out.flags |= LDT_GNSS_LOCATION_INFO_SESSION_STATUS_BIT;
    out.sessionStatus = status;

    if (GPS_LOCATION_EXTENDED_HAS_INTEGRITY_RISK_USED & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_INTEGRITY_RISK_USED_BIT;
        out.integrityRiskUsed = locationExtended.integrityRiskUsed;
    }
    if (GPS_LOCATION_EXTENDED_HAS_PROTECT_ALONG_TRACK & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_PROTECT_ALONG_TRACK_BIT;
        out.protectAlongTrack = locationExtended.protectAlongTrack;
    }
    if (GPS_LOCATION_EXTENDED_HAS_PROTECT_CROSS_TRACK & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_PROTECT_CROSS_TRACK_BIT;
        out.protectCrossTrack = locationExtended.protectCrossTrack;
    }
    if (GPS_LOCATION_EXTENDED_HAS_PROTECT_VERTICAL & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_PROTECT_VERTICAL_BIT;
        out.protectVertical = locationExtended.protectVertical;
    }

    if (GPS_LOCATION_EXTENDED_HAS_DGNSS_STATION_ID & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_DGNSS_STATION_ID_BIT;
        out.numOfDgnssStationId = locationExtended.numOfDgnssStationId;
        for (uint32_t i = 0; i < locationExtended.numOfDgnssStationId; i++) {
            out.dgnssStationId[i] = locationExtended.dgnssStationId[i];
        }
    }
    if (GPS_LOCATION_EXTENDED_HAS_LEAP_SECONDS_UNC & locationExtended.flags) {
        out.flags |= LDT_GNSS_LOCATION_INFO_LEAP_SECONDS_UNC_BIT;
        out.leapSecondsUnc = locationExtended.leapSecondsUnc;
    }

}

inline uint32_t
GnssAdapter::convertSuplVersion(const GnssConfigSuplVersion suplVersion)
{
    switch (suplVersion) {
        case GNSS_CONFIG_SUPL_VERSION_2_0_4:
            return 0x00020004;
        case GNSS_CONFIG_SUPL_VERSION_2_0_0:
            return 0x00020000;
        case GNSS_CONFIG_SUPL_VERSION_2_0_2:
            return 0x00020002;
        case GNSS_CONFIG_SUPL_VERSION_1_0_0:
        default:
            return 0x00010000;
    }
}

uint32_t
GnssAdapter::convertLppeCp(const GnssConfigLppeControlPlaneMask lppeControlPlaneMask)
{
    uint32_t mask = 0;
    if (GNSS_CONFIG_LPPE_CONTROL_PLANE_DBH_BIT & lppeControlPlaneMask) {
        mask |= (1<<0);
    }
    if (GNSS_CONFIG_LPPE_CONTROL_PLANE_WLAN_AP_MEASUREMENTS_BIT & lppeControlPlaneMask) {
        mask |= (1<<1);
    }
    if (GNSS_CONFIG_LPPE_CONTROL_PLANE_SENSOR_BARO_MEASUREMENTS_BIT & lppeControlPlaneMask) {
        mask |= (1<<3);
    }
    if (GNSS_CONFIG_LPPE_CONTROL_PLANE_NON_E911_BIT & lppeControlPlaneMask) {
        mask |= (1<<4);
    }
    if (GNSS_CONFIG_LPPE_CONTROL_PLANE_CIV_ADDRESS_BIT & lppeControlPlaneMask) {
        mask |= (1<<5);
    }
    return mask;
}

uint32_t
GnssAdapter::convertLppeUp(const GnssConfigLppeUserPlaneMask lppeUserPlaneMask)
{
    uint32_t mask = 0;
    if (GNSS_CONFIG_LPPE_USER_PLANE_DBH_BIT & lppeUserPlaneMask) {
        mask |= (1<<0);
    }
    if (GNSS_CONFIG_LPPE_USER_PLANE_WLAN_AP_MEASUREMENTS_BIT & lppeUserPlaneMask) {
        mask |= (1<<1);
    }
    if (GNSS_CONFIG_LPPE_USER_PLANE_SENSOR_BARO_MEASUREMENTS_BIT & lppeUserPlaneMask) {
        mask |= (1<<3);
    }
    if (GNSS_CONFIG_LPPE_USER_PLANE_NON_E911_BIT & lppeUserPlaneMask) {
        mask |= (1<<4);
    }
    if (GNSS_CONFIG_LPPE_USER_PLANE_CIV_ADDRESS_BIT & lppeUserPlaneMask) {
        mask |= (1<<5);
    }
    return mask;
}

uint32_t
GnssAdapter::convertAGloProt(const GnssConfigAGlonassPositionProtocolMask aGloPositionProtocolMask)
{
    uint32_t mask = 0;
    if (GNSS_CONFIG_RRC_CONTROL_PLANE_BIT & aGloPositionProtocolMask) {
        mask |= (1<<0);
    }
    if (GNSS_CONFIG_RRLP_USER_PLANE_BIT & aGloPositionProtocolMask) {
        mask |= (1<<1);
    }
    if (GNSS_CONFIG_LLP_USER_PLANE_BIT & aGloPositionProtocolMask) {
        mask |= (1<<2);
    }
    if (GNSS_CONFIG_LLP_CONTROL_PLANE_BIT & aGloPositionProtocolMask) {
        mask |= (1<<3);
    }
    return mask;
}

uint32_t
GnssAdapter::convertEP4ES(const GnssConfigEmergencyPdnForEmergencySupl emergencyPdnForEmergencySupl)
{
    switch (emergencyPdnForEmergencySupl) {
       case GNSS_CONFIG_EMERGENCY_PDN_FOR_EMERGENCY_SUPL_YES:
           return 1;
       case GNSS_CONFIG_EMERGENCY_PDN_FOR_EMERGENCY_SUPL_NO:
       default:
           return 0;
    }
}

uint32_t
GnssAdapter::convertSuplEs(const GnssConfigSuplEmergencyServices suplEmergencyServices)
{
    switch (suplEmergencyServices) {
       case GNSS_CONFIG_SUPL_EMERGENCY_SERVICES_YES:
           return 1;
       case GNSS_CONFIG_SUPL_EMERGENCY_SERVICES_NO:
       default:
           return 0;
    }
}

uint32_t
GnssAdapter::convertSuplMode(const GnssConfigSuplModeMask suplModeMask)
{
    uint32_t mask = 0;
    if (GNSS_CONFIG_SUPL_MODE_MSB_BIT & suplModeMask) {
        mask |= (1<<0);
    }
    if (GNSS_CONFIG_SUPL_MODE_MSA_BIT & suplModeMask) {
        mask |= (1<<1);
    }
    return mask;
}

void GnssAdapter::readNfwLockConfig()
{
    char nfwCpPackageName[LOC_MAX_PARAM_STRING];
    char nfwSuplPackageName[LOC_MAX_PARAM_STRING];
    char nfwImsPackageName[LOC_MAX_PARAM_STRING];
    char nfwSimPackageName[LOC_MAX_PARAM_STRING];
    char nfwMdtPackageName[LOC_MAX_PARAM_STRING];
    char nfwTlocPackageName[LOC_MAX_PARAM_STRING];
    char nfwRlocPackageName[LOC_MAX_PARAM_STRING];
    char nfwV2xPackageName[LOC_MAX_PARAM_STRING];
    char nfwR1PackageName[LOC_MAX_PARAM_STRING];
    char nfwR2PackageName[LOC_MAX_PARAM_STRING];
    char nfwR3PackageName[LOC_MAX_PARAM_STRING];

    const loc_param_s_type nfw_packages_table[] =
    {
        { "NFW_CLIENT_CP",      &nfwCpPackageName,      NULL, 's' },
        { "NFW_CLIENT_SUPL",    &nfwSuplPackageName,    NULL, 's' },
        { "NFW_CLIENT_IMS",     &nfwImsPackageName,     NULL, 's' },
        { "NFW_CLIENT_SIM",     &nfwSimPackageName,     NULL, 's' },
        { "NFW_CLIENT_MDT",     &nfwMdtPackageName,     NULL, 's' },
        { "NFW_CLIENT_TLOC",    &nfwTlocPackageName,    NULL, 's' },
        { "NFW_CLIENT_RLOC",    &nfwRlocPackageName,    NULL, 's' },
        { "NFW_CLIENT_V2X",     &nfwV2xPackageName,     NULL, 's' },
        { "NFW_CLIENT_R1",      &nfwR1PackageName,      NULL, 's' },
        { "NFW_CLIENT_R2",      &nfwR2PackageName,      NULL, 's' },
        { "NFW_CLIENT_R3",      &nfwR3PackageName,      NULL, 's' },
    };
    UTIL_READ_CONF(LOC_PATH_GPS_CONF_STR, nfw_packages_table);

    mNfws[nfwImsPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_IMS;
    mNfws[nfwSimPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_SIM;
    mNfws[nfwMdtPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_MDT;
    mNfws[nfwTlocPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_TLOC;
    mNfws[nfwRlocPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_RLOC;
    mNfws[nfwV2xPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_V2X;
    mNfws[nfwR1PackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_R1;
    mNfws[nfwR2PackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_R2;
    mNfws[nfwR3PackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_R3;
    mNfws[nfwSuplPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_SUPL;
    mNfws[nfwCpPackageName] |= GNSS_CONFIG_GPS_LOCK_NFW_CP;
}

void
GnssAdapter::readConfigCommand()
{
    LOC_LOGd();

    struct MsgReadConfig : public LocMsg {
        GnssAdapter* mAdapter;
        ContextBase& mContext;
        inline MsgReadConfig(GnssAdapter* adapter,
                             ContextBase& context) :
            LocMsg(),
            mAdapter(adapter),
            mContext(context) {}
        inline virtual void proc() const {
            static bool confReadDone = false;
            if (!confReadDone) {
                confReadDone = true;
                // reads config into mContext->mGps_conf
                mContext.readConfig();
                mAdapter->readNfwLockConfig();
            }
        }
    };

    if (mContext != NULL) {
        sendMsg(new MsgReadConfig(this, *mContext));
    }
}

void
GnssAdapter::setSuplHostServer(const char* server, int port, LocServerType type)
{
    if (ContextBase::mGps_conf.AGPS_CONFIG_INJECT) {
        char serverUrl[MAX_URL_LEN] = {};
        int32_t length = -1;
        const char noHost[] = "NONE";

        if ((NULL == server) || (server[0] == 0) ||
                (strncasecmp(noHost, server, sizeof(noHost)) == 0)) {
            serverUrl[0] = '\0';
            length = 0;
        } else if (port > 0) {
            length = snprintf(serverUrl, sizeof(serverUrl), "%s:%u", server, port);
        }
        if (LOC_AGPS_SUPL_SERVER != type && LOC_AGPS_MO_SUPL_SERVER != type) {
            LOC_LOGe("Invalid type=%d", type);
        } else if (length >= 0) {
            if (LOC_AGPS_SUPL_SERVER == type) {
                getServerUrl().assign(serverUrl);
                strlcpy(ContextBase::mGps_conf.SUPL_HOST,
                        (nullptr == server) ? serverUrl : server,
                        LOC_MAX_PARAM_STRING);
                ContextBase::mGps_conf.SUPL_PORT = port;
            } else {
                if (strncasecmp(getMoServerUrl().c_str(), serverUrl, sizeof(serverUrl)) != 0) {
                    getMoServerUrl().assign(serverUrl);
                }
            }
        }
    }
}

void
GnssAdapter::setConfig()
{
    LOC_LOGd();

    updateClientsEventMask();

    if (!ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_ENGINE_DEBUG_DATA)) {
        // set nmea mask type
        uint32_t mask = 0;
        if (NMEA_PROVIDER_MP == ContextBase::mGps_conf.NMEA_PROVIDER) {
            mask |= LOC_NMEA_ALL_GENERAL_SUPPORTED_MASK;
            if (ContextBase::mGps_conf.NMEA_TAG_BLOCK_GROUPING_ENABLED) {
                mask |= LOC_NMEA_MASK_TAGBLOCK_V02;
            }
        }

        if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_DEBUG_NMEA_V02)) {
            mask |= LOC_NMEA_MASK_DEBUG_V02;
        }

        if (mNmeaMask != mask) {
            mNmeaMask = mask;
            if (mNmeaMask) {
                for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
                    if ((it->second.gnssNmeaCb != nullptr)) {
                        updateEvtMask(LOC_API_ADAPTER_BIT_NMEA_1HZ_REPORT,
                                LOC_REGISTRATION_MASK_ENABLED);
                        break;
                    }
                }
            }
        }
    } else {
        // Modem does not provide noraml NMEA if ENGINE_DEBUG_DATA feature is available
        // ensuring AP to nmea generation in this case
        ContextBase::mGps_conf.NMEA_PROVIDER =  NMEA_PROVIDER_AP;
        updateEvtMask(LOC_API_ADAPTER_BIT_ENGINE_DEBUG_DATA_REPORT, LOC_REGISTRATION_MASK_ENABLED);
    }

    std::string oldMoServerUrl = getMoServerUrl();
    setSuplHostServer(ContextBase::mGps_conf.SUPL_HOST,
                      ContextBase::mGps_conf.SUPL_PORT,
                      LOC_AGPS_SUPL_SERVER);
    setSuplHostServer(ContextBase::mGps_conf.MO_SUPL_HOST,
                      ContextBase::mGps_conf.MO_SUPL_PORT,
                      LOC_AGPS_MO_SUPL_SERVER);

    std::string moServerUrl = getMoServerUrl();
    std::string serverUrl = getServerUrl();
    // inject the configurations into modem
    loc_gps_cfg_s gpsConf = ContextBase::mGps_conf;
    loc_sap_cfg_s_type sapConf = ContextBase::mSap_conf;

    //cache the injected configuration with GnssConfigRequested struct
    GnssConfig gnssConfigRequested = {};
    gnssConfigRequested.flags |= GNSS_CONFIG_FLAGS_GPS_LOCK_VALID_BIT |
            GNSS_CONFIG_FLAGS_BLACKLISTED_SV_IDS_BIT;
    /* Here we process an SSR. We need to set the GPS_LOCK to the proper values, as follows:
    1. Q behavior. This is identified by mSupportNfwControl being 1. In this case
    ContextBase::mGps_conf.GPS_LOCK is a "state", meaning it should reflect the
    NV value. Therefore we will set the NV to ContextBase::mGps_conf.GPS_LOCK
    2. P behavior. This is identified by mSupportNfwControl being 0. In this case
    ContextBase::mGps_conf.GPS_LOCK is a "configuration", meaning it should hold
    the "mask" for NI. There are two subcases:
    a. Location enabled in GUI (1 == getAfwControlId()). We need to set
    the NV to GNSS_CONFIG_GPS_LOCK_NONE (both MO and NI enabled)
    b. Location disabled in GUI (0 == getAfwControlId()). We need to set
    the NV to ContextBase::mGps_conf.GPS_LOCK (the "mask", which is SIM-card
    specific)
    */
    if (mSupportNfwControl || (0 == getAfwControlId())) {
        gnssConfigRequested.gpsLock = gpsConf.GPS_LOCK;
    } else {
        gnssConfigRequested.gpsLock = GNSS_CONFIG_GPS_LOCK_NONE;
    }
    gnssConfigRequested.flags |= GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT |
            GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT |
            GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT |
            GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT;
    gnssConfigRequested.suplVersion = mLocApi->convertSuplVersion(gpsConf.SUPL_VER);
    gnssConfigRequested.lppProfileMask = gpsConf.LPP_PROFILE;
    gnssConfigRequested.aGlonassPositionProtocolMask = gpsConf.A_GLONASS_POS_PROTOCOL_SELECT;
    if (gpsConf.LPPE_CP_TECHNOLOGY) {
        gnssConfigRequested.flags |= GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT;
        gnssConfigRequested.lppeControlPlaneMask =
                mLocApi->convertLppeCp(gpsConf.LPPE_CP_TECHNOLOGY);
    }

    if (gpsConf.LPPE_UP_TECHNOLOGY) {
        gnssConfigRequested.flags |= GNSS_CONFIG_FLAGS_LPPE_USER_PLANE_VALID_BIT;
        gnssConfigRequested.lppeUserPlaneMask =
                mLocApi->convertLppeUp(gpsConf.LPPE_UP_TECHNOLOGY);
    }
    gnssConfigRequested.blacklistedSvIds.assign(mBlacklistedSvIds.begin(),
                                                mBlacklistedSvIds.end());
    mLocApi->sendMsg(new LocApiMsg(
            [this, gpsConf, sapConf, oldMoServerUrl, moServerUrl,
            serverUrl, gnssConfigRequested] () mutable {
        gnssUpdateConfig(oldMoServerUrl, moServerUrl, serverUrl,
                gnssConfigRequested, gnssConfigRequested);

        // set nmea mask type
        uint32_t mask = 0;
        if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_ENGINE_DEBUG_DATA)) {
            mask |= LOC_API_ADAPTER_BIT_ENGINE_DEBUG_DATA_REPORT;
        } else {
            if (NMEA_PROVIDER_MP == gpsConf.NMEA_PROVIDER) {
                mask |= LOC_NMEA_ALL_GENERAL_SUPPORTED_MASK;
                if (gpsConf.NMEA_TAG_BLOCK_GROUPING_ENABLED) {
                    mask |= LOC_NMEA_MASK_TAGBLOCK_V02;
                }
            }
            if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_DEBUG_NMEA_V02)) {
                mask |= LOC_NMEA_MASK_DEBUG_V02;
            }

            if (mask != 0) {
                mLocApi->setNMEATypesSync(mask);
            }
        }
        // load tunc configuration from config file on first boot-up,
        // e.g.: adapter.mLocConfigInfo.tuncConfigInfo.isValid is false
        if (mLocConfigInfo.tuncConfigInfo.isValid == false) {
            mLocConfigInfo.tuncConfigInfo.isValid = true;
            mLocConfigInfo.tuncConfigInfo.enable =
                    (gpsConf.CONSTRAINED_TIME_UNCERTAINTY_ENABLED == 1);
            mLocConfigInfo.tuncConfigInfo.tuncThresholdMs =
                   (float)gpsConf.CONSTRAINED_TIME_UNCERTAINTY_THRESHOLD;
            mLocConfigInfo.tuncConfigInfo.energyBudget =
                   gpsConf.CONSTRAINED_TIME_UNCERTAINTY_ENERGY_BUDGET;
        }

        mLocApi->setConstrainedTuncMode(
                mLocConfigInfo.tuncConfigInfo.enable,
                mLocConfigInfo.tuncConfigInfo.tuncThresholdMs,
                mLocConfigInfo.tuncConfigInfo.energyBudget);

        // load pace configuration from config file on first boot-up,
        // e.g.: adapter.mLocConfigInfo.paceConfigInfo.isValid is false
        if (mLocConfigInfo.paceConfigInfo.isValid == false) {
            mLocConfigInfo.paceConfigInfo.isValid = true;
            mLocConfigInfo.paceConfigInfo.enable =
                    (gpsConf.POSITION_ASSISTED_CLOCK_ESTIMATOR_ENABLED==1);
        }
        mLocApi->setPositionAssistedClockEstimatorMode(
                mLocConfigInfo.paceConfigInfo.enable);

        if (mLocConfigInfo.robustLocationConfigInfo.isValid == false) {
            mLocConfigInfo.robustLocationConfigInfo.isValid = true;
            // robust location to be enabled on bootup for auto targets
#ifdef FEATURE_AUTOMOTIVE
            mLocConfigInfo.robustLocationConfigInfo.enable = true;
            mLocConfigInfo.robustLocationConfigInfo.enableFor911 = true;
#else
            // robust location to be disabled on bootup for non-auto targets
            mLocConfigInfo.robustLocationConfigInfo.enable = false;
            mLocConfigInfo.robustLocationConfigInfo.enableFor911 = false;
#endif
        }
        mLocApi->configRobustLocation(
                mLocConfigInfo.robustLocationConfigInfo.enable,
                mLocConfigInfo.robustLocationConfigInfo.enableFor911,
                nullptr, true);

        if (sapConf.GYRO_BIAS_RANDOM_WALK_VALID ||
            sapConf.ACCEL_RANDOM_WALK_SPECTRAL_DENSITY_VALID ||
            sapConf.ANGLE_RANDOM_WALK_SPECTRAL_DENSITY_VALID ||
            sapConf.RATE_RANDOM_WALK_SPECTRAL_DENSITY_VALID ||
            sapConf.VELOCITY_RANDOM_WALK_SPECTRAL_DENSITY_VALID ) {
            mLocApi->setSensorPropertiesSync(
                sapConf.GYRO_BIAS_RANDOM_WALK_VALID,
                sapConf.GYRO_BIAS_RANDOM_WALK,
                sapConf.ACCEL_RANDOM_WALK_SPECTRAL_DENSITY_VALID,
                sapConf.ACCEL_RANDOM_WALK_SPECTRAL_DENSITY,
                sapConf.ANGLE_RANDOM_WALK_SPECTRAL_DENSITY_VALID,
                sapConf.ANGLE_RANDOM_WALK_SPECTRAL_DENSITY,
                sapConf.RATE_RANDOM_WALK_SPECTRAL_DENSITY_VALID,
                sapConf.RATE_RANDOM_WALK_SPECTRAL_DENSITY,
                sapConf.VELOCITY_RANDOM_WALK_SPECTRAL_DENSITY_VALID,
                sapConf.VELOCITY_RANDOM_WALK_SPECTRAL_DENSITY);
        }
        mLocApi->setSensorPerfControlConfigSync(
                sapConf.SENSOR_CONTROL_MODE,
                sapConf.SENSOR_ACCEL_SAMPLES_PER_BATCH,
                sapConf.SENSOR_ACCEL_BATCHES_PER_SEC,
                sapConf.SENSOR_GYRO_SAMPLES_PER_BATCH,
                sapConf.SENSOR_GYRO_BATCHES_PER_SEC,
                sapConf.SENSOR_ACCEL_SAMPLES_PER_BATCH_HIGH,
                sapConf.SENSOR_ACCEL_BATCHES_PER_SEC_HIGH,
                sapConf.SENSOR_GYRO_SAMPLES_PER_BATCH_HIGH,
                sapConf.SENSOR_GYRO_BATCHES_PER_SEC_HIGH,
                sapConf.SENSOR_ALGORITHM_CONFIG_MASK);
    } ));
    // deal with Measurement Corrections
    if (true == mIsMeasCorrInterfaceOpen) {
        initMeasCorr(true);
    }
}

std::vector<LocationError> GnssAdapter::gnssUpdateConfig(const std::string& oldMoServerUrl,
        const std::string& moServerUrl, const std::string& serverUrl,
        GnssConfig& gnssConfigRequested, GnssConfig& gnssConfigNeedEngineUpdate, size_t count) {
    size_t index = 0;
    LocationError err = LOCATION_ERROR_SUCCESS;
    std::vector<LocationError> errsList = {err};
    if (count > 0) {
        errsList.insert(errsList.begin(), count, LOCATION_ERROR_SUCCESS);
    }

    int serverUrlLen = serverUrl.length();
    int moServerUrlLen = moServerUrl.length();

    if (!ContextBase::mGps_conf.AGPS_CONFIG_INJECT) {
        LOC_LOGd("AGPS_CONFIG_INJECT is 0. Not setting flags for AGPS configurations");
        gnssConfigRequested.flags &= ~(GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT |
                GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT |
                GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT |
                GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT |
                GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT |
                GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT);
    }

    if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_GPS_LOCK_VALID_BIT) {
        if (gnssConfigNeedEngineUpdate.flags & GNSS_CONFIG_FLAGS_GPS_LOCK_VALID_BIT) {
            err = mLocApi->setGpsLockSync(gnssConfigRequested.gpsLock);
            if (index < count) {
                errsList[index] = err;
            }
        }
        index++;
    }

    if (gnssConfigRequested.flags &
            GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT) {
        if (gnssConfigNeedEngineUpdate.flags &
                GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT) {
            if (gnssConfigNeedEngineUpdate.assistanceServer.type ==
                    GNSS_ASSISTANCE_TYPE_SUPL) {
                err = mLocApi->setServerSync(
                        serverUrl.c_str(), serverUrlLen, LOC_AGPS_SUPL_SERVER);
                if (index < count) {
                    errsList[index] = err;
                }
                if (0 != oldMoServerUrl.compare(moServerUrl)) {
                    LocationError locErr =
                        mLocApi->setServerSync(moServerUrl.c_str(),
                                moServerUrlLen,
                                LOC_AGPS_MO_SUPL_SERVER);
                    if (locErr != LOCATION_ERROR_SUCCESS) {
                        LOC_LOGe("Error while setting MO SUPL_HOST server:%s",
                                moServerUrl.c_str());
                    }
                }
            } else if (gnssConfigNeedEngineUpdate.assistanceServer.type ==
                    GNSS_ASSISTANCE_TYPE_C2K) {
                struct in_addr addr;
                struct hostent* hp;
                bool resolveAddrSuccess = true;

                hp = gethostbyname(
                        gnssConfigNeedEngineUpdate.assistanceServer.hostName);
                if (hp != NULL) { /* DNS OK */
                    memcpy(&addr, hp->h_addr_list[0], hp->h_length);
                } else {
                    /* Try IP representation */
                    if (inet_aton(
                                gnssConfigNeedEngineUpdate.assistanceServer.hostName,
                                &addr) == 0) {
                        /* IP not valid */
                        LOC_LOGe("hostname '%s' cannot be resolved ",
                                gnssConfigNeedEngineUpdate.assistanceServer.hostName);
                        if (index < count) {
                            errsList[index] = LOCATION_ERROR_INVALID_PARAMETER;
                        }
                    } else {
                        resolveAddrSuccess = false;
                    }
                }

                if (resolveAddrSuccess) {
                    unsigned int ip = htonl(addr.s_addr);
                    err = mLocApi->setServerSync(ip,
                            gnssConfigNeedEngineUpdate.assistanceServer.port,
                            LOC_AGPS_CDMA_PDE_SERVER);
                    if (index < count) {
                        errsList[index] = err;
                    }
                }
            }
        }
        index++;
    }

    if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT) {
        if (gnssConfigNeedEngineUpdate.flags &
                GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT) {
            err = mLocApi->setSUPLVersionSync(gnssConfigRequested.suplVersion);
            if (index < count) {
                errsList[index] = err;
            }
        }
        index++;
    }

    if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT) {
        if (gnssConfigNeedEngineUpdate.flags &
                GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT) {
            err = mLocApi->setLPPConfigSync(gnssConfigRequested.lppProfileMask);
            if (index < count) {
                errsList[index] = err;
            }
        }
        index++;
    }

    if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT) {
        if (gnssConfigNeedEngineUpdate.flags &
                GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT) {
            err = mLocApi->setLPPeProtocolCpSync(
                    gnssConfigRequested.lppeControlPlaneMask);
            if (index < count) {
                errsList[index] = err;
            }
        }
        index++;
    }

    if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_LPPE_USER_PLANE_VALID_BIT) {
        if (gnssConfigNeedEngineUpdate.flags &
                GNSS_CONFIG_FLAGS_LPPE_USER_PLANE_VALID_BIT) {
            err = mLocApi->setLPPeProtocolUpSync(
                    gnssConfigRequested.lppeUserPlaneMask);
            if (index < count) {
                errsList[index] = err;
            }
        }
        index++;
    }

    if (gnssConfigRequested.flags &
            GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT) {
        if (gnssConfigNeedEngineUpdate.flags &
                GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT) {
            err = mLocApi->setAGLONASSProtocolSync(
                    gnssConfigRequested.aGlonassPositionProtocolMask);
            if (index < count) {
                errsList[index] = err;
            }
        }
        index++;
    }
    if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_BLACKLISTED_SV_IDS_BIT) {
        // Check if feature is supported
        if (!ContextBase::isFeatureSupported(
                    LOC_SUPPORTED_FEATURE_CONSTELLATION_ENABLEMENT_V02)) {
            LOC_LOGe("Feature constellation enablement not supported.");
            err = LOCATION_ERROR_NOT_SUPPORTED;
        } else {
            // Send the SV ID Config to Modem
            mBlacklistedSvIds.assign(gnssConfigRequested.blacklistedSvIds.begin(),
                    gnssConfigRequested.blacklistedSvIds.end());
            err = gnssSvIdConfigUpdateSync(gnssConfigRequested.blacklistedSvIds);
            if (LOCATION_ERROR_SUCCESS != err) {
                LOC_LOGe("Failed to send config to modem, err %d", err);
            }
        }
        if (index < count) {
            errsList[index] = err;
        }
        index++;
    }
    if (gnssConfigRequested.flags &
            GNSS_CONFIG_FLAGS_EMERGENCY_EXTENSION_SECONDS_BIT) {
        if (gnssConfigNeedEngineUpdate.flags &
                GNSS_CONFIG_FLAGS_EMERGENCY_EXTENSION_SECONDS_BIT) {
            err = mLocApi->setEmergencyExtensionWindowSync(
                    gnssConfigRequested.emergencyExtensionSeconds);
            if (index < count) {
                errsList[index] = err;
            }
        }
        index++;
    }

    if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT) {
        GnssConfig gnssConfig = {};
        gnssConfig.flags = GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT;
        gnssConfig.minSvElevation = gnssConfigRequested.minSvElevation;
        err = mLocApi->setParameterSync(gnssConfig);
        if (index < count) {
            errsList[index] = err;
        }
        index++;
    }

    return errsList;
}

uint32_t*
GnssAdapter::gnssUpdateConfigCommand(const GnssConfig& config)
{
    // count the number of bits set
    GnssConfigFlagsMask flagsCopy = config.flags;
    size_t count = 0;
    while (flagsCopy > 0) {
        if (flagsCopy & 1) {
            count++;
        }
        flagsCopy >>= 1;
    }
    std::string idsString = "[";
    uint32_t* ids = NULL;
    if (count > 0) {
        ids = new uint32_t[count];
        if (ids == nullptr) {
            LOC_LOGe("new allocation failed, fatal error.");
            return nullptr;
        }
        for (size_t i=0; i < count; ++i) {
            ids[i] = generateSessionId();
            IF_LOC_LOGD {
                idsString += std::to_string(ids[i]) + " ";
            }
        }
    }
    idsString += "]";

    LOC_LOGd("ids %s flags 0x%X", idsString.c_str(), config.flags);

    struct MsgGnssUpdateConfig : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        GnssConfig mConfig;
        size_t mCount;
        uint32_t* mIds;
        inline MsgGnssUpdateConfig(GnssAdapter& adapter,
                                   LocApiBase& api,
                                   GnssConfig config,
                                   uint32_t* ids,
                                   size_t count) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mConfig(config),
            mCount(count),
            mIds(nullptr) {
                if (mCount > 0) {
                    mIds = new uint32_t[count];
                    if (mIds) {
                        for (uint32_t index = 0; index < count; index++) {
                            mIds[index] = ids[index];
                        }
                    } else {
                        LOC_LOGe("memory allocation for mIds failed");
                    }
                }
        }

        inline MsgGnssUpdateConfig(const MsgGnssUpdateConfig& obj) :
                MsgGnssUpdateConfig(obj.mAdapter, obj.mApi, obj.mConfig,
                        obj.mIds, obj.mCount) {}

        inline virtual ~MsgGnssUpdateConfig()
        {
            if (nullptr != mIds) delete[] mIds;
        }

        inline virtual void proc() const {
            if (!mAdapter.isEngineCapabilitiesKnown()) {
                mAdapter.mPendingMsgs.push_back(new MsgGnssUpdateConfig(*this));
                return;
            }
            GnssAdapter& adapter = mAdapter;
            size_t countOfConfigs = mCount;
            GnssConfig gnssConfigRequested = mConfig;
            GnssConfig gnssConfigNeedEngineUpdate = mConfig;

            std::vector<uint32_t> sessionIds;
            sessionIds.assign(mIds, mIds + mCount);
            std::vector<LocationError> errs(mCount, LOCATION_ERROR_SUCCESS);
            int index = 0;
            bool needSuspendResume = false;

            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_GPS_LOCK_VALID_BIT) {
                GnssConfigGpsLock newGpsLock = gnssConfigRequested.gpsLock;

                newGpsLock |= GNSS_CONFIG_GPS_LOCK_MO;
                ContextBase::mGps_conf.GPS_LOCK = newGpsLock;
                /* If we get here it means that the changes in the framework to request for
                   'P' behavior were made, and therefore we need to "behave" as in 'P'
                However, we need to determine if enableCommand function has already been
                called, since it could get called before this function.*/
                if (0 != mAdapter.getAfwControlId()) {
                    /* enableCommand function has already been called since getAfwControlId
                    returns non zero. Now there are two possible cases:
                    1. This is the first time this function is called
                       (mSupportNfwControl is true). We need to behave as in 'P', but
                       for the first time, meaning MO was enabled, but NI was not, so
                       we need to unlock NI
                    2. This is not the first time this function is called, meaning we
                       are already behaving as in 'P'. No need to update the configuration
                       in this case (return to 'P' code) */
                    if (mAdapter.mSupportNfwControl) {
                        // case 1 above
                        newGpsLock = GNSS_CONFIG_GPS_LOCK_NONE;
                    } else {
                        // case 2 above
                        gnssConfigNeedEngineUpdate.flags &= ~(GNSS_CONFIG_FLAGS_GPS_LOCK_VALID_BIT);
                    }
                }
                gnssConfigRequested.gpsLock = newGpsLock;
                mAdapter.mSupportNfwControl = false;
                index++;
            }

            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT) {
                uint32_t newSuplVersion =
                        mAdapter.convertSuplVersion(gnssConfigRequested.suplVersion);
                ContextBase::mGps_conf.SUPL_VER = newSuplVersion;
                index++;
            }
            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT) {
                if (GNSS_ASSISTANCE_TYPE_SUPL == mConfig.assistanceServer.type) {
                    mAdapter.setSuplHostServer(mConfig.assistanceServer.hostName,
                            mConfig.assistanceServer.port,
                            LOC_AGPS_SUPL_SERVER);
                } else {
                    LOC_LOGE("MsgGnssUpdateConfig: not a valid gnss assistance type %u",
                             mConfig.assistanceServer.type);
                    errs.at(index) = LOCATION_ERROR_INVALID_PARAMETER;
                    gnssConfigNeedEngineUpdate.flags &=
                            ~(GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT);
                }
                index++;
            }
            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT) {
                uint32_t newLppProfileMask = gnssConfigRequested.lppProfileMask;
                ContextBase::mGps_conf.LPP_PROFILE = newLppProfileMask;
                index++;
            }
            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT) {
                uint32_t newLppeControlPlaneMask =
                        mAdapter.convertLppeCp(gnssConfigRequested.lppeControlPlaneMask);
                ContextBase::mGps_conf.LPPE_CP_TECHNOLOGY = newLppeControlPlaneMask;
                index++;
            }
            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_LPPE_USER_PLANE_VALID_BIT) {
                uint32_t newLppeUserPlaneMask =
                        mAdapter.convertLppeUp(gnssConfigRequested.lppeUserPlaneMask);
                ContextBase::mGps_conf.LPPE_UP_TECHNOLOGY = newLppeUserPlaneMask;
                index++;
            }
            if (gnssConfigRequested.flags &
                GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT) {
                uint32_t newAGloProtMask =
                        mAdapter.convertAGloProt(gnssConfigRequested.aGlonassPositionProtocolMask);
                ContextBase::mGps_conf.A_GLONASS_POS_PROTOCOL_SELECT = newAGloProtMask;
                index++;
            }
            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_EM_PDN_FOR_EM_SUPL_VALID_BIT) {
                uint32_t newEP4ES = mAdapter.convertEP4ES(
                        gnssConfigRequested.emergencyPdnForEmergencySupl);
                if (newEP4ES != ContextBase::mGps_conf.USE_EMERGENCY_PDN_FOR_EMERGENCY_SUPL) {
                    ContextBase::mGps_conf.USE_EMERGENCY_PDN_FOR_EMERGENCY_SUPL = newEP4ES;
                }
                index++;
            }
            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_SUPL_EM_SERVICES_BIT) {
                uint32_t newSuplEs = mAdapter.convertSuplEs(
                        gnssConfigRequested.suplEmergencyServices);
                if (newSuplEs != ContextBase::mGps_conf.SUPL_ES) {
                    ContextBase::mGps_conf.SUPL_ES = newSuplEs;
                }
                index++;
            }
            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_SUPL_MODE_BIT) {
                uint32_t newSuplMode = mAdapter.convertSuplMode(gnssConfigRequested.suplModeMask);
                ContextBase::mGps_conf.SUPL_MODE = newSuplMode;
                mAdapter.broadcastCapabilities(mAdapter.getCapabilities());
                index++;
            }

            if (gnssConfigRequested.flags & GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT) {
                needSuspendResume = true;
                index++;
            }

            if (needSuspendResume == true) {
                mAdapter.suspendSessions();
            }
            LocApiCollectiveResponse *configCollectiveResponse = new LocApiCollectiveResponse(
                    *adapter.getContext(),
                    [&adapter, sessionIds, countOfConfigs] (std::vector<LocationError> errs) {

                    std::vector<uint32_t> ids(sessionIds);
                    adapter.reportResponse(countOfConfigs, errs.data(), ids.data());
            });

            std::string moServerUrl = adapter.getMoServerUrl();
            std::string serverUrl = adapter.getServerUrl();
            mApi.sendMsg(new LocApiMsg(
                    [&adapter, gnssConfigRequested, gnssConfigNeedEngineUpdate,
                    moServerUrl, serverUrl, countOfConfigs, configCollectiveResponse,
                    errs] () mutable {
                std::vector<LocationError> errsList = adapter.gnssUpdateConfig("",
                        moServerUrl, serverUrl,
                        gnssConfigRequested, gnssConfigNeedEngineUpdate, countOfConfigs);

                configCollectiveResponse->returnToSender(errsList);
            }));

            if (needSuspendResume == true) {
                mAdapter.restartSessions();
            }
        }
    };

    if (NULL != ids) {
        sendMsg(new MsgGnssUpdateConfig(*this, *mLocApi, config, ids, count));
    } else {
        LOC_LOGe("No GNSS config items to update");
    }

    return ids;
}

void
GnssAdapter::combineBlacklistSvs(const GnssSvIdConfig& blacklistSvs,
        const GnssSvTypeConfig& constellationConfig,
        GnssSvIdConfig& combinedBlacklistSvs) {
    // blacklisted SVs
    combinedBlacklistSvs = blacklistSvs;

    // Blacklist all SVs for each disabled constellation
    if (constellationConfig.blacklistedSvTypesMask) {
        if (constellationConfig.blacklistedSvTypesMask & GNSS_SV_TYPES_MASK_GPS_BIT) {
            combinedBlacklistSvs.gpsBlacklistSvMask = GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK;
        }
        if (constellationConfig.blacklistedSvTypesMask & GNSS_SV_TYPES_MASK_GLO_BIT) {
            combinedBlacklistSvs.gloBlacklistSvMask = GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK;
        }
        if (constellationConfig.blacklistedSvTypesMask & GNSS_SV_TYPES_MASK_BDS_BIT) {
            combinedBlacklistSvs.bdsBlacklistSvMask = GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK;
        }
        if (constellationConfig.blacklistedSvTypesMask & GNSS_SV_TYPES_MASK_QZSS_BIT) {
            combinedBlacklistSvs.qzssBlacklistSvMask = GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK;
        }
        if (constellationConfig.blacklistedSvTypesMask & GNSS_SV_TYPES_MASK_GAL_BIT) {
            combinedBlacklistSvs.galBlacklistSvMask = GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK;
        }
        if (constellationConfig.blacklistedSvTypesMask & GNSS_SV_TYPES_MASK_NAVIC_BIT) {
            combinedBlacklistSvs.navicBlacklistSvMask = GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK;
        }
    }
    LOC_LOGv("combined blacklist gps 0x%" PRIx64 ", bds 0x%" PRIx64 ", glo 0x%" PRIx64
            ", qzss 0x%" PRIx64 ", gal 0x%" PRIx64 ", sbas 0x%" PRIx64 ", navic 0x%" PRIx64,
            combinedBlacklistSvs.gpsBlacklistSvMask, combinedBlacklistSvs.bdsBlacklistSvMask,
            combinedBlacklistSvs.gloBlacklistSvMask, combinedBlacklistSvs.qzssBlacklistSvMask,
            combinedBlacklistSvs.galBlacklistSvMask, combinedBlacklistSvs.sbasBlacklistSvMask,
            combinedBlacklistSvs.navicBlacklistSvMask);
}

LocationError
GnssAdapter::gnssSvIdConfigUpdateSync(const std::vector<GnssSvIdSource>& blacklistedSvIds)
{
    // Clear the existing config
    memset(&mGnssSvIdConfig, 0, sizeof(GnssSvIdConfig));

    // Convert the sv id lists to masks
    convertToGnssSvIdConfig(blacklistedSvIds, mGnssSvIdConfig);

    // Now send to Modem
    return gnssSvConfigUpdate();
}

// This function will combine SV config command from Android and from XTRA daemon
// as follows so the callflow would work for regardless whether modem support
// constellation enablement or disablement
//
// step 1: obtain SV constellation enablement/disablement info via XTRA QCC path
// step 2: combine SV constellation enablement/disablement info with info in step 1
//   rule #1: if all SVs in a constellation are blacklisted, treat that constellation
//            are disabled as well.
//   rule #2: if a constellation is disabled, then mark all SVs in that constellation
//            are blacklisted
//   rule #3: if a constellation is both enabled and disabled, treat it as disabled
// We do not cache the current config applied to QMI Loc api and compare with new incoming
// config, as in case the command failed, cached current config may cause subsequent attempts to
// set config to current config to always fail
LocationError
GnssAdapter::gnssSvConfigUpdate()
{
    static GnssSvTypesMask currentSvTypeEnabled = 0;

    GnssSvIdConfig newBlacklistConfig = {};
    // combine sv constellation enablement/disablement from all sources (SDK and XTRA
    GnssSvTypeConfig newSvTypeConfig = gnssCombineSvTypeConfigs();
    // combine sv constellation enablement/disablement (newSvTypeConfig)
    // with blacklist info (mGnssSvIdConfig) into newBlacklistConfig
    combineBlacklistSvs(mGnssSvIdConfig, newSvTypeConfig, newBlacklistConfig);
    mLocApi->setBlacklistSv(newBlacklistConfig);

    if (newSvTypeConfig.size == 0) {
        mLocApi->resetConstellationControl();
    } else {
       // if constellation disablement is not supported, and if constellation need to
       // disabled, we will need to do reset, so that constellation can be disabled,
       // otherwise, that constellation may only get blacklisted
       bool disableSupported = ContextBase::isFeatureSupported(
               LOC_SUPPORTED_FEATURE_CONSTELLATION_DISABLEMENT);
       LOC_LOGd("disablement cap %d, current enabled constellation 0x%" PRIx64 ","
                "new enabled constellation 0x%" PRIx64 "",
                disableSupported, currentSvTypeEnabled, newSvTypeConfig.enabledSvTypesMask);
        if (false == disableSupported) {
            GnssSvTypesMask newSvTypeEnabled = newSvTypeConfig.enabledSvTypesMask;
            if (currentSvTypeEnabled & (currentSvTypeEnabled ^ newSvTypeEnabled)) {
                mLocApi->resetConstellationControl();
            }
        }
        mLocApi->setConstellationControl(newSvTypeConfig);
    }

    currentSvTypeEnabled = newSvTypeConfig.enabledSvTypesMask;

    return LOCATION_ERROR_SUCCESS;
}

void
GnssAdapter::gnssSecondaryBandConfigUpdate(LocApiResponse* locApiResponse)
{
    LOC_LOGd("secondary band config, size %d, enabled constellation 0x%" PRIx64 ","
             "disabled constellation 0x%" PRIx64 "", mGnssSeconaryBandConfig.size,
             mGnssSeconaryBandConfig.enabledSvTypesMask,
             mGnssSeconaryBandConfig.blacklistedSvTypesMask);
    if (mGnssSeconaryBandConfig.size == sizeof(mGnssSeconaryBandConfig)) {
        // Now set required secondary band config
        mLocApi->configConstellationMultiBand(mGnssSeconaryBandConfig, locApiResponse);
    }
}

uint32_t*
GnssAdapter::gnssGetConfigCommand(GnssConfigFlagsMask configMask) {

    // count the number of bits set
    GnssConfigFlagsMask flagsCopy = configMask;
    size_t count = 0;
    while (flagsCopy > 0) {
        if (flagsCopy & 1) {
            count++;
        }
        flagsCopy >>= 1;
    }
    std::string idsString = "[";
    uint32_t* ids = NULL;
    if (count > 0) {
        ids = new uint32_t[count];
        if (nullptr == ids) {
            LOC_LOGe("new allocation failed, fatal error.");
            return nullptr;
        }
        for (size_t i=0; i < count; ++i) {
            ids[i] = generateSessionId();
            IF_LOC_LOGD {
                idsString += std::to_string(ids[i]) + " ";
            }
        }
    }
    idsString += "]";

    LOC_LOGd("ids %s flags 0x%X", idsString.c_str(), configMask);

    struct MsgGnssGetConfig : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        GnssConfigFlagsMask mConfigMask;
        uint32_t* mIds;
        size_t mCount;
        inline MsgGnssGetConfig(GnssAdapter& adapter,
                                LocApiBase& api,
                                GnssConfigFlagsMask configMask,
                                uint32_t* ids,
                                size_t count) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mConfigMask(configMask),
            mCount(count),
            mIds(nullptr) {
                if (mCount > 0) {
                    mIds = new uint32_t[count];
                    if (mIds) {
                        for (uint32_t index = 0; index < count; index++) {
                            mIds[index] = ids[index];
                        }
                    } else {
                        LOC_LOGe("memory allocation for mIds failed");
                    }
                }
        }

        inline MsgGnssGetConfig(const MsgGnssGetConfig& obj) :
                MsgGnssGetConfig(obj.mAdapter, obj.mApi, obj.mConfigMask,
                        obj.mIds, obj.mCount) {}

        inline virtual ~MsgGnssGetConfig()
        {
            if (nullptr != mIds) delete[] mIds;
        }
        inline virtual void proc() const {
            if (!mAdapter.isEngineCapabilitiesKnown()) {
                mAdapter.mPendingMsgs.push_back(new MsgGnssGetConfig(*this));
                return;
            }
            LocationError* errs = new LocationError[mCount];
            LocationError err = LOCATION_ERROR_SUCCESS;
            uint32_t index = 0;

            if (nullptr == errs) {
                LOC_LOGE("MsgGnssGetConfig, memory allocation failed");
                return;
            }

            if (mConfigMask & GNSS_CONFIG_FLAGS_GPS_LOCK_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_LPPE_CONTROL_PLANE_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_LPPE_USER_PLANE_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_EM_PDN_FOR_EM_SUPL_VALID_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_SUPL_EM_SERVICES_BIT) {
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_SUPL_MODE_BIT) {
                err = LOCATION_ERROR_NOT_SUPPORTED;
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_BLACKLISTED_SV_IDS_BIT) {
                // Check if feature is supported
                if (!ContextBase::isFeatureSupported(
                        LOC_SUPPORTED_FEATURE_CONSTELLATION_ENABLEMENT_V02)) {
                    LOC_LOGW("MsgGnssGetConfig, CONSTELLATION_ENABLEMENT not supported.");
                    err = LOCATION_ERROR_NOT_SUPPORTED;
                } else {
                    // Send request to Modem to fetch the config
                    mApi.getBlacklistSv();
                    err = LOCATION_ERROR_SUCCESS;
                }
                if (index < mCount) {
                    errs[index++] = err;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_EMERGENCY_EXTENSION_SECONDS_BIT) {
                err = LOCATION_ERROR_NOT_SUPPORTED;
                if (index < mCount) {
                    errs[index++] = LOCATION_ERROR_NOT_SUPPORTED;
                }
            }
            if (mConfigMask & GNSS_CONFIG_FLAGS_ROBUST_LOCATION_BIT) {
                uint32_t sessionId = *(mIds+index);
                LocApiResponse* locApiResponse =
                        new LocApiResponse(*mAdapter.getContext(),
                                           [&mAdapter = mAdapter, sessionId] (LocationError err) {
                                           mAdapter.reportResponse(err, sessionId);});
                if (!locApiResponse) {
                    LOC_LOGE("MsgGnssGetConfig, memory allocation failed");
                    mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, sessionId);
                } else {
                   mApi.getRobustLocationConfig(sessionId, locApiResponse);
                }
            }

            if (mConfigMask & GNSS_CONFIG_FLAGS_MIN_GPS_WEEK_BIT) {
                uint32_t sessionId = *(mIds+index);
                LocApiResponse* locApiResponse =
                        new LocApiResponse(*mAdapter.getContext(),
                                           [&mAdapter = mAdapter, sessionId] (LocationError err) {
                                           mAdapter.reportResponse(err, sessionId);});
                if (!locApiResponse) {
                    LOC_LOGE("MsgGnssGetConfig, memory allocation failed");
                    mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, sessionId);
                } else {
                   mApi.getMinGpsWeek(sessionId, locApiResponse);
                }
            }

            if (mConfigMask & GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT) {
                uint32_t sessionId = *(mIds+index);
                LocApiResponse* locApiResponse =
                        new LocApiResponse(*mAdapter.getContext(),
                                           [&mAdapter = mAdapter, sessionId] (LocationError err) {
                                           mAdapter.reportResponse(err, sessionId);});
                if (!locApiResponse) {
                    LOC_LOGE("MsgGnssGetConfig, memory allocation failed");
                    mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, sessionId);
                } else {
                    mApi.getParameter(sessionId, GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT,
                                      locApiResponse);
                }
            }

            mAdapter.reportResponse(index, errs, mIds);
            delete[] errs;

        }
    };

    if (NULL != ids) {
        sendMsg(new MsgGnssGetConfig(*this, *mLocApi, configMask, ids, count));
    } else {
        LOC_LOGe("No GNSS config items to Get");
    }

    return ids;
}

bool
GnssAdapter::convertToGnssSvIdConfig(
        const std::vector<GnssSvIdSource>& blacklistedSvIds, GnssSvIdConfig& config)
{
    bool retVal = false;
    config.size = sizeof(GnssSvIdConfig);

    // Empty vector => Clear any previous blacklisted SVs
    if (0 == blacklistedSvIds.size()) {
        config.gpsBlacklistSvMask = 0;
        config.gloBlacklistSvMask = 0;
        config.bdsBlacklistSvMask = 0;
        config.qzssBlacklistSvMask = 0;
        config.galBlacklistSvMask = 0;
        config.sbasBlacklistSvMask = 0;
        config.navicBlacklistSvMask = 0;
        retVal = true;
    } else {
        // Parse the vector and convert SV IDs to mask values
        for (GnssSvIdSource source : blacklistedSvIds) {
            uint64_t* svMaskPtr = NULL;
            GnssSvId initialSvId = 0;
            uint16_t svIndexOffset = 0;
            switch (source.constellation) {
            case GNSS_SV_TYPE_GPS:
                svMaskPtr = &config.gpsBlacklistSvMask;
                initialSvId = GNSS_SV_CONFIG_GPS_INITIAL_SV_ID;
                break;
            case GNSS_SV_TYPE_GLONASS:
                svMaskPtr = &config.gloBlacklistSvMask;
                initialSvId = GNSS_SV_CONFIG_GLO_INITIAL_SV_ID;
                break;
            case GNSS_SV_TYPE_BEIDOU:
                svMaskPtr = &config.bdsBlacklistSvMask;
                initialSvId = GNSS_SV_CONFIG_BDS_INITIAL_SV_ID;
                break;
            case GNSS_SV_TYPE_QZSS:
                svMaskPtr = &config.qzssBlacklistSvMask;
                initialSvId = GNSS_SV_CONFIG_QZSS_INITIAL_SV_ID;
                break;
            case GNSS_SV_TYPE_GALILEO:
                svMaskPtr = &config.galBlacklistSvMask;
                initialSvId = GNSS_SV_CONFIG_GAL_INITIAL_SV_ID;
                break;
            case GNSS_SV_TYPE_SBAS:
                // SBAS does not support enable/disable whole constellation
                // so do not set up svTypeMask for SBAS
                svMaskPtr = &config.sbasBlacklistSvMask;
                // SBAS currently has two ranges, [120, 158] and [183, 191]
                if (0 == source.svId) {
                    LOC_LOGd("blacklist all SBAS SV");
                } else if (source.svId >= GNSS_SV_CONFIG_SBAS_INITIAL2_SV_ID) {
                    // handle SV id in range [183, 191]
                    initialSvId = GNSS_SV_CONFIG_SBAS_INITIAL2_SV_ID;
                    svIndexOffset = GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH;
                } else if ((source.svId >= GNSS_SV_CONFIG_SBAS_INITIAL_SV_ID) &&
                           (source.svId < (GNSS_SV_CONFIG_SBAS_INITIAL_SV_ID +
                                           GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH))){
                        // handle SV id in range of [120, 158]
                        initialSvId = GNSS_SV_CONFIG_SBAS_INITIAL_SV_ID;
                    } else {
                        LOC_LOGe("invalid SBAS sv id %d", source.svId);
                        svMaskPtr = nullptr;
                    }
                    break;
            case GNSS_SV_TYPE_NAVIC:
                 svMaskPtr = &config.navicBlacklistSvMask;
                 initialSvId = GNSS_SV_CONFIG_NAVIC_INITIAL_SV_ID;
                break;
            default:
                break;
            }

            if (NULL == svMaskPtr) {
                LOC_LOGe("Invalid constellation %d", source.constellation);
            } else {
                // SV ID 0 = All SV IDs
                if (0 == source.svId) {
                    *svMaskPtr = GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK;
                } else if (source.svId < initialSvId || source.svId >= initialSvId + 64) {
                    LOC_LOGe("Invalid sv id %d for sv type %d",
                            source.svId, source.constellation);
                } else {
                    uint32_t shiftCnt = source.svId + svIndexOffset - initialSvId;
                    *svMaskPtr |= (1ULL << shiftCnt);
                }
            }
        }

        // Return true if any one source is valid
        if (0 != config.gloBlacklistSvMask ||
                0 != config.gpsBlacklistSvMask ||
                0 != config.bdsBlacklistSvMask ||
                0 != config.galBlacklistSvMask ||
                0 != config.qzssBlacklistSvMask ||
                0 != config.sbasBlacklistSvMask ||
                0 != config.navicBlacklistSvMask) {
            retVal = true;
        }
    }

    LOC_LOGd("blacklist gps 0x%" PRIx64 ", bds 0x%" PRIx64 ", glo 0x%" PRIx64
            ", qzss 0x%" PRIx64 ", gal 0x%" PRIx64 ", sbas 0x%" PRIx64 ", navic 0x%" PRIx64,
             config.gpsBlacklistSvMask, config.bdsBlacklistSvMask, config.gloBlacklistSvMask,
             config.qzssBlacklistSvMask, config.galBlacklistSvMask,
            config.sbasBlacklistSvMask, config.navicBlacklistSvMask);

    return retVal;
}

void GnssAdapter::convertFromGnssSvIdConfig(
        const GnssSvIdConfig& svConfig, std::vector<GnssSvIdSource>& blacklistedSvIds)
{
    // Convert blacklisted SV mask values to vectors
    if (svConfig.gpsBlacklistSvMask) {
        convertGnssSvIdMaskToList(
                svConfig.gpsBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_GPS_INITIAL_SV_ID, GNSS_SV_TYPE_GPS);
    }
    if (svConfig.bdsBlacklistSvMask) {
        convertGnssSvIdMaskToList(
                svConfig.bdsBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_BDS_INITIAL_SV_ID, GNSS_SV_TYPE_BEIDOU);
    }
    if (svConfig.galBlacklistSvMask) {
        convertGnssSvIdMaskToList(
                svConfig.galBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_GAL_INITIAL_SV_ID, GNSS_SV_TYPE_GALILEO);
    }
    if (svConfig.gloBlacklistSvMask) {
        convertGnssSvIdMaskToList(
                svConfig.gloBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_GLO_INITIAL_SV_ID, GNSS_SV_TYPE_GLONASS);
    }
    if (svConfig.qzssBlacklistSvMask) {
        convertGnssSvIdMaskToList(
                svConfig.qzssBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_QZSS_INITIAL_SV_ID, GNSS_SV_TYPE_QZSS);
    }
    if (svConfig.sbasBlacklistSvMask) {
        // SBAS - SV 120 to 158, maps to 0 to 38
        //        SV 183 to 191, maps to 39 to 47
        uint64_t sbasBlacklistSvMask = svConfig.sbasBlacklistSvMask;
        // operate on 120 and 158 first
        sbasBlacklistSvMask <<= (64 - GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH);
        sbasBlacklistSvMask >>= (64 - GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH);
        convertGnssSvIdMaskToList(
                sbasBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_SBAS_INITIAL_SV_ID, GNSS_SV_TYPE_SBAS);
        // operate on the second range
        sbasBlacklistSvMask = svConfig.sbasBlacklistSvMask;
        sbasBlacklistSvMask >>= GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH;
        convertGnssSvIdMaskToList(
                sbasBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_SBAS_INITIAL2_SV_ID, GNSS_SV_TYPE_SBAS);
    }
    if (svConfig.navicBlacklistSvMask) {
        convertGnssSvIdMaskToList(
                svConfig.navicBlacklistSvMask, blacklistedSvIds,
                GNSS_SV_CONFIG_NAVIC_INITIAL_SV_ID, GNSS_SV_TYPE_NAVIC);
    }
}

void GnssAdapter::convertGnssSvIdMaskToList(
        uint64_t svIdMask, std::vector<GnssSvIdSource>& svIds,
        GnssSvId initialSvId, GnssSvType svType)
{
    GnssSvIdSource source = {};
    source.size = sizeof(GnssSvIdSource);
    source.constellation = svType;

    // SV ID 0 => All SV IDs in mask
    if (GNSS_SV_CONFIG_ALL_BITS_ENABLED_MASK == svIdMask) {
        LOC_LOGd("blacklist all SVs in constellation %d", source.constellation);
        source.svId = 0;
        svIds.push_back(source);
        return;
    }

    // Convert each bit in svIdMask to vector entry
    uint32_t bitNumber = 0;
    while (svIdMask > 0) {
        if (svIdMask & 0x1) {
            source.svId = bitNumber + initialSvId;
            // SBAS has two ranges:
            // SBAS - SV 120 to 158, maps to 0 to 38
            //        SV 183 to 191, maps to 39 to 47
            // #define GNSS_SV_CONFIG_SBAS_INITIAL_SV_ID     120
            // #define GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH 39
            // #define GNSS_SV_CONFIG_SBAS_INITIAL2_SV_ID    183
            if (svType == GNSS_SV_TYPE_SBAS) {
                if (bitNumber >= GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH) {
                    source.svId = bitNumber - GNSS_SV_CONFIG_SBAS_INITIAL_SV_LENGTH +
                                  GNSS_SV_CONFIG_SBAS_INITIAL2_SV_ID;
                }
            }
            svIds.push_back(source);
        }
        bitNumber++;
        svIdMask >>= 1;
    }
}

void GnssAdapter::reportGnssSvIdConfigEvent(const GnssSvIdConfig& config)
{
    struct MsgReportGnssSvIdConfig : public LocMsg {
        GnssAdapter& mAdapter;
        const GnssSvIdConfig mConfig;
        inline MsgReportGnssSvIdConfig(GnssAdapter& adapter,
                                 const GnssSvIdConfig& config) :
            LocMsg(),
            mAdapter(adapter),
            mConfig(config) {}
        inline virtual void proc() const {
            mAdapter.reportGnssSvIdConfig(mConfig);
        }
    };

    sendMsg(new MsgReportGnssSvIdConfig(*this, config));
}

void GnssAdapter::reportGnssSvIdConfig(const GnssSvIdConfig& svIdConfig)
{
    GnssConfig config = {};
    config.size = sizeof(GnssConfig);

    // Invoke control clients config callback
    if (nullptr != mControlCallbacks.gnssConfigCb &&
            svIdConfig.size == sizeof(GnssSvIdConfig)) {

        convertFromGnssSvIdConfig(svIdConfig, config.blacklistedSvIds);
        if (config.blacklistedSvIds.size() > 0) {
            config.flags |= GNSS_CONFIG_FLAGS_BLACKLISTED_SV_IDS_BIT;
        }
        LOC_LOGd("blacklist gps 0x%" PRIx64 ", bds 0x%" PRIx64 ", glo 0x%" PRIx64 ", "
                 "qzss 0x%" PRIx64 ", gal 0x%" PRIx64 ", sbas 0x%" PRIx64 ", navic 0x%" PRIx64,
                 svIdConfig.gpsBlacklistSvMask,
                 svIdConfig.bdsBlacklistSvMask, svIdConfig.gloBlacklistSvMask,
                 svIdConfig.qzssBlacklistSvMask, svIdConfig.galBlacklistSvMask,
                 svIdConfig.sbasBlacklistSvMask,  svIdConfig.navicBlacklistSvMask);
        // use 0 session id to indicate that receiver does not yet care about session id
        mControlCallbacks.gnssConfigCb(0, config);
    } else {
        LOC_LOGe("Failed to report, size %d", (uint32_t)config.size);
    }
}

void
GnssAdapter::gnssUpdateSvTypeConfigCommand(const GnssSvTypeConfig& config,
        GnssSvTypeConfigSource source)
{
    struct MsgGnssUpdateSvTypeConfig : public LocMsg {
        GnssAdapter* mAdapter;
        LocApiBase* mApi;
        GnssSvTypeConfig mConfig;
        GnssSvTypeConfigSource mSource;
        inline MsgGnssUpdateSvTypeConfig(
                GnssAdapter* adapter,
                LocApiBase* api,
                const GnssSvTypeConfig& config,
                GnssSvTypeConfigSource source) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mConfig(config),
            mSource(source) {}
        inline virtual void proc() const {
            if (!mAdapter->isEngineCapabilitiesKnown()) {
                mAdapter->mPendingMsgs.push_back(new MsgGnssUpdateSvTypeConfig(*this));
                return;
            }
            // Check if feature is supported
            if (!ContextBase::isFeatureSupported(
                    LOC_SUPPORTED_FEATURE_CONSTELLATION_ENABLEMENT_V02)) {
                LOC_LOGE("MsgGnssUpdateSvTypeConfig, CONSTELLATION_ENABLEMENT not supported.");
            } else {
                if (mAdapter->gnssSetSvTypeConfig(mConfig, mSource)) {
                    mAdapter->gnssSvConfigUpdate();
                }
            }
        }
    };

    sendMsg(new MsgGnssUpdateSvTypeConfig(this, mLocApi, config, source));
}

bool
GnssAdapter::gnssSetSvTypeConfig(const GnssSvTypeConfig& config,
                                 GnssSvTypeConfigSource source) {
    if (source >= SV_TYPE_CONFIG_MAX_SOURCE) {
        LOC_LOGe("unsupported source %d > SV_TYPE_CONFIG_MAX_SOURCE %d",
                source, SV_TYPE_CONFIG_MAX_SOURCE);
        return false;
    }

    mGnssSvTypeConfigs[source].isValid = true;
    mGnssSvTypeConfigs[source].gnssSvTypeConfig = config;
    return true;
}

GnssSvTypeConfig
GnssAdapter::gnssCombineSvTypeConfigs() {
    GnssSvTypeConfig svTypeConfig = {};

    // bit OR enableMask, disableMask from different SW API clients
    for (int i = 0; i < SV_TYPE_CONFIG_MAX_SOURCE; i++) {
        if (mGnssSvTypeConfigs[i].isValid) {
            svTypeConfig.enabledSvTypesMask |=
                mGnssSvTypeConfigs[i].gnssSvTypeConfig.enabledSvTypesMask;
            svTypeConfig.blacklistedSvTypesMask |=
                mGnssSvTypeConfigs[i].gnssSvTypeConfig.blacklistedSvTypesMask;

            LOC_LOGd("sv type config source %d, enableMask 0x%" PRIx64 " disableMask 0x%" PRIx64,
                    i, svTypeConfig.enabledSvTypesMask, svTypeConfig.blacklistedSvTypesMask);
        }
    }

    for (GnssSvIdSource svIdSource : mBlacklistedSvIds) {
       if (svIdSource.svId == 0) {
          LOC_LOGd("in blacklist sv list, svId is set to 0 for constellation of %d",
                   svIdSource.constellation);
          switch (svIdSource.constellation) {
          // Note: SBAS constellation need special handling
          // If GPS is disabled, then SBAS will be disabled.
          // If GPS is not disabled, then SBAS can not be disabled, but can be blacklisted
          case GNSS_SV_TYPE_GPS:
             svTypeConfig.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_GPS_BIT;
             break;
          case GNSS_SV_TYPE_GLONASS:
             svTypeConfig.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_GLO_BIT;
             break;
          case GNSS_SV_TYPE_QZSS:
             svTypeConfig.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_QZSS_BIT;
             break;
          case GNSS_SV_TYPE_BEIDOU:
             svTypeConfig.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_BDS_BIT;
             break;
          case GNSS_SV_TYPE_GALILEO:
             svTypeConfig.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_GAL_BIT;
             break;
          case GNSS_SV_TYPE_NAVIC:
             svTypeConfig.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_NAVIC_BIT;
             break;
          default:
              break;
          }
       }
    }

    // if client set size == 0, means reset constellation
    // size non zero shall overwrite zero
    if (svTypeConfig.enabledSvTypesMask || svTypeConfig.blacklistedSvTypesMask) {
       svTypeConfig.size = sizeof(svTypeConfig);
       // enabled mask is the opporsite of disabled mask
       svTypeConfig.enabledSvTypesMask = (~svTypeConfig.blacklistedSvTypesMask) &
                                          GNSS_SV_TYPES_MASK_ALL;
    }

    LOC_LOGd("Combined svTypeConfig size %d, enableMask 0x%" PRIx64 " disableMask 0x%" PRIx64,
             svTypeConfig.size, svTypeConfig.enabledSvTypesMask,
             svTypeConfig.blacklistedSvTypesMask);

    return svTypeConfig;
}

void
GnssAdapter::gnssGetSvTypeConfigCommand(GnssSvTypeConfigCallback callback)
{
    struct MsgGnssGetSvTypeConfig : public LocMsg {
        GnssAdapter* mAdapter;
        LocApiBase* mApi;
        GnssSvTypeConfigCallback mCallback;
        inline MsgGnssGetSvTypeConfig(
                GnssAdapter* adapter,
                LocApiBase* api,
                GnssSvTypeConfigCallback callback) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mCallback(callback) {}
        inline virtual void proc() const {
            if (!mAdapter->isEngineCapabilitiesKnown()) {
                mAdapter->mPendingMsgs.push_back(new MsgGnssGetSvTypeConfig(*this));
                return;
            }
            if (!ContextBase::isFeatureSupported(
                    LOC_SUPPORTED_FEATURE_CONSTELLATION_ENABLEMENT_V02)) {
                LOC_LOGE("MsgGnssGetSvTypeConfig, CONSTELLATION_ENABLEMENT not supported.");
            } else {
                // Save the callback
                mAdapter->gnssSetSvTypeConfigCallback(mCallback);
                // Send GET request to modem
                mApi->getConstellationControl();
            }
        }
    };

    sendMsg(new MsgGnssGetSvTypeConfig(this, mLocApi, callback));
}

void
GnssAdapter::gnssResetSvTypeConfigCommand()
{
    struct MsgGnssResetSvTypeConfig : public LocMsg {
        GnssAdapter* mAdapter;
        LocApiBase* mApi;
        inline MsgGnssResetSvTypeConfig(
                GnssAdapter* adapter,
                LocApiBase* api) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api) {}
        inline virtual void proc() const {
            if (!mAdapter->isEngineCapabilitiesKnown()) {
                mAdapter->mPendingMsgs.push_back(new MsgGnssResetSvTypeConfig(*this));
                return;
            }
            if (!ContextBase::isFeatureSupported(
                    LOC_SUPPORTED_FEATURE_CONSTELLATION_ENABLEMENT_V02)) {
                LOC_LOGE("MsgGnssResetSvTypeConfig, CONSTELLATION_ENABLEMENT not supported.");
            } else {
                // only being called by LocSDK, the default main client
                if (mAdapter->gnssSetSvTypeConfig({0, 0, 0}, SV_TYPE_CONFIG_FROM_API)) {
                    mAdapter->gnssSvConfigUpdate();
                }
            }
        }
    };

    sendMsg(new MsgGnssResetSvTypeConfig(this, mLocApi));
}

void GnssAdapter::reportGnssSvTypeConfigEvent(const GnssSvTypeConfig& config)
{
    struct MsgReportGnssSvTypeConfig : public LocMsg {
        GnssAdapter& mAdapter;
        const GnssSvTypeConfig mConfig;
        inline MsgReportGnssSvTypeConfig(GnssAdapter& adapter,
                                 const GnssSvTypeConfig& config) :
            LocMsg(),
            mAdapter(adapter),
            mConfig(config) {}
        inline virtual void proc() const {
            mAdapter.reportGnssSvTypeConfig(mConfig);
        }
    };

    sendMsg(new MsgReportGnssSvTypeConfig(*this, config));
}

void GnssAdapter::reportGnssSvTypeConfig(const GnssSvTypeConfig& config)
{
    // Invoke Get SV Type Callback
    if (NULL != mGnssSvTypeConfigCb &&
            config.size == sizeof(GnssSvTypeConfig)) {
        LOC_LOGd("constellations blacklisted 0x%" PRIx64 ", enabled 0x%" PRIx64,
                 config.blacklistedSvTypesMask, config.enabledSvTypesMask);
        mGnssSvTypeConfigCb(config);
    } else {
        LOC_LOGe("Failed to report, size %d", (uint32_t)config.size);
    }
}

void GnssAdapter::deleteAidingData(const GnssAidingData &data, uint32_t sessionId) {
    struct timespec bootDeleteAidingDataTime = {};
    int64_t bootDeleteTimeMs;
    if (clock_gettime(CLOCK_BOOTTIME, &bootDeleteAidingDataTime) == 0) {
        bootDeleteTimeMs = (int64_t)bootDeleteAidingDataTime.tv_sec * 1000;
        int64_t diffTimeBFirSecDelete = bootDeleteTimeMs - mLastDeleteAidingDataTime;
        if (diffTimeBFirSecDelete > DELETE_AIDING_DATA_EXPECTED_TIME_MS) {
            mLocApi->deleteAidingData(data, new LocApiResponse(*getContext(),
                    [this, sessionId] (LocationError err) {
                        reportResponse(err, sessionId);
                    }));
            mLastDeleteAidingDataTime = bootDeleteTimeMs;
        } else {
            LOC_LOGw("Two delete aiding data in less than %d ms, ignore second one",
                    DELETE_AIDING_DATA_EXPECTED_TIME_MS);
            reportResponse(LOCATION_ERROR_GENERAL_FAILURE, sessionId);
        }
    } else {
        LOC_LOGe("clock_gettime failed");
        reportResponse(LOCATION_ERROR_GENERAL_FAILURE, sessionId);
    }
}

uint32_t
GnssAdapter::gnssDeleteAidingDataCommand(const GnssAidingData& data)
{
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("id %u", sessionId);

    struct MsgDeleteAidingData : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t mSessionId;
        GnssAidingData mData;
        inline MsgDeleteAidingData(GnssAdapter& adapter,
                                   uint32_t sessionId,
                                   const GnssAidingData& data) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mData(data) {}
        inline virtual void proc() const {
            // suspend all tracking sessions to apply the aiding data deletion
            mAdapter.suspendSessions();

            if ((mData.posEngineMask & STANDARD_POSITIONING_ENGINE) != 0) {
                mAdapter.deleteAidingData(mData, mSessionId);
                if (mData.deleteAll) {
                    SystemStatus* s = mAdapter.getSystemStatus();
                    if (nullptr != s) {
                        s->setDefaultGnssEngineStates();
                    }
                    // inform xtra daemon that XTRA assistance data gets deleted
                    mAdapter.mXtraObserver.updateXtraDataDeletion();
                }
            }

            bool retVal = mAdapter.mEngHubProxy->gnssDeleteAidingData(mData);
            // When SPE engine is invoked, responseCb will be invoked
            // from QMI Loc API call.
            // When SPE engine is not invoked, we also need to deliver responseCb
            if ((mData.posEngineMask & STANDARD_POSITIONING_ENGINE) == 0) {
                LocationError err = LOCATION_ERROR_NOT_SUPPORTED;
                if (retVal == true) {
                    err = LOCATION_ERROR_SUCCESS;
                }
                mAdapter.reportResponse(err, mSessionId);
            }

            // resume all tracking sessions after applying aiding data deletion
            mAdapter.restartSessions(false);
        }
    };

    sendMsg(new MsgDeleteAidingData(*this, sessionId, data));
    return sessionId;
}

void
GnssAdapter::gnssUpdateXtraThrottleCommand(const bool enabled)
{
    LOC_LOGd("enabled:%d", enabled);

    struct UpdateXtraThrottleMsg : public LocMsg {
        GnssAdapter& mAdapter;
        const bool mEnabled;
        inline UpdateXtraThrottleMsg(GnssAdapter& adapter, const bool enabled) :
            LocMsg(),
            mAdapter(adapter),
            mEnabled(enabled) {}
        inline virtual void proc() const {
                mAdapter.mXtraObserver.updateXtraThrottle(mEnabled);
        }
    };

    sendMsg(new UpdateXtraThrottleMsg(*this, enabled));
}

void
GnssAdapter::injectLocationCommand(double latitude, double longitude, float accuracy)
{
    LOC_LOGd("latitude %8.4f longitude %8.4f accuracy %8.4f",
             latitude, longitude, accuracy);

    struct MsgInjectLocation : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        ContextBase& mContext;
        BlockCPIInfo& mBlockCPI;
        double mLatitude;
        double mLongitude;
        float mAccuracy;
        bool mOnDemandCpi;
        inline MsgInjectLocation(GnssAdapter& adapter,
                                 LocApiBase& api,
                                 ContextBase& context,
                                 BlockCPIInfo& blockCPIInfo,
                                 double latitude,
                                 double longitude,
                                 float accuracy,
                                 bool onDemandCpi) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mContext(context),
            mBlockCPI(blockCPIInfo),
            mLatitude(latitude),
            mLongitude(longitude),
            mAccuracy(accuracy),
            mOnDemandCpi(onDemandCpi) {}
        inline virtual void proc() const {
            if ((uptimeMillis() <= mBlockCPI.blockedTillTsMs) &&
                    (fabs(mLatitude-mBlockCPI.latitude) <= mBlockCPI.latLonDiffThreshold) &&
                    (fabs(mLongitude-mBlockCPI.longitude) <= mBlockCPI.latLonDiffThreshold)) {
                LOC_LOGD("MsgInjectLocation, pos injection blocked for "
                         "lat: %f, lon: %f, accuracy: %f",
                         mLatitude, mLongitude, mAccuracy);
            } else {
                if ((mAdapter.mOdcpiStateMask & CIVIC_ADDRESS_REQ_ACTIVE) &&
                        mAdapter.mAddressRequestCb != nullptr) {
                    Location location = {};
                    location.flags |= LOCATION_HAS_LAT_LONG_BIT;
                    location.latitude = mLatitude;
                    location.longitude = mLongitude;
                    location.flags |= LOCATION_HAS_ACCURACY_BIT;
                    location.accuracy = mAccuracy;
                    mAdapter.mAddressRequestCb(location);
                }

                mApi.injectPosition(mLatitude, mLongitude, mAccuracy, mOnDemandCpi);
            }
        }
    };

    sendMsg(new MsgInjectLocation(*this, *mLocApi, *mContext, mBlockCPIInfo,
                                  latitude, longitude, accuracy,
                                  mOdcpiStateMask & ODCPI_REQ_ACTIVE));
}

void
GnssAdapter::injectLocationExtCommand(const GnssLocationInfoNotification &locationInfo)
{
    LOC_LOGd("latitude %8.4f longitude %8.4f accuracy %8.4f, tech mask 0x%x",
             locationInfo.location.latitude, locationInfo.location.longitude,
             locationInfo.location.accuracy, locationInfo.location.techMask);

    struct MsgInjectLocationExt : public LocMsg {
        LocApiBase& mApi;
        ContextBase& mContext;
        GnssLocationInfoNotification mLocationInfo;
        inline MsgInjectLocationExt(LocApiBase& api,
                                    ContextBase& context,
                                    GnssLocationInfoNotification locationInfo) :
            LocMsg(),
            mApi(api),
            mContext(context),
            mLocationInfo(locationInfo) {}
        inline virtual void proc() const {
            // false to indicate for none-ODCPI
            mApi.injectPosition(mLocationInfo, false);
        }
    };

    sendMsg(new MsgInjectLocationExt(*mLocApi, *mContext, locationInfo));
}

void
GnssAdapter::injectTimeCommand(int64_t time, int64_t timeReference, int32_t uncertainty)
{
    LOC_LOGd("time %lld timeReference %lld uncertainty %d",
             (long long)time, (long long)timeReference, uncertainty);

    struct MsgInjectTime : public LocMsg {
        LocApiBase& mApi;
        ContextBase& mContext;
        int64_t mTime;
        int64_t mTimeReference;
        int32_t mUncertainty;
        inline MsgInjectTime(LocApiBase& api,
                             ContextBase& context,
                             int64_t time,
                             int64_t timeReference,
                             int32_t uncertainty) :
            LocMsg(),
            mApi(api),
            mContext(context),
            mTime(time),
            mTimeReference(timeReference),
            mUncertainty(uncertainty) {}
        inline virtual void proc() const {
            mApi.setTime(mTime, mTimeReference, mUncertainty);
        }
    };

    sendMsg(new MsgInjectTime(*mLocApi, *mContext, time, timeReference, uncertainty));
}

// This command is to called to block the position to be injected to the modem.
// This can happen for network position that comes from modem.
void
GnssAdapter::blockCPICommand(double latitude, double longitude,
                             float accuracy, int blockDurationMsec,
                             double latLonDiffThreshold)
{
    struct MsgBlockCPI : public LocMsg {
        BlockCPIInfo& mDstCPIInfo;
        BlockCPIInfo mSrcCPIInfo;

        inline MsgBlockCPI(BlockCPIInfo& dstCPIInfo,
                           BlockCPIInfo& srcCPIInfo) :
            mDstCPIInfo(dstCPIInfo),
            mSrcCPIInfo(srcCPIInfo) {}
        inline virtual void proc() const {
            // in the same hal thread, save the cpi to be blocked
            // the global variable
            mDstCPIInfo = mSrcCPIInfo;
        }
    };

    // construct the new block CPI info and queue on the same thread
    // for processing
    BlockCPIInfo blockCPIInfo;
    blockCPIInfo.latitude = latitude;
    blockCPIInfo.longitude = longitude;
    blockCPIInfo.accuracy = accuracy;
    blockCPIInfo.blockedTillTsMs = uptimeMillis() + blockDurationMsec;
    blockCPIInfo.latLonDiffThreshold = latLonDiffThreshold;

    LOC_LOGd("block CPI lat: %f, lon: %f ", latitude, longitude);
    // send a message to record down the coarse position
    // to be blocked from injection in the master copy (mBlockCPIInfo)
    sendMsg(new MsgBlockCPI(mBlockCPIInfo, blockCPIInfo));
}

void
GnssAdapter::setEsStatusCallbackCommand(std::function<void(bool)> esStatusCb)
{
    LOC_LOGd();

    struct MsgReportEsStatus : public LocMsg {
        GnssAdapter& mAdapter;
        std::function<void(bool)> mEsStatusCb;
        inline MsgReportEsStatus(GnssAdapter& adapter,
                                 std::function<void(bool)> esStatusCb) :
            LocMsg(),
            mAdapter(adapter),
            mEsStatusCb(esStatusCb) {}
        inline virtual void proc() const {
            mAdapter.setEsStatusCallback(mEsStatusCb);
        }
    };

    sendMsg(new MsgReportEsStatus(*this, esStatusCb));
}

void
GnssAdapter::setTribandState() {
    bool enabled = false;
    if (isInSession() && mEngServiceInfo.ppeIntEnabled && isQppeEnabled()) {
        enabled = true;
    }
    LOC_LOGd("enabled:%d", enabled);
    mLocApi->setTribandState(enabled);
}

void
GnssAdapter::updateSystemPowerState(PowerStateType systemPowerState) {
    if (POWER_STATE_UNKNOWN != systemPowerState) {
        mSystemPowerState = systemPowerState;

        /*Manage active GNSS sessions based on power event*/
        switch (systemPowerState){

            case POWER_STATE_SUSPEND:
            case POWER_STATE_SHUTDOWN:
            case POWER_STATE_DEEP_SLEEP_ENTRY:
                LOC_LOGd("Suspending all active sessions -- powerState: %d", systemPowerState);
                suspendSessions();
                break;
            case POWER_STATE_RESUME:
            case POWER_STATE_DEEP_SLEEP_EXIT:
                LOC_LOGd("Re-starting all active sessions -- powerState: %d", systemPowerState);
                restartSessions(false);
                if (loadLocSlatePUNCModel && mLocGlinkProxy) {
                    mLocGlinkProxy->getPropogatedPuncFromSlate();
                }
                break;
            default:
                break;
        } // switch
        mLocApi->updateSystemPowerState(mSystemPowerState);
        mContext->getLBSProxyBase()->notifyPowerState(systemPowerState);
        mEngHubProxy->sendPowerStateInfo(mSystemPowerState);
    }
}

void
GnssAdapter::updateSystemPowerStateCommand(PowerStateType systemPowerState) {
    LOC_LOGd("power event %d", systemPowerState);

    struct MsgUpdatePowerState : public LocMsg {
        GnssAdapter& mAdapter;
        PowerStateType mSystemPowerState;

        inline MsgUpdatePowerState(GnssAdapter& adapter,
                                   PowerStateType systemPowerState) :
            LocMsg(),
            mAdapter(adapter),
            mSystemPowerState(systemPowerState) {}
        inline virtual void proc() const {
            mAdapter.updateSystemPowerState(mSystemPowerState);
            mAdapter.mXtraObserver.updatePowerState(mSystemPowerState);
        }
    };

    sendMsg(new MsgUpdatePowerState(*this, systemPowerState));
}

void
GnssAdapter::updatePowerConnectStateCommand(bool connected) {
    LOC_LOGd("power connected %d", connected);

    struct MsgUpdatePowerConnectState : public LocMsg {
        GnssAdapter& mAdapter;
        bool mConnected;

        inline MsgUpdatePowerConnectState(GnssAdapter& adapter,
                                          bool connected) :
            LocMsg(),
            mAdapter(adapter),
            mConnected(connected) {}
        inline virtual void proc() const {
            mAdapter.mPowerConnectState =
                    (mConnected == true)? POWER_CONNECT_YES : POWER_CONNECT_NO;
            mAdapter.mLocApi->updatePowerConnectState(mConnected);
            mAdapter.mSystemStatus->updatePowerConnectState(mConnected);
        }
    };

    sendMsg(new MsgUpdatePowerConnectState(*this, connected));
}

void
GnssAdapter::addClientCommand(LocationAPI* client, const LocationCallbacks& callbacks)
{
    LOC_LOGd("client %p", client);

    struct MsgAddClient : public LocMsg {
        GnssAdapter& mAdapter;
        LocationAPI* mClient;
        const LocationCallbacks mCallbacks;
        inline MsgAddClient(GnssAdapter& adapter,
                            LocationAPI* client,
                            const LocationCallbacks& callbacks) :
            LocMsg(),
            mAdapter(adapter),
            mClient(client),
            mCallbacks(callbacks) {}
        inline virtual void proc() const {
            // check whether we need to notify client of cached location system info
            mAdapter.notifyClientOfCachedLocationSystemInfo(mClient, mCallbacks);
            mAdapter.saveClient(mClient, mCallbacks);
        }
    };

    sendMsg(new MsgAddClient(*this, client, callbacks));
}

void
GnssAdapter::stopClientSessions(LocationAPI* client, bool eraseSession)
{
    LOC_LOGd("client %p, eraseSession %d", client, eraseSession);

    /* Time-based Tracking */
    std::vector<LocationSessionKey> vTimeBasedTrackingClient;
    for (auto it : mTimeBasedTrackingSessions) {
        if (client == it.first.client) {
            vTimeBasedTrackingClient.emplace_back(it.first.client, it.first.id);
        }
    }
    for (auto key : vTimeBasedTrackingClient) {
        stopTimeBasedTrackingMultiplex(key.client, key.id);
        if (eraseSession)
            eraseTrackingSession(key.client, key.id);
    }

    /* Distance-based Tracking */
    for (auto it = mDistanceBasedTrackingSessions.begin();
              it != mDistanceBasedTrackingSessions.end(); /* no increment here*/) {
        if (client == it->first.client) {
            mLocApi->stopDistanceBasedTracking(it->first.id, new LocApiResponse(*getContext(),
                          [this, client, id=it->first.id, eraseSession] (LocationError err) {
                    if (LOCATION_ERROR_SUCCESS == err) {
                        if (eraseSession)
                            eraseTrackingSession(client, id);
                    }
                }
            ));
        }
        ++it; // increment only when not erasing an iterator
    }

}

void
GnssAdapter::updateClientsEventMask()
{
    // need to register for leap second info
    // for proper nmea generation
    LOC_API_ADAPTER_EVENT_MASK_T mask = LOC_API_ADAPTER_BIT_LOC_SYSTEM_INFO |
            LOC_API_ADAPTER_BIT_EVENT_REPORT_INFO |
            LOC_API_ADAPTER_BIT_FEATURE_STATUS_UPDATE;
    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (it->second.trackingCb != nullptr ||
            it->second.gnssLocationInfoCb != nullptr ||
            it->second.engineLocationsInfoCb != nullptr) {
            mask |= LOC_API_ADAPTER_BIT_PARSED_POSITION_REPORT;
        }
        if (it->second.gnssNmeaCb != nullptr || it->second.engineNmeaCb != nullptr) {
            mask |= LOC_API_ADAPTER_BIT_PARSED_POSITION_REPORT;
            mask |= LOC_API_ADAPTER_BIT_SATELLITE_REPORT;
        }
        if (it->second.gnssSvCb != nullptr) {
            mask |= LOC_API_ADAPTER_BIT_SATELLITE_REPORT;
        }
        if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_ENGINE_DEBUG_DATA)) {
            mask |= LOC_API_ADAPTER_BIT_ENGINE_DEBUG_DATA_REPORT;
        } else {
            if ((it->second.gnssNmeaCb != nullptr) && (mNmeaMask)) {
                mask |= LOC_API_ADAPTER_BIT_NMEA_1HZ_REPORT;
            }
        }
        if (it->second.gnssMeasurementsCb != nullptr) {
            mask |= LOC_API_ADAPTER_BIT_GNSS_MEASUREMENT;
            if (nullptr != mPowerIndicationCb) {
                /* If power reporting is requested this implies Android 'S' or higher,
                   meaning we need to enable poly message (necessary for satellite
                   PVT report). We do it this way since satellite PVT are reported
                   in the measurements cb, they don't have their own cb, and we want
                   to enable poly message only for Android 'S' or higher */
                mask |= LOC_API_ADAPTER_BIT_GNSS_SV_POLYNOMIAL_REPORT;
            }
        }
        if (it->second.gnssNHzMeasurementsCb != nullptr) {
            mask |= LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT;
        }
        if (it->second.gnssDataCb != nullptr) {
            mask |= LOC_API_ADAPTER_BIT_PARSED_POSITION_REPORT;
            if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_ENGINE_DEBUG_DATA)) {
                mask |= LOC_API_ADAPTER_BIT_ENGINE_DEBUG_DATA_REPORT;
            } else {
                mask |= LOC_API_ADAPTER_BIT_NMEA_1HZ_REPORT;
                updateNmeaMask(mNmeaMask | LOC_NMEA_MASK_DEBUG_V02);
            }
        }
        if (it->second.gnssDcReportCb != nullptr) {
            mask |= LOC_API_ADAPTER_BIT_DISASTER_CRISIS_REPORT;
        }
        if (it->second.gnssSignalTypesCb != nullptr) {
            // GNSS Bands supported
            LOC_LOGd("GNSS Bands supported, mGnssCapabNotification.count = %d",
                     mGnssCapabNotification.count);
            mask |= LOC_API_ADAPTER_BIT_GNSS_BANDS_SUPPORTED;
            // Calling gnssSignalTypesCb here to pass VTS
            // not necessary during normal operation, but doesn't hurt
            if (mGnssCapabNotification.count > 0) {
                it->second.gnssSignalTypesCb(mGnssCapabNotification);
            }
        }
        if (it->second.svEphemerisCb != nullptr) {
            LOC_LOGd("GNSS EPH supported");
            mask |= LOC_API_ADAPTER_BIT_GNSS_SV_EPHEMERIS_REPORT;
        }
    }

    /*
    ** For Automotive use cases we need to enable MEASUREMENT, POLY and EPHEMERIS
    ** when QDR is enabled (e.g.: either enabled via conf file or
    ** engine hub is loaded successfully).
    ** Note: this need to be called from msg queue thread.
    */
    if (1 == ContextBase::mGps_conf.EXTERNAL_DR_ENABLED ||
        (true == isPreciseEnabled())) {
        mask |= LOC_API_ADAPTER_BIT_GNSS_MEASUREMENT;
        mask |= LOC_API_ADAPTER_BIT_GNSS_SV_POLYNOMIAL_REPORT;
        mask |= LOC_API_ADAPTER_BIT_PARSED_UNPROPAGATED_POSITION_REPORT;
        mask |= LOC_API_ADAPTER_BIT_GNSS_SV_EPHEMERIS_REPORT;

        // Nhz measurement bit is set based on callback from loc eng hub
        // for Nhz engines.
        mask |= checkMask(LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT);
        /*
        ** To avoid the possibility of NHz mask reset by saveClient() in cases
        ** where EHUB loading takes time. When the EHUB is delayed and QWES feature
        ** update has not been completed, control does not enter here and as a result,
        ** local variable mask is not populated by NHZ flag hence later when
        ** a call is made to updateClientMask() with status = 2, the mEvtmask is
        ** overwritten and PPE engine is unable to receive NHz meas.
        */
        if (mNHzNeeded) {
            mask |= LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT;
        }
        LOC_LOGd("Auto usecase, Enable MEAS/POLY/EPHEMERIS - mask 0x%" PRIx64 "",
                mask);
    } else if (true == isStandAloneCDParserPELib()) {
        mask |= LOC_API_ADAPTER_BIT_GNSS_SV_POLYNOMIAL_REPORT;
        mask |= LOC_API_ADAPTER_BIT_PARSED_UNPROPAGATED_POSITION_REPORT;
        mask |= LOC_API_ADAPTER_BIT_GNSS_SV_EPHEMERIS_REPORT;
        LOC_LOGw("QPPE load as SSR parser, Enable POLY/PVT/EPHEMERIS");
    }

    if (mAgpsManager.isRegistered()) {
        mask |= LOC_API_ADAPTER_BIT_LOCATION_SERVER_REQUEST;
    }
    // Add ODCPI handling
    if (nullptr != mControlCallbacks.odcpiReqCb) {
        mask |= LOC_API_ADAPTER_BIT_REQUEST_WIFI;
    }

    // need to register for leap second info
    // for proper nmea generation
    mask |= LOC_API_ADAPTER_BIT_LOC_SYSTEM_INFO;

    // always register for NI NOTIFY VERIFY to handle internally in HAL
    mask |= LOC_API_ADAPTER_BIT_NI_NOTIFY_VERIFY_REQUEST;

    // register for engine lock state
    mask |= LOC_API_ADAPTER_BIT_ENGINE_LOCK_STATE_DATA_REPORT;

    // Enable the latency report
    if (mask & LOC_API_ADAPTER_BIT_GNSS_MEASUREMENT) {
        if (mLogger.isLogEnabled()) {
            mask |= LOC_API_ADAPTER_BIT_LATENCY_INFORMATION;
        }
    }

    updateEvtMask(mask, LOC_REGISTRATION_MASK_SET);
}

void
GnssAdapter::handleEngineLockStatusEvent(EngineLockState engineLockState) {

    LOC_LOGd("Engine state : %d", engineLockState);

    struct MsgEngineLockStateEvent : public LocMsg {
        GnssAdapter& mAdapter;
        EngineLockState mEngineLockState;

        inline MsgEngineLockStateEvent(GnssAdapter& adapter, EngineLockState engineLockState) :
            LocMsg(),
            mAdapter(adapter),
            mEngineLockState(engineLockState) {}

        virtual void proc() const {
            mAdapter.handleEngineLockStatus(mEngineLockState);
        }
    };

    sendMsg(new MsgEngineLockStateEvent(*this, engineLockState));
}

void
GnssAdapter::handleEngineLockStatus(EngineLockState engineLockState) {

    GnssConfigGpsLock gpsLock = GNSS_CONFIG_GPS_LOCK_MO_AND_NI;
    if (ENGINE_LOCK_STATE_DISABLED != engineLockState) {
        for (auto msg: mPendingMsgs) {
            sendMsg(msg);
        }
        mPendingMsgs.clear();

        if ((POWER_STATE_SUSPEND != mSystemPowerState) &&
            (POWER_STATE_DEEP_SLEEP_ENTRY != mSystemPowerState) &&
            POWER_STATE_SHUTDOWN != mSystemPowerState) {
            restartSessions(false);
        }
        // Send gps lock enabled only in case
        // when TZ is unlocked and AFW location is enabled
        if (0 != getAfwControlId()) {
            gpsLock = gpsLock & ~(GNSS_CONFIG_GPS_LOCK_MO);
        }
    }
    LOC_LOGv("send gps lock state: 0x%X", gpsLock);
    mXtraObserver.updateLockStatus(gpsLock);
}

void
GnssAdapter::handleEngineUpEvent()
{
    LOC_LOGd("mPpFeatureStatusMask: %x, isInSession(): %d, mAppHash: %s",
            mPpFeatureStatusMask, isInSession(), mAppHash.c_str());

    struct MsgHandleEngineUpEvent : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        inline MsgHandleEngineUpEvent(GnssAdapter& adapter, LocApiBase& api) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api) {}
        virtual void proc() const {
            mAdapter.setEngineCapabilitiesKnown(true);
            mAdapter.broadcastCapabilities(mAdapter.getCapabilities());
            // must be called only after capabilities are known
            mAdapter.setConfig();
            mAdapter.setTribandState();
            mAdapter.gnssSvConfigUpdate();
            mAdapter.updateSystemPowerState(mAdapter.getSystemPowerState());
            if (mAdapter.mPowerConnectState != POWER_CONNECT_UNKNOWN) {
                mAdapter.mLocApi->updatePowerConnectState(
                   mAdapter.mPowerConnectState == POWER_CONNECT_YES);
            }
            // inject AppHash when modem SSR but there is session ongoing
            if (mAdapter.isInSession() && !mAdapter.mAppHash.empty()) {
                if (mAdapter.isPreciseEnabled()) { //RTK will also enable eDGNSS, check DLP at first
                    mAdapter.mLocApi->configPrecisePositioning(QESDK_FEATURE_ID_RTK, true,
                            mAdapter.mAppHash);
                } else if (mAdapter.isMlpEnabled()) { //eDGNSS only
                    mAdapter.mLocApi->configPrecisePositioning(QESDK_FEATURE_ID_EDGNSS, true,
                            mAdapter.mAppHash);
                }
            }

            mAdapter.gnssSecondaryBandConfigUpdate();
            //Reset data connection when modem SSR
            mAdapter.mAgpsManager.handleModemSSR();

            // restart sessions only when Lock state is enabled and in power state resume
            mAdapter.initGnssPowerStatistics();
            if (ENGINE_LOCK_STATE_DISABLED != mApi.getEngineLockState()) {
                for (auto msg: mAdapter.mPendingMsgs) {
                    mAdapter.sendMsg(msg);
                }
                mAdapter.mPendingMsgs.clear();

                if ((POWER_STATE_SUSPEND != mAdapter.mSystemPowerState) &&
                    (POWER_STATE_DEEP_SLEEP_ENTRY != mAdapter.mSystemPowerState) &&
                    POWER_STATE_SHUTDOWN != mAdapter.mSystemPowerState) {
                    mAdapter.restartSessions(true);
                }
            }
        }
    };

    readConfigCommand();
    sendMsg(new MsgHandleEngineUpEvent(*this, *mLocApi));
}

void
GnssAdapter::restartSessions(bool modemSSR)
{
    LOC_LOGi(":enter, power state = %d, modemSSR = %d",
             mSystemPowerState, modemSSR);

    if ((POWER_STATE_SUSPEND == mSystemPowerState) ||
        (POWER_STATE_DEEP_SLEEP_ENTRY == mSystemPowerState) ||
        (POWER_STATE_SHUTDOWN == mSystemPowerState)) {
        LOC_LOGi("power state = %d, session not resumed", mSystemPowerState);
        return;
    }

    if (modemSSR) {
        // odcpi session is no longer active after restart
        mOdcpiStateMask = 0;
    }

    // SPE will be restarted now, so set this variable to false.
    mSPEAlreadyRunningAtHighestInterval = false;

    checkAndRestartSPESession();
}

void GnssAdapter::checkAndRestartSPESession()
{
    // SPE will be restarted now, so set this variable to false.
    mSPEAlreadyRunningAtHighestInterval = false;

    reStartTimeBasedTracking();

    for (auto it = mDistanceBasedTrackingSessions.begin();
        it != mDistanceBasedTrackingSessions.end(); ++it) {
        mLocApi->startDistanceBasedTracking(it->first.id, it->second,
                                            new LocApiResponse(*getContext(),
                                            [] (LocationError /*err*/) {}));
    }
}

// suspend all on-going sessions
void
GnssAdapter::suspendSessions()
{
    LOC_LOGi(":enter");

    if (!mTimeBasedTrackingSessions.empty()) {
        stopTracking();
    }
}

void
GnssAdapter::notifyClientOfCachedLocationSystemInfo(
        LocationAPI* client, const LocationCallbacks& callbacks) {

    if (mLocSystemInfo.systemInfoMask) {
        // notify client of cached location system info
        if (callbacks.locationSystemInfoCb) {
            auto it = mClientData.find(client);
            if (it != mClientData.end()) {
                callbacks.locationSystemInfoCb(mLocSystemInfo);
            }
        }
    }
}

bool
GnssAdapter::isTimeBasedTrackingSession(LocationAPI* client, uint32_t sessionId)
{
    LocationSessionKey key(client, sessionId);
    return (mTimeBasedTrackingSessions.find(key) != mTimeBasedTrackingSessions.end());
}

bool
GnssAdapter::isDistanceBasedTrackingSession(LocationAPI* client, uint32_t sessionId)
{
    LocationSessionKey key(client, sessionId);
    return (mDistanceBasedTrackingSessions.find(key) != mDistanceBasedTrackingSessions.end());
}

bool
GnssAdapter::hasCallbacksToStartTracking(LocationAPI* client)
{
    bool allowed = false;
    auto it = mClientData.find(client);
    if (it != mClientData.end()) {
        if (it->second.trackingCb || it->second.gnssLocationInfoCb ||
                it->second.engineLocationsInfoCb || it->second.gnssMeasurementsCb ||
                it->second.gnssNHzMeasurementsCb || it->second.gnssDataCb ||
                it->second.gnssSvCb || it->second.gnssNmeaCb || it->second.gnssDcReportCb ||
                it->second.gnssSignalTypesCb || it->second.engineNmeaCb ||
                it->second.svEphemerisCb) {
            allowed = true;
        } else {
            LOC_LOGi("missing right callback to start tracking")
        }
    } else {
        LOC_LOGi("client %p not found", client)
    }
    return allowed;
}

void
GnssAdapter::reportPowerStateIfChanged()
{
    bool newPowerOn = !mTimeBasedTrackingSessions.empty() ||
                      !mDistanceBasedTrackingSessions.empty();
    if (newPowerOn != mPowerOn) {
        mPowerOn = newPowerOn;
        if (mPowerStateCb != nullptr) {
            mPowerStateCb(mPowerOn);
        }
    }
}

void
GnssAdapter::getPowerStateChangesCommand(std::function<void(bool)> powerStateCb)
{
    struct MsgReportLocation : public LocMsg {
        GnssAdapter& mAdapter;
        std::function<void(bool)> mPowerStateCb;
        inline MsgReportLocation(GnssAdapter& adapter,
                                 std::function<void(bool)> powerStateCb) :
            LocMsg(),
            mAdapter(adapter),
            mPowerStateCb(powerStateCb) {}
        inline virtual void proc() const {
            mAdapter.savePowerStateCallback(mPowerStateCb);
            mPowerStateCb(mAdapter.getPowerState());
        }
    };

    sendMsg(new MsgReportLocation(*this, powerStateCb));
}

void
GnssAdapter::saveTrackingSession(LocationAPI* client, uint32_t sessionId,
                                const TrackingOptions& options)
{
    if (nullptr != client) {
        LocationSessionKey key(client, sessionId);
        if ((options.minDistance > 0) &&
            ContextBase::isMessageSupported(LOC_API_ADAPTER_MESSAGE_DISTANCE_BASE_TRACKING)) {
            mDistanceBasedTrackingSessions[key] = options;
        } else {
            mTimeBasedTrackingSessions[key] = options;
        }
        reportPowerStateIfChanged();
        // notify SystemStatus the engine tracking status
        getSystemStatus()->eventSetTracking(isInSession(), true);
    }
}

void
GnssAdapter::eraseTrackingSession(LocationAPI* client, uint32_t sessionId)
{
    if (nullptr != client) {
        LocationSessionKey key(client, sessionId);
        auto it = mTimeBasedTrackingSessions.find(key);
        if (it != mTimeBasedTrackingSessions.end()) {
            mTimeBasedTrackingSessions.erase(it);
        } else {
            auto itr = mDistanceBasedTrackingSessions.find(key);
            if (itr != mDistanceBasedTrackingSessions.end()) {
                mDistanceBasedTrackingSessions.erase(itr);
            }
        }
        reportPowerStateIfChanged();
        getSystemStatus()->eventSetTracking(isInSession(), true);
    }
}

void GnssAdapter::testLaunchQppeBringUp() {
    std::thread testLaunchThead([&](){
        int retryAttempts = 60;
        mQppeResp = false;
        getSystemStatus()->eventPreciseLocation(true);
        getSystemStatus()->eventSetTracking(true, true);
        while (retryAttempts >= 0 && !mQppeResp) {
            LOC_LOGd("testLaunchQppeBringUp, retry %d", (60 - retryAttempts));
            sleep(1);
            retryAttempts--;
        }
        if (!(mPpFeatureStatusMask & DLP_FEATURE_STATUS_LIBRARY_PRESENT)) {
            LOC_LOGd("timeout, no response from Qppe process.");
            getSystemStatus()->eventPreciseLocation(false);
        }
        getSystemStatus()->eventSetTracking(isInSession(), false);
    });
    testLaunchThead.detach();
}

bool GnssAdapter::setLocPositionMode(const LocPosMode& mode) {
    if (!mLocPositionMode.equals(mode)) {
        mLocPositionMode = mode;
        return true;
    } else {
        return false;
    }
}

void
GnssAdapter::reportResponse(LocationAPI* client, LocationError err, uint32_t sessionId)
{
    LOC_LOGd("client %p id %u err %u", client, sessionId, err);

    if (nullptr != client) {
        auto it = mClientData.find(client);
        if (it != mClientData.end() && it->second.responseCb != nullptr) {
            it->second.responseCb(err, sessionId);
        } else {
            LOC_LOGw("client %p id %u not found in data", client, sessionId);
        }
    }
}

void
GnssAdapter::reportResponse(LocationError err, uint32_t sessionId)
{
    LOC_LOGd("id %u err %u", sessionId, err);

    if (mControlCallbacks.size > 0 && mControlCallbacks.responseCb != nullptr) {
        mControlCallbacks.responseCb(err, sessionId);
    } else {
        LOC_LOGw("control client response callback not found");
    }
}

void
GnssAdapter::reportResponse(size_t count, LocationError* errs, uint32_t* ids)
{
    IF_LOC_LOGD {
        std::string idsString = "[";
        std::string errsString = "[";
        if (NULL != ids && NULL != errs) {
            for (size_t i=0; i < count; ++i) {
                idsString += std::to_string(ids[i]) + " ";
                errsString += std::to_string(errs[i]) + " ";
            }
        }
        idsString += "]";
        errsString += "]";

        LOC_LOGd("ids %s errs %s", idsString.c_str(), errsString.c_str());
    }

    if (mControlCallbacks.size > 0 && mControlCallbacks.collectiveResponseCb != nullptr) {
        mControlCallbacks.collectiveResponseCb(count, errs, ids);
    } else {
        LOC_LOGw("control client callback not found");
    }
}

uint32_t
GnssAdapter::startTrackingCommand(LocationAPI* client, const TrackingOptions& options)
{
    uint32_t sessionId = generateSessionId();
    LOC_LOGi("client %p id %u minInterval %u minDistance %u mode %u powermode %u tbm %u",
             client, sessionId, options.minInterval, options.minDistance, options.mode,
             options.powerMode, options.tbm);

    struct MsgStartTracking : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        LocationAPI* mClient;
        uint32_t mSessionId;
        mutable TrackingOptions mOptions;
        inline MsgStartTracking(GnssAdapter& adapter,
                               LocApiBase& api,
                               LocationAPI* client,
                               uint32_t sessionId,
                               const TrackingOptions& options) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mClient(client),
            mSessionId(sessionId),
            mOptions(options) {}
        inline virtual void proc() const {
            // distance based tracking will need to know engine capabilities before it can start
            if (!mAdapter.isEngineCapabilitiesKnown() && mOptions.minDistance > 0) {
                mAdapter.mPendingMsgs.push_back(new MsgStartTracking(*this));
                return;
            }
            LocationError err = LOCATION_ERROR_SUCCESS;
            if (!mAdapter.hasCallbacksToStartTracking(mClient)) {
                err = LOCATION_ERROR_CALLBACK_MISSING;
            } else if (0 == mOptions.size) {
                err = LOCATION_ERROR_INVALID_PARAMETER;
            } else {

                uint32_t minIntervalToSet = NHZ_NOT_ENABLED_MIN_TRACKING_INTERVAL;
                bool nHzStatus = mAdapter.getCapabilities() & LOCATION_CAPABILITIES_QWES_GNSS_NHZ;
                if (nHzStatus) {
                    minIntervalToSet = NHZ_ENABLED_MIN_TRACKING_INTERVAL;
                }
                if (mOptions.minInterval < minIntervalToSet) {
                    mOptions.minInterval = minIntervalToSet;
                }
                LOC_LOGd("Updated min Interval: %d, nHzEnabled: %s",
                        mOptions.minInterval, nHzStatus ? "true" : "false");

                if (mOptions.minDistance > 0 &&
                        ContextBase::isMessageSupported(
                        LOC_API_ADAPTER_MESSAGE_DISTANCE_BASE_TRACKING)) {
                    mAdapter.saveTrackingSession(mClient, mSessionId, mOptions);
                    mApi.startDistanceBasedTracking(mSessionId, mOptions,
                            new LocApiResponse(*mAdapter.getContext(),
                            [&mAdapter = mAdapter, mSessionId = mSessionId, mClient = mClient,
                            &mApi = mApi]
                            (LocationError err) {
                        if (ENGINE_LOCK_STATE_DISABLED != mApi.getEngineLockState() &&
                            LOCATION_ERROR_SUCCESS != err) {
                            mAdapter.eraseTrackingSession(mClient, mSessionId);
                        }
                        mAdapter.reportResponse(mClient, err, mSessionId);
                    }));
                } else {
                    if (GNSS_POWER_MODE_M4 == mOptions.powerMode &&
                            mOptions.tbm > TRACKING_TBM_THRESHOLD_MILLIS) {
                        LOC_LOGD("MsgStartTracking, TBM (%d) > %d Falling back to M2 power mode",
                                mOptions.tbm, TRACKING_TBM_THRESHOLD_MILLIS);
                        mOptions.powerMode = GNSS_POWER_MODE_M2;
                    }
                    // Api doesn't support multiple clients for time based tracking, so mutiplex
                    bool reportToClientWithNoWait =
                            mAdapter.startTimeBasedTrackingMultiplex(mClient, mSessionId, mOptions);
                    mAdapter.saveTrackingSession(mClient, mSessionId, mOptions);
                    mAdapter.setTribandState();

                    if (reportToClientWithNoWait) {
                        mAdapter.reportResponse(mClient, LOCATION_ERROR_SUCCESS, mSessionId);
                    }
                }
            }
        }
    };

    sendMsg(new MsgStartTracking(*this, *mLocApi, client, sessionId, options));
    return sessionId;

}

// Restarting the session after suspend
void GnssAdapter::reStartTimeBasedTracking() {
    if (!mTimeBasedTrackingSessions.empty()) {
        TrackingOptions multiplexedOptions;
        bool optionSetOnce = false;
        for (auto it2 = mTimeBasedTrackingSessions.begin();
                 it2 != mTimeBasedTrackingSessions.end(); ++it2) {
            if (!optionSetOnce) {
                multiplexedOptions = it2->second;
                optionSetOnce = true;
            } else {
                multiplexedOptions.multiplexWithForTimeBasedRequest(it2->second);
            }
        }

        startTimeBasedTracking(nullptr, 0, multiplexedOptions);
    }
}

bool
GnssAdapter::startTimeBasedTrackingMultiplex(LocationAPI* client, uint32_t sessionId,
                                             const TrackingOptions& options)
{
    bool reportToClientWithNoWait = true;

    if (mTimeBasedTrackingSessions.empty()) {
        /*Reset previous NMEA reported time stamp */
        mPrevNmeaRptTimeNsec = 0;
        startTimeBasedTracking(client, sessionId, options);
        // need to wait for QMI callback
        reportToClientWithNoWait = false;
    } else {
        TrackingOptions multiplexedOptions;
        bool optionSetOnce = false;
        for (auto it2 = mTimeBasedTrackingSessions.begin();
             it2 != mTimeBasedTrackingSessions.end(); ++it2) {
            if (!optionSetOnce) {
                multiplexedOptions = it2->second;
                optionSetOnce = true;
            } else {
                multiplexedOptions.multiplexWithForTimeBasedRequest(it2->second);
            }
        }
        TrackingOptions priorOptions = multiplexedOptions;
        multiplexedOptions.multiplexWithForTimeBasedRequest(options);
        if (!priorOptions.equalsInTimeBasedRequest(multiplexedOptions)) {
            startTimeBasedTracking(client, sessionId, multiplexedOptions);
            // need to wait for QMI callback
            reportToClientWithNoWait = false;
        }
    }

    return reportToClientWithNoWait;
}

void
GnssAdapter::startTimeBasedTracking(LocationAPI* client, uint32_t sessionId,
        const TrackingOptions& trackingOptions)
{
    //Don't allow session start in SUSPEND/SHUTDOWN state
    if ((POWER_STATE_SUSPEND == mSystemPowerState) ||
        (POWER_STATE_DEEP_SLEEP_ENTRY == mSystemPowerState) ||
        POWER_STATE_SHUTDOWN == mSystemPowerState) {

        LOC_LOGe("Not allowed in powerstate %d, return!", mSystemPowerState);
        reportResponse(client, LOCATION_ERROR_NOT_SUPPORTED, sessionId);
        return;
    }

    LOC_LOGd("minInterval %u minDistance %u mode %u powermode %u tbm %u",
            trackingOptions.minInterval, trackingOptions.minDistance,
            trackingOptions.mode, trackingOptions.powerMode, trackingOptions.tbm);
    LocPosMode locPosMode = {};
    convertOptions(locPosMode, trackingOptions);
    // save position mode parameters
    setLocPositionMode(locPosMode);
    // inform engine hub that GNSS session is about to start
    mEngHubProxy->gnssSetFixMode(mLocPositionMode);
    mEngHubProxy->gnssStartFix();
    // inform CDFW that GNSS session is about to start
    if (mCdfwInterface) {
        mCdfwInterface->updateTrackingStatus(true);
    }

    // want to run SPE session at a fixed min interval in some automotive scenarios
    // use a local copy of TrackingOptions as the TBF may get modified in the
    // checkAndSetSPEToRunforNHz function
    TrackingOptions tempOptions(trackingOptions);
    if (!checkAndSetSPEToRunforNHz(tempOptions)) {
        mLocApi->startTimeBasedTracking(tempOptions, new LocApiResponse(*getContext(),
                          [this, client, sessionId] (LocationError err) {
                if (ENGINE_LOCK_STATE_DISABLED != mLocApi->getEngineLockState() &&
                    LOCATION_ERROR_SUCCESS != err) {
                    eraseTrackingSession(client, sessionId);
                } else {
                    checkUpdateDgnssNtrip(false);
                }

                reportResponse(client, err, sessionId);
            }
        ));
    } else {
        reportResponse(client, LOCATION_ERROR_SUCCESS, sessionId);
    }

}

void
GnssAdapter::updateTracking(LocationAPI* client, uint32_t sessionId,
        const TrackingOptions& updatedOptions, const TrackingOptions& oldOptions)
{
    LocPosMode locPosMode = {};
    convertOptions(locPosMode, updatedOptions);
    // save position mode parameters
    setLocPositionMode(locPosMode);

    // inform engine hub that GNSS session is about to start
    mEngHubProxy->gnssSetFixMode(mLocPositionMode);
    mEngHubProxy->gnssStartFix();
    // inform CDFW that GNSS session is about to start
    if (mCdfwInterface) {
        mCdfwInterface->updateTrackingStatus(true);
    }

    // want to run SPE session at a fixed min interval in some automotive scenarios
    // use a local copy of TrackingOptions as the TBF may get modified in the
    // checkAndSetSPEToRunforNHz function
    TrackingOptions tempOptions(updatedOptions);
    if (!checkAndSetSPEToRunforNHz(tempOptions)) {
        mLocApi->startTimeBasedTracking(tempOptions, new LocApiResponse(*getContext(),
                          [this, client, sessionId, oldOptions] (LocationError err) {
                if (ENGINE_LOCK_STATE_DISABLED != mLocApi->getEngineLockState() &&
                    LOCATION_ERROR_SUCCESS != err) {
                    // restore the old LocationOptions
                    saveTrackingSession(client, sessionId, oldOptions);
                }
                reportResponse(client, err, sessionId);
            }
        ));
    } else {
        reportResponse(client, LOCATION_ERROR_SUCCESS, sessionId);
    }
}

void
GnssAdapter::updateTrackingOptionsCommand(LocationAPI* client, uint32_t id,
                                          const TrackingOptions& options)
{
    LOC_LOGd("client %p id %u minInterval %u mode %u",
             client, id, options.minInterval, options.mode);

    struct MsgUpdateTracking : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        LocationAPI* mClient;
        uint32_t mSessionId;
        mutable TrackingOptions mOptions;
        inline MsgUpdateTracking(GnssAdapter& adapter,
                                LocApiBase& api,
                                LocationAPI* client,
                                uint32_t sessionId,
                                const TrackingOptions& options) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mClient(client),
            mSessionId(sessionId),
            mOptions(options) {}
        inline virtual void proc() const {
            // distance based tracking will need to know engine capabilities before it can start
            if (!mAdapter.isEngineCapabilitiesKnown() && mOptions.minDistance > 0) {
                mAdapter.mPendingMsgs.push_back(new MsgUpdateTracking(*this));
                return;
            }
            LocationError err = LOCATION_ERROR_SUCCESS;
            bool isTimeBased = mAdapter.isTimeBasedTrackingSession(mClient, mSessionId);
            bool isDistanceBased = mAdapter.isDistanceBasedTrackingSession(mClient, mSessionId);
            if (!isTimeBased && !isDistanceBased) {
                err = LOCATION_ERROR_ID_UNKNOWN;
            } else if (0 == mOptions.size) {
                err = LOCATION_ERROR_INVALID_PARAMETER;
            }
            if (LOCATION_ERROR_SUCCESS != err) {
                mAdapter.reportResponse(mClient, err, mSessionId);
            } else {
                if (GNSS_POWER_MODE_M4 == mOptions.powerMode &&
                        mOptions.tbm > TRACKING_TBM_THRESHOLD_MILLIS) {
                    LOC_LOGD("MsgUpdateTracking, TBM (%d) > %d Falling back to M2 power mode",
                            mOptions.tbm, TRACKING_TBM_THRESHOLD_MILLIS);
                    mOptions.powerMode = GNSS_POWER_MODE_M2;
                }
                uint32_t minIntervalToSet = NHZ_NOT_ENABLED_MIN_TRACKING_INTERVAL;
                bool nHzStatus = mAdapter.getCapabilities() & LOCATION_CAPABILITIES_QWES_GNSS_NHZ;
                if (nHzStatus) {
                    minIntervalToSet = NHZ_ENABLED_MIN_TRACKING_INTERVAL;
                }
                if (mOptions.minInterval < minIntervalToSet) {
                    mOptions.minInterval = minIntervalToSet;
                }
                LOC_LOGd("Updated min Interval: %d, nHzEnabled: %s",
                        mOptions.minInterval, nHzStatus ? "true" : "false");

                // Now update session as required
                if (isTimeBased && mOptions.minDistance > 0) {
                    // switch from time based to distance based
                    // Api doesn't support multiple clients for time based tracking, so mutiplex
                    bool reportToClientWithNoWait =
                        mAdapter.stopTimeBasedTrackingMultiplex(mClient, mSessionId);
                    // erases the time based Session
                    mAdapter.eraseTrackingSession(mClient, mSessionId);
                    if (reportToClientWithNoWait) {
                        mAdapter.reportResponse(mClient, LOCATION_ERROR_SUCCESS, mSessionId);
                    }
                    // saves as distance based Session
                    mAdapter.saveTrackingSession(mClient, mSessionId, mOptions);
                    mApi.startDistanceBasedTracking(mSessionId, mOptions,
                            new LocApiResponse(*mAdapter.getContext(),
                                        [] (LocationError /*err*/) {}));
                } else if (isDistanceBased && mOptions.minDistance == 0) {
                    // switch from distance based to time based
                    mAdapter.eraseTrackingSession(mClient, mSessionId);
                    mApi.stopDistanceBasedTracking(mSessionId, new LocApiResponse(
                            *mAdapter.getContext(),
                            [&mAdapter = mAdapter, mSessionId = mSessionId, mOptions = mOptions,
                            mClient = mClient] (LocationError /*err*/) {
                        // Api doesn't support multiple clients for time based tracking,
                        // so mutiplex
                        bool reportToClientWithNoWait =
                                mAdapter.startTimeBasedTrackingMultiplex(mClient, mSessionId,
                                                                         mOptions);
                        mAdapter.saveTrackingSession(mClient, mSessionId, mOptions);

                        if (reportToClientWithNoWait) {
                            mAdapter.reportResponse(mClient, LOCATION_ERROR_SUCCESS, mSessionId);
                        }
                    }));
                } else if (isTimeBased) {
                    // update time based tracking
                    // Api doesn't support multiple clients for time based tracking, so mutiplex
                    bool reportToClientWithNoWait =
                            mAdapter.updateTrackingMultiplex(mClient, mSessionId, mOptions);
                    mAdapter.saveTrackingSession(mClient, mSessionId, mOptions);

                    if (reportToClientWithNoWait) {
                        mAdapter.reportResponse(mClient, err, mSessionId);
                    }
                } else if (isDistanceBased) {
                    // restart distance based tracking
                    mApi.stopDistanceBasedTracking(mSessionId, new LocApiResponse(
                            *mAdapter.getContext(),
                            [&mAdapter = mAdapter, mSessionId = mSessionId, mOptions = mOptions,
                            mClient = mClient, &mApi = mApi] (LocationError err) {
                        if (LOCATION_ERROR_SUCCESS == err) {
                            mApi.startDistanceBasedTracking(mSessionId, mOptions,
                                    new LocApiResponse(*mAdapter.getContext(),
                                    [&mAdapter, mClient, mSessionId, mOptions, &mApi = mApi]
                                    (LocationError err) {
                                if (ENGINE_LOCK_STATE_DISABLED == mApi.getEngineLockState() ||
                                    LOCATION_ERROR_SUCCESS == err) {
                                    mAdapter.saveTrackingSession(mClient, mSessionId, mOptions);
                                }
                                mAdapter.reportResponse(mClient, err, mSessionId);
                            }));
                        }
                    }));
                }
            }
        }
    };

    sendMsg(new MsgUpdateTracking(*this, *mLocApi, client, id, options));
}

bool
GnssAdapter::updateTrackingMultiplex(LocationAPI* client, uint32_t id,
                                     const TrackingOptions& trackingOptions)
{
    bool reportToClientWithNoWait = true;

    LocationSessionKey key(client, id);
    // get the session we are updating
    auto it = mTimeBasedTrackingSessions.find(key);

    // if session we are updating exists and the minInterval or powerMode has changed
    if (it != mTimeBasedTrackingSessions.end() && !it->second.equalsInTimeBasedRequest(trackingOptions)) {
        TrackingOptions multiplexedOptions; // size is 0 until set for the first time
        bool optionSetOnce = false;
        for (auto it2 = mTimeBasedTrackingSessions.begin();
             it2 != mTimeBasedTrackingSessions.end(); ++it2) {
            if (it2->first != key) {
                if (!optionSetOnce) {
                    multiplexedOptions = it2->second;
                    optionSetOnce = true;
                } else {
                    multiplexedOptions.multiplexWithForTimeBasedRequest(it2->second);
                }
            }
        }

        // if optionSetOnce is false when it gets here, it means that there is only one session
        // in the Sessions table, which is the one to be updated.
        if (false == optionSetOnce) {
            // check whether client updates to the option or not
            bool sameOption = it->second.equalsInTimeBasedRequest(trackingOptions);
            LOC_LOGd("same client, option same %d", sameOption);
            if (false == sameOption) {
                // restart time based tracking with the newly updated options
                updateTracking(client, id, trackingOptions, it->second);
                // need to wait for QMI callback
                reportToClientWithNoWait = false;
            }
        } else {
            TrackingOptions priorOptions = multiplexedOptions;
            priorOptions.multiplexWithForTimeBasedRequest(it->second);
            multiplexedOptions.multiplexWithForTimeBasedRequest(trackingOptions);
            if (false == priorOptions.equalsInTimeBasedRequest(multiplexedOptions)) {
                // restart time based tracking with the newly updated options
                updateTracking(client, id, multiplexedOptions, it->second);
                // need to wait for QMI callback
                reportToClientWithNoWait = false;
            }
        }
    }

    return reportToClientWithNoWait;
}

void
GnssAdapter::stopTrackingCommand(LocationAPI* client, uint32_t id)
{
    LOC_LOGd("client %p id %u", client, id);

    struct MsgStopTracking : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        LocationAPI* mClient;
        uint32_t mSessionId;
        inline MsgStopTracking(GnssAdapter& adapter,
                               LocApiBase& api,
                               LocationAPI* client,
                               uint32_t sessionId) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mClient(client),
            mSessionId(sessionId) {}
        inline virtual void proc() const {
            bool isTimeBased = mAdapter.isTimeBasedTrackingSession(mClient, mSessionId);
            bool isDistanceBased = mAdapter.isDistanceBasedTrackingSession(mClient, mSessionId);
            if (isTimeBased || isDistanceBased) {
                if (isTimeBased) {
                    // Api doesn't support multiple clients for time based tracking, so mutiplex
                    bool reportToClientWithNoWait =
                        mAdapter.stopTimeBasedTrackingMultiplex(mClient, mSessionId);
                    mAdapter.eraseTrackingSession(mClient, mSessionId);
                    mAdapter.setTribandState();

                    if (reportToClientWithNoWait) {
                        mAdapter.reportResponse(mClient, LOCATION_ERROR_SUCCESS, mSessionId);
                    }
                } else if (isDistanceBased) {
                    mApi.stopDistanceBasedTracking(mSessionId, new LocApiResponse(
                            *mAdapter.getContext(),
                            [&mAdapter = mAdapter, mSessionId = mSessionId, mClient = mClient,
                            &mApi = mApi]
                            (LocationError err) {
                        if (ENGINE_LOCK_STATE_DISABLED == mApi.getEngineLockState() ||
                            LOCATION_ERROR_SUCCESS == err) {
                            mAdapter.eraseTrackingSession(mClient, mSessionId);
                        }
                        mAdapter.reportResponse(mClient, err, mSessionId);
                    }));
                }
            } else {
                mAdapter.reportResponse(mClient, LOCATION_ERROR_ID_UNKNOWN, mSessionId);
            }
        }
    };

    sendMsg(new MsgStopTracking(*this, *mLocApi, client, id));
}

bool
GnssAdapter::stopTimeBasedTrackingMultiplex(LocationAPI* client, uint32_t id)
{
    bool reportToClientWithNoWait = true;

    if (1 == mTimeBasedTrackingSessions.size()) {
        stopTracking(client, id);
        // need to wait for QMI callback
        reportToClientWithNoWait = false;
    } else {
        LocationSessionKey key(client, id);

        // get the session we are stopping
        auto it = mTimeBasedTrackingSessions.find(key);
        if (it != mTimeBasedTrackingSessions.end()) {
            // find the smallest interval and powerMode, other than the session we are stopping
            TrackingOptions multiplexedOptions; // size is 0 until set for the first time
            bool optionSetOnce = false;
            for (auto it2 = mTimeBasedTrackingSessions.begin();
                 it2 != mTimeBasedTrackingSessions.end(); ++it2) {
                if (it2->first != key) {
                    if (!optionSetOnce) {
                        multiplexedOptions = it2->second;
                        optionSetOnce = true;
                    } else {
                        multiplexedOptions.multiplexWithForTimeBasedRequest(it2->second);
                    }
                }
            }
            TrackingOptions priorOptions = multiplexedOptions;
            priorOptions.multiplexWithForTimeBasedRequest(it->second);

            if (!priorOptions.equalsInTimeBasedRequest(multiplexedOptions)) {
                // restart time based tracking with the newly updated options
                startTimeBasedTracking(client, id, multiplexedOptions);
                // need to wait for QMI callback
                reportToClientWithNoWait = false;
            }
            // else part: no QMI call is made, need to report back to client right away
        }
    }
    return reportToClientWithNoWait;
}

void
GnssAdapter::stopTracking(LocationAPI* client, uint32_t id)
{
    // inform engine hub that GNSS session has stopped
    mEngHubProxy->gnssStopFix();

    // client is nullptr when we want to stop any tracking session,
    // e.g. when suspend.
    mLocApi->stopTimeBasedTracking((nullptr == client) ? nullptr :
            new LocApiResponse(*getContext(),
                               [this, client, id] (LocationError err) {
        reportResponse(client, err, id);
    }));

    if (isDgnssNmeaRequired()) {
        mDgnssState &= ~DGNSS_STATE_NO_NMEA_PENDING;
    }
    stopDgnssNtrip();
    if (mCdfwInterface) {
        mCdfwInterface->updateTrackingStatus(false);
    }

    mPositionElapsedRealTimeCal.reset();

    mSPEAlreadyRunningAtHighestInterval = false;
}

bool
GnssAdapter::hasNiNotifyCallback(LocationAPI* client)
{
    auto it = mClientData.find(client);
    return (it != mClientData.end() && it->second.gnssNiCb);
}

void
GnssAdapter::gnssNiResponseCommand(LocationAPI* client,
                                   uint32_t id,
                                   GnssNiResponse response)
{
    LOC_LOGd("client %p id %u response %u", client, id, response);

    struct MsgGnssNiResponse : public LocMsg {
        GnssAdapter& mAdapter;
        LocationAPI* mClient;
        uint32_t mSessionId;
        GnssNiResponse mResponse;
        inline MsgGnssNiResponse(GnssAdapter& adapter,
                                 LocationAPI* client,
                                 uint32_t sessionId,
                                 GnssNiResponse response) :
            LocMsg(),
            mAdapter(adapter),
            mClient(client),
            mSessionId(sessionId),
            mResponse(response) {}
        inline virtual void proc() const {
            NiData& niData = mAdapter.getNiData();
            LocationError err = LOCATION_ERROR_SUCCESS;
            if (!mAdapter.hasNiNotifyCallback(mClient)) {
                err = LOCATION_ERROR_ID_UNKNOWN;
            } else {
                NiSession* pSession = NULL;
                if (mSessionId == niData.sessionEs.reqID &&
                    NULL != niData.sessionEs.rawRequest) {
                    pSession = &niData.sessionEs;
                    // ignore any SUPL NI non-Es session if a SUPL NI ES is accepted
                    if (mResponse == GNSS_NI_RESPONSE_ACCEPT &&
                        NULL != niData.session.rawRequest) {
                            pthread_mutex_lock(&niData.session.tLock);
                            niData.session.resp = GNSS_NI_RESPONSE_IGNORE;
                            niData.session.respRecvd = true;
                            pthread_cond_signal(&niData.session.tCond);
                            pthread_mutex_unlock(&niData.session.tLock);
                    }
                } else if (mSessionId == niData.session.reqID &&
                    NULL != niData.session.rawRequest) {
                    pSession = &niData.session;
                }

                if (pSession) {
                    LOC_LOGI("MsgGnssNiResponse: send user mResponse %u for id %u",
                             mResponse, mSessionId);
                    pthread_mutex_lock(&pSession->tLock);
                    pSession->resp = mResponse;
                    pSession->respRecvd = true;
                    pthread_cond_signal(&pSession->tCond);
                    pthread_mutex_unlock(&pSession->tLock);
                } else {
                    err = LOCATION_ERROR_ID_UNKNOWN;
                    LOC_LOGE("MsgGnssNiResponse: id %u not an active session",
                             mSessionId);
                }
            }
            mAdapter.reportResponse(mClient, err, mSessionId);
        }
    };

    sendMsg(new MsgGnssNiResponse(*this, client, id, response));

}

void
GnssAdapter::gnssNiResponseCommand(GnssNiResponse response, void* rawRequest)
{
    LOC_LOGd("response %u", response);

    struct MsgGnssNiResponse : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        const GnssNiResponse mResponse;
        const void* mPayload;
        inline MsgGnssNiResponse(GnssAdapter& adapter,
                                 LocApiBase& api,
                                 const GnssNiResponse response,
                                 const void* rawRequest) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mResponse(response),
            mPayload(rawRequest) {}
        inline virtual ~MsgGnssNiResponse() {
        }
        inline virtual void proc() const {
            mApi.informNiResponse(mResponse, mPayload);
        }
    };

    sendMsg(new MsgGnssNiResponse(*this, *mLocApi, response, rawRequest));

}

uint32_t
GnssAdapter::enableCommand(LocationTechnologyType techType)
{
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("id %u techType %u", sessionId, techType);

    struct MsgEnableGnss : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        ContextBase& mContext;
        uint32_t mSessionId;
        LocationTechnologyType mTechType;
        inline MsgEnableGnss(GnssAdapter& adapter,
                             LocApiBase& api,
                             ContextBase& context,
                             uint32_t sessionId,
                             LocationTechnologyType techType) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mContext(context),
            mSessionId(sessionId),
            mTechType(techType) {}
        inline virtual void proc() const {
            LocationError err = LOCATION_ERROR_SUCCESS;
            uint32_t afwControlId = mAdapter.getAfwControlId();
            if (mTechType != LOCATION_TECHNOLOGY_TYPE_GNSS) {
                err = LOCATION_ERROR_INVALID_PARAMETER;
            } else if (afwControlId > 0) {
                err = LOCATION_ERROR_ALREADY_STARTED;
            } else {
                mContext.modemPowerVote(true);
                mAdapter.setAfwControlId(mSessionId);
                mAdapter.mSystemStatus->eventGpsEnabled(true);
                GnssConfigGpsLock gpsLock = GNSS_CONFIG_GPS_LOCK_NONE;
                if (mAdapter.mSupportNfwControl) {
                    ContextBase::mGps_conf.GPS_LOCK &= GNSS_CONFIG_GPS_LOCK_NFW_ALL;
                    gpsLock = ContextBase::mGps_conf.GPS_LOCK;
                }
                mApi.sendMsg(new LocApiMsg([&mApi = mApi, gpsLock]() {
                    mApi.setGpsLockSync(gpsLock);
                }));
                // check TZ lock status
                if (ENGINE_LOCK_STATE_DISABLED == mApi.getEngineLockState()) {
                    mAdapter.mXtraObserver.updateLockStatus(gpsLock | GNSS_CONFIG_GPS_LOCK_MO);
                } else {
                    mAdapter.mXtraObserver.updateLockStatus(gpsLock);
                }
            }
            mAdapter.reportResponse(err, mSessionId);
        }
    };

    if (mContext != NULL) {
        sendMsg(new MsgEnableGnss(*this, *mLocApi, *mContext, sessionId, techType));
    } else {
        LOC_LOGe("Context is NULL");
    }

    return sessionId;
}

void
GnssAdapter::disableCommand(uint32_t id)
{
    LOC_LOGd("id %u", id);

    struct MsgDisableGnss : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        ContextBase& mContext;
        uint32_t mSessionId;
        inline MsgDisableGnss(GnssAdapter& adapter,
                             LocApiBase& api,
                             ContextBase& context,
                             uint32_t sessionId) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mContext(context),
            mSessionId(sessionId) {}
        inline virtual void proc() const {
            LocationError err = LOCATION_ERROR_SUCCESS;
            uint32_t afwControlId = mAdapter.getAfwControlId();
            if (afwControlId != mSessionId) {
                err = LOCATION_ERROR_ID_UNKNOWN;
            } else {
                mContext.modemPowerVote(false);
                mAdapter.setAfwControlId(0);
                mAdapter.mSystemStatus->eventGpsEnabled(false);

                if (mAdapter.mSupportNfwControl) {
                    /* We need to disable MO (AFW) */
                    ContextBase::mGps_conf.GPS_LOCK |= GNSS_CONFIG_GPS_LOCK_MO;
                }
                GnssConfigGpsLock gpsLock = ContextBase::mGps_conf.GPS_LOCK;
                mApi.sendMsg(new LocApiMsg([&mApi = mApi, gpsLock]() {
                    mApi.setGpsLockSync(gpsLock);
                }));

                // check the TZ lock
                if (ENGINE_LOCK_STATE_DISABLED == mApi.getEngineLockState()) {
                    mAdapter.mXtraObserver.updateLockStatus(gpsLock | GNSS_CONFIG_GPS_LOCK_MO);
                } else {
                    mAdapter.mXtraObserver.updateLockStatus(gpsLock);
                }
            }
            mAdapter.reportResponse(err, mSessionId);
        }
    };

    if (mContext != NULL) {
        sendMsg(new MsgDisableGnss(*this, *mLocApi, *mContext, id));
    }

}

// This function computes the VRP based latitude, longitude and alittude, and
// north, east and up velocity and save the result into EHubTechReport.
void
GnssAdapter::computeVRPBasedLla(const UlpLocation& loc, GpsLocationExtended& locExt,
                                const LeverArmConfigInfo& leverArmConfigInfo) {

    float leverArm[3];
    float rollPitchYaw[3];
    double lla[3];

    uint16_t locFlags = loc.gpsLocation.flags;

    // we can only do translation if we have VRP based lever ARM info
    LeverArmTypeMask leverArmFlags = leverArmConfigInfo.leverArmValidMask;
    if (!(leverArmFlags & LEVER_ARM_TYPE_GNSS_TO_VRP_BIT)) {
        return;
    }

    leverArm[0] = leverArmConfigInfo.gnssToVRP.forwardOffsetMeters;
    leverArm[1] = leverArmConfigInfo.gnssToVRP.sidewaysOffsetMeters;
    leverArm[2] = leverArmConfigInfo.gnssToVRP.upOffsetMeters;

    if ((locFlags & LOC_GPS_LOCATION_HAS_LAT_LONG) &&
        (locFlags & LOC_GPS_LOCATION_HAS_ALTITUDE) &&
        (locFlags & LOCATION_HAS_BEARING_BIT)) {

        lla[0] = loc.gpsLocation.latitude * DEG2RAD;
        lla[1] = loc.gpsLocation.longitude * DEG2RAD;
        lla[2] = loc.gpsLocation.altitude;

        rollPitchYaw[0] = 0.0f;
        rollPitchYaw[1] = 0.0f;
        rollPitchYaw[2] = loc.gpsLocation.bearing * DEG2RAD;

        loc_convert_lla_gnss_to_vrp(lla, rollPitchYaw, leverArm);

        // assign the converted value into position report and
        // set up valid mask
        locExt.llaVRPBased.latitude  = lla[0] * RAD2DEG;
        locExt.llaVRPBased.longitude = lla[1] * RAD2DEG;
        locExt.llaVRPBased.altitude  = lla[2];
        locExt.flags |= GPS_LOCATION_EXTENDED_HAS_LLA_VRP_BASED;
    }
}

void
GnssAdapter::reportPositionEvent(const UlpLocation& ulpLocation,
                                 const GpsLocationExtended& locationExtended,
                                 enum loc_sess_status status,
                                 LocPosTechMask techMask,
                                 GnssDataNotification* pDataNotify,
                                 int msInWeek)
{
    // this position is from QMI LOC API, then send report to engine hub
    // also, send out SPE fix promptly to the clients that have registered
    // with SPE report
    LOC_LOGd("reportPositionEvent, eng type: %d, unpro %d, sess status %d msInWeek %d",
             locationExtended.locOutputEngType,
             ulpLocation.unpropagatedPosition, status, msInWeek);

    struct MsgReportSPEPosition : public LocMsg {
        GnssAdapter& mAdapter;
        mutable UlpLocation mUlpLocation;
        mutable GpsLocationExtended mLocationExtended;
        enum loc_sess_status mStatus;
        LocPosTechMask mTechMask;
        mutable GnssDataNotification mDataNotify;
        int mMsInWeek;

        inline MsgReportSPEPosition(GnssAdapter& adapter,
                                    const UlpLocation& ulpLocation,
                                    const GpsLocationExtended& locationExtended,
                                    enum loc_sess_status status,
                                    LocPosTechMask techMask,
                                    GnssDataNotification dataNotify,
                                    int msInWeek) :
            LocMsg(),
            mAdapter(adapter),
            mUlpLocation(ulpLocation),
            mLocationExtended(locationExtended),
            mStatus(status),
            mTechMask(techMask),
            mDataNotify(dataNotify),
            mMsInWeek(msInWeek) {}
        inline virtual void proc() const {
            if (mAdapter.mTimeBasedTrackingSessions.empty() &&
                mAdapter.mDistanceBasedTrackingSessions.empty()) {
                LOC_LOGD("MsgReportSPEPosition, no session on-going, "
                         "throw away the SPE reports");
                return;
            }

            if (mDataNotify.size != 0) {
                if (mMsInWeek >= 0) {
                    mAdapter.getDataInformation((GnssDataNotification&)mDataNotify,
                                                mMsInWeek);
                }
                mAdapter.reportData(mDataNotify);
            }

            // save the association of GPS timestamp and qtimer tick cnt in PVT report
            mAdapter.mPositionElapsedRealTimeCal
                    .saveGpsTimeAndQtimerPairInPvtReport(mLocationExtended);

            // save sv used in fix and mb sv used in fix info from propagated report
            mAdapter.mGnssSvIdUsedInPosAvail = false;
            mAdapter.mGnssMbSvIdUsedInPosAvail = false;
            if (mLocationExtended.flags & GPS_LOCATION_EXTENDED_HAS_GNSS_SV_USED_DATA) {
                mAdapter.mGnssSvIdUsedInPosAvail = true;
                mAdapter.mGnssSvIdUsedInPosition = mLocationExtended.gnss_sv_used_ids;
                if (mLocationExtended.flags & GPS_LOCATION_EXTENDED_HAS_MULTIBAND) {
                    mAdapter.mGnssMbSvIdUsedInPosAvail = true;
                    mAdapter.mGnssMbSvIdUsedInPosition = mLocationExtended.gnss_mb_sv_used_ids;
                }
            }

            // obtain the VRP based latitude/longitude/altitude for SPE fix
            mAdapter.computeVRPBasedLla(mUlpLocation,
                    mLocationExtended, mAdapter.mLocConfigInfo.leverArmConfigInfo);

            if (GPS_LOCATION_DESIRED_FLAGS ==
                    (mUlpLocation.gpsLocation.flags & (GPS_LOCATION_DESIRED_FLAGS)) &&
                   ((int)mUlpLocation.gpsLocation.accuracy < 15000)) {
                if (loadLocSlatePUNCModel && mAdapter.mLocGlinkProxy) {
                    LOC_LOGA("reportPositionEvent, inject location to slate");
                    mAdapter.mLocGlinkProxy->injectLocation(mUlpLocation.gpsLocation);
                }
            }

            if (!mAdapter.reportSpeAsEnginePosition(mUlpLocation, mLocationExtended, mStatus)){
                // extract bug report info - this returns true if consumed by systemstatus
                SystemStatus* s = mAdapter.getSystemStatus();
                if ((nullptr != s) &&
                    ((LOC_SESS_SUCCESS == mStatus) || (LOC_SESS_INTERMEDIATE == mStatus))){
                    s->eventPosition(mUlpLocation, mLocationExtended);
                }

                mAdapter.reportPosition(mUlpLocation, mLocationExtended, mStatus, mTechMask);
                mAdapter.reportPositionNmea(mUlpLocation, mLocationExtended, mStatus, mTechMask);
            }
        }
    };

    // some position engine requires the QMI order of PVT report and SV measurement
    // report to be preserved. So, send out both SV measurement report and PVT report
    // directly to engine hub
    mEngHubProxy->gnssReportPosition(ulpLocation, locationExtended, status);

    // unpropagated report: is only for engine hub to consume and no need
    // to send out to the clients
    if (!ulpLocation.unpropagatedPosition) {
        GnssDataNotification dataNotifyCopy = {};
        uint64_t pvtReportTimeDelta = 0ULL;

        if (pDataNotify) {
            dataNotifyCopy = *pDataNotify;
            dataNotifyCopy.size = sizeof(dataNotifyCopy);
        }

        if (locationExtended.isReportTimeAccurate()) {
#define NSEC_IN_ONE_MSEC 1000000ULL

            uint64_t hlosQtimerTick = getQTimerTickCount();

            // locationExtended.systemTick contains the PVT applicable time,
            // if it is in the future, do not report it promptly
            if (locationExtended.systemTick > hlosQtimerTick) {
                uint64_t hlosQtimerNanos = qTimerTicksToNanos(double(hlosQtimerTick));
                uint64_t gpsPvtApplicableNanos =
                        qTimerTicksToNanos(double(locationExtended.systemTick));
                pvtReportTimeDelta =
                        (gpsPvtApplicableNanos - hlosQtimerNanos)/NSEC_IN_ONE_MSEC;
            }
        }

        MsgReportSPEPosition* pLocMsg = new MsgReportSPEPosition(*this, ulpLocation,
                locationExtended, status, techMask, dataNotifyCopy, msInWeek);
        sendMsg((const LocMsg*)pLocMsg, (uint32_t)pvtReportTimeDelta);
    }
}
void
GnssAdapter::reportPropogatedPuncEvent(LocGpsLocation location)
{
    struct MsgReportPropogatedPuncEvent : public LocMsg {
        LocApiBase& mApi;
        ContextBase& mContext;
        LocGpsLocation mLocation;
        inline MsgReportPropogatedPuncEvent(LocApiBase& api,
                                            ContextBase& context,
                                            LocGpsLocation location) :
            LocMsg(),
            mApi(api),
            mContext(context),
            mLocation(location) {
        }

        inline virtual void proc() const {
            LOC_LOGA("inject propogated SLATE punc:%lf %lf %f",
                     mLocation.latitude,
                     mLocation.longitude,
                     mLocation.accuracy);
            mApi.injectPosition(mLocation.latitude, mLocation.longitude, mLocation.accuracy, false);
        }
    };

    sendMsg(new MsgReportPropogatedPuncEvent(*mLocApi, *mContext, location));
}

void
GnssAdapter::reportEnginePositionsEvent(unsigned int count,
                                        EngineLocationInfo* locationArr)
{
    struct MsgReportEnginePositions : public LocMsg {
        GnssAdapter& mAdapter;
        unsigned int mCount;
        EngineLocationInfo mEngLocInfo[LOC_OUTPUT_ENGINE_COUNT];
        inline MsgReportEnginePositions(GnssAdapter& adapter,
                                        unsigned int count,
                                        EngineLocationInfo* locationArr) :
            LocMsg(),
            mAdapter(adapter),
            mCount(count) {
            if (mCount > LOC_OUTPUT_ENGINE_COUNT) {
                mCount = LOC_OUTPUT_ENGINE_COUNT;
            }
            if (mCount > 0) {
                memcpy(mEngLocInfo, locationArr, sizeof(EngineLocationInfo)*mCount);
            }
        }
        inline virtual void proc() const {
            mAdapter.reportEnginePositions(mCount, mEngLocInfo);
        }
    };

    if (isPreciseEnabled()) {
        sendMsg(new MsgReportEnginePositions(*this, count, locationArr));
    }
}

bool
GnssAdapter::needReportForAllClients(const UlpLocation& ulpLocation,
                                     enum loc_sess_status status,
                                     LocPosTechMask techMask) {
    bool reported = false;

#ifdef USE_GLIB
    if (true == isPreciseEnabled()) {
        reported = true;
    }
#endif

    return reported || LocApiBase::needReport(ulpLocation, status, techMask);
}

bool GnssAdapter::needReportForClient(LocationAPI* client, enum loc_sess_status status) {
    if (LOC_SESS_SUCCESS == status || (client == nullptr && LOC_SESS_INTERMEDIATE == status &&
                mDistanceBasedTrackingSessions.size() > 0)) {
        return true;
    }
    if (status != LOC_SESS_FAILURE) {
        for (auto it = mDistanceBasedTrackingSessions.begin();
                it != mDistanceBasedTrackingSessions.end(); ++it) {
            if (it->first.client == client) { // Always report intermediate fixes to dbt clients
                return true;
            }
        }
    }
    for (auto it = mTimeBasedTrackingSessions.begin();
            it != mTimeBasedTrackingSessions.end(); ++it) {
        // report intermediate fix when TBT session allows, like flp;
        // report any fix (even failed fix) when TBT session allows, like LE.
        if ((it->first.client == client || client == nullptr) &&
                it->second.qualityLevelAccepted >= status) {
            return true;
        }
    }
    return false;
}

/** Y2038- Compliant */
bool GnssAdapter::needToGenerateNmeaReport(const uint32_t &gpsTimeOfWeekMs,
        const struct timespec64_t &apTimeStamp)
{
    bool retVal = false;
    uint64_t currentTimeNsec = 0;

    if (NMEA_PROVIDER_AP == ContextBase::mGps_conf.NMEA_PROVIDER &&
            !mTimeBasedTrackingSessions.empty()) {
        currentTimeNsec = (apTimeStamp.tv_sec * BILLION_NSEC + apTimeStamp.tv_nsec);
        if ((GNSS_NMEA_REPORT_RATE_NHZ == ContextBase::sNmeaReportRate) ||
                (GPS_DEFAULT_FIX_INTERVAL_MS <= mLocPositionMode.min_interval)) {
            retVal = true;
        } else { /*tbf is less than 1000 milli-seconds and NMEA reporting rate is set to 1Hz */
            /* Always send NMEA string for first position report
             * Send when gpsTimeOfWeekMs is closely aligned with integer boundary
             */
            if ((0 == mPrevNmeaRptTimeNsec) ||
                ((0 != gpsTimeOfWeekMs) && (NMEA_MIN_THRESHOLD_MSEC >= (gpsTimeOfWeekMs % 1000)))) {
                retVal = true;
            } else {
                int64_t timeDiffMsec = ((currentTimeNsec - mPrevNmeaRptTimeNsec) / 1000000);
                // Send when the delta time becomes >= 1 sec
                if (NMEA_MAX_THRESHOLD_MSEC <= timeDiffMsec) {
                    retVal = true;
                }
            }
        }
        if (true == retVal) {
            mPrevNmeaRptTimeNsec = currentTimeNsec;
        }
    }
    return retVal;
}

bool GnssAdapter::needReportEnginePosition()
{
    bool needReportEnginePosition = false;
    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (nullptr != it->second.engineLocationsInfoCb) {
            needReportEnginePosition = true;
            break;
        }
    }
    return needReportEnginePosition;
}

void GnssAdapter::notifyPreciseLocation() {
    bool enable = isEngineServiceEnable();
    getSystemStatus()->eventPreciseLocation(enable);
    updateClientsEventMask();
    setTribandState();
}

void
GnssAdapter::logLatencyInfo()
{
    if (0 == mGnssLatencyInfoQueue.size()) {
        return;
    }
    mGnssLatencyInfoQueue.front().hlosQtimer5 = getQTimerTickCount();
    if (0 == mGnssLatencyInfoQueue.front().hlosQtimer3) {
        /* if SPE from engine hub is not reported then hlosQtimer3 = 0, set it
        equal to hlosQtimer2 to make sense */
        mGnssLatencyInfoQueue.front().hlosQtimer3 = mGnssLatencyInfoQueue.front().hlosQtimer2;
    }
    if (0 == mGnssLatencyInfoQueue.front().hlosQtimer4) {
        /* if PPE from engine hub is not reported then hlosQtimer4 = 0, set it
        equal to hlosQtimer3 to make sense */
        mGnssLatencyInfoQueue.front().hlosQtimer4 = mGnssLatencyInfoQueue.front().hlosQtimer3;
    }
    if (mGnssLatencyInfoQueue.front().hlosQtimer4 < mGnssLatencyInfoQueue.front().hlosQtimer3) {
        /* hlosQtimer3 is timestamped when SPE from engine hub is reported,
        and hlosQtimer4 is timestamped when PPE from engine hub is reported.
        The order is random though, hence making sure the timestamps are sorted */
        std::swap(mGnssLatencyInfoQueue.front().hlosQtimer3,
                  mGnssLatencyInfoQueue.front().hlosQtimer4);
    }
    LOC_LOGa("meQtimer1=%" PRIi64 " "
             "meQtimer2=%" PRIi64 " "
             "meQtimer3=%" PRIi64 " "
             "peQtimer1=%" PRIi64 " "
             "peQtimer2=%" PRIi64 " "
             "peQtimer3=%" PRIi64 " "
             "smQtimer1=%" PRIi64 " "
             "smQtimer2=%" PRIi64 " "
             "smQtimer3=%" PRIi64 " "
             "locMwQtimer=%" PRIi64 " "
             "hlosQtimer1=%" PRIi64 " "
             "hlosQtimer2=%" PRIi64 " "
             "hlosQtimer3=%" PRIi64 " "
             "hlosQtimer4=%" PRIi64 " "
             "hlosQtimer5=%" PRIi64 " ",
             mGnssLatencyInfoQueue.front().meQtimer1, mGnssLatencyInfoQueue.front().meQtimer2,
             mGnssLatencyInfoQueue.front().meQtimer3, mGnssLatencyInfoQueue.front().peQtimer1,
             mGnssLatencyInfoQueue.front().peQtimer2, mGnssLatencyInfoQueue.front().peQtimer3,
             mGnssLatencyInfoQueue.front().smQtimer1, mGnssLatencyInfoQueue.front().smQtimer2,
             mGnssLatencyInfoQueue.front().smQtimer3, mGnssLatencyInfoQueue.front().locMwQtimer,
             mGnssLatencyInfoQueue.front().hlosQtimer1, mGnssLatencyInfoQueue.front().hlosQtimer2,
             mGnssLatencyInfoQueue.front().hlosQtimer3, mGnssLatencyInfoQueue.front().hlosQtimer4,
             mGnssLatencyInfoQueue.front().hlosQtimer5);
    mLogger.log(mGnssLatencyInfoQueue.front());
    mGnssLatencyInfoQueue.pop();
}

LocReqEngineTypeMask convertEngTypeToEngMask(const LocOutputEngineType &engType) {
    LocReqEngineTypeMask engMask = LOC_REQ_ENGINE_FUSED_BIT;
    switch (engType) {
    case LOC_OUTPUT_ENGINE_FUSED:
        engMask = LOC_REQ_ENGINE_FUSED_BIT;
        break;
    case LOC_OUTPUT_ENGINE_SPE:
        engMask = LOC_REQ_ENGINE_SPE_BIT;
        break;
    case LOC_OUTPUT_ENGINE_PPE:
        engMask = LOC_REQ_ENGINE_PPE_BIT;
        break;
    case LOC_OUTPUT_ENGINE_VPE:
        engMask = LOC_REQ_ENGINE_VPE_BIT;
        break;
    default:
        break;
    }
    return engMask;
}

void GnssAdapter::reportNmeaArray(std::vector<std::string>& nmeaArraystr,
                                  LocOutputEngineType engineType,
                                  bool isSvNmea) {
    stringstream ss;
    for (auto itor = nmeaArraystr.begin(); itor != nmeaArraystr.end(); ++itor) {
        ss << *itor;
    }
    string s = ss.str();
    reportNmea(s.c_str(), s.length(), engineType, isSvNmea);
}

void GnssAdapter::reportPositionNmea(const UlpLocation& ulpLocation,
                                     const GpsLocationExtended& locationExtended,
                                     enum loc_sess_status status,
                                     LocPosTechMask techMask) {
    bool reportToAllClients = needReportForAllClients(ulpLocation, status, techMask);
    bool needReportEngineNmea = false;
    bool needReportGnssNmea = false;
    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (nullptr != it->second.engineNmeaCb) {
            needReportEngineNmea = true;
            break;
        }
    }
    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (nullptr != it->second.gnssNmeaCb) {
            needReportGnssNmea = true;
            break;
        }
    }
    if (needToGenerateNmeaReport(locationExtended.gpsTime.gpsTimeOfWeekMs,
            locationExtended.timeStamp.apTimeStamp)) {
        /*Only BlankNMEA sentence needs to be processed and sent, if both lat, long is 0 &
          horReliability is not set. */

        bool blank_fix = ((0 == ulpLocation.gpsLocation.latitude) &&
                          (0 == ulpLocation.gpsLocation.longitude) &&
                          (LOC_RELIABILITY_NOT_SET == locationExtended.horizontal_reliability));
        uint8_t generate_nmea = (reportToAllClients && LOC_SESS_SUCCESS == status && !blank_fix);
        bool custom_nmea_gga = (1 == ContextBase::mGps_conf.CUSTOM_NMEA_GGA_FIX_QUALITY_ENABLED);
        bool isTagBlockGroupingEnabled =
                (1 == ContextBase::mGps_conf.NMEA_TAG_BLOCK_GROUPING_ENABLED);
        std::vector<std::string> nmeaArraystr;
        int indexOfGGA = -1;
        bool nmeaGenerated = false;
        if (needReportEngineNmea || needReportGnssNmea) {
            loc_nmea_generate_pos(ulpLocation, locationExtended, mLocSystemInfo, generate_nmea,
                     custom_nmea_gga, nmeaArraystr, indexOfGGA, isTagBlockGroupingEnabled);
            nmeaGenerated = true;
            if (false == isPreciseEnabled()) {
                if (mNmeaReqEngTypeMask & LOC_REQ_ENGINE_FUSED_BIT) {
                    reportNmeaArray(nmeaArraystr, LOC_OUTPUT_ENGINE_FUSED, false);
                }
                if (mNmeaReqEngTypeMask & LOC_REQ_ENGINE_SPE_BIT) {
                    reportNmeaArray(nmeaArraystr, LOC_OUTPUT_ENGINE_SPE, false);
                }
            } else {
                LocReqEngineTypeMask engMask = convertEngTypeToEngMask(
                        locationExtended.locOutputEngType);
                if (mNmeaReqEngTypeMask & engMask) {
                    reportNmeaArray(nmeaArraystr, locationExtended.locOutputEngType, false);
                }
            }
        }
        /* DgnssNtrip */
        if (isDgnssNmeaRequired()) {
            if (!nmeaGenerated && (LOC_OUTPUT_ENGINE_SPE == locationExtended.locOutputEngType)) {
                loc_nmea_generate_pos(ulpLocation, locationExtended, mLocSystemInfo, generate_nmea,
                     custom_nmea_gga, nmeaArraystr, indexOfGGA, isTagBlockGroupingEnabled);
            }
            if (-1 != indexOfGGA) {
                mDgnssState |= DGNSS_STATE_NO_NMEA_PENDING;
                mStartDgnssNtripParams.nmea = std::move(nmeaArraystr[indexOfGGA]);
                bool isLocationValid = (0 != ulpLocation.gpsLocation.latitude) ||
                        (0 != ulpLocation.gpsLocation.longitude);
                checkUpdateDgnssNtrip(isLocationValid);
            }
        }
    }
}

// only fused report (when engine hub is enabled) or
// SPE report (when engine hub is disabled) will reach this function
void
GnssAdapter::reportPosition(const UlpLocation& ulpLocation,
                            const GpsLocationExtended& locationExtended,
                            enum loc_sess_status status,
                            LocPosTechMask techMask)
{
    bool reportToAllClients = needReportForAllClients(ulpLocation, status, techMask);
    bool reportToAnyClient = needReportForAnyClient(status);

    LOC_LOGd("reportToAllClients %d, reportToAnyClient %d, status %d, eng type %d, "
             "precise location enabled %d",
             reportToAllClients, reportToAnyClient, status,
             locationExtended.locOutputEngType, isPreciseEnabled());

    if (reportToAllClients || reportToAnyClient) {
        GnssLocationInfoNotification locationInfo = {};
        list<trackingCallback> cbRunnables;
        convertLocationInfo(locationInfo, locationExtended, status);
        convertLocation(locationInfo.location, ulpLocation, locationExtended, status);
        fillElapsedRealTime(locationExtended, locationInfo);
        logLatencyInfo();

        for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
            if (reportToAllClients || needReportForClient(it->first, status)) {
                if (nullptr != it->second.gnssLocationInfoCb) {
                    it->second.gnssLocationInfoCb(locationInfo);
                } else if ((nullptr != it->second.engineLocationsInfoCb) &&
                        (false == isPreciseEnabled())) {
                    // if engine hub is disabled, this is SPE fix from modem
                    // we need to have one copy marked as fused and leave the other copy
                    // unmodified (which is marked as SPE fix in LocAPIV02.cpp) and
                    // dispatch both copies to the engineLocationsInfoCb
                    GnssLocationInfoNotification engLocationsInfo[2];
                    engLocationsInfo[0] = locationInfo;
                    engLocationsInfo[0].locOutputEngType = LOC_OUTPUT_ENGINE_FUSED;
                    engLocationsInfo[0].flags |= LDT_GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT;
                    engLocationsInfo[1] = locationInfo;
                    it->second.engineLocationsInfoCb(2, engLocationsInfo);
                } else if (nullptr != it->second.trackingCb) {
                    if (reportToAnyClient) {
                        cbRunnables.emplace_back([ cb=it->second.trackingCb ] (Location location) {
                            cb(location);
                        });
                    }
                }
            }
        }

        if (cbRunnables.size() > 0) {
            mContext->getLBSProxyBase()->populateAltitudeAndBroadCast(locationInfo.location,
                [ cbRunnables ] (Location location) {
                    for (auto cb : cbRunnables) {
                        cb(location);
                    }
                });
        }

        if (reportToAllClients) {
            // if PACE is enabled
            if ((true == mLocConfigInfo.paceConfigInfo.isValid) &&
                (true == mLocConfigInfo.paceConfigInfo.enable)) {
                // If fix has sensor contribution, and it is fused fix with DRE engine
                // contributing to the fix, inject to modem
                if ((LOC_POS_TECH_MASK_SENSORS & techMask) &&
                        (locationInfo.flags & LDT_GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT) &&
                        (locationInfo.locOutputEngType == LOC_OUTPUT_ENGINE_FUSED) &&
                        (locationInfo.flags & LDT_GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT) &&
                        (locationInfo.locOutputEngMask & DEAD_RECKONING_ENGINE)) {
                    mLocApi->injectPosition(locationInfo, false);
                }
            }
        }
    }
}

void GnssAdapter::reportEngDebugDataInfo(const GnssEngineDebugDataInfo& gnssEngineDebugDataInfo) {
    if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_ENGINE_DEBUG_DATA)) {
        LOC_LOGd("Recived Engine debug data info");
        SystemStatus* s = getSystemStatus();
        if (nullptr != s) {
            s->setEngineDebugDataInfo(gnssEngineDebugDataInfo);
        }
    }
}

void GnssAdapter::reportEngDebugDataInfoEvent(GnssEngineDebugDataInfo& gnssEngineDebugDataInfo) {
    struct MsgReportEngDebugDataInfo : public LocMsg {
        GnssAdapter& mAdapter;
        const GnssEngineDebugDataInfo mGnssEngineDebugDataInfo;
        inline MsgReportEngDebugDataInfo(GnssAdapter& adapter, GnssEngineDebugDataInfo&
            gnssEngineDebugDataInfo) : mGnssEngineDebugDataInfo(gnssEngineDebugDataInfo),
                mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter.reportEngDebugDataInfo(mGnssEngineDebugDataInfo);
        }
    };
    sendMsg(new MsgReportEngDebugDataInfo(*this, gnssEngineDebugDataInfo));
}

void
GnssAdapter::reportLatencyInfoEvent(const GnssLatencyInfo& gnssLatencyInfo) {
    struct MsgReportLatencyInfo : public LocMsg {
        GnssAdapter& mAdapter;
        GnssLatencyInfo mGnssLatencyInfo;
        inline MsgReportLatencyInfo(GnssAdapter& adapter,
            const GnssLatencyInfo& gnssLatencyInfo) :
            mGnssLatencyInfo(gnssLatencyInfo),
            mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter.mGnssLatencyInfoQueue.push(mGnssLatencyInfo);
        }
    };
    sendMsg(new MsgReportLatencyInfo(*this, gnssLatencyInfo));
}

bool
GnssAdapter::reportSpeAsEnginePosition(const UlpLocation& ulpLocation,
                                   const GpsLocationExtended& locationExtended,
                                   enum loc_sess_status status) {
    bool enginePositionReported = isPreciseEnabled();
    if (enginePositionReported) {
        EngineLocationInfo engLocationInfo = {};
        engLocationInfo.location = ulpLocation;
        engLocationInfo.locationExtended = locationExtended;
        engLocationInfo.sessionStatus = status;

        enginePositionReported = reportEnginePositions(1, &engLocationInfo);
    }
    return enginePositionReported;
}

bool
GnssAdapter::reportEnginePositions(unsigned int count,
                                   const EngineLocationInfo* locationArr) {
    bool isPrecisePositioningEnabled = isPreciseEnabled();
    if (isPrecisePositioningEnabled) {
        bool needReportEnginePositions = needReportEnginePosition();
        GnssLocationInfoNotification locationInfo[LOC_OUTPUT_ENGINE_COUNT] = {};
        for (unsigned int i = 0; i < count; i++) {
            const EngineLocationInfo* engLocation = (locationArr+i);
            // if it is fused/default location, call reportPosition maintain legacy behavior
            if ((GPS_LOCATION_EXTENDED_HAS_OUTPUT_ENG_TYPE & engLocation->locationExtended.flags) &&
                (LOC_OUTPUT_ENGINE_FUSED == engLocation->locationExtended.locOutputEngType)) {
                reportPosition(engLocation->location,
                               engLocation->locationExtended,
                               engLocation->sessionStatus,
                               engLocation->location.tech_mask);
            }

            if (needReportEnginePositions) {
                convertLocationInfo(locationInfo[i], engLocation->locationExtended,
                                    engLocation->sessionStatus);
                convertLocation(locationInfo[i].location,
                                engLocation->location,
                                engLocation->locationExtended,
                                engLocation->sessionStatus);
                fillElapsedRealTime(engLocation->locationExtended,
                                    locationInfo[i]);
            }

            reportPositionNmea(engLocation->location,
                           engLocation->locationExtended,
                           engLocation->sessionStatus,
                           engLocation->location.tech_mask);

       }

        const EngineLocationInfo* engLocation = locationArr;

        if (0 != mGnssLatencyInfoQueue.size()) {
            if ((GPS_LOCATION_EXTENDED_HAS_OUTPUT_ENG_TYPE & engLocation->locationExtended.flags) &&
                (LOC_OUTPUT_ENGINE_SPE == engLocation->locationExtended.locOutputEngType)) {
                mGnssLatencyInfoQueue.front().hlosQtimer3 = getQTimerTickCount();
            }
            if ((GPS_LOCATION_EXTENDED_HAS_OUTPUT_ENG_TYPE & engLocation->locationExtended.flags) &&
                (LOC_OUTPUT_ENGINE_PPE == engLocation->locationExtended.locOutputEngType)) {
                mGnssLatencyInfoQueue.front().hlosQtimer4 = getQTimerTickCount();
            }
        }
        if (needReportEnginePositions) {
            for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
                if ((nullptr != it->second.engineLocationsInfoCb) &&
                    (needReportForClient(it->first, engLocation->sessionStatus))) {
                    it->second.engineLocationsInfoCb(count, locationInfo);
                }
            }
        }
    }
    return isPrecisePositioningEnabled;
}

void
GnssAdapter::reportSvEvent(const GnssSvNotification& svNotify)
{
    struct MsgReportSv : public LocMsg {
        GnssAdapter& mAdapter;
        const GnssSvNotification mSvNotify;
        inline MsgReportSv(GnssAdapter& adapter,
                           const GnssSvNotification& svNotify) :
            LocMsg(),
            mAdapter(adapter),
            mSvNotify(svNotify) {}
        inline virtual void proc() const {
            mAdapter.reportSv((GnssSvNotification&)mSvNotify);
        }
    };

    sendMsg(new MsgReportSv(*this, svNotify));
}

void
GnssAdapter::reportSv(GnssSvNotification& svNotify)
{
    int numSv = svNotify.count;
    uint16_t gnssSvId = 0;
    uint64_t svUsedIdMask = 0;

    for (int i=0; i < numSv; i++) {
        svUsedIdMask = 0;
        gnssSvId = svNotify.gnssSvs[i].svId;
        GnssSignalTypeMask signalTypeMask = svNotify.gnssSvs[i].gnssSignalTypeMask;
        switch (svNotify.gnssSvs[i].type) {
            case GNSS_SV_TYPE_GPS:
                if (mGnssSvIdUsedInPosAvail) {
                    if (mGnssMbSvIdUsedInPosAvail) {
                        switch (signalTypeMask) {
                        case GNSS_SIGNAL_GPS_L1CA:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.gps_l1ca_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_GPS_L1C:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.gps_l1c_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_GPS_L2:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.gps_l2_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_GPS_L5:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.gps_l5_sv_used_ids_mask;
                            break;
                        }
                    } else {
                        svUsedIdMask = mGnssSvIdUsedInPosition.gps_sv_used_ids_mask;
                    }
                }
                break;
            case GNSS_SV_TYPE_GLONASS:
                if (mGnssSvIdUsedInPosAvail) {
                    if (mGnssMbSvIdUsedInPosAvail) {
                        switch (signalTypeMask) {
                        case GNSS_SIGNAL_GLONASS_G1:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.glo_g1_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_GLONASS_G2:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.glo_g2_sv_used_ids_mask;
                            break;
                        }
                    } else {
                        svUsedIdMask = mGnssSvIdUsedInPosition.glo_sv_used_ids_mask;
                    }
                }
                // map the svid to respective constellation range 1..xx
                // then repective constellation svUsedIdMask map correctly to svid
                gnssSvId = gnssSvId - GLO_SV_PRN_MIN + 1;
                break;
            case GNSS_SV_TYPE_BEIDOU:
                if (mGnssSvIdUsedInPosAvail) {
                    if (mGnssMbSvIdUsedInPosAvail) {
                        switch (signalTypeMask) {
                        case GNSS_SIGNAL_BEIDOU_B1I:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.bds_b1i_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B1C:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.bds_b1c_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B2I:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.bds_b2i_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B2AI:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.bds_b2ai_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B2AQ:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.bds_b2aq_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B2BI:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.bds_b2bi_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B2BQ:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.bds_b2bq_sv_used_ids_mask;
                            break;
                        }
                    } else {
                        svUsedIdMask = mGnssSvIdUsedInPosition.bds_sv_used_ids_mask;
                    }
                }
                gnssSvId = gnssSvId - BDS_SV_PRN_MIN + 1;
                break;
            case GNSS_SV_TYPE_GALILEO:
                if (mGnssSvIdUsedInPosAvail) {
                    if (mGnssMbSvIdUsedInPosAvail) {
                        switch (signalTypeMask) {
                        case GNSS_SIGNAL_GALILEO_E1:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.gal_e1_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_GALILEO_E5A:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.gal_e5a_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_GALILEO_E5B:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.gal_e5b_sv_used_ids_mask;
                            break;
                        }
                    } else {
                        svUsedIdMask = mGnssSvIdUsedInPosition.gal_sv_used_ids_mask;
                    }
                }
                gnssSvId = gnssSvId - GAL_SV_PRN_MIN + 1;
                break;
            case GNSS_SV_TYPE_QZSS:
                if (mGnssSvIdUsedInPosAvail) {
                    if (mGnssMbSvIdUsedInPosAvail) {
                        switch (signalTypeMask) {
                        case GNSS_SIGNAL_QZSS_L1CA:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.qzss_l1ca_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_QZSS_L1S:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.qzss_l1s_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_QZSS_L2:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.qzss_l2_sv_used_ids_mask;
                            break;
                        case GNSS_SIGNAL_QZSS_L5:
                            svUsedIdMask = mGnssMbSvIdUsedInPosition.qzss_l5_sv_used_ids_mask;
                            break;
                        }
                    } else {
                        svUsedIdMask = mGnssSvIdUsedInPosition.qzss_sv_used_ids_mask;
                    }
                }
                gnssSvId = gnssSvId - QZSS_SV_PRN_MIN + 1;
                break;
            case GNSS_SV_TYPE_NAVIC:
                if (mGnssSvIdUsedInPosAvail) {
                    svUsedIdMask = mGnssSvIdUsedInPosition.navic_sv_used_ids_mask;
                }
                gnssSvId = gnssSvId - NAVIC_SV_PRN_MIN + 1;
                break;
            default:
                svUsedIdMask = 0;
                break;
        }

        // If SV ID was used in previous position fix, then set USED_IN_FIX
        // flag, else clear the USED_IN_FIX flag.
        if (svFitsMask(svUsedIdMask, gnssSvId) && (svUsedIdMask & (1ULL << (gnssSvId - 1)))) {
            svNotify.gnssSvs[i].gnssSvOptionsMask |= GNSS_SV_OPTIONS_USED_IN_FIX_BIT;
        }
    }

    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (nullptr != it->second.gnssSvCb) {
            it->second.gnssSvCb(svNotify);
        }
    }

    if (NMEA_PROVIDER_AP == ContextBase::mGps_conf.NMEA_PROVIDER &&
        !mTimeBasedTrackingSessions.empty()) {
        std::vector<std::string> nmeaArraystr;
        LocOutputEngineType engineType = LOC_OUTPUT_ENGINE_SPE;
        loc_nmea_generate_sv(svNotify, nmeaArraystr);
        reportNmeaArray(nmeaArraystr, engineType, true);
    }

    // report to engine hub to deliver to registered plugin
    mEngHubProxy->gnssReportSv(svNotify);
}

void
GnssAdapter::reportNmeaEvent(const char* nmea, size_t length)
{
    if (NMEA_PROVIDER_AP == ContextBase::mGps_conf.NMEA_PROVIDER &&
        !loc_nmea_is_debug(nmea, length)) {
        return;
    }

    struct MsgReportNmea : public LocMsg {
        GnssAdapter& mAdapter;
        const char* mNmea;
        size_t mLength;
        inline MsgReportNmea(GnssAdapter& adapter,
                             const char* nmea,
                             size_t length) :
            LocMsg(),
            mAdapter(adapter),
            mNmea(new char[length+1]),
            mLength(length) {
                if (mNmea == nullptr) {
                    LOC_LOGE("MsgReportNmea, memory alloc failed");
                    return;
                }
                strlcpy((char*)mNmea, nmea, length+1);
            }
        inline virtual ~MsgReportNmea()
        {
            delete[] mNmea;
        }
        inline virtual void proc() const {
            // extract bug report info - this returns true if consumed by systemstatus
            bool ret = false;
            SystemStatus* s = mAdapter.getSystemStatus();
            if (nullptr != s) {
                ret = s->setNmeaString(mNmea, mLength);
            }
            if (false == ret) {
                // forward NMEA message to upper layer
                mAdapter.reportNmea(mNmea, mLength);
                // DgnssNtrip
                mAdapter.reportGGAToNtrip(mNmea);
            }
        }
    };

    sendMsg(new MsgReportNmea(*this, nmea, length));
}

void
GnssAdapter::reportNmea(const char* nmea,
                        size_t length,
                        LocOutputEngineType engineType,
                        bool isSvNmea)
{
    GnssNmeaNotification nmeaNotification = {};
    nmeaNotification.size = sizeof(GnssNmeaNotification);

    struct timeval tv;
    gettimeofday(&tv, (struct timezone *) NULL);
    int64_t now = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    nmeaNotification.timestamp = now;
    nmeaNotification.nmea = nmea;
    nmeaNotification.length = length;
    nmeaNotification.locOutputEngType = engineType;
    nmeaNotification.isSvNmea = isSvNmea;
    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (nullptr != it->second.engineNmeaCb) {
            it->second.engineNmeaCb(nmeaNotification);
        } else if (nullptr != it->second.gnssNmeaCb) {
            it->second.gnssNmeaCb(nmeaNotification);
        }
    }

    if (isNMEAPrintEnabled()) {
        LOC_LOGd("[%" PRId64 ", %zu] %s", now, length, nmea);
    }
}

void
GnssAdapter::reportDataEvent(const GnssDataNotification& dataNotify,
                             int msInWeek)
{
    struct MsgReportData : public LocMsg {
        GnssAdapter& mAdapter;
        GnssDataNotification mDataNotify;
        int mMsInWeek;
        inline MsgReportData(GnssAdapter& adapter,
                             const GnssDataNotification& dataNotify,
                             int msInWeek) :
            LocMsg(),
            mAdapter(adapter),
            mDataNotify(dataNotify),
            mMsInWeek(msInWeek) {
        }
        inline virtual void proc() const {
            if (mMsInWeek >= 0) {
                mAdapter.getDataInformation((GnssDataNotification&)mDataNotify,
                                            mMsInWeek);
            }
            mAdapter.reportData((GnssDataNotification&)mDataNotify);
        }
    };

    sendMsg(new MsgReportData(*this, dataNotify, msInWeek));
}

void
GnssAdapter::reportData(GnssDataNotification& dataNotify)
{
    IF_LOC_LOGA {
        for (int sig = 0; sig < GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES; sig++) {
            if (GNSS_LOC_DATA_JAMMER_IND_BIT ==
                (dataNotify.gnssDataMask[sig] & GNSS_LOC_DATA_JAMMER_IND_BIT)) {
                LOC_LOGA("jammerInd[%d]=%f", sig, dataNotify.jammerInd[sig]);
            }
            if (GNSS_LOC_DATA_AGC_BIT ==
                (dataNotify.gnssDataMask[sig] & GNSS_LOC_DATA_AGC_BIT)) {
                LOC_LOGA("agc[%d]=%f", sig, dataNotify.agc[sig]);
            }
        }
    }
    for (auto it = mClientData.begin(); it != mClientData.end(); ++it) {
        if (nullptr != it->second.gnssDataCb) {
            it->second.gnssDataCb(dataNotify);
        }
    }
}

bool
GnssAdapter::requestNiNotifyEvent(const GnssNiNotification &notify, const void* data,
                                  const LocInEmergency emergencyState)
{
    LOC_LOGi("notif_type: %d, timeout: %d, default_resp: %d"
             "requestor_id: %s (encoding: %d) text: %s text (encoding: %d) extras: %s "
             "emergencyState = %d",
             notify.type, notify.timeout, notify.timeoutResponse,
             notify.requestor, notify.requestorEncoding,
             notify.message, notify.messageEncoding, notify.extras,
             emergencyState);

    struct MsgReportNiNotify : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        const GnssNiNotification mNotify;
        const void* mData;
        const LocInEmergency mEmergencyState;
        inline MsgReportNiNotify(GnssAdapter& adapter,
                                 LocApiBase& api,
                                 const GnssNiNotification& notify,
                                 const void* data,
                                 const LocInEmergency emergencyState) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mNotify(notify),
            mData(data),
            mEmergencyState(emergencyState) {}
        inline virtual void proc() const {
            bool bIsInEmergency = false;
            bool bInformNiAccept = false;

            bIsInEmergency = ((LOC_IN_EMERGENCY_UNKNOWN == mEmergencyState) && // older modems
                    (mAdapter.getE911State(mNotify.type))) ||
                    (LOC_IN_EMERGENCY_SET == mEmergencyState); // newer modems

            if ((mAdapter.mSupportNfwControl || 0 == mAdapter.getAfwControlId()) &&
                (GNSS_NI_TYPE_SUPL == mNotify.type || GNSS_NI_TYPE_EMERGENCY_SUPL == mNotify.type)
                && !bIsInEmergency &&
                !(GNSS_NI_OPTIONS_PRIVACY_OVERRIDE_BIT & mNotify.options) &&
                (GNSS_CONFIG_GPS_LOCK_NFW_SUPL & ContextBase::mGps_conf.GPS_LOCK) &&
                1 == ContextBase::mGps_conf.NI_SUPL_DENY_ON_NFW_LOCKED) {
                /* If all these conditions are TRUE, then deny the NI Request:
                -'Q' Lock behavior OR 'P' Lock behavior and GNSS is Locked
                -NI SUPL Request type or NI SUPL Emergency Request type
                -NOT in an Emergency Call Session
                -NOT Privacy Override option
                -NFW is locked and config item NI_SUPL_DENY_ON_NFW_LOCKED = 1 */
                mApi.informNiResponse(GNSS_NI_RESPONSE_DENY, mData);
            } else if ((GNSS_NI_TYPE_SUPL == mNotify.type ||
                        GNSS_NI_TYPE_EMERGENCY_SUPL == mNotify.type)
                      && (GNSS_NI_OPTIONS_PRIVACY_OVERRIDE_BIT & mNotify.options)) {
                mApi.informNiResponse(GNSS_NI_RESPONSE_ACCEPT, mData);
            } else if (GNSS_NI_TYPE_EMERGENCY_SUPL == mNotify.type) {
                bInformNiAccept = bIsInEmergency ||
                        (GNSS_CONFIG_SUPL_EMERGENCY_SERVICES_NO == ContextBase::mGps_conf.SUPL_ES);

                if (bInformNiAccept) {
                    mAdapter.requestNiNotify(mNotify, mData, bInformNiAccept);
                } else {
                    mApi.informNiResponse(GNSS_NI_RESPONSE_DENY, mData);
                }
            } else if (GNSS_NI_TYPE_CONTROL_PLANE == mNotify.type) {
                if (bIsInEmergency && (1 == ContextBase::mGps_conf.CP_MTLR_ES)) {
                    mApi.informNiResponse(GNSS_NI_RESPONSE_ACCEPT, mData);
                } else {
                    mAdapter.requestNiNotify(mNotify, mData, false);
                }
            } else {
                mAdapter.requestNiNotify(mNotify, mData, false);
            }
        }
    };

    sendMsg(new MsgReportNiNotify(*this, *mLocApi, notify, data, emergencyState));

    return true;
}

void
GnssAdapter::reportLocationSystemInfoEvent(const LocationSystemInfo & locationSystemInfo) {

    // send system info to engine hub
    mEngHubProxy->gnssReportSystemInfo(locationSystemInfo);

    struct MsgLocationSystemInfo : public LocMsg {
        GnssAdapter& mAdapter;
        LocationSystemInfo mSystemInfo;
        inline MsgLocationSystemInfo(GnssAdapter& adapter,
            const LocationSystemInfo& systemInfo) :
            LocMsg(),
            mAdapter(adapter),
            mSystemInfo(systemInfo) {}
        inline virtual void proc() const {
            mAdapter.reportLocationSystemInfo(mSystemInfo);
        }
    };

    sendMsg(new MsgLocationSystemInfo(*this, locationSystemInfo));
}

void
GnssAdapter::reportLocationSystemInfo(const LocationSystemInfo & locationSystemInfo) {
    // save the info into the master copy piece by piece, as other system info
    // may come at different time
    if (locationSystemInfo.systemInfoMask & LOCATION_SYS_INFO_LEAP_SECOND) {
        mLocSystemInfo.systemInfoMask |= LOCATION_SYS_INFO_LEAP_SECOND;

        const LeapSecondSystemInfo &srcLeapSecondSysInfo = locationSystemInfo.leapSecondSysInfo;
        LeapSecondSystemInfo &dstLeapSecondSysInfo = mLocSystemInfo.leapSecondSysInfo;
        if (srcLeapSecondSysInfo.leapSecondInfoMask &
                LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT) {
            dstLeapSecondSysInfo.leapSecondInfoMask |=
                LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT;
            dstLeapSecondSysInfo.leapSecondCurrent = srcLeapSecondSysInfo.leapSecondCurrent;
        }
        // once leap second change event is complete, modem may send up event invalidate the leap
        // second change info while AP is still processing report during leap second transition
        // so, we choose to keep this info around even though it is old
        if (srcLeapSecondSysInfo.leapSecondInfoMask & LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT) {
            dstLeapSecondSysInfo.leapSecondInfoMask |= LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT;
            dstLeapSecondSysInfo.leapSecondChangeInfo = srcLeapSecondSysInfo.leapSecondChangeInfo;
        }
    }

    // we received new info, inform client of the newly received info
    if (locationSystemInfo.systemInfoMask) {
        for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
            if (it->second.locationSystemInfoCb != nullptr) {
                it->second.locationSystemInfoCb(locationSystemInfo);
            }
        }
    }
}

void
GnssAdapter::reportDcMessage(const GnssDcReportInfo& dcReport) {
    LOC_LOGv("received dc report message");
        struct MsgDcReport : public LocMsg {
        GnssAdapter& mAdapter;
        GnssDcReportInfo mDcReport;
        inline MsgDcReport(GnssAdapter& adapter,
                           const GnssDcReportInfo& dcReport) :
            LocMsg(),
            mAdapter(adapter),
            mDcReport(dcReport) {}
        inline virtual void proc() const {
            for (auto it = mAdapter.mClientData.begin(); it != mAdapter.mClientData.end(); ++it) {
                if (it->second.gnssDcReportCb != nullptr) {
                    it->second.gnssDcReportCb(mDcReport);
                }
            }
        }
    };

    sendMsg(new MsgDcReport(*this, dcReport));
}

void
GnssAdapter::reportSignalTypeCapabilities(const GnssCapabNotification& gnssCapabNotification) {
    LOC_LOGv("received SignalTypeCapabilities report message");
    struct MsgSignalTypeReport : public LocMsg {
        GnssAdapter& mAdapter;
        GnssCapabNotification mGnssCapabNotification;
        inline MsgSignalTypeReport(GnssAdapter& adapter,
            const GnssCapabNotification& gnssCapabNotification) :
            LocMsg(),
            mAdapter(adapter),
            mGnssCapabNotification(gnssCapabNotification) {}
        inline virtual void proc() const {
            // cache the data (for VTS only)
            mAdapter.mGnssCapabNotification = mGnssCapabNotification;
            for (auto it = mAdapter.mClientData.begin(); it != mAdapter.mClientData.end(); ++it) {
                if (it->second.gnssSignalTypesCb != nullptr) {
                    LOC_LOGA("MsgSignalTypeReport: calling gnssSignalTypesCb");
                    it->second.gnssSignalTypesCb(mGnssCapabNotification);
                }
            }
        }
    };

    sendMsg(new MsgSignalTypeReport(*this, gnssCapabNotification));
}

static void* niThreadProc(void *args)
{
    NiSession* pSession = (NiSession*)args;
    int rc = 0;          /* return code from pthread calls */

    struct timespec present_time = {};
    struct timespec expire_time = {};

    pthread_mutex_lock(&pSession->tLock);
    /* Calculate absolute expire time */
    clock_gettime(CLOCK_REALTIME, &present_time);
    expire_time.tv_sec  = present_time.tv_sec + pSession->respTimeLeft;
    expire_time.tv_nsec = present_time.tv_nsec;
    LOC_LOGd("time out set for abs time %ld with delay %d sec",
             (long)expire_time.tv_sec, pSession->respTimeLeft);

    while (!pSession->respRecvd) {
        rc = pthread_cond_timedwait(&pSession->tCond,
                                    &pSession->tLock,
                                    &expire_time);
        if (rc == ETIMEDOUT) {
            pSession->resp = GNSS_NI_RESPONSE_NO_RESPONSE;
            LOC_LOGd("time out after valting for specified time. Ret Val %d", rc);
            break;
        }
    }
    LOC_LOGd("Java layer has sent us a user response and return value from "
             "pthread_cond_timedwait = %d pSession->resp is %u", rc, pSession->resp);
    pSession->respRecvd = false; /* Reset the user response flag for the next session*/

    // adding this check to support modem restart, in which case, we need the thread
    // to exit without calling sending data. We made sure that rawRequest is NULL in
    // loc_eng_ni_reset_on_engine_restart()
    GnssAdapter* adapter = pSession->adapter;
    GnssNiResponse resp;
    void* rawRequest = NULL;
    bool sendResponse = false;

    if (NULL != pSession->rawRequest) {
        if (pSession->resp != GNSS_NI_RESPONSE_IGNORE) {
            resp = pSession->resp;
            rawRequest = pSession->rawRequest;
            sendResponse = true;
        } else {
            free(pSession->rawRequest);
        }
        pSession->rawRequest = NULL;
    }
    pthread_mutex_unlock(&pSession->tLock);

    pSession->respTimeLeft = 0;
    pSession->reqID = 0;

    if (sendResponse) {
        adapter->gnssNiResponseCommand(resp, rawRequest);
    }

    return NULL;
}

bool
GnssAdapter::requestNiNotify(const GnssNiNotification& notify, const void* data,
                             const bool bInformNiAccept)
{
    NiSession* pSession = NULL;
    gnssNiCallback gnssNiCb = nullptr;

    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (nullptr != it->second.gnssNiCb) {
            gnssNiCb = it->second.gnssNiCb;
            break;
        }
    }
    if (nullptr == gnssNiCb) {
        if (GNSS_NI_TYPE_EMERGENCY_SUPL == notify.type) {
            if (bInformNiAccept) {
                mLocApi->informNiResponse(GNSS_NI_RESPONSE_ACCEPT, data);
                NiData& niData = getNiData();
                // ignore any SUPL NI non-Es session if a SUPL NI ES is accepted
                if (NULL != niData.session.rawRequest) {
                    pthread_mutex_lock(&niData.session.tLock);
                    niData.session.resp = GNSS_NI_RESPONSE_IGNORE;
                    niData.session.respRecvd = true;
                    pthread_cond_signal(&niData.session.tCond);
                    pthread_mutex_unlock(&niData.session.tLock);
                }
            }
        }
        EXIT_LOG(%s, "no clients with gnssNiCb.");
        return false;
    }

    if (notify.type == GNSS_NI_TYPE_EMERGENCY_SUPL) {
        if (NULL != mNiData.sessionEs.rawRequest) {
            LOC_LOGi("supl es NI in progress, new supl es NI ignored, type: %d",
                     notify.type);
            if (NULL != data) {
                free((void*)data);
            }
        } else {
            pSession = &mNiData.sessionEs;
        }
    } else {
        if (NULL != mNiData.session.rawRequest ||
            NULL != mNiData.sessionEs.rawRequest) {
            LOC_LOGi("supl NI in progress, new supl NI ignored, type: %d",
                     notify.type);
            if (NULL != data) {
                free((void*)data);
            }
        } else {
            pSession = &mNiData.session;
        }
    }

    if (pSession) {
        /* Save request */
        pSession->rawRequest = (void*)data;
        pSession->reqID = ++mNiData.reqIDCounter;
        pSession->adapter = this;

        int sessionId = pSession->reqID;

        /* For robustness, spawn a thread at this point to timeout to clear up the notification
         * status, even though the OEM layer in java does not do so.
         **/
        pSession->respTimeLeft =
             5 + (notify.timeout != 0 ? notify.timeout : LOC_NI_NO_RESPONSE_TIME);

        int rc = 0;
        rc = pthread_create(&pSession->thread, NULL, niThreadProc, pSession);
        if (rc) {
            LOC_LOGe("Loc NI thread is not created.");
        }
        pthread_setname_np(pSession->thread, "NiThread");

        rc = pthread_detach(pSession->thread);
        if (rc) {
            LOC_LOGe("Loc NI thread is not detached.");
        }

        if (nullptr != gnssNiCb) {
            gnssNiCb(sessionId, notify);
        }
    }

    return true;
}

void
GnssAdapter::reportGnssMeasurementsEvent(const GnssMeasurements& gnssMeasurements,
                                            int msInWeek)
{
    LOC_LOGv("msInWeek=%d, isNhz = %d", msInWeek, gnssMeasurements.gnssSvMeasurementSet.isNhz);

    struct MsgReportGnssMeasurementData : public LocMsg {
        GnssAdapter& mAdapter;
        GnssMeasurements mGnssMeasurements;
        inline MsgReportGnssMeasurementData(GnssAdapter& adapter,
                                            const GnssMeasurements& gnssMeasurements,
                                            int msInWeek) :
                LocMsg(),
                mAdapter(adapter),
                mGnssMeasurements(gnssMeasurements) {
            if (-1 != msInWeek && 0 != gnssMeasurements.gnssMeasNotification.count) {
                mAdapter.getAgcInformation(mGnssMeasurements.gnssMeasNotification, msInWeek);
            }
        }

        inline virtual void proc() const {
            mAdapter.mPositionElapsedRealTimeCal.saveGpsTimeAndQtimerPairInMeasReport(
                    mGnssMeasurements.gnssSvMeasurementSet);
            mAdapter.reportGnssMeasurementData(mGnssMeasurements.gnssMeasNotification);
            if ((false == mGnssMeasurements.gnssSvMeasurementSet.isNhz) &&
                    mAdapter.mDGnssNeedReport) {
                mAdapter.reportDGnssDataUsable(mGnssMeasurements.gnssSvMeasurementSet);
            }
        }
    };

    sendMsg(new MsgReportGnssMeasurementData(*this, gnssMeasurements, msInWeek));

    // some position engine requires the QMI order of PVT report and SV measurement
    // report to be preserved. So, send out both SV measurement report and PVT report
    // directly to engine hub
    mEngHubProxy->gnssReportSvMeasurement(gnssMeasurements.gnssSvMeasurementSet);
}

void
GnssAdapter::reportGnssMeasurementData(const GnssMeasurementsNotification& measurements)
{
    for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
        if (!measurements.isNhz) {
            if (nullptr != it->second.gnssMeasurementsCb) {
                it->second.gnssMeasurementsCb(measurements);
            }
        } else { // nHz measurement
            if (nullptr != it->second.gnssNHzMeasurementsCb) {
                it->second.gnssNHzMeasurementsCb(measurements);
            }
        }
    }
}

void
GnssAdapter::reportDGnssDataUsable(const GnssSvMeasurementSet &svMeasurementSet)
{
    uint32_t i;
    bool preDGnssDataUsage = mDGnssDataUsage;

    mDGnssDataUsage = false;
    for (i = 0; i < svMeasurementSet.svMeasCount; i++) {
        const Gnss_SVMeasurementStructType& svMeas = svMeasurementSet.svMeas[i];
        if (svMeas.dgnssSvMeas.dgnssMeasStatus) {
            mDGnssDataUsage = true;
            break;
        }
    }
    if (mDGnssDataUsage != preDGnssDataUsage) {
        if (mCdfwInterface) {
            mCdfwInterface->reportUsable(mQDgnssListenerHDL, mDGnssDataUsage);
        }
    }
}

void
GnssAdapter::reportSvPolynomialEvent(GnssSvPolynomial &svPolynomial)
{
    mEngHubProxy->gnssReportSvPolynomial(svPolynomial);
}

bool GnssAdapter::isEphNetworkBased(const GnssEphCommon& commanEphRpt)
{
    if (GNSS_EPH_ACTION_UPDATE_SRC_NETWORK_V02 == commanEphRpt.updateAction ||
            GNSS_EPH_ACTION_DELETE_SRC_NETWORK_V02 == commanEphRpt.updateAction) {
        return true;
    }
    return false;
}

void GnssAdapter::convertGpsEphemeris(const GpsEphemerisResponse& ephRpt,
            GpsEphemerisResponse& halEph) {
    /** Keep count of number of OTA based ephemeris to be reported to LCA */
    uint16_t numEph = 0;
    uint16_t maxNumEph = std::min(ephRpt.numOfEphemeris,
            (uint16_t)GNSS_EPHEMERIS_LIST_MAX_SIZE_V02);

    for (uint16_t idx = 0; idx < maxNumEph; idx++) {
        if (isEphNetworkBased(ephRpt.gpsEphemerisData[idx].commonEphemerisData)) {
            continue;
        }
        halEph.gpsEphemerisData[numEph] = ephRpt.gpsEphemerisData[idx];
        numEph++;
    }

    halEph.numOfEphemeris = numEph;
}

void GnssAdapter::convertGalEphemeris(const GalileoEphemerisResponse& ephRpt,
            GalileoEphemerisResponse& halEph) {

    /** Keep count of number of OTA based ephemeris to be reported to LCA */
    uint16_t numEph = 0;
    uint16_t maxNumEph = std::min(ephRpt.numOfEphemeris,
            (uint16_t)GNSS_EPHEMERIS_LIST_MAX_SIZE_V02);

    for (uint16_t idx = 0; idx < maxNumEph; idx++) {
        if (isEphNetworkBased(ephRpt.galEphemerisData[idx].commonEphemerisData)) {
            continue;
        }
        halEph.galEphemerisData[numEph] = ephRpt.galEphemerisData[idx];
        numEph++;
    }

    halEph.numOfEphemeris = numEph;

}

void GnssAdapter::convertGloEphemeris(const GlonassEphemerisResponse& ephRpt,
            GlonassEphemerisResponse& halEph) {

    /** Keep count of number of OTA based ephemeris to be reported to LCA */
    uint16_t numEph = 0;
    uint16_t maxNumEph = std::min(ephRpt.numOfEphemeris,
            (uint16_t)GNSS_EPHEMERIS_LIST_MAX_SIZE_V02);

    for (uint16_t idx = 0; idx < maxNumEph; idx++) {
        if (GNSS_EPH_ACTION_UPDATE_SRC_NETWORK_V02 == ephRpt.gloEphemerisData[idx].updateAction ||
                GNSS_EPH_ACTION_DELETE_SRC_NETWORK_V02 ==
                ephRpt.gloEphemerisData[idx].updateAction) {
            continue;
        }
        halEph.gloEphemerisData[numEph] = ephRpt.gloEphemerisData[idx];
        numEph++;
    }

    halEph.numOfEphemeris = numEph;

}

void GnssAdapter::convertBdsEphemeris(const BdsEphemerisResponse& ephRpt,
            BdsEphemerisResponse& halEph) {

    /** Keep count of number of OTA based ephemeris to be reported to LCA */
    uint16_t numEph = 0;
    uint16_t maxNumEph = std::min(ephRpt.numOfEphemeris,
            (uint16_t)GNSS_EPHEMERIS_LIST_MAX_SIZE_V02);

    for (uint16_t idx = 0; idx < maxNumEph; idx++) {
        if (isEphNetworkBased(ephRpt.bdsEphemerisData[idx].commonEphemerisData)) {
            continue;
        }
        halEph.bdsEphemerisData[numEph] = ephRpt.bdsEphemerisData[idx];
        numEph++;
    }

    halEph.numOfEphemeris = numEph;
}

void GnssAdapter::convertQzssEphemeris(const QzssEphemerisResponse& ephRpt,
            QzssEphemerisResponse& halEph) {

    /** Keep count of number of OTA based ephemeris to be reported to LCA */
    uint16_t numEph = 0;
    uint16_t maxNumEph = std::min(ephRpt.numOfEphemeris,
            (uint16_t)GNSS_EPHEMERIS_LIST_MAX_SIZE_V02);

    for (uint16_t idx = 0; idx < maxNumEph; idx++) {
        if (isEphNetworkBased(ephRpt.qzssEphemerisData[idx].commonEphemerisData)) {
            continue;
        }

        halEph.qzssEphemerisData[numEph] = ephRpt.qzssEphemerisData[idx];
        numEph++;
    }

    halEph.numOfEphemeris = numEph;
}
void GnssAdapter::convertNavicEphemeris(const NavicEphemerisResponse& ephRpt,
            NavicEphemerisResponse& halEph) {

    /** Keep count of number of OTA based ephemeris to be reported to LCA */
    uint16_t numEph = 0;
    uint16_t maxNumEph = std::min(ephRpt.numOfEphemeris,
            (uint16_t)GNSS_EPHEMERIS_LIST_MAX_SIZE_V02);

    for (uint16_t idx = 0; idx < maxNumEph; idx++) {
        if (isEphNetworkBased(ephRpt.navicEphemerisData[idx].commonEphemerisData)) {
            continue;
        }
        halEph.navicEphemerisData[numEph] = ephRpt.navicEphemerisData[idx];
        numEph++;
    }

    halEph.numOfEphemeris = numEph;
}

void
GnssAdapter::convertEphReportInfo(const GnssSvEphemerisReport& svEphemeris,
            GnssSvEphemerisReport& ephReport, bool& needToReportEph) {

    switch (svEphemeris.gnssConstellation) {
        case GNSS_LOC_SV_SYSTEM_GPS:
            ephReport.gnssConstellation = GNSS_LOC_SV_SYSTEM_GPS;
                convertGpsEphemeris(svEphemeris.ephInfo.gpsEphemeris,
                    ephReport.ephInfo.gpsEphemeris);
            if (!ephReport.ephInfo.gpsEphemeris.numOfEphemeris) {
                needToReportEph = false;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_GALILEO:
            ephReport.gnssConstellation = GNSS_LOC_SV_SYSTEM_GALILEO;
            convertGalEphemeris(svEphemeris.ephInfo.galileoEphemeris,
                    ephReport.ephInfo.galileoEphemeris);
            if (!ephReport.ephInfo.galileoEphemeris.numOfEphemeris) {
                needToReportEph = false;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_GLONASS:
            ephReport.gnssConstellation = GNSS_LOC_SV_SYSTEM_GLONASS;
            convertGloEphemeris(svEphemeris.ephInfo.glonassEphemeris,
                    ephReport.ephInfo.glonassEphemeris);
            if (!ephReport.ephInfo.glonassEphemeris.numOfEphemeris) {
                needToReportEph = false;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_BDS:
            ephReport.gnssConstellation = GNSS_LOC_SV_SYSTEM_BDS;
            convertBdsEphemeris(svEphemeris.ephInfo.bdsEphemeris,
                    ephReport.ephInfo.bdsEphemeris);
            if (!ephReport.ephInfo.bdsEphemeris.numOfEphemeris) {
                needToReportEph = false;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_QZSS:
            ephReport.gnssConstellation = GNSS_LOC_SV_SYSTEM_QZSS;
            convertQzssEphemeris(svEphemeris.ephInfo.qzssEphemeris,
                    ephReport.ephInfo.qzssEphemeris);
            if (!ephReport.ephInfo.qzssEphemeris.numOfEphemeris) {
                needToReportEph = false;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_NAVIC:
            ephReport.gnssConstellation = GNSS_LOC_SV_SYSTEM_NAVIC;
            convertNavicEphemeris(svEphemeris.ephInfo.navicEphemeris,
                    ephReport.ephInfo.navicEphemeris);
            if (!ephReport.ephInfo.navicEphemeris.numOfEphemeris) {
                needToReportEph = false;
            }
            break;
        default:
                LOC_LOGe(" Unknown System for Ephemeris Data ");
                needToReportEph = false;
            break;
    }

    if (needToReportEph && svEphemeris.isSystemTimeValid) {
        ephReport.isSystemTimeValid = 1;
        ephReport.systemTime = svEphemeris.systemTime;
    }
}

void
GnssAdapter::reportSvEphemerisData (const GnssSvEphemerisReport& svEphemeris) {
    GnssSvEphemerisReport svEphRpt = {};
    bool needToReportEph = true;
    convertEphReportInfo(svEphemeris, svEphRpt, needToReportEph);
    /** If all the Eph Data is network based, we do not report it to LCA */
    if (needToReportEph) {
        for (auto it=mClientData.begin(); it != mClientData.end(); ++it) {
            if (nullptr != it->second.svEphemerisCb) {
                it->second.svEphemerisCb(svEphRpt);
            }
        }
    }
}

void
GnssAdapter::reportSvEphemerisEvent(GnssSvEphemerisReport & svEphemeris)
{

    struct MsgReportSvEphemerisData : public LocMsg {
        GnssAdapter& mAdapter;
        GnssSvEphemerisReport mSvEphemerisData;
        inline MsgReportSvEphemerisData(GnssAdapter& adapter,
                                            const GnssSvEphemerisReport& svEphemerisData) :
                LocMsg(),
                mAdapter(adapter),
                mSvEphemerisData(svEphemerisData) {
        }

        inline virtual void proc() const {
            mAdapter.reportSvEphemerisData(mSvEphemerisData);
        }
    };

    sendMsg(new MsgReportSvEphemerisData(*this, svEphemeris));
    mEngHubProxy->gnssReportSvEphemeris(svEphemeris);
}


bool
GnssAdapter::requestOdcpiEvent(OdcpiRequestInfo& request)
{
    struct MsgRequestOdcpi : public LocMsg {
        GnssAdapter& mAdapter;
        OdcpiRequestInfo mOdcpiRequest;
        inline MsgRequestOdcpi(GnssAdapter& adapter, OdcpiRequestInfo& request) :
                LocMsg(),
                mAdapter(adapter),
                mOdcpiRequest(request) {}
        inline virtual void proc() const {
            mAdapter.requestOdcpi(mOdcpiRequest);
        }
    };

    sendMsg(new MsgRequestOdcpi(*this, request));
    return true;
}

void GnssAdapter::requestOdcpi(const OdcpiRequestInfo& request)
{
    if (nullptr != mControlCallbacks.odcpiReqCb) {
        bool sendEmergencyCallStatusEvent = false;
        LOC_LOGd("request: type %d, tbf %d, isEmergency %d"
                 " requestActive: %d timerActive: %d",
                 request.type, request.tbfMillis, request.isEmergencyMode,
                 mOdcpiStateMask, mOdcpiTimer.isActive());
        // ODCPI START and ODCPI STOP from modem can come in quick succession
        // so the mOdcpiTimer helps avoid spamming the framework as well as
        // extending the odcpi session past 30 seconds if needed
        if (ODCPI_REQUEST_TYPE_START == request.type) {
            if (!(mOdcpiStateMask & ODCPI_REQ_ACTIVE)  && false == mOdcpiTimer.isActive()) {
                fireOdcpiRequest(request);
                mOdcpiStateMask |= ODCPI_REQ_ACTIVE;
                if (nullptr != mEsStatusCb) {
                    mEsStatusCb(request.isEmergencyMode);
                }
                mOdcpiTimer.start();
                sendEmergencyCallStatusEvent = true;
            // if the current active odcpi session is non-emergency, and the new
            // odcpi request is emergency, replace the odcpi request with new request
            // and restart the timer
            } else if (false == mOdcpiRequest.isEmergencyMode &&
                       true == request.isEmergencyMode) {
                fireOdcpiRequest(request);
                mOdcpiStateMask |= ODCPI_REQ_ACTIVE;
                if (nullptr != mEsStatusCb) {
                    mEsStatusCb(request.isEmergencyMode);
                }
                if (true == mOdcpiTimer.isActive()) {
                    mOdcpiTimer.restart();
                } else {
                    mOdcpiTimer.start();
                }
                sendEmergencyCallStatusEvent = true;
            // if ODCPI request is not active but the timer is active, then
            // just update the active state and wait for timer to expire
            // before requesting new ODCPI to avoid spamming ODCPI requests
            } else if (!(mOdcpiStateMask & ODCPI_REQ_ACTIVE) && true == mOdcpiTimer.isActive()) {
                mOdcpiStateMask |= ODCPI_REQ_ACTIVE;
                if (nullptr != mEsStatusCb) {
                    mEsStatusCb(request.isEmergencyMode);
                }
                sendEmergencyCallStatusEvent = true;
            }
            mOdcpiRequest = request;

            //Check if Modem needs civic address
            if (mAddressRequestCb != nullptr && request.isCivicAddressRequired) {
                mOdcpiStateMask |= CIVIC_ADDRESS_REQ_ACTIVE;
            }
        // the request is being stopped, but allow timer to expire first
        // before stopping the timer just in case more ODCPI requests come
        // to avoid spamming more odcpi requests to the framework
        } else if (ODCPI_REQUEST_TYPE_STOP == request.type) {
            LOC_LOGd("request: type %d, isEmergency %d", request.type, request.isEmergencyMode);
            fireOdcpiRequest(request);
            mOdcpiStateMask = 0;
            sendEmergencyCallStatusEvent = true;
            if (nullptr != mEsStatusCb) {
                mEsStatusCb(false);
            }
        } else {
            LOC_LOGe("Invalid ODCPI request type.");
        }

        // Raise InEmergencyCall event
        if (sendEmergencyCallStatusEvent && request.isEmergencyMode) {
            SystemStatus* systemstatus = getSystemStatus();
            if (nullptr != systemstatus) {
                systemstatus->eventInEmergencyCall(0 != mOdcpiStateMask);
            } else {
                LOC_LOGe("Failed to get system status instance.");
            }
        }
    } else {
        LOC_LOGw("ODCPI request not supported");
    }
}

bool GnssAdapter::reportDeleteAidingDataEvent(GnssAidingData& aidingData)
{
    mEngHubProxy->gnssDeleteAidingData(aidingData);
    return true;
}

bool GnssAdapter::reportKlobucharIonoModelEvent(GnssKlobucharIonoModel & ionoModel)
{
    mEngHubProxy->gnssReportKlobucharIonoModel(ionoModel);
    return true;
}

bool GnssAdapter::reportGnssAdditionalSystemInfoEvent(
        GnssAdditionalSystemInfo & additionalSystemInfo)
{
    mEngHubProxy->gnssReportAdditionalSystemInfo(additionalSystemInfo);
    return true;
}

void GnssAdapter::reportModemGnssQesdkFeatureStatus(const ModemGnssQesdkFeatureMask& mask) {
    struct MsgSetModemQesdkFeatureStatus : public LocMsg {
        GnssAdapter& mAdapter;
        const ModemGnssQesdkFeatureMask mMask;

        inline MsgSetModemQesdkFeatureStatus(GnssAdapter& adapter,
                const ModemGnssQesdkFeatureMask& mask) :
            LocMsg(),
            mAdapter(adapter),
            mMask(mask) {}

        inline virtual void proc() const {
            if (mMask & MODEM_QESDK_FEATURE_DGNSS) {
                mAdapter.mPpFeatureStatusMask |= MLP_FEATURE_ENABLED_BY_QESDK;
            } else {
                mAdapter.mPpFeatureStatusMask &= (~MLP_FEATURE_ENABLED_BY_QESDK);
            }
        }
    };
    sendMsg(new MsgSetModemQesdkFeatureStatus(*this, mask));
}

void GnssAdapter::handleQesdkQwesStatusFromEHub(
        const std::unordered_map<LocationQwesFeatureType, bool> &featureMap)
{
    struct MsgReportQwesStatusFromEHub : public LocMsg {
        GnssAdapter& mAdapter;
        const std::unordered_map<LocationQwesFeatureType, bool> mFeatureMap;
        inline MsgReportQwesStatusFromEHub(GnssAdapter& adapter,
                const std::unordered_map<LocationQwesFeatureType, bool> &featureMap) :
            LocMsg(),
            mAdapter(adapter),
            mFeatureMap(featureMap) {}
        inline virtual void proc() const {
            LOC_LOGD("MsgReportQwesStatusFromEHub: before mPpFeatureStatusMask: 0x%x",
                     mAdapter.mPpFeatureStatusMask);
            auto ppeInFeatureMap = mFeatureMap.find(LOCATION_QWES_FEATURE_TYPE_PPE);
            auto dlpQesdkInFeatureMap = mFeatureMap.find(LOCATION_QWES_FEATURE_TYPE_DLP_QESDK);
            auto qfeInFeatureMap = mFeatureMap.find(LOCATION_QWES_FEATURE_TYPE_QDR3);
            auto cdParserInFeatureMap = mFeatureMap.find(LOCATION_FEATURE_TYPE_CORR_DATA_PARSER);

            //QESDK feature status call back handling logic:
            //1, If LOCATION_QWES_FEATURE_TYPE_PPE is presented in feature map,
            //   It means Qwes status callback is triggered by Engine Servive try
            //   to register to Engine Hub, set DLP_FEATURE_STATUS_QPPE_LIBRARY_PRESENT
            //   bit, and set DLP_FEATURE_ENABLED_BY_DEFAULT bit according to
            //   PPE feature status;
            //2, If LOCATION_QWES_FEATURE_TYPE_QDR3 is presented in feature map,
            //   It means Qwes status callback is triggered by Engine Servive try
            //   to register to Engine Hub, set DLP_FEATURE_STATUS_QFE_LIBRARY_PRESENT
            //   bit, and set DLP_FEATURE_ENABLED_BY_DEFAULT bit according to
            //   PPE feature status;
            //3, If LOCATION_QWES_FEATURE_TYPE_DLP_QESDK is presented in feature map,
            //   It means Qwes status callback is triggered when Engine hub recieves
            //   configPreciseLocation command from GnssAdapter, and already checked
            //   QESDK feature status via QWES call checkInstalledLicense, set
            //   DLP_FEATURE_ENABLED_BY_QESDK bit according to QESDK feature status.
            if (ppeInFeatureMap != mFeatureMap.end() || qfeInFeatureMap != mFeatureMap.end()) {
                if (ppeInFeatureMap != mFeatureMap.end()) {
                    mAdapter.mPpFeatureStatusMask |= DLP_FEATURE_STATUS_QPPE_LIBRARY_PRESENT;
                }
                if (qfeInFeatureMap != mFeatureMap.end()) {
                    mAdapter.mPpFeatureStatusMask |= DLP_FEATURE_STATUS_QFE_LIBRARY_PRESENT;
                }
                if ((ppeInFeatureMap != mFeatureMap.end() && ppeInFeatureMap->second) ||
                        (qfeInFeatureMap != mFeatureMap.end() && qfeInFeatureMap->second)) {
                    mAdapter.mPpFeatureStatusMask |= DLP_FEATURE_ENABLED_BY_DEFAULT;
                    mAdapter.notifyPreciseLocation();
                } else {
                    mAdapter.mPpFeatureStatusMask &= (~DLP_FEATURE_ENABLED_BY_DEFAULT);
                    mAdapter.notifyPreciseLocation();
                }
                mAdapter.mQppeResp = true;
            } else if (dlpQesdkInFeatureMap != mFeatureMap.end()) {
                if (dlpQesdkInFeatureMap->second) {
                    mAdapter.mPpFeatureStatusMask |= DLP_FEATURE_ENABLED_BY_QESDK;
                    //Send enable precise location data item to loclauncher to inform
                    //it QPPE engine-service need to launch
                    if (mAdapter.mPpFeatureStatusMask & DLP_FEATURE_STATUS_LIBRARY_PRESENT) {
                        mAdapter.notifyPreciseLocation();
                    }
                } else {
                    mAdapter.mPpFeatureStatusMask &= (~DLP_FEATURE_ENABLED_BY_QESDK);
                    //Send disable precise location data item to loclauncher to inform
                    //it QPPE engine-service need to exit
                    if (mAdapter.mPpFeatureStatusMask & DLP_FEATURE_STATUS_LIBRARY_PRESENT) {
                        mAdapter.notifyPreciseLocation();
                    }
                }
            } else if (cdParserInFeatureMap != mFeatureMap.end()) {
                // If EngineHubMgr calls this cb, QPPE is loaded as 3GPP SSR2OSR
                // correction data parser, so set QDGNSS_3GPP_EP_PARSER_AVAIL
                mAdapter.m3GppSourceMask |= QDGNSS_3GPP_EP_PARSER_AVAIL;
                LOC_LOGD("MsgReportQwesStatusFromEHub, set QDGNSS_3GPP_EP_PARSER_AVAIL");
                mAdapter.notifyPreciseLocation();
            }

            LOC_LOGD("MsgReportQwesStatusFromEHub, after mPpFeatureStatusMask: 0x%x",
                     mAdapter.mPpFeatureStatusMask);
        }
    };

    sendMsg(new MsgReportQwesStatusFromEHub(*this, featureMap));
}

bool GnssAdapter::reportQwesCapabilities(
        const std::unordered_map<LocationQwesFeatureType, bool> &featureMap)
{
    struct MsgReportQwesFeatureStatus : public LocMsg {
        GnssAdapter& mAdapter;
        const std::unordered_map<LocationQwesFeatureType, bool> mFeatureMap;
        inline MsgReportQwesFeatureStatus(GnssAdapter& adapter,
                const std::unordered_map<LocationQwesFeatureType, bool> &featureMap) :
            LocMsg(),
            mAdapter(adapter),
            mFeatureMap(std::move(featureMap)) {}
        inline virtual void proc() const {
            LOC_LOGI("ReportQwesFeatureStatus before caps %" PRIx64 " ",
                     mAdapter.getCapabilities());

            //Set Mlp feature bit
            auto iter = mFeatureMap.find(LOCATION_QWES_FEATURE_TYPE_DGNSS);
            if (iter != mFeatureMap.end() && iter->second) {
                mAdapter.mPpFeatureStatusMask |= MLP_FEATURE_ENABLED_BY_DEFAULT;
            }
            LOC_LOGI("ReportQwesFeatureStatus after caps %" PRIx64 " ",
                     mAdapter.getCapabilities());
            mAdapter.broadcastCapabilities(mAdapter.getCapabilities());
        }
    };

    sendMsg(new MsgReportQwesFeatureStatus(*this, featureMap));
    return true;
}

void GnssAdapter::initOdcpiCommand(const odcpiRequestCallback& callback,
                                   OdcpiPrioritytype priority,
                                   OdcpiCallbackTypeMask typeMask)
{
    struct MsgInitOdcpi : public LocMsg {
        GnssAdapter& mAdapter;
        odcpiRequestCallback mOdcpiCb;
        OdcpiPrioritytype mPriority;
        OdcpiCallbackTypeMask mTypeMask;
        inline MsgInitOdcpi(GnssAdapter& adapter,
                const odcpiRequestCallback& callback,
                OdcpiPrioritytype priority,
                OdcpiCallbackTypeMask typeMask) :
                LocMsg(),
                mAdapter(adapter),
                mOdcpiCb(callback), mPriority(priority),
                mTypeMask(typeMask) {}
        inline virtual void proc() const {
            mAdapter.initOdcpi(mOdcpiCb, mPriority, mTypeMask);
        }
    };

    sendMsg(new MsgInitOdcpi(*this, callback, priority, typeMask));
}

void GnssAdapter::deRegisterOdcpiCommand(OdcpiPrioritytype priority,
        OdcpiCallbackTypeMask typeMask) {
    struct MsgDeRegisterNonEsOdcpi : public LocMsg {
        GnssAdapter& mAdapter;
        OdcpiPrioritytype mPriority;
        OdcpiCallbackTypeMask mTypeMask;
        inline MsgDeRegisterNonEsOdcpi(GnssAdapter& adapter,
                OdcpiPrioritytype priority,
                OdcpiCallbackTypeMask typeMask) :
            LocMsg(),
            mAdapter(adapter),
            mPriority(priority),
            mTypeMask(typeMask) {}
        inline virtual void proc() const {
            mAdapter.deRegisterOdcpi(mPriority, mTypeMask);
        }
    };

    sendMsg(new MsgDeRegisterNonEsOdcpi(*this, priority, typeMask));
}

void GnssAdapter::fireOdcpiRequest(const OdcpiRequestInfo& request) {
    if (request.isEmergencyMode) {
        mControlCallbacks.odcpiReqCb(request);
    } else {
        std::unordered_map<OdcpiPrioritytype, odcpiRequestCallback>::iterator iter;
        for (int priority = ODCPI_HANDLER_PRIORITY_HIGH;
                priority >= ODCPI_HANDLER_PRIORITY_LOW && iter == mNonEsOdcpiReqCbMap.end();
                priority--) {
            iter = mNonEsOdcpiReqCbMap.find((OdcpiPrioritytype)priority);
        }
        if (iter != mNonEsOdcpiReqCbMap.end()) {
            iter->second(request);
        }
    }
}

void GnssAdapter::initOdcpi(const odcpiRequestCallback& callback,
            OdcpiPrioritytype priority, OdcpiCallbackTypeMask typeMask) {
    if (typeMask & EMERGENCY_ODCPI) {
        LOC_LOGd("In priority: %d, Curr priority: %d", priority, mCallbackPriority);
        if (priority >= mCallbackPriority) {
            mControlCallbacks.odcpiReqCb = callback;
            mCallbackPriority = priority;
            /* Register for WIFI request */
            updateEvtMask(LOC_API_ADAPTER_BIT_REQUEST_WIFI,
                    LOC_REGISTRATION_MASK_ENABLED);
        }
    }
    if (typeMask & NON_EMERGENCY_ODCPI) {
        //If this is for non emergency odcpi,
        //Only set callback to mNonEsOdcpiReqCbMap according to its priority
        //Will overwrite callback with same priority in this map
        mNonEsOdcpiReqCbMap[priority] = callback;
    }
    UTIL_READ_CONF(LOC_PATH_IZAT_CONF, izatConfParamTable);
}

void GnssAdapter::injectOdcpiCommand(const Location& location)
{
    struct MsgInjectOdcpi : public LocMsg {
        GnssAdapter& mAdapter;
        Location mLocation;
        inline MsgInjectOdcpi(GnssAdapter& adapter, const Location& location, ContextBase& context):
                LocMsg(),
                mAdapter(adapter),
                mLocation(location) {
            // Update techMask to tell whether ALE FLP or Android FLP
            if (context.getLBSProxyBase()->getIzatFusedProviderOverride()) {
                mLocation.techMask |= LOCATION_TECHNOLOGY_HYBRID_ALE_BIT;
            }
        }
        inline virtual void proc() const {
            mAdapter.injectOdcpi(mLocation);
        }
    };

    sendMsg(new MsgInjectOdcpi(*this, location, *mContext));
}

void GnssAdapter::injectOdcpi(const Location& location)
{
    LOC_LOGd("ODCPI Injection: requestActive: %d timerActive: %d"
             "lat %.7f long %.7f",
            mOdcpiStateMask, mOdcpiTimer.isActive(),
            location.latitude, location.longitude);

    if ((mOdcpiStateMask & CIVIC_ADDRESS_REQ_ACTIVE) && mAddressRequestCb != nullptr) {
        mAddressRequestCb(location);
    }

    mLocApi->injectPosition(location, mOdcpiTimer.isActive());
}

void GnssAdapter::setAddressRequestCbCommand(
        const std::function<void(const Location&)>& addressRequestCb)
{
    struct MsgSetAddrReqCb : public LocMsg {
        GnssAdapter& mAdapter;
        std::function<void(const Location&)> mAddrReqCb;
        inline MsgSetAddrReqCb(GnssAdapter& adapter,
                const std::function<void(const Location&)>& addressRequestCb) :
            LocMsg(),
            mAdapter(adapter),
            mAddrReqCb(addressRequestCb){}
        inline virtual void proc() const {
            mAdapter.setAddressRequestCb(mAddrReqCb);
        }
    };
    sendMsg(new MsgSetAddrReqCb(*this, addressRequestCb));
}

void GnssAdapter::injectLocationAndAddrCommand(
        const Location& location, const GnssCivicAddress& addr)
{
    struct MsgInjectLocAndAddr : public LocMsg {
        GnssAdapter& mAdapter;
        Location mLocation;
        GnssCivicAddress mAddress;
        inline MsgInjectLocAndAddr(GnssAdapter& adapter,
                const Location& location, const GnssCivicAddress& addr) :
            LocMsg(), mAdapter(adapter), mLocation(location), mAddress(addr) {}
        inline virtual void proc() const {
            LOC_LOGD("MsgInjectLocAndAddr: Location: %x, %" PRIu64 ", %f, %f, %f \n"
                    "Address: adminArea: %s, countryCode: %s, countryName: %s,\n"
                    "featureName: %s, latitude: %f, longitude: %f\n"
                    "locale: %s, locality: %s, phone: %s, postalCode: %s\n"
                    "premises: %s, subAdminArea: %s, subLocality: %s"
                    "thoroughfare: %s, subThoroughfare: %s, url: %s",
                    mLocation.flags, mLocation.timestamp,
                    mLocation.latitude, mLocation.longitude,
                    mLocation.accuracy, mAddress.adminArea.c_str(),
                    mAddress.countryCode.c_str(), mAddress.countryName.c_str(),
                    mAddress.featureName.c_str(), mAddress.latitude, mAddress.longitude,
                    mAddress.locale.c_str(), mAddress.locality.c_str(),
                    mAddress.phone.c_str(), mAddress.postalCode.c_str(),
                    mAddress.premises.c_str(), mAddress.subAdminArea.c_str(),
                    mAddress.subLocality.c_str(), mAddress.thoroughfare.c_str(),
                    mAddress.subThoroughfare.c_str(), mAddress.url.c_str());

            mAdapter.injectLocationAndAddr(mLocation, mAddress);
        }
    };
    sendMsg(new MsgInjectLocAndAddr(*this, location, addr));
}

// Called in the context of LocTimer thread
void OdcpiTimer::timeOutCallback()
{
    if (nullptr != mAdapter) {
        mAdapter->odcpiTimerExpireEvent();
    }
}
// Called in the context of LocTimer thread
void GnssAdapter::odcpiTimerExpireEvent()
{
    struct MsgOdcpiTimerExpire : public LocMsg {
        GnssAdapter& mAdapter;
        inline MsgOdcpiTimerExpire(GnssAdapter& adapter) :
                LocMsg(),
                mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter.odcpiTimerExpire();
        }
    };
    sendMsg(new MsgOdcpiTimerExpire(*this));
}
void GnssAdapter::odcpiTimerExpire()
{
    LOC_LOGd("requestActive: %d timerActive: %d",
            mOdcpiStateMask, mOdcpiTimer.isActive());

    // if ODCPI request is still active after timer
    // expires, request again and restart timer
    if (mOdcpiStateMask & ODCPI_REQ_ACTIVE) {
        fireOdcpiRequest(mOdcpiRequest);
        mOdcpiTimer.restart();
    } else {
        mOdcpiTimer.stop();
    }
}

void
GnssAdapter::invokeGnssEnergyConsumedCallback(uint64_t energyConsumedSinceFirstBoot) {
    if (mGnssEnergyConsumedCb) {
        mGnssEnergyConsumedCb(energyConsumedSinceFirstBoot);
        mGnssEnergyConsumedCb = nullptr;
    }
    if (!mGnssPowerStatisticsInit) {
        /* We need to change the reference in case HAL process restarts (i.e. crashes)
           We maintain mBootReferenceEnergy value in the file system
           At the point mBootReferenceEnergy needs to get initialized we will use
           the following logic:

           if (boot time > 30 sec)
                use the value in the file system for mBootReferenceEnergy
           else
                use the value we get here from the modem for mBootReferenceEnergy */

        struct timespec currentTime = {};
        int64_t sinceBootTimeNanos = 0;
        FILE *fp = NULL;

        mBootReferenceEnergy = energyConsumedSinceFirstBoot;
        if (NULL != (fp = fopen("/data/vendor/location/energy.conf", "a+b"))) {
            rewind(fp);
            if (RealtimeEstimator::getCurrentTime(currentTime, sinceBootTimeNanos)) {
                LOC_LOGv("sinceBootTimeNanos: %" PRIu64 " ", sinceBootTimeNanos);
                if ((uint32_t)(sinceBootTimeNanos / 1000000000) > 30) {
                    int fr = fread(&mBootReferenceEnergy, sizeof(mBootReferenceEnergy), 1, fp);
                    if (1 != fr) {
                        mBootReferenceEnergy = energyConsumedSinceFirstBoot;
                        LOC_LOGw("fread failed ferror(fp)=%d fr=%d", ferror(fp), fr);
                    }
                } else {
                    fwrite(&mBootReferenceEnergy, sizeof(mBootReferenceEnergy), 1, fp);
                }
            } else {
                LOC_LOGw("getCurrentTime failed");
                fwrite(&mBootReferenceEnergy, sizeof(mBootReferenceEnergy), 1, fp);
            }
            fclose(fp);
        } else {
            LOC_LOGw("fopen failed");
        }
        LOC_LOGd("mBootReferenceEnergy: %" PRIu64 " energyConsumedSinceFirstBoot: %" PRIu64 " ",
                 mBootReferenceEnergy, energyConsumedSinceFirstBoot);

        mGnssPowerStatisticsInit = true;
        mPowerElapsedRealTimeCal.reset();
    } else if (nullptr != mPowerIndicationCb) {
        GnssPowerStatistics gnssPowerStatistics = {};
        gnssPowerStatistics.size = sizeof(GnssPowerStatistics);

        if (energyConsumedSinceFirstBoot >= mBootReferenceEnergy) {
            gnssPowerStatistics.totalEnergyMilliJoule =
                    (double)(energyConsumedSinceFirstBoot - mBootReferenceEnergy) / 10.0;
        } else {
            LOC_LOGe("energyConsumedSinceFirstBoot %" PRIu64 " is smaller than"
                     "mBootReferenceEnergy %" PRIu64 ","
                     "Set totalEnergyMilliJoule to 0",
                     energyConsumedSinceFirstBoot, mBootReferenceEnergy);
            gnssPowerStatistics.totalEnergyMilliJoule = 0;
        }

        LOC_LOGv("energyConsumedSinceFirstBoot: %" PRId64", "
                 " mBootReferenceEnergy: %" PRId64", "
                 " gnssPowerStatistics.totalEnergyMilliJoule: %f",
                 energyConsumedSinceFirstBoot,
                 mBootReferenceEnergy,
                 gnssPowerStatistics.totalEnergyMilliJoule);

        gnssPowerStatistics.elapsedRealTime = elapsedRealtime() * 1000000LL;
        gnssPowerStatistics.elapsedRealTimeUnc =
                mPowerElapsedRealTimeCal.getElapsedRealtimeUncNanos();
        mPowerIndicationCb(gnssPowerStatistics);
    }
}

bool
GnssAdapter::reportGnssEngEnergyConsumedEvent(uint64_t energyConsumedSinceFirstBoot){
    LOC_LOGd("energyConsumedSinceFirstBoot: %" PRIu64 " ", energyConsumedSinceFirstBoot);

    struct MsgReportGnssGnssEngEnergyConsumed : public LocMsg {
        GnssAdapter& mAdapter;
        uint64_t mGnssEnergyConsumedSinceFirstBoot;
        inline MsgReportGnssGnssEngEnergyConsumed(GnssAdapter& adapter,
                                                  uint64_t energyConsumed) :
                LocMsg(),
                mAdapter(adapter),
                mGnssEnergyConsumedSinceFirstBoot(energyConsumed) {}
        inline virtual void proc() const {
            mAdapter.invokeGnssEnergyConsumedCallback(mGnssEnergyConsumedSinceFirstBoot);
        }
    };

    if (~0 != energyConsumedSinceFirstBoot) {
        sendMsg(new MsgReportGnssGnssEngEnergyConsumed(*this, energyConsumedSinceFirstBoot));
    } else {
        LOC_LOGe("energyConsumedSinceFirstBoot not valid!");
    }
    return true;
}

void GnssAdapter::initDefaultAgps() {
    LOC_LOGd();
    void *handle = nullptr;

    LocAgpsGetAgpsCbInfo getAgpsCbInfo =
        (LocAgpsGetAgpsCbInfo)dlGetSymFromLib(handle, "libloc_net_iface.so",
            "LocNetIfaceAgps_getAgpsCbInfo");
    // Below step is to make sure we init nativeAgpsHandler
    // for Android platforms only
    AgpsCbInfo cbInfo = {};
    if (nullptr != getAgpsCbInfo) {
        cbInfo = getAgpsCbInfo(agpsOpenResultCb, agpsCloseResultCb, this);
    } else {
        cbInfo = mNativeAgpsHandler.getAgpsCbInfo();
    }

    if (cbInfo.statusV4Cb == nullptr) {
        LOC_LOGe("statusV4Cb is nullptr!");
        if (nullptr != handle) {
            // handle could be null if dlGetSymFromLib fails
            dlclose(handle);
        }
        return;
    }

    initAgps(cbInfo);
}

void GnssAdapter::initDefaultAgpsCommand() {
    LOC_LOGd();

    struct MsgInitDefaultAgps : public LocMsg {
        GnssAdapter& mAdapter;
        inline MsgInitDefaultAgps(GnssAdapter& adapter) :
            LocMsg(),
            mAdapter(adapter) {
            }
        inline virtual void proc() const {
            mAdapter.initDefaultAgps();
        }
    };

    sendMsg(new MsgInitDefaultAgps(*this));
}

/* INIT LOC AGPS MANAGER */

void GnssAdapter::initAgps(const AgpsCbInfo& cbInfo) {
    LOC_LOGd("cbInfo.atlType - %d", cbInfo.atlType);

    if (!((ContextBase::mGps_conf.CAPABILITIES & LOC_GPS_CAPABILITY_MSB) ||
            (ContextBase::mGps_conf.CAPABILITIES & LOC_GPS_CAPABILITY_MSA))) {
        return;
    }

    mAgpsManager.createAgpsStateMachines(cbInfo);
    /* Register for AGPS event mask */
    updateEvtMask(LOC_API_ADAPTER_BIT_LOCATION_SERVER_REQUEST,
            LOC_REGISTRATION_MASK_ENABLED);
}

void GnssAdapter::initAgpsCommand(const AgpsCbInfo& cbInfo){
    LOC_LOGI("GnssAdapter::initAgpsCommand");

    /* Message to initialize AGPS module */
    struct AgpsMsgInit: public LocMsg {
        const AgpsCbInfo mCbInfo;
        GnssAdapter& mAdapter;

        inline AgpsMsgInit(const AgpsCbInfo& cbInfo,
                GnssAdapter& adapter) :
                LocMsg(), mCbInfo(cbInfo), mAdapter(adapter) {
        }

        inline virtual void proc() const {
            LOC_LOGV("AgpsMsgInit::proc()");
            mAdapter.initAgps(mCbInfo);
        }
    };

    /* Send message to initialize AGPS Manager */
    sendMsg(new AgpsMsgInit(cbInfo, *this));
}

void GnssAdapter::initNfwCommand(const NfwCbInfo& cbInfo) {
    LOC_LOGi("GnssAdapter::initNfwCommand");

    /* Message to initialize NFW */
    struct MsgInitNfw : public LocMsg {
        const NfwCbInfo mCbInfo;
        GnssAdapter& mAdapter;

        inline MsgInitNfw(const NfwCbInfo& cbInfo,
            GnssAdapter& adapter) :
            LocMsg(), mCbInfo(cbInfo), mAdapter(adapter) {
        }

        inline virtual void proc() const {
            LOC_LOGV("MsgInitNfw::proc()");
            mAdapter.initNfw(mCbInfo);
        }
    };

    /* Send message to initialize NFW */
    sendMsg(new MsgInitNfw(cbInfo, *this));
}

void GnssAdapter::reportNfwNotificationEvent(GnssNfwNotification& notification) {
    LOC_LOGi("GnssAdapter::reportNfwNotificationEvent");

    struct MsgReportNfwNotification : public LocMsg {
        const GnssNfwNotification mNotification;
        GnssAdapter& mAdapter;

        inline MsgReportNfwNotification(const GnssNfwNotification& notification,
            GnssAdapter& adapter) :
            LocMsg(), mNotification(notification), mAdapter(adapter) {
        }

        inline virtual void proc() const {
            LOC_LOGV("MsgReportNfwNotification::proc()");
            mAdapter.reportNfwNotification(mNotification);
        }
    };

    sendMsg(new MsgReportNfwNotification(notification, *this));
}

/* GnssAdapter::requestATL
 * Method triggered in QMI thread as part of handling below message:
 * eQMI_LOC_SERVER_REQUEST_OPEN_V02
 * Triggers the AGPS state machine to setup AGPS call for below WWAN types:
 * eQMI_LOC_WWAN_TYPE_INTERNET_V02
 * eQMI_LOC_WWAN_TYPE_AGNSS_V02
 * eQMI_LOC_WWAN_TYPE_AGNSS_EMERGENCY_V02 */
bool GnssAdapter::requestATL(int connHandle, LocAGpsType agpsType,
                             LocApnTypeMask apnTypeMask, SubId subId){

    LOC_LOGi("GnssAdapter::requestATL handle=%d agpsType=0x%X apnTypeMask=0x%X subId=%d",
             connHandle, agpsType, apnTypeMask, subId);

    sendMsg( new AgpsMsgRequestATL( &mAgpsManager, connHandle, (AGpsExtType)agpsType,
                                    apnTypeMask, subId));

    return true;
}

/* GnssAdapter::releaseATL
 * Method triggered in QMI thread as part of handling below message:
 * eQMI_LOC_SERVER_REQUEST_CLOSE_V02
 * Triggers teardown of an existing AGPS call */
bool GnssAdapter::releaseATL(int connHandle){

    LOC_LOGI("GnssAdapter::releaseATL");

    /* Release SUPL/INTERNET/SUPL_ES ATL */
    struct AgpsMsgReleaseATL: public LocMsg {

        AgpsManager* mAgpsManager;
        int mConnHandle;

        inline AgpsMsgReleaseATL(AgpsManager* agpsManager, int connHandle) :
                LocMsg(), mAgpsManager(agpsManager), mConnHandle(connHandle) {
        }

        inline virtual void proc() const {
            LOC_LOGV("AgpsMsgReleaseATL::proc()");
            mAgpsManager->releaseATL(mConnHandle);
        }
    };

    sendMsg( new AgpsMsgReleaseATL(&mAgpsManager, connHandle));

    return true;
}

void GnssAdapter::reportPdnTypeFromWds(int pdnType, AGpsExtType agpsType, std::string apnName,
        AGpsBearerType bearerType) {
    LOC_LOGd("pdnType from WDS QMI: %d, agpsType: %d, apnName: %s, bearerType: %d",
            pdnType, agpsType, apnName.c_str(), bearerType);

    struct MsgReportAtlPdn : public LocMsg {
        GnssAdapter& mAdapter;
        int mPdnType;
        AgpsManager* mAgpsManager;
        AGpsExtType mAgpsType;
        string mApnName;
        AGpsBearerType mBearerType;

        inline MsgReportAtlPdn(GnssAdapter& adapter, int pdnType,
                AgpsManager* agpsManager, AGpsExtType agpsType,
                const string& apnName, AGpsBearerType bearerType) :
            LocMsg(), mAgpsManager(agpsManager), mAgpsType(agpsType),
            mApnName(apnName), mBearerType(bearerType),
            mAdapter(adapter), mPdnType(pdnType) {}
        inline virtual void proc() const {
            mAgpsManager->reportAtlOpenSuccess(mAgpsType,
                    const_cast<char*>(mApnName.c_str()),
                    mApnName.length(), mPdnType<=0? mBearerType:mPdnType);
        }
    };

    AGpsBearerType atlPdnType = (pdnType+1) & 3; // convert WDS QMI pdn type to AgpsBearerType
    sendMsg(new MsgReportAtlPdn(*this, atlPdnType, &mAgpsManager,
                agpsType, apnName, bearerType));
}


void GnssAdapter::dataConnOpenCommand(
        AGpsExtType agpsType,
        const char* apnName, int apnLen, AGpsBearerType bearerType){

    LOC_LOGd("GnssAdapter::frameworkDataConnOpen");

    struct AgpsMsgAtlOpenSuccess: public LocMsg {
        GnssAdapter& mAdapter;
        AgpsManager* mAgpsManager;
        AGpsExtType mAgpsType;
        char* mApnName;
        AGpsBearerType mBearerType;

        inline AgpsMsgAtlOpenSuccess(GnssAdapter& adapter, AgpsManager* agpsManager,
                AGpsExtType agpsType, const char* apnName, int apnLen, AGpsBearerType bearerType) :
                LocMsg(), mAgpsManager(agpsManager), mAgpsType(agpsType), mApnName(
                        new char[apnLen + 1]), mBearerType(bearerType), mAdapter(adapter) {
            if (mApnName == nullptr) {
                LOC_LOGE("AgpsMsgAtlOpenSuccess: new allocation failed, fatal error.");
                // Reporting the failure here
                mAgpsManager->reportAtlClosed(mAgpsType);
                return;
            }
            memcpy(mApnName, apnName, apnLen);
            mApnName[apnLen] = 0;
        }

        inline ~AgpsMsgAtlOpenSuccess() {
            delete[] mApnName;
        }

        inline virtual void proc() const {
            LOC_LOGV("AgpsMsgAtlOpenSuccess::proc()");
            string apn(mApnName);
            //Use QMI WDS API to query IP Protocol from modem profile
            void* libHandle = nullptr;
            getPdnTypeFromWds* getPdnTypeFunc = (getPdnTypeFromWds*)dlGetSymFromLib(libHandle,
            #ifdef USE_GLIB
                    "libloc_api_wds.so", "_Z10getPdnTypeRKNSt7__cxx1112basic_string"\
                    "IcSt11char_traitsIcESaIcEEESt8functionIFviEE");
            #else
                    "libloc_api_wds.so", "_Z10getPdnTypeRKNSt3__112basic_stringIcNS_11char_traits"\
                    "IcEENS_9allocatorIcEEEENS_8functionIFviEEE");
            #endif

            std::function<void(int)> wdsPdnTypeCb = std::bind(&GnssAdapter::reportPdnTypeFromWds,
                    &mAdapter, std::placeholders::_1, mAgpsType, apn, mBearerType);
           if (getPdnTypeFunc != nullptr && apn.length() > 0) {
               LOC_LOGv("AgpsMsgAtlOpenSuccess: dlGetSymFromLib success");
               (*getPdnTypeFunc)(apn, wdsPdnTypeCb);
           } else {
               mAgpsManager->reportAtlOpenSuccess(mAgpsType, mApnName, apn.length(), mBearerType);
           }
        }
    };
    // Added inital length checks for apnlen check to avoid security issues
    // In case of failure reporting the same
    if (NULL == apnName || apnLen > MAX_APN_LEN || (strlen(apnName) != apnLen)) {
        LOC_LOGe("incorrect apnlen length or incorrect apnName");
        mAgpsManager.reportAtlClosed(agpsType);
    } else {
        sendMsg( new AgpsMsgAtlOpenSuccess(*this,
                    &mAgpsManager, agpsType, apnName, apnLen, bearerType));
    }
}

void GnssAdapter::dataConnClosedCommand(AGpsExtType agpsType){

    LOC_LOGI("GnssAdapter::frameworkDataConnClosed");

    struct AgpsMsgAtlClosed: public LocMsg {

        AgpsManager* mAgpsManager;
        AGpsExtType mAgpsType;

        inline AgpsMsgAtlClosed(AgpsManager* agpsManager, AGpsExtType agpsType) :
                LocMsg(), mAgpsManager(agpsManager), mAgpsType(agpsType) {
        }

        inline virtual void proc() const {
            LOC_LOGV("AgpsMsgAtlClosed::proc()");
            mAgpsManager->reportAtlClosed(mAgpsType);
        }
    };

    sendMsg( new AgpsMsgAtlClosed(&mAgpsManager, (AGpsExtType)agpsType));
}

void GnssAdapter::dataConnFailedCommand(AGpsExtType agpsType){

    LOC_LOGi();

    struct AgpsMsgAtlOpenFailed: public LocMsg {

        AgpsManager* mAgpsManager;
        AGpsExtType mAgpsType;

        inline AgpsMsgAtlOpenFailed(AgpsManager* agpsManager, AGpsExtType agpsType) :
                LocMsg(), mAgpsManager(agpsManager), mAgpsType(agpsType) {
        }

        inline virtual void proc() const {
            LOC_LOGV("AgpsMsgAtlOpenFailed::proc()");
            mAgpsManager->reportAtlOpenFailed(mAgpsType);
        }
    };

    sendMsg( new AgpsMsgAtlOpenFailed(&mAgpsManager, (AGpsExtType)agpsType));
}

void GnssAdapter::convertSatelliteInfo(std::vector<GnssDebugSatelliteInfo>& out,
                                       const GnssSvType& in_constellation,
                                       const SystemStatusReports& in)
{
    uint64_t sv_mask = 0ULL;
    uint32_t svid_min = 0;
    uint32_t svid_num = 0;
    uint32_t svid_idx = 0;

    uint64_t eph_health_good_mask = 0ULL;
    uint64_t eph_health_bad_mask = 0ULL;
    uint64_t server_perdiction_available_mask = 0ULL;
    float server_perdiction_age = 0.0f;

    // set constellationi based parameters
    switch (in_constellation) {
        case GNSS_SV_TYPE_GPS:
            svid_min = GNSS_BUGREPORT_GPS_MIN;
            svid_num = GPS_NUM;
            svid_idx = 0;
            if (!in.mSvHealth.empty()) {
                eph_health_good_mask = in.mSvHealth.back().mGpsGoodMask;
                eph_health_bad_mask  = in.mSvHealth.back().mGpsBadMask;
            }
            if (!in.mXtra.empty()) {
                server_perdiction_available_mask = in.mXtra.back().mGpsXtraValid;
                server_perdiction_age = (float)(in.mXtra.back().mGpsXtraAge);
            }
            break;
        case GNSS_SV_TYPE_GLONASS:
            svid_min = GNSS_BUGREPORT_GLO_MIN;
            svid_num = GLO_NUM;
            svid_idx = GPS_NUM;
            if (!in.mSvHealth.empty()) {
                eph_health_good_mask = in.mSvHealth.back().mGloGoodMask;
                eph_health_bad_mask  = in.mSvHealth.back().mGloBadMask;
            }
            if (!in.mXtra.empty()) {
                server_perdiction_available_mask = in.mXtra.back().mGloXtraValid;
                server_perdiction_age = (float)(in.mXtra.back().mGloXtraAge);
            }
            break;
        case GNSS_SV_TYPE_QZSS:
            svid_min = GNSS_BUGREPORT_QZSS_MIN;
            svid_num = QZSS_NUM;
            svid_idx = GPS_NUM+GLO_NUM+BDS_NUM+GAL_NUM;
            if (!in.mSvHealth.empty()) {
                eph_health_good_mask = in.mSvHealth.back().mQzssGoodMask;
                eph_health_bad_mask  = in.mSvHealth.back().mQzssBadMask;
            }
            if (!in.mXtra.empty()) {
                server_perdiction_available_mask = in.mXtra.back().mQzssXtraValid;
                server_perdiction_age = (float)(in.mXtra.back().mQzssXtraAge);
            }
            break;
        case GNSS_SV_TYPE_BEIDOU:
            svid_min = GNSS_BUGREPORT_BDS_MIN;
            svid_num = BDS_NUM;
            svid_idx = GPS_NUM+GLO_NUM;
            if (!in.mSvHealth.empty()) {
                eph_health_good_mask = in.mSvHealth.back().mBdsGoodMask;
                eph_health_bad_mask  = in.mSvHealth.back().mBdsBadMask;
            }
            if (!in.mXtra.empty()) {
                server_perdiction_available_mask = in.mXtra.back().mBdsXtraValid;
                server_perdiction_age = (float)(in.mXtra.back().mBdsXtraAge);
            }
            break;
        case GNSS_SV_TYPE_GALILEO:
            svid_min = GNSS_BUGREPORT_GAL_MIN;
            svid_num = GAL_NUM;
            svid_idx = GPS_NUM+GLO_NUM+BDS_NUM;
            if (!in.mSvHealth.empty()) {
                eph_health_good_mask = in.mSvHealth.back().mGalGoodMask;
                eph_health_bad_mask  = in.mSvHealth.back().mGalBadMask;
            }
            if (!in.mXtra.empty()) {
                server_perdiction_available_mask = in.mXtra.back().mGalXtraValid;
                server_perdiction_age = (float)(in.mXtra.back().mGalXtraAge);
            }
            break;
        case GNSS_SV_TYPE_NAVIC:
            svid_min = GNSS_BUGREPORT_NAVIC_MIN;
            svid_num = NAVIC_NUM;
            svid_idx = GPS_NUM+GLO_NUM+QZSS_NUM+BDS_NUM+GAL_NUM;
            if (!in.mSvHealth.empty()) {
                eph_health_good_mask = in.mSvHealth.back().mNavicGoodMask;
                eph_health_bad_mask  = in.mSvHealth.back().mNavicBadMask;
            }
            if (!in.mXtra.empty()) {
                server_perdiction_available_mask = in.mXtra.back().mNavicXtraValid;
                server_perdiction_age = (float)(in.mXtra.back().mNavicXtraAge);
            }
            break;
        default:
            return;
    }

    // extract each sv info from systemstatus report
    for (uint32_t i=0; i<svid_num && (svid_idx+i)<SV_ALL_NUM; i++) {

        GnssDebugSatelliteInfo s = {};
        s.size = sizeof(s);
        s.svid = i + svid_min;
        s.constellation = in_constellation;

        if (!in.mNavData.empty()) {
            s.mEphemerisType   = in.mNavData.back().mNav[svid_idx+i].mType;
            s.mEphemerisSource = in.mNavData.back().mNav[svid_idx+i].mSource;
        }
        else {
            s.mEphemerisType   = GNSS_EPH_TYPE_UNKNOWN;
            s.mEphemerisSource = GNSS_EPH_SOURCE_UNKNOWN;
        }

        sv_mask = 0x1ULL << i;
        if (eph_health_good_mask & sv_mask) {
            s.mEphemerisHealth = GNSS_EPH_HEALTH_GOOD;
        }
        else if (eph_health_bad_mask & sv_mask) {
            s.mEphemerisHealth = GNSS_EPH_HEALTH_BAD;
        }
        else {
            s.mEphemerisHealth = GNSS_EPH_HEALTH_UNKNOWN;
        }

        if (!in.mNavData.empty()) {
            s.ephemerisAgeSeconds =
                (float)(in.mNavData.back().mNav[svid_idx+i].mAgeSec);
        }
        else {
            s.ephemerisAgeSeconds = 0.0f;
        }

        if (server_perdiction_available_mask & sv_mask) {
            s.serverPredictionIsAvailable = true;
        }
        else {
            s.serverPredictionIsAvailable = false;
        }

        s.serverPredictionAgeSeconds = server_perdiction_age;
        out.push_back(s);
    }

    return;
}

bool GnssAdapter::getDebugReport(GnssDebugReport& r)
{
    SystemStatus* systemstatus = getSystemStatus();
    if (nullptr == systemstatus) {
        return false;
    }

    SystemStatusReports reports;
    systemstatus->getReport(reports, true, false);

    r.size = sizeof(r);

    // location block
    r.mLocation.size = sizeof(r.mLocation);
    if (!reports.mLocation.empty() && reports.mLocation.back().mValid) {
        r.mLocation.mValid = true;
        r.mLocation.mLocation.latitude =
            reports.mLocation.back().mLocation.gpsLocation.latitude;
        r.mLocation.mLocation.longitude =
            reports.mLocation.back().mLocation.gpsLocation.longitude;
        r.mLocation.mLocation.altitude =
            reports.mLocation.back().mLocation.gpsLocation.altitude;
        r.mLocation.mLocation.speed =
            (double)(reports.mLocation.back().mLocation.gpsLocation.speed);
        r.mLocation.mLocation.bearing =
            (double)(reports.mLocation.back().mLocation.gpsLocation.bearing);
        r.mLocation.mLocation.accuracy =
            (double)(reports.mLocation.back().mLocation.gpsLocation.accuracy);

        r.mLocation.verticalAccuracyMeters =
            reports.mLocation.back().mLocationEx.vert_unc;
        r.mLocation.speedAccuracyMetersPerSecond =
            reports.mLocation.back().mLocationEx.speed_unc;
        r.mLocation.bearingAccuracyDegrees =
            reports.mLocation.back().mLocationEx.bearing_unc;

        r.mLocation.mUtcReported =
            reports.mLocation.back().mUtcReported;
    }
    else if (!reports.mBestPosition.empty() && reports.mBestPosition.back().mValid) {
        r.mLocation.mValid = true;
        r.mLocation.mLocation.latitude =
                (double)(reports.mBestPosition.back().mBestLat) * RAD2DEG;
        r.mLocation.mLocation.longitude =
                (double)(reports.mBestPosition.back().mBestLon) * RAD2DEG;
        r.mLocation.mLocation.altitude = reports.mBestPosition.back().mBestAlt;
        r.mLocation.mLocation.accuracy =
                (double)(reports.mBestPosition.back().mBestHepe);

        r.mLocation.mUtcReported = reports.mBestPosition.back().mUtcReported;
    }
    else {
        r.mLocation.mValid = false;
    }

    if (r.mLocation.mValid) {
        LOC_LOGa("lat=%f lon=%f alt=%f speed=%f",
            r.mLocation.mLocation.latitude,
            r.mLocation.mLocation.longitude,
            r.mLocation.mLocation.altitude,
            r.mLocation.mLocation.speed);
    }

    // time block
    r.mTime.size = sizeof(r.mTime);
    if (!reports.mTimeAndClock.empty() && reports.mTimeAndClock.back().mTimeValid) {
        r.mTime.mValid = true;
        r.mTime.timeEstimate =
            (((int64_t)(reports.mTimeAndClock.back().mGpsWeek)*7 +
                        GNSS_UTC_TIME_OFFSET)*24*60*60 -
              (int64_t)(reports.mTimeAndClock.back().mLeapSeconds))*1000ULL +
              (int64_t)(reports.mTimeAndClock.back().mGpsTowMs);

        if (reports.mTimeAndClock.back().mTimeUncNs > 0) {
            // TimeUncNs value is available
            r.mTime.timeUncertaintyNs =
                    (float)(reports.mTimeAndClock.back().mLeapSecUnc)*1000.0f +
                    (float)(reports.mTimeAndClock.back().mTimeUncNs);
        } else {
            // fall back to legacy TimeUnc
            r.mTime.timeUncertaintyNs =
                    ((float)(reports.mTimeAndClock.back().mTimeUnc) +
                     (float)(reports.mTimeAndClock.back().mLeapSecUnc))*1000.0f;
        }

        r.mTime.frequencyUncertaintyNsPerSec =
            (float)(reports.mTimeAndClock.back().mClockFreqBiasUnc);
        LOC_LOGa("timeestimate=%" PRIu64 " unc=%f frequnc=%f",
                r.mTime.timeEstimate,
                r.mTime.timeUncertaintyNs, r.mTime.frequencyUncertaintyNsPerSec);
    }
    else {
        r.mTime.mValid = false;
    }

    // satellite info block
    convertSatelliteInfo(r.mSatelliteInfo, GNSS_SV_TYPE_GPS, reports);
    convertSatelliteInfo(r.mSatelliteInfo, GNSS_SV_TYPE_GLONASS, reports);
    convertSatelliteInfo(r.mSatelliteInfo, GNSS_SV_TYPE_QZSS, reports);
    convertSatelliteInfo(r.mSatelliteInfo, GNSS_SV_TYPE_BEIDOU, reports);
    convertSatelliteInfo(r.mSatelliteInfo, GNSS_SV_TYPE_GALILEO, reports);
    convertSatelliteInfo(r.mSatelliteInfo, GNSS_SV_TYPE_NAVIC, reports);
    LOC_LOGa("satellite=%zu", r.mSatelliteInfo.size());

    return true;
}

/* get AGC information from system status and fill it */
void
GnssAdapter::getAgcInformation(GnssMeasurementsNotification& measurements, int msInWeek)
{
    SystemStatus* systemstatus = getSystemStatus();

    if (nullptr != systemstatus) {
        SystemStatusReports reports = {};
        systemstatus->getReport(reports, true, false);

        if ((!reports.mRfAndParams.empty()) && (!reports.mTimeAndClock.empty()) &&
            (abs(msInWeek - (int)reports.mTimeAndClock.back().mGpsTowMs) < 2000)) {

            for (size_t i = 0; i < measurements.count; i++) {
                switch (measurements.measurements[i].svType) {
                case GNSS_SV_TYPE_GPS:
                case GNSS_SV_TYPE_QZSS:
                    measurements.measurements[i].agcLevelDb =
                            -((double)reports.mRfAndParams.back().mJammerGps / 100.0);
                    measurements.measurements[i].flags |=
                            GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT;
                    break;

                case GNSS_SV_TYPE_GALILEO:
                    measurements.measurements[i].agcLevelDb =
                            -((double)reports.mRfAndParams.back().mJammerGal / 100.0);
                    measurements.measurements[i].flags |=
                            GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT;
                    break;

                case GNSS_SV_TYPE_GLONASS:
                    measurements.measurements[i].agcLevelDb =
                            -((double)reports.mRfAndParams.back().mJammerGlo / 100.0);
                    measurements.measurements[i].flags |=
                            GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT;
                    break;

                case GNSS_SV_TYPE_BEIDOU:
                    measurements.measurements[i].agcLevelDb =
                            -((double)reports.mRfAndParams.back().mJammerBds / 100.0);
                    measurements.measurements[i].flags |=
                            GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT;
                    break;

                case GNSS_SV_TYPE_SBAS:
                case GNSS_SV_TYPE_UNKNOWN:
                default:
                    break;
                }
            }
        }
    }
}

/* get Data information from system status and fill it */
void
GnssAdapter::getDataInformation(GnssDataNotification& data, int msInWeek)
{
    SystemStatus* systemstatus = getSystemStatus();

    LOC_LOGa("msInWeek=%d", msInWeek);
    if (nullptr != systemstatus) {
        SystemStatusReports reports = {};
        systemstatus->getReport(reports, true, false);

        if ((!reports.mRfAndParams.empty()) && (!reports.mTimeAndClock.empty()) &&
            (abs(msInWeek - (int)reports.mTimeAndClock.back().mGpsTowMs) < 2000)) {
            int maxSig = std::min((int)GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES,
                    (int)reports.mRfAndParams.back().mJammerInd.size());
            for (int sig = GNSS_LOC_SIGNAL_TYPE_GPS_L1CA; sig < maxSig; sig++) {
                data.gnssDataMask[sig] = 0;
                data.jammerInd[sig] = 0.0;
                data.agc[sig] = 0.0;
               if (GNSS_INVALID_JAMMER_IND !=
                       reports.mRfAndParams.back().mJammerInd[sig]) {
                   data.jammerInd[sig] =
                           (double)reports.mRfAndParams.back().mJammerInd[sig];
                   data.agc[sig] = -(double)reports.mRfAndParams.back().mJammerInd[sig];
                   data.gnssDataMask[sig] |= (GNSS_LOC_DATA_JAMMER_IND_BIT | GNSS_LOC_DATA_AGC_BIT);
               }
            }

            if (GNSS_INVALID_JAMMER_IND != reports.mRfAndParams.back().mJammerGps) {
                data.gnssDataMask[GNSS_LOC_SIGNAL_TYPE_GPS_L1CA] |=
                        GNSS_LOC_DATA_AGC_BIT | GNSS_LOC_DATA_JAMMER_IND_BIT;
                data.agc[GNSS_LOC_SIGNAL_TYPE_GPS_L1CA] =
                        -((double)reports.mRfAndParams.back().mJammerGps / 100.0);
                data.jammerInd[GNSS_LOC_SIGNAL_TYPE_GPS_L1CA] =
                        ((double)reports.mRfAndParams.back().mJammerGps / 100.0);
                data.gnssDataMask[GNSS_LOC_SIGNAL_TYPE_QZSS_L1CA] |=
                        GNSS_LOC_DATA_AGC_BIT | GNSS_LOC_DATA_JAMMER_IND_BIT;
                data.agc[GNSS_LOC_SIGNAL_TYPE_QZSS_L1CA] =
                        -((double)reports.mRfAndParams.back().mJammerGps / 100.0);
                data.jammerInd[GNSS_LOC_SIGNAL_TYPE_QZSS_L1CA] =
                        ((double)reports.mRfAndParams.back().mJammerGps / 100.0);
                data.gnssDataMask[GNSS_LOC_SIGNAL_TYPE_SBAS_L1_CA] |=
                        GNSS_LOC_DATA_AGC_BIT | GNSS_LOC_DATA_JAMMER_IND_BIT;
                data.agc[GNSS_LOC_SIGNAL_TYPE_SBAS_L1_CA] =
                        -((double)reports.mRfAndParams.back().mJammerGps / 100.0);
                data.jammerInd[GNSS_LOC_SIGNAL_TYPE_SBAS_L1_CA] =
                        ((double)reports.mRfAndParams.back().mJammerGps / 100.0);
            }
            if (GNSS_INVALID_JAMMER_IND != reports.mRfAndParams.back().mJammerGlo) {
                data.gnssDataMask[GNSS_LOC_SIGNAL_TYPE_GLONASS_G1] |=
                        GNSS_LOC_DATA_AGC_BIT | GNSS_LOC_DATA_JAMMER_IND_BIT;
                data.agc[GNSS_LOC_SIGNAL_TYPE_GLONASS_G1] =
                        -((double)reports.mRfAndParams.back().mJammerGlo / 100.0);
                data.jammerInd[GNSS_LOC_SIGNAL_TYPE_GLONASS_G1] =
                        ((double)reports.mRfAndParams.back().mJammerGlo / 100.0);
            }
            if (GNSS_INVALID_JAMMER_IND != reports.mRfAndParams.back().mJammerBds) {
                data.gnssDataMask[GNSS_LOC_SIGNAL_TYPE_BEIDOU_B1_I] |=
                        GNSS_LOC_DATA_AGC_BIT | GNSS_LOC_DATA_JAMMER_IND_BIT;
                data.agc[GNSS_LOC_SIGNAL_TYPE_BEIDOU_B1_I] =
                        -((double)reports.mRfAndParams.back().mJammerBds / 100.0);
                data.jammerInd[GNSS_LOC_SIGNAL_TYPE_BEIDOU_B1_I] =
                        ((double)reports.mRfAndParams.back().mJammerBds / 100.0);
            }
            if (GNSS_INVALID_JAMMER_IND != reports.mRfAndParams.back().mJammerGal) {
                data.gnssDataMask[GNSS_LOC_SIGNAL_TYPE_GALILEO_E1_C] |=
                        GNSS_LOC_DATA_AGC_BIT | GNSS_LOC_DATA_JAMMER_IND_BIT;
                data.agc[GNSS_LOC_SIGNAL_TYPE_GALILEO_E1_C] =
                        -((double)reports.mRfAndParams.back().mJammerGal / 100.0);
                data.jammerInd[GNSS_LOC_SIGNAL_TYPE_GALILEO_E1_C] =
                        ((double)reports.mRfAndParams.back().mJammerGal / 100.0);
            }
        }
    }
}

/* Callbacks registered with loc_net_iface library */
static void agpsOpenResultCb (bool isSuccess, AGpsExtType agpsType, const char* apn,
        AGpsBearerType bearerType, void* userDataPtr) {

    LOC_LOGd("isSuccess %d, agpsType %d", isSuccess, agpsType);
    if (userDataPtr == nullptr) {
        LOC_LOGe("userDataPtr is nullptr.");
        return;
    }
    if (apn == nullptr) {
        LOC_LOGe("apn is nullptr.");
        return;
    }
    GnssAdapter* adapter = (GnssAdapter*)userDataPtr;
    if (isSuccess) {
        adapter->dataConnOpenCommand(agpsType, apn, strlen(apn), bearerType);
    } else {
        adapter->dataConnFailedCommand(agpsType);
    }
}

static void agpsCloseResultCb (bool isSuccess, AGpsExtType agpsType, void* userDataPtr) {
    LOC_LOGd("isSuccess %d, agpsType %d", isSuccess, agpsType);
    if (userDataPtr == nullptr) {
        LOC_LOGe("userDataPtr is nullptr.");
        return;
    }
    GnssAdapter* adapter = (GnssAdapter*)userDataPtr;
    if (isSuccess) {
        adapter->dataConnClosedCommand(agpsType);
    } else {
        adapter->dataConnFailedCommand(agpsType);
    }
}

void
GnssAdapter::saveGnssEnergyConsumedCallback(GnssEnergyConsumedCallback energyConsumedCb) {
    mGnssEnergyConsumedCb = energyConsumedCb;
}

void
GnssAdapter::getGnssEnergyConsumedCommand(GnssEnergyConsumedCallback energyConsumedCb) {
    struct MsgGetGnssEnergyConsumed : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        GnssEnergyConsumedCallback mEnergyConsumedCb;
        inline MsgGetGnssEnergyConsumed(GnssAdapter& adapter, LocApiBase& api,
                                        GnssEnergyConsumedCallback energyConsumedCb) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mEnergyConsumedCb(energyConsumedCb){}

        inline virtual void proc() const {
            mAdapter.saveGnssEnergyConsumedCallback(mEnergyConsumedCb);
            mApi.getGnssEnergyConsumed();
        }
    };

    sendMsg(new MsgGetGnssEnergyConsumed(*this, *mLocApi, energyConsumedCb));
}

uint32_t GnssAdapter::getNfwControlBits(const std::vector<std::string>& enabledNfws) {
    uint32_t nfwControlBits = 0;

    for (auto& pkgName : enabledNfws) {
        auto nfw = mNfws.find(pkgName);
        if (mNfws.end() != nfw) {
            nfwControlBits |= nfw->second;
        }
    }

    nfwControlBits = ~nfwControlBits & GNSS_CONFIG_GPS_LOCK_NFW_ALL;
    LOC_LOGd("nfwControlBits=0x%X", nfwControlBits);
    return nfwControlBits;
}

void
GnssAdapter::nfwControlCommand(const std::vector<std::string>& enabledNfws) {
    struct MsgControlNfwLocationAccess : public LocMsg {
        GnssAdapter& mAdapter;
        LocApiBase& mApi;
        const std::vector<std::string> mEnabledNfws;
        inline MsgControlNfwLocationAccess(GnssAdapter& adapter, LocApiBase& api,
            const std::vector<std::string>& enabledNfws) :
            LocMsg(),
            mAdapter(adapter),
            mApi(api),
            mEnabledNfws(std::move(enabledNfws)) {}
        inline virtual void proc() const {
            if (!mAdapter.isEngineCapabilitiesKnown()) {
                mAdapter.mPendingMsgs.push_back(
                        new MsgControlNfwLocationAccess(mAdapter, mApi, mEnabledNfws));
                return;
            }

            if (mAdapter.mSupportNfwControl) {
                GnssConfigGpsLock gpsLock;

                uint32_t nfwControlBits;
                nfwControlBits = mAdapter.getNfwControlBits(mEnabledNfws);
                gpsLock = ContextBase::mGps_conf.GPS_LOCK;
                gpsLock &= GNSS_CONFIG_GPS_LOCK_MO;
                gpsLock |= nfwControlBits;
                ContextBase::mGps_conf.GPS_LOCK = gpsLock;

                LOC_LOGD("MsgControlNfwLocationAccess gpsLock = 0x%X nfwControlBits = 0x%X",
                         gpsLock, nfwControlBits);
                mApi.sendMsg(new LocApiMsg([&mApi = mApi, gpsLock]() {
                             mApi.setGpsLockSync((GnssConfigGpsLock)gpsLock);
                }));
            } else {
                LOC_LOGW("MsgControlNfwLocationAccess, NFW control is not supported, "
                         "do not use this for NFW status");
            }
        }
    };

    sendMsg(new MsgControlNfwLocationAccess(*this, *mLocApi, enabledNfws));

}

// Set tunc constrained mode, use 0 session id to indicate
// that no callback is needed. Session id 0 is used for calls that
// are not invoked from the integration api, e.g.: initial configuration
// from the configure file
void
GnssAdapter::setConstrainedTunc(bool enable, float tuncConstraint,
                                uint32_t energyBudget, uint32_t sessionId) {

    mLocConfigInfo.tuncConfigInfo.isValid = true;
    mLocConfigInfo.tuncConfigInfo.enable = enable;
    mLocConfigInfo.tuncConfigInfo.tuncThresholdMs = tuncConstraint;
    mLocConfigInfo.tuncConfigInfo.energyBudget = energyBudget;

    LocApiResponse* locApiResponse = nullptr;
    if (sessionId != 0) {
        locApiResponse =
                new LocApiResponse(*getContext(),
                                   [this, sessionId] (LocationError err) {
                                    reportResponse(err, sessionId);});
        if (!locApiResponse) {
            LOC_LOGe("memory alloc failed");
        }
    }
    mLocApi->setConstrainedTuncMode(
            enable, tuncConstraint, energyBudget, locApiResponse);
}

uint32_t
GnssAdapter::setConstrainedTuncCommand (bool enable, float tuncConstraint,
                                        uint32_t energyBudget) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgEnableTUNC : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t mSessionId;
        bool mEnable;
        float mTuncConstraint;
        uint32_t mEnergyBudget;

        inline MsgEnableTUNC(GnssAdapter& adapter,
                             uint32_t sessionId,
                             bool enable,
                             float tuncConstraint,
                             uint32_t energyBudget) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mEnable(enable),
            mTuncConstraint(tuncConstraint),
            mEnergyBudget(energyBudget) {}
        inline virtual void proc() const {
            mAdapter.setConstrainedTunc(mEnable, mTuncConstraint,
                                        mEnergyBudget, mSessionId);
        }
    };

    sendMsg(new MsgEnableTUNC(*this, sessionId, enable,
                              tuncConstraint, energyBudget));

    return sessionId;
}

// Set position assisted clock estimator, use 0 session id to indicate
// that no callback is needed. Session id 0 is used for calls that are
// not invoked from the integration api, e.g.: initial configuration
// from the configure file.
void
GnssAdapter::setPositionAssistedClockEstimator(bool enable,
                                               uint32_t sessionId) {

    mLocConfigInfo.paceConfigInfo.isValid = true;
    mLocConfigInfo.paceConfigInfo.enable = enable;
    LocApiResponse* locApiResponse = nullptr;
    if (sessionId != 0) {
        locApiResponse =
                new LocApiResponse(*getContext(),
                                   [this, sessionId] (LocationError err) {
                                   reportResponse(err, sessionId);});
        if (!locApiResponse) {
            LOC_LOGe("memory alloc failed");
        }
    }
    mLocApi->setPositionAssistedClockEstimatorMode(enable, locApiResponse);
}

uint32_t
GnssAdapter::setPositionAssistedClockEstimatorCommand(bool enable) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgEnablePACE : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t mSessionId;
        bool mEnable;
        inline MsgEnablePACE(GnssAdapter& adapter,
                             uint32_t sessionId, bool enable) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mEnable(enable){}
        inline virtual void proc() const {
            mAdapter.setPositionAssistedClockEstimator(mEnable, mSessionId);
        }
    };

    sendMsg(new MsgEnablePACE(*this, sessionId, enable));
    return sessionId;
}

void GnssAdapter::gnssUpdateSvConfig(
        uint32_t sessionId, const GnssSvTypeConfig& constellationEnablementConfig,
        const GnssSvIdConfig&   blacklistSvConfig) {
    GnssSvTypeConfig currentSvTypeConfig = gnssCombineSvTypeConfigs();
    // suspend all tracking sessions to apply the constellation config
    suspendSessions();
    if (constellationEnablementConfig.size == sizeof(constellationEnablementConfig)) {
        // check whether if any constellation is removed from the new config
        GnssSvTypesMask currentEnabledMask = currentSvTypeConfig.enabledSvTypesMask;
        GnssSvTypesMask newEnabledMask = constellationEnablementConfig.enabledSvTypesMask;
        GnssSvTypesMask enabledRemoved = currentEnabledMask & (currentEnabledMask ^ newEnabledMask);
        // Send reset if any constellation is removed from the enabled list
        if (enabledRemoved != 0) {
            mLocApi->resetConstellationControl();
        }

        // if the constellation config is valid, issue request to modem
        // to enable/disable constellation
        mLocApi->setConstellationControl(currentSvTypeConfig);
    } else if (constellationEnablementConfig.size == 0) {
        // when the size is not set, meaning reset to modem default
        mLocApi->resetConstellationControl();
    }
    // save the constellation settings to be used for modem SSR
    gnssSetSvTypeConfig(constellationEnablementConfig, SV_TYPE_CONFIG_FROM_API);

    // handle blacklisted SV settings
    mGnssSvIdConfig   = blacklistSvConfig;
    // process blacklist svs info
    mBlacklistedSvIds.clear();
    // need to save the balcklisted sv info into mBlacklistedSvIds as well
    convertFromGnssSvIdConfig(blacklistSvConfig, mBlacklistedSvIds);
    LocApiResponse* locApiResponse = new LocApiResponse(*getContext(),
            [this, sessionId] (LocationError err) {
            reportResponse(err, sessionId);});
    if (!locApiResponse) {
        LOC_LOGe("memory alloc failed");
    }
    mLocApi->setBlacklistSv(mGnssSvIdConfig, locApiResponse);

    // resume all tracking sessions after the constellation config has been applied
    restartSessions(false);
}

uint32_t
GnssAdapter::gnssUpdateSvConfigCommand(
        const GnssSvTypeConfig& constellationEnablementConfig,
        const GnssSvIdConfig& blacklistSvConfig) {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgUpdateSvConfig : public LocMsg {
        GnssAdapter&     mAdapter;
        uint32_t         mSessionId;
        GnssSvTypeConfig mConstellationEnablementConfig;
        GnssSvIdConfig   mBlacklistSvIdConfig;

        inline MsgUpdateSvConfig(GnssAdapter& adapter,
                                 uint32_t sessionId,
                                 const GnssSvTypeConfig& constellationEnablementConfig,
                                 const GnssSvIdConfig& blacklistSvConfig) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mConstellationEnablementConfig(constellationEnablementConfig),
            mBlacklistSvIdConfig(blacklistSvConfig) {}
        inline virtual void proc() const {
            mAdapter.gnssUpdateSvConfig(mSessionId, mConstellationEnablementConfig,
                                        mBlacklistSvIdConfig);
        }
    };

    if (sessionId != 0) {
        sendMsg(new MsgUpdateSvConfig(*this, sessionId, constellationEnablementConfig,
                                      blacklistSvConfig));
    }
    return sessionId;
}

void GnssAdapter::gnssUpdateSecondaryBandConfig(
        uint32_t sessionId, const GnssSvTypeConfig& secondaryBandConfig) {

    LocApiResponse* locApiResponse = new LocApiResponse(*getContext(),
            [this, sessionId] (LocationError err) {
            reportResponse(err, sessionId);});
    if (!locApiResponse) {
        LOC_LOGe("memory alloc failed");
    }

    // handle secondary band info
    mGnssSeconaryBandConfig = secondaryBandConfig;
    gnssSecondaryBandConfigUpdate(locApiResponse);
}

uint32_t
GnssAdapter::gnssUpdateSecondaryBandConfigCommand(
        const GnssSvTypeConfig& secondaryBandConfig) {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgUpdateSecondaryBandConfig : public LocMsg {
        GnssAdapter&     mAdapter;
        uint32_t         mSessionId;
        GnssSvTypeConfig mSecondaryBandConfig;

        inline MsgUpdateSecondaryBandConfig(GnssAdapter& adapter,
                                 uint32_t sessionId,
                                 const GnssSvTypeConfig& secondaryBandConfig) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mSecondaryBandConfig(secondaryBandConfig) {}
        inline virtual void proc() const {
            mAdapter.gnssUpdateSecondaryBandConfig(mSessionId,  mSecondaryBandConfig);
        }
    };

    if (sessionId != 0) {
        sendMsg(new MsgUpdateSecondaryBandConfig(*this, sessionId, secondaryBandConfig));
    }
    return sessionId;
}

// This function currently retrieves secondary band configuration
// for constellation enablement/disablement.
void
GnssAdapter::gnssGetSecondaryBandConfig(uint32_t sessionId) {

    LocApiResponse* locApiResponse = new LocApiResponse(*getContext(),
            [this, sessionId] (LocationError err) {
            reportResponse(err, sessionId);});
    if (!locApiResponse) {
        LOC_LOGe("memory alloc failed");
    }

    mLocApi->getConstellationMultiBandConfig(sessionId, locApiResponse);
}

uint32_t
GnssAdapter::gnssGetSecondaryBandConfigCommand() {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgGetSecondaryBandConfig : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t     mSessionId;
        inline MsgGetSecondaryBandConfig(GnssAdapter& adapter,
                              uint32_t sessionId) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId) {}
        inline virtual void proc() const {
            mAdapter.gnssGetSecondaryBandConfig(mSessionId);
        }
    };

    if (sessionId != 0) {
        sendMsg(new MsgGetSecondaryBandConfig(*this, sessionId));
    }
    return sessionId;
}

void
GnssAdapter::configLeverArm(uint32_t sessionId,
                            const LeverArmConfigInfo& configInfo) {

    LocationError err = LOCATION_ERROR_NOT_SUPPORTED;

    // save the lever ARM config info for translating SPE positions from
    // GNSS antenna based to VRP based
    if (configInfo.leverArmValidMask & LEVER_ARM_TYPE_GNSS_TO_VRP_BIT) {
        mLocConfigInfo.leverArmConfigInfo.leverArmValidMask |=
                LEVER_ARM_TYPE_GNSS_TO_VRP_BIT;
        mLocConfigInfo.leverArmConfigInfo.gnssToVRP = configInfo.gnssToVRP;
        err = LOCATION_ERROR_SUCCESS;
    }

    if (true == mEngHubLoadSuccessful) {
        if (false == mEngHubProxy->configLeverArm(configInfo)) {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        } else {
            err = LOCATION_ERROR_SUCCESS;
        }
    }

    reportResponse(err, sessionId);
}

uint32_t
GnssAdapter::configLeverArmCommand(const LeverArmConfigInfo& configInfo) {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgConfigLeverArm : public LocMsg {
        GnssAdapter&       mAdapter;
        uint32_t           mSessionId;
        LeverArmConfigInfo mConfigInfo;

        inline MsgConfigLeverArm(GnssAdapter& adapter,
                                 uint32_t sessionId,
                                 const LeverArmConfigInfo& configInfo) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mConfigInfo(configInfo) {}
        inline virtual void proc() const {
            mAdapter.configLeverArm(mSessionId, mConfigInfo);
        }
    };

    sendMsg(new MsgConfigLeverArm(*this, sessionId, configInfo));
    return sessionId;
}

bool GnssAdapter::initMeasCorr(bool bSendCbWhenNotSupported) {
    LOC_LOGv("GnssAdapter::initMeasCorr");
    /* Message to initialize Measurement Corrections */
    struct MsgInitMeasCorr : public LocMsg {
        GnssAdapter& mAdapter;
        GnssMeasurementCorrectionsCapabilitiesMask mCapMask;

        inline MsgInitMeasCorr(GnssAdapter& adapter,
                GnssMeasurementCorrectionsCapabilitiesMask capMask) :
            LocMsg(), mAdapter(adapter), mCapMask(capMask) {
        }

        inline virtual void proc() const {
            LOC_LOGV("MsgInitMeasCorr::proc()");

            mAdapter.mControlCallbacks.measCorrSetCapabilitiesCb(mCapMask);
        }
    };
    if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_MEASUREMENTS_CORRECTION)) {
        sendMsg(new MsgInitMeasCorr(*this, GNSS_MEAS_CORR_LOS_SATS |
                GNSS_MEAS_CORR_EXCESS_PATH_LENGTH | GNSS_MEAS_CORR_REFLECTING_PLANE));
        return true;
    } else {
        LOC_LOGv("MEASUREMENTS_CORRECTION feature is not supported in the modem");
        if (bSendCbWhenNotSupported) {
            sendMsg(new MsgInitMeasCorr(*this, 0));
        }
        return false;
    }
}

bool GnssAdapter::openMeasCorrCommand(const measCorrSetCapabilitiesCallback setCapabilitiesCb) {
    LOC_LOGi("GnssAdapter::openMeasCorrCommand");

    /* Send message to initialize Measurement Corrections */
        mControlCallbacks.measCorrSetCapabilitiesCb = setCapabilitiesCb;
        mIsMeasCorrInterfaceOpen = true;
        if (isEngineCapabilitiesKnown()) {
        LOC_LOGv("Capabilities are known, proceed with measurement corrections init");
            return initMeasCorr(false);
        } else {
        LOC_LOGv("Capabilities are not known, wait for open");
            return true;
        }
}

bool GnssAdapter::measCorrSetCorrectionsCommand(const GnssMeasurementCorrections& gnssMeasCorr) {
    LOC_LOGi("GnssAdapter::measCorrSetCorrectionsCommand");

    /* Message to set Measurement Corrections */
    struct MsgSetCorrectionsMeasCorr : public LocMsg {
        const GnssMeasurementCorrections mGnssMeasCorr;
        GnssAdapter& mAdapter;
        LocApiBase& mApi;

        inline MsgSetCorrectionsMeasCorr(
            const GnssMeasurementCorrections& gnssMeasCorr,
            GnssAdapter& adapter,
            LocApiBase& api) :
            LocMsg(),
            mGnssMeasCorr(gnssMeasCorr),
            mAdapter(adapter),
            mApi(api) {
        }

        inline virtual void proc() const {
            LOC_LOGV("MsgSetCorrectionsMeasCorr::proc()");
            if (HMAC_CONFIG_ENABLED != mAdapter.mHmacConfig) {
                mApi.setMeasurementCorrections(mGnssMeasCorr);
            } else {
                LOC_LOGD("MsgSetCorrectionsMeasCorr: mapDataAvailable is true, "
                         "use MapData for aiding");
            }
        }
    };

    if (HMAC_CONFIG_UNKNOWN == mHmacConfig) {
        unsigned int processListLength = 0;
        loc_process_info_s_type* processInfoList = nullptr;
        int rc = loc_read_process_conf(LOC_PATH_IZAT_CONF, &processListLength,
                                       &processInfoList);
        if (0 == rc) {
            // go over the conf table to see whether any plugin daemon is enabled
            mHmacConfig = HMAC_CONFIG_DISABLED;
            for (unsigned int i = 0; i < processListLength; i++) {
                if ((0 == strncmp(processInfoList[i].name[0], PROCESS_NAME_SAP_MAP,
                            strlen (PROCESS_NAME_SAP_MAP))) &&
                                    (ENABLED == processInfoList[i].proc_status)) {
                    mHmacConfig = HMAC_CONFIG_ENABLED;
                    break;
                }
            }
            char mapDataTestMode[LOC_MAX_PARAM_STRING];
            loc_param_s_type izatMapDataTable[] =
            {
                { "MAP_DATA_TEST_MODE", &mapDataTestMode, NULL, 's' },
            };
            UTIL_READ_CONF(LOC_PATH_IZAT_CONF, izatMapDataTable);
            if (strcmp(mapDataTestMode, "ENABLED") == 0) {
                LOC_LOGd("MAP_DATA_TEST_MODE mode set to ENABLED");
                mHmacConfig = HMAC_CONFIG_TEST_MODE;
            }
        } else {
            LOC_LOGe("failed to parse conf file");
        }
    }

    if (ContextBase::isFeatureSupported(LOC_SUPPORTED_FEATURE_MEASUREMENTS_CORRECTION)) {
        sendMsg(new MsgSetCorrectionsMeasCorr(gnssMeasCorr, *this, *mLocApi));
        return true;
    } else {
        LOC_LOGw("Measurement Corrections are not supported!");
        return false;
    }
}

uint32_t GnssAdapter::getAntennaeInfoCommand(AntennaInfoCallback* antennaInfoCallback) {
    /* Message to get Antenna Information */
    struct MsgReportAi : public LocMsg {
        GnssAdapter& mAdapter;
        AntennaInfoCallback* mAntennaInfoCb;

        inline MsgReportAi(GnssAdapter& adapter, AntennaInfoCallback* cb) :
            LocMsg(), mAdapter(adapter), mAntennaInfoCb(cb) {
        }

        inline virtual void proc() const {
            mAdapter.reportGnssAntennaInformation(mAntennaInfoCb);
        }
    };

    if (nullptr == antennaInfoCallback) {
        LOC_LOGe("NULL antennaInfoCallback");
        return LOCATION_ERROR_GENERAL_FAILURE;
    }
    sendMsg(new MsgReportAi(*this, antennaInfoCallback));
    return LOCATION_ERROR_SUCCESS;
}

void
GnssAdapter::configRobustLocation(uint32_t sessionId,
                                  bool enable, bool enableForE911) {

    mLocConfigInfo.robustLocationConfigInfo.isValid = true;
    mLocConfigInfo.robustLocationConfigInfo.enable = enable;
    mLocConfigInfo.robustLocationConfigInfo.enableFor911 = enableForE911;

    LocApiResponse* locApiResponse = nullptr;
    if (sessionId != 0) {
        locApiResponse =
                new LocApiResponse(*getContext(),
                                   [this, sessionId] (LocationError err) {
                                   reportResponse(err, sessionId);});
        if (!locApiResponse) {
            LOC_LOGe("memory alloc failed");
        }
    }
    mLocApi->configRobustLocation(enable, enableForE911, locApiResponse, true);
}

uint32_t GnssAdapter::configRobustLocationCommand(
        bool enable, bool enableForE911) {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgConfigRobustLocation : public LocMsg {
        GnssAdapter&     mAdapter;
        uint32_t         mSessionId;
        bool             mEnable;
        bool             mEnableForE911;

        inline MsgConfigRobustLocation(GnssAdapter& adapter,
                                uint32_t sessionId,
                                bool     enable,
                                bool     enableForE911) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mEnable(enable),
            mEnableForE911(enableForE911) {}
        inline virtual void proc() const {
            mAdapter.configRobustLocation(mSessionId, mEnable, mEnableForE911);
        }
    };

    sendMsg(new MsgConfigRobustLocation(*this, sessionId, enable, enableForE911));
    return sessionId;
}

void
GnssAdapter::configMinGpsWeek(uint32_t sessionId, uint16_t minGpsWeek) {
    // suspend all sessions for modem to take the min GPS week config
    suspendSessions();

    LocApiResponse* locApiResponse = nullptr;
    if (sessionId != 0) {
        locApiResponse =
                new LocApiResponse(*getContext(),
                                   [this, sessionId] (LocationError err) {
                                   reportResponse(err, sessionId);});
        if (!locApiResponse) {
            LOC_LOGe("memory alloc failed");
        }
    }
    mLocApi->configMinGpsWeek(minGpsWeek, locApiResponse);

    // resume all tracking sessions after the min GPS week config
    // has been changed
    restartSessions(false);
}

uint32_t GnssAdapter::configMinGpsWeekCommand(uint16_t minGpsWeek) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgConfigMinGpsWeek : public LocMsg {
        GnssAdapter&     mAdapter;
        uint32_t         mSessionId;
        uint16_t         mMinGpsWeek;

        inline MsgConfigMinGpsWeek(GnssAdapter& adapter,
                                   uint32_t sessionId,
                                   uint16_t minGpsWeek) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mMinGpsWeek(minGpsWeek) {}
        inline virtual void proc() const {
            mAdapter.configMinGpsWeek(mSessionId, mMinGpsWeek);
        }
    };

    sendMsg(new MsgConfigMinGpsWeek(*this, sessionId, minGpsWeek));
    return sessionId;
}

uint32_t GnssAdapter::configDeadReckoningEngineParamsCommand(
        const DeadReckoningEngineConfig& dreConfig) {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgConfigDrEngine : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t     mSessionId;
        DeadReckoningEngineConfig mDreConfig;

        inline MsgConfigDrEngine(GnssAdapter& adapter,
                                  uint32_t sessionId,
                                  const DeadReckoningEngineConfig& dreConfig) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mDreConfig(dreConfig) {}
        inline virtual void proc() const {
            LocationError err = LOCATION_ERROR_NOT_SUPPORTED;
            if (true == mAdapter.mEngHubProxy->configDeadReckoningEngineParams(mDreConfig)) {
                err = LOCATION_ERROR_SUCCESS;
            }
            mAdapter.reportResponse(err, mSessionId);
        }
    };

    sendMsg(new MsgConfigDrEngine(*this, sessionId, dreConfig));
    return sessionId;
}

void halResponseTimer:: timeOutCallback() {
    mHal->reportResponse(mErr, mSessionID);
}

uint32_t GnssAdapter::configEngineRunStateCommand(
    PositioningEngineMask engType, LocEngineRunState engState) {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u, eng type 0x%x, eng state %d, dre enabled %d",
        sessionId, engType, engState, mEngServiceInfo.dreIntEnabled);

    struct MsgConfigEngineRunState : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t     mSessionId;
        PositioningEngineMask mEngType;
        LocEngineRunState mEngState;

        inline MsgConfigEngineRunState(GnssAdapter& adapter,
            uint32_t sessionId,
            PositioningEngineMask engType,
            LocEngineRunState engState) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mEngType(engType),
            mEngState(engState) {}
        inline virtual void proc() const {
            LocationError err = LOCATION_ERROR_NOT_SUPPORTED;
            // Currently, only DR engine supports pause/resume request
            if ((mEngType == DEAD_RECKONING_ENGINE) &&
                (mAdapter.mEngServiceInfo.dreIntEnabled == true)) {
                if (true == mAdapter.mEngHubProxy->configEngineRunState(mEngType, mEngState)) {
                    err = LOCATION_ERROR_SUCCESS;
                }
            }
            if (LOC_ENGINE_RUN_STATE_PAUSE_RETAIN == mEngState ||
                    LOC_ENGINE_RUN_STATE_PAUSE == mEngState) {
               /** wait for 400msec before sending Pause/ Pause-retain acknowledgement
                *  This allows engines to do all cleanup operations and go to pause state
                *  On timer expiry, acknowledgement shall be sent out */
                mAdapter.halResponseTimerStart(err, mSessionId, LOC_WAIT_TIME_MILLI_SEC);
            } else {
                mAdapter.reportResponse(err, mSessionId);
            }
        }
    };

    sendMsg(new MsgConfigEngineRunState(*this, sessionId, engType, engState));

    return sessionId;
}

uint32_t GnssAdapter::configOutputNmeaTypesCommand(GnssNmeaTypesMask enabledNmeaTypes,
                                                   GnssGeodeticDatumType nmeaDatumType,
                                                   LocReqEngineTypeMask nmeaReqEngTypeMask) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u, enabled nmea = 0x%x, datum type = %d, engine type mask = 0x%x",
             sessionId, enabledNmeaTypes, nmeaDatumType, nmeaReqEngTypeMask);
    struct MsgConfigOutputNmeaType : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t     mSessionId;
        GnssNmeaTypesMask mEnabledNmeaTypes;
        GnssGeodeticDatumType mNmeaDatumType;
        LocReqEngineTypeMask  mNmeaReqEngTypeMask;

        inline MsgConfigOutputNmeaType(GnssAdapter& adapter,
                                       uint32_t sessionId,
                                       GnssNmeaTypesMask enabledNmeaTypes,
                                       GnssGeodeticDatumType nmeaDatumType,
                                       LocReqEngineTypeMask nmeaReqEngTypeMask) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mEnabledNmeaTypes(enabledNmeaTypes),
            mNmeaDatumType(nmeaDatumType),
            mNmeaReqEngTypeMask(nmeaReqEngTypeMask) {}
        inline virtual void proc() const {
           loc_nmea_config_output_types(mEnabledNmeaTypes, mNmeaDatumType);
           mAdapter.setNmeaReqEngTypeMask(mNmeaReqEngTypeMask);
           mAdapter.reportResponse(LOCATION_ERROR_SUCCESS, mSessionId);
        }
    };

    sendMsg(new MsgConfigOutputNmeaType(*this, sessionId, enabledNmeaTypes, nmeaDatumType,
            nmeaReqEngTypeMask));

    return sessionId;
}

void GnssAdapter::powerIndicationInitCommand(const powerIndicationCb powerIndicationCallback) {
    LOC_LOGi("GnssAdapter::powerIndicationInitCommand");

    struct MsgPowerIndicationInit : public LocMsg {
        const powerIndicationCb mPowerIndicationCb;
        GnssAdapter& mAdapter;

        inline MsgPowerIndicationInit(const powerIndicationCb powerIndicationCallback,
                                      GnssAdapter& adapter) :
            LocMsg(), mPowerIndicationCb(powerIndicationCallback), mAdapter(adapter) {
        }

        inline virtual void proc() const {
            LOC_LOGV("MsgPowerIndicationInit::proc()");
            mAdapter.setPowerIndicationCb(mPowerIndicationCb);
        }
    };
    sendMsg(new MsgPowerIndicationInit(powerIndicationCallback, *this));
}

void GnssAdapter::powerIndicationRequestCommand() {
    LOC_LOGi("GnssAdapter::powerIndicationRequestCommand");

    struct MsgPowerIndicationRequest : public LocMsg {
        LocApiBase& mApi;

        inline MsgPowerIndicationRequest(LocApiBase& api) :
            LocMsg(), mApi(api) {
        }

        inline virtual void proc() const {
            LOC_LOGV("MsgPowerIndicationRequest::proc()");
            mApi.getGnssEnergyConsumed();
        }
    };
    sendMsg(new MsgPowerIndicationRequest(*mLocApi));
}

uint32_t GnssAdapter::configEngineIntegrityRiskCommand(
        PositioningEngineMask engType, uint32_t integrityRisk) {

    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u, eng type 0x%x, integrity risk %u, ppe enabled %d",
             sessionId, engType, integrityRisk, mEngServiceInfo.ppeEnabled);

    struct MsgConfigEngineIntegrityRisk : public LocMsg {
        GnssAdapter&          mAdapter;
        uint32_t              mSessionId;
        PositioningEngineMask mEngType;
        uint32_t              mIntegrityRisk;

        inline MsgConfigEngineIntegrityRisk(GnssAdapter& adapter,
                                            uint32_t sessionId,
                                            PositioningEngineMask engType,
                                            uint32_t integrityRisk) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mEngType(engType),
            mIntegrityRisk(integrityRisk) {}
        inline virtual void proc() const {
            LocationError err = LOCATION_ERROR_NOT_SUPPORTED;
            // Currently, only PPE engine supports integrity risk config request
            if ((mEngType == PRECISE_POSITIONING_ENGINE) &&
                    (mAdapter.mEngServiceInfo.ppeEnabled == true)) {
                if (true == mAdapter.mEngHubProxy->configEngineIntegrityRisk(
                        mEngType, mIntegrityRisk)) {
                    err = LOCATION_ERROR_SUCCESS;
                }
            }
            mAdapter.reportResponse(err, mSessionId);
        }
    };

    sendMsg(new MsgConfigEngineIntegrityRisk(*this, sessionId, engType, integrityRisk));

    return sessionId;
}

void GnssAdapter::reportXtraMpDisabledEvent() {
    struct MsgReportXtraMpDisabled : public LocMsg {
        GnssAdapter& mAdapter;

        inline MsgReportXtraMpDisabled(GnssAdapter& adapter) :
            LocMsg(), mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter.mMpXtraEnabled = false;
        }
    };

    sendMsg(new MsgReportXtraMpDisabled(*this));
}

uint32_t GnssAdapter::configXtraParamsCommand(bool enable,
                                              const XtraConfigParams& xtraParams) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u, xtra enable %d", sessionId, enable);

    struct MsgConfigXtraParams : public LocMsg {
        GnssAdapter&     mAdapter;
        uint32_t         mSessionId;
        bool             mEnable;
        XtraConfigParams mXtraParams;

        inline MsgConfigXtraParams(GnssAdapter& adapter,
                                   uint32_t sessionId,
                                   bool enable,
                                   XtraConfigParams xtraParams) :
            LocMsg(), mAdapter(adapter), mSessionId(sessionId),
            mEnable(enable), mXtraParams(xtraParams) {}
        inline virtual void proc() const {
            if (mAdapter.mMpXtraEnabled == false) {
                mAdapter.reportResponse(LOCATION_ERROR_NOT_SUPPORTED, mSessionId);
            } else {
                if (true == mAdapter.mXtraObserver.updateXtraConfig(mEnable, mXtraParams)) {
                    mAdapter.reportResponse(LOCATION_ERROR_SUCCESS, mSessionId);
                } else {
                    mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, mSessionId);
                }
            }
        }
    };

    sendMsg(new MsgConfigXtraParams(*this, sessionId, enable, xtraParams));

    return sessionId;
}

uint32_t GnssAdapter::getXtraStatusCommand() {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u", sessionId);

    struct MsgGetXtraStatus : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t     mSessionId;

        inline MsgGetXtraStatus(GnssAdapter& adapter,
                                uint32_t sessionId) :
            LocMsg(), mAdapter(adapter), mSessionId(sessionId) {}
        inline virtual void proc() const {
            if (false == mAdapter.mXtraObserver.getXtraStatus(mSessionId)) {
                mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, mSessionId);
            }
        }
    };

    sendMsg(new MsgGetXtraStatus(*this, sessionId));

    return sessionId;
}

uint32_t GnssAdapter::registerXtraStatusUpdateCommand(bool registerUpdate) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u, register for update %d", sessionId, registerUpdate);

    struct MsgRegisterXtraStatusUpdate : public LocMsg {
        GnssAdapter&     mAdapter;
        uint32_t         mSessionId;
        bool             mRegisterUpdate;

        inline MsgRegisterXtraStatusUpdate(GnssAdapter& adapter,
                                           uint32_t sessionId,
                                           bool registerUpdate) :
            LocMsg(), mAdapter(adapter), mSessionId(sessionId),
            mRegisterUpdate(registerUpdate) {}
        inline virtual void proc() const {
            if (false == mAdapter.mXtraObserver.registerXtraStatusUpdate(
                    mSessionId, mRegisterUpdate)) {
                mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, mSessionId);
            } else if (!mRegisterUpdate) {
                // de-register will not have status callback, send out response now
                mAdapter.reportResponse(LOCATION_ERROR_SUCCESS, mSessionId);
            }
        }
    };

    sendMsg(new MsgRegisterXtraStatusUpdate(*this, sessionId, registerUpdate));
    return sessionId;
}

void GnssAdapter::configPrecisePositioningCommand(
        uint32_t featureId, bool enable, const std::string& appHash) {

    struct MsgConfigPrecisePositioning : public LocMsg {
        GnssAdapter& mAdapter;
        bool mEnable;
        std::string mAppHash;
        uint32_t mFeatureId;

        inline MsgConfigPrecisePositioning(GnssAdapter& adapter,
                                           bool enable,
                                           const std::string& appHash,
                                           uint32_t featureId) :
            LocMsg(),
            mAdapter(adapter),
            mEnable(enable),
            mAppHash(appHash),
            mFeatureId(featureId) {}
        inline virtual void proc() const {
            LOC_LOGD("ConfigPrecisePositioning: enable: %d, appHash: %s, featureId: %d", mEnable,
                    mAppHash.c_str(), mFeatureId);
            if (QESDK_FEATURE_ID_EDGNSS == mFeatureId || QESDK_FEATURE_ID_RTK == mFeatureId) {
                mAdapter.mEngHubProxy->configPrecisePositioning(mFeatureId, mEnable, mAppHash);
                mAdapter.mLocApi->configPrecisePositioning(mFeatureId, mEnable, mAppHash);
                // cache the Qesdk Feature Status
                mAdapter.mAppHash = mAppHash;
            } else if (QESDK_FEATURE_ID_RL == mFeatureId) {
                mAdapter.mLocApi->configPrecisePositioning(mFeatureId, mEnable, mAppHash);
                mAdapter.mLocApi->configRobustLocation(mEnable, false);
            }
        }
    };
    sendMsg(new MsgConfigPrecisePositioning(*this, enable, appHash, featureId));
}

#ifdef USE_GLIB
uint32_t GnssAdapter::configMerkleTreeCommand(const char * merkleTreeConfigBuffer, int bufLen) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u, merkle tree buffer length %d", sessionId, bufLen);
    struct MsgConfigMerkleTreeParams : public LocMsg {
        GnssAdapter&     mAdapter;
        LocApiBase&      mApi;
        uint32_t         mSessionId;
        char*            mMerkleTreeConfigBuffer;
        int              mBufferLen;

        inline MsgConfigMerkleTreeParams(GnssAdapter& adapter, LocApiBase& api,
                uint32_t sessionId, const char* merkleTreeBuf, int bufLen) :
                LocMsg(), mAdapter(adapter), mApi(api), mSessionId(sessionId), mBufferLen(bufLen) {
            mMerkleTreeConfigBuffer = new char[bufLen+1];
            strlcpy(mMerkleTreeConfigBuffer, merkleTreeBuf, bufLen+1);
        }
        inline ~MsgConfigMerkleTreeParams() {
            if (mMerkleTreeConfigBuffer) {
                delete[] mMerkleTreeConfigBuffer;
            }
        }
        inline virtual void proc() const {
            mgpOsnmaPublicKeyAndMerkleTreeStruct* treeParam =
                    new mgpOsnmaPublicKeyAndMerkleTreeStruct[2];
            if (nullptr == treeParam) {
                LOC_LOGE("MsgConfigMerkleTreeParams: Merkle tree struct malloc failed");
                return;
            }
            memset(treeParam, 0, sizeof(mgpOsnmaPublicKeyAndMerkleTreeStruct)*2);
            int retVal = loc_read_conf_xml(mMerkleTreeConfigBuffer, mBufferLen, treeParam);
            if (retVal != 0) {
                mAdapter.reportResponse(LOCATION_ERROR_INVALID_PARAMETER, mSessionId);
                LOC_LOGE("MsgConfigMerkleTreeParams: Merkle tree config file parse failed");
                if (treeParam != nullptr) {
                    delete treeParam;
                }
                return;
            }
            //inject Merkle tree Parameter into PE
            LocationError err = LOCATION_ERROR_SUCCESS;
            int keyNum = (treeParam[1].zPublicKey.uFlag == 1)? 2: 1;
            LocApiResponse* locApiResponse = new LocApiResponse(*mAdapter.getContext(),
                    [&mAdapter = mAdapter, mSessionId = mSessionId, treeParam, keyNum,
                    &mApi = mApi] (LocationError err) mutable {
                // when there are two valid public keys, inject second key as well
                if (keyNum == 2) {
                    LocApiResponse* locApiResp = new LocApiResponse(*mAdapter.getContext(),
                            [&mAdapter = mAdapter, mSessionId = mSessionId, treeParam] (
                            LocationError err) mutable {
                        mAdapter.reportResponse(err, mSessionId);
                        // clean treeParam when response for the last public key reports
                        if (treeParam != nullptr) {
                            delete treeParam;
                            treeParam = nullptr;
                        }
                    });
                    if (!locApiResp) {
                        LOC_LOGE("MsgConfigMerkleTreeParams: memory alloc failed");
                        mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, mSessionId);
                        if (treeParam != nullptr) {
                            delete treeParam;
                            treeParam = nullptr;
                        }
                    } else {
                        mApi.configMerkleTree(treeParam + 1, locApiResp);
                    }
                } else { // when there is single valid public key,
                    mAdapter.reportResponse(err, mSessionId);
                    // clean treeParam after response for the only injection reports
                    if (treeParam != nullptr) {
                        delete treeParam;
                        treeParam = nullptr;
                    }
                }
            });
            if (!locApiResponse) {
                LOC_LOGE("MsgConfigMerkleTreeParams: memory alloc failed");
                mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, mSessionId);
                if (treeParam != nullptr) {
                    delete treeParam;
                    treeParam = nullptr;
                }
            } else {
                LOC_LOGD("MsgConfigMerkleTreeParams: Merkle tree key num: %d", keyNum);
                mApi.configMerkleTree(treeParam, locApiResponse);
            }
        }
    };

    sendMsg(new MsgConfigMerkleTreeParams(*this, *mLocApi, sessionId,
            merkleTreeConfigBuffer, bufLen));
    return sessionId;
}

uint32_t GnssAdapter::configOsnmaEnablementCommand(bool enable) {
    // generated session id will be none-zero
    uint32_t sessionId = generateSessionId();
    LOC_LOGd("session id %u, osnma enablement %d", sessionId, enable);
    struct MsgConfigOsnmaEnablementParams : public LocMsg {
        GnssAdapter&     mAdapter;
        LocApiBase&      mApi;
        uint32_t         mSessionId;
        bool             mEnable;

        inline MsgConfigOsnmaEnablementParams(GnssAdapter& adapter, LocApiBase& api,
                uint32_t sessionId, bool enable) :
                LocMsg(), mAdapter(adapter), mApi(api), mSessionId(sessionId), mEnable(enable) {}
        inline ~MsgConfigOsnmaEnablementParams() {}
        inline virtual void proc() const {
            //inject Merkle tree Parameter into PE
            LocationError err = LOCATION_ERROR_SUCCESS;
            LocApiResponse* locApiResponse = new LocApiResponse(*mAdapter.getContext(),
                    [&mAdapter = mAdapter, mSessionId = mSessionId] (LocationError err) mutable {
                mAdapter.reportResponse(err, mSessionId);
            });
            if (!locApiResponse) {
                LOC_LOGE("MsgConfigOsnmaEnablementParams: memory alloc failed");
                mAdapter.reportResponse(LOCATION_ERROR_GENERAL_FAILURE, mSessionId);
            } else {
                mApi.configOsnmaEnablement(mEnable, locApiResponse);
            }
        }
    };

    sendMsg(new MsgConfigOsnmaEnablementParams(*this, *mLocApi, sessionId, enable));
    return sessionId;
}
#endif

void GnssAdapter::reportGnssConfigEvent(uint32_t sessionId, const GnssConfig& gnssConfig)
{
    struct MsgReportGnssConfig : public LocMsg {
        GnssAdapter& mAdapter;
        uint32_t     mSessionId;
        mutable GnssConfig   mGnssConfig;
        inline MsgReportGnssConfig(GnssAdapter& adapter,
                                   uint32_t sessionId,
                                   const GnssConfig& gnssConfig) :
            LocMsg(),
            mAdapter(adapter),
            mSessionId(sessionId),
            mGnssConfig(gnssConfig) {}
        inline virtual void proc() const {
            // Invoke control clients config callback
            if (nullptr != mAdapter.mControlCallbacks.gnssConfigCb) {
                mAdapter.mControlCallbacks.gnssConfigCb(mSessionId, mGnssConfig);
            } else {
                LOC_LOGE("MsgReportGnssConfig: no callback registered");
            }
            // pipe XtraStatus to DebugReportService for LA
            if (nullptr != mAdapter.mControlCallbacks.xtraStatusCb) {
                mAdapter.mControlCallbacks.xtraStatusCb(mSessionId, mGnssConfig.xtraStatus);
            }
        }
    };

    sendMsg(new MsgReportGnssConfig(*this, sessionId, gnssConfig));
}

void
GnssAdapter::initLocGlinkCommand() {

    UTIL_READ_CONF(LOC_PATH_IZAT_CONF, izatConfLocGlinkParamTable);
    LOC_LOGd("LOAD_LOC_SLATE_PUNC_MODEL %d", loadLocSlatePUNCModel);
    struct MsgInitLocGlink: public LocMsg {
        GnssAdapter* mAdapter;
        inline MsgInitLocGlink(GnssAdapter* adapter) :
            LocMsg(),
            mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter->initLocGlinkProxy();
        }
    };
    if (loadLocSlatePUNCModel) {
        sendMsg(new MsgInitLocGlink(this));
    }
}

bool
GnssAdapter::initLocGlinkProxy() {
    static bool firstTime = true;
    static bool locGlinkLoadSuccessful = false;

    const char *error = nullptr;

    do {
        // load loc glink only once
        if (firstTime == false) {
            break;
        }
        // load the location glink .so, if the .so is not present
        // all location link calls will turn into no-op.
        void *handle = nullptr;
        if ((handle = dlopen("libloc_glink.so", RTLD_NOW)) == nullptr) {
            if ((error = dlerror()) != nullptr) {
                LOC_LOGe("libloc_glink.so not found %s !", error);
            }
            break;
        }
        loc_core::LocGlinkBase::GnssAdapterReportPuncEventCb reportPuncEventCb =
            [this](LocGpsLocation location) {
                // propogated punc reported from slate
                reportPropogatedPuncEvent(location);
            };

        loc_core::LocGlinkBase::getLocGlinkProxyFn* getter =
            (loc_core::LocGlinkBase::getLocGlinkProxyFn*) dlsym(handle, "getLocGlink");
        if (getter != nullptr) {
            LocGlinkBase* glinkProxy = (*getter) (mMsgTask, reportPuncEventCb);
            if (glinkProxy != nullptr) {
                delete mLocGlinkProxy;
                mLocGlinkProxy == nullptr;
                mLocGlinkProxy = glinkProxy;
                locGlinkLoadSuccessful = true;
            }
        }
        else {
            LOC_LOGe("entered, did not find function");
        }

        LOC_LOGd("first time initialization %d, returned %d",
                 firstTime, locGlinkLoadSuccessful);

    } while (0);


    firstTime = false;
    return locGlinkLoadSuccessful;
}

/* ==== Eng Hub Proxy ================================================================= */
/* ======== UTILITIES ================================================================= */
void
GnssAdapter::initEngHubProxyCommand() {
    LOC_LOGd();

    struct MsgInitEngHubProxy : public LocMsg {
        GnssAdapter* mAdapter;
        inline MsgInitEngHubProxy(GnssAdapter* adapter) :
            LocMsg(),
            mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter->initEngHubProxy();
        }
    };

    sendMsg(new MsgInitEngHubProxy(this));
}

bool
GnssAdapter::initEngHubProxy() {
    static bool firstTime = true;

    const char *error = nullptr;
    unsigned int processListLength = 0;
    loc_process_info_s_type* processInfoList = nullptr;
    do {
        // load eng hub only once
        if (firstTime == false) {
            break;
        }

        int rc = loc_read_process_conf(LOC_PATH_IZAT_CONF, &processListLength,
                                       &processInfoList);
        if (rc != 0) {
            LOC_LOGe("failed to parse conf file");
            break;
        }

        bool pluginDaemonEnabled = false;
        // go over the conf table to see whether any plugin daemon is enabled
        for (unsigned int i = 0; i < processListLength; i++) {
            if ((strncmp(processInfoList[i].name[0], PROCESS_NAME_ENGINE_SERVICE,
                         strlen(PROCESS_NAME_ENGINE_SERVICE)) == 0) &&
                (processInfoList[i].proc_status == ENABLED)) {
                pluginDaemonEnabled = true;
                if (processInfoList[i].args[1]!= nullptr) {
                    // check if this is DRE-INT engine
                    if (strncmp(processInfoList[i].args[1], "DRE-INT", sizeof("DRE-INT")) == 0) {
                        mEngServiceInfo.dreIntEnabled = true;
                    } else if (strncmp(processInfoList[i].args[1], "PPE", sizeof("PPE")) == 0) {
                        mEngServiceInfo.ppeEnabled = true;
                    } else if (strncmp(processInfoList[i].args[1], "PPE-INT", sizeof("PPE-INT"))
                               == 0) {
                        mEngServiceInfo.ppeIntEnabled = true;
                        mEngServiceInfo.ppeEnabled = true;
                    }
                }
            }
        }

        // no plugin daemon is enabled for this platform,
        // check if external engine is present for which we need
        // libloc_eng_hub.so to be loaded
        if (pluginDaemonEnabled == false) {
            UTIL_READ_CONF(LOC_PATH_IZAT_CONF, izatConfParamTable);
            if (!loadEngHubForExternalEngine) {
                break;
            }
        }

        // load the engine hub .so, if the .so is not present
        // all EngHubProxyBase calls will turn into no-op.
        void *handle = nullptr;
        if ((handle = dlopen("libloc_eng_hub.so", RTLD_NOW)) == nullptr) {
            if ((error = dlerror()) != nullptr) {
                LOC_LOGe("libloc_eng_hub.so not found %s !", error);
            }
            break;
        }

        // prepare the callback functions
        // callback function for engine hub to report back position event
        GnssAdapterReportEnginePositionsEventCb reportPositionEventCb =
            [this](int count, EngineLocationInfo* locationArr) {
                    // report from engine hub on behalf of PPE will be treated as fromUlp
                    reportEnginePositionsEvent(count, locationArr);
            };

        // callback function for engine hub to request for complete aiding data
        GnssAdapterReqAidingDataCb reqAidingDataCb =
            [this] (const GnssAidingDataSvMask& svDataMask) {
            mLocApi->requestForAidingData(svDataMask);
        };

        GnssAdapterUpdateNHzRequirementCb updateNHzRequirementCb =
            [this] (bool nHzNeeded, bool nHzMeasNeeded) {
            // External engines can subscribe to Nhz Meas OR PVT report
            // only if SPE supports NHz
            if ((this->getCapabilities() & LOCATION_CAPABILITIES_QWES_GNSS_NHZ)) {
                if (nHzMeasNeeded &&
                        (!checkMask(LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT))) {
                    updateEvtMask(LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT,
                        LOC_REGISTRATION_MASK_ENABLED);
                } else if (checkMask(LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT)) {
                    updateEvtMask(LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT,
                        LOC_REGISTRATION_MASK_DISABLED);
                }

                if (mNHzNeeded != nHzNeeded) {
                    mNHzNeeded = nHzNeeded;
                    checkAndRestartSPESession();
                }
            }
        };

        GnssAdapterUpdateQwesFeatureStatusCb updateQwesFeatureStatusCb =
            [this] (const std::unordered_map<LocationQwesFeatureType, bool> &featureMap) {
            handleQesdkQwesStatusFromEHub(featureMap);
            ContextBase::setQwesFeatureStatus(featureMap);
            reportQwesCapabilities(featureMap);
        };

        getEngHubProxyFn* getter = (getEngHubProxyFn*) dlsym(handle, "getEngHubProxy");
        if (getter != nullptr) {
            // Wait for the script(rootdir/etc/init.qcom.rc) to create socket folder
            locUtilWaitForDir(SOCKET_DIR_EHUB);
            EngineHubProxyBase* hubProxy = (*getter) (mMsgTask, mContext,
                      mSystemStatus->getOsObserver(),
                      mEngServiceInfo, reportPositionEventCb, reqAidingDataCb,
                      updateNHzRequirementCb, updateQwesFeatureStatusCb,
                      [ this ] { return isEngineServiceEnable(); });
            if (hubProxy != nullptr) {
                mEngHubProxy = hubProxy;
                mEngHubLoadSuccessful = true;
            }
        }
        else {
            LOC_LOGd("entered, did not find function of getEngHubProxy");
        }

        LOC_LOGd("first time initialization %d, returned %d",
                 firstTime, mEngHubLoadSuccessful);

    } while (0);

    if (processInfoList != nullptr) {
        free (processInfoList);
        processInfoList = nullptr;
    }

    firstTime = false;
    return mEngHubLoadSuccessful;
}

std::vector<double>
GnssAdapter::parseDoublesString(char* dString) {
    std::vector<double> dVector;
    char* tmp = NULL;
    char* substr;

    dVector.clear();
    for (substr = strtok_r(dString, " ", &tmp);
        substr != NULL;
        substr = strtok_r(NULL, " ", &tmp)) {
        dVector.push_back(std::stod(substr));
    }
    return dVector;
}

void GnssAdapter::initGnssPowerStatistics() {
    if (!mGnssPowerStatisticsInit) {
        mLocApi->getGnssEnergyConsumed();
    }
}

void
GnssAdapter::reportGnssAntennaInformation(AntennaInfoCallback* cb)
{
#define MAX_TEXT_WIDTH      50
#define MAX_COLUMN_WIDTH    20

    /* parse antenna_corrections file and fill in
    a vector of GnssAntennaInformation data structure */

    std::vector<GnssAntennaInformation> gnssAntennaInformations;
    GnssAntennaInformation gnssAntennaInfo;

    uint32_t antennaInfoVectorSize;
    loc_param_s_type ant_info_vector_table[] =
    {
        { "ANTENNA_INFO_VECTOR_SIZE", &antennaInfoVectorSize, NULL, 'n' }
    };
    UTIL_READ_CONF(LOC_PATH_ANT_CORR, ant_info_vector_table);

    for (uint32_t i = 0; i < antennaInfoVectorSize; i++) {
        double carrierFrequencyMHz;
        char pcOffsetStr[LOC_MAX_PARAM_STRING];
        uint32_t numberOfRows = 0;
        uint32_t numberOfColumns = 0;
        uint32_t numberOfRowsSGC = 0;
        uint32_t numberOfColumnsSGC = 0;

        gnssAntennaInfo.phaseCenterVariationCorrectionMillimeters.clear();
        gnssAntennaInfo.phaseCenterVariationCorrectionUncertaintyMillimeters.clear();
        gnssAntennaInfo.signalGainCorrectionDbi.clear();
        gnssAntennaInfo.signalGainCorrectionUncertaintyDbi.clear();
        string s1 = "CARRIER_FREQUENCY_";
        s1 += to_string(i);
        string s2 = "PC_OFFSET_";
        s2 += to_string(i);
        string s3 = "NUMBER_OF_ROWS_";
        s3 += to_string(i);
        string s4 = "NUMBER_OF_COLUMNS_";
        s4 += to_string(i);
        string s5 = "NUMBER_OF_ROWS_SGC_";
        s5 += to_string(i);
        string s6 = "NUMBER_OF_COLUMNS_SGC_";
        s6 += to_string(i);

        gnssAntennaInfo.size = sizeof(gnssAntennaInfo);
        loc_param_s_type ant_cf_table[] =
        {
            { s1.c_str(), &carrierFrequencyMHz, NULL, 'f' },
            { s2.c_str(), &pcOffsetStr, NULL, 's' },
            { s3.c_str(), &numberOfRows, NULL, 'n' },
            { s4.c_str(), &numberOfColumns, NULL, 'n' },
            { s5.c_str(), &numberOfRowsSGC, NULL, 'n' },
            { s6.c_str(), &numberOfColumnsSGC, NULL, 'n' },
        };
        UTIL_READ_CONF(LOC_PATH_ANT_CORR, ant_cf_table);

        if (0 == numberOfRowsSGC) {
            numberOfRowsSGC = numberOfRows;
        }
        if (0 == numberOfColumnsSGC) {
            numberOfColumnsSGC = numberOfColumns;
        }

        gnssAntennaInfo.carrierFrequencyMHz = carrierFrequencyMHz;

        // now parse pcOffsetStr to get each entry
        std::vector<double> pcOffset;
        pcOffset = parseDoublesString(pcOffsetStr);
        gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters.size =
                sizeof(gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters);
        gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters.x = pcOffset[0];
        gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters.xUncertainty = pcOffset[1];
        gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters.y = pcOffset[2];
        gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters.yUncertainty = pcOffset[3];
        gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters.z = pcOffset[4];
        gnssAntennaInfo.phaseCenterOffsetCoordinateMillimeters.zUncertainty = pcOffset[5];

        uint16_t array_size = MAX_TEXT_WIDTH + MAX_COLUMN_WIDTH*numberOfColumns;
        uint16_t array_size_SGC = MAX_TEXT_WIDTH + MAX_COLUMN_WIDTH*numberOfColumnsSGC;
        for (uint32_t j = 0; j < numberOfRows; j++) {
            char pcVarCorrStr[array_size];
            char pcVarCorrUncStr[array_size];

            string s1 = "PC_VARIATION_CORRECTION_" + to_string(i) + "_ROW_";
            s1 += to_string(j);
            string s2 = "PC_VARIATION_CORRECTION_UNC_" + to_string(i) + "_ROW_";
            s2 += to_string(j);

            loc_param_s_type ant_row_table[] =
            {
                { s1.c_str(), &pcVarCorrStr, NULL, 's' },
                { s2.c_str(), &pcVarCorrUncStr, NULL, 's' },
            };
            UTIL_READ_CONF_LONG(LOC_PATH_ANT_CORR, ant_row_table, array_size);

            gnssAntennaInfo.phaseCenterVariationCorrectionMillimeters.push_back(
                    parseDoublesString(pcVarCorrStr));
            gnssAntennaInfo.phaseCenterVariationCorrectionUncertaintyMillimeters.push_back(
                    parseDoublesString(pcVarCorrUncStr));
        }
        for (uint32_t j = 0; j < numberOfRowsSGC; j++) {
            char sigGainCorrStr[array_size_SGC];
            char sigGainCorrUncStr[array_size_SGC];

            string s3 = "SIGNAL_GAIN_CORRECTION_" + to_string(i) + "_ROW_";
            s3 += to_string(j);
            string s4 = "SIGNAL_GAIN_CORRECTION_UNC_" + to_string(i) + "_ROW_";
            s4 += to_string(j);

            loc_param_s_type ant_row_table[] =
            {
                { s3.c_str(), &sigGainCorrStr, NULL, 's' },
                { s4.c_str(), &sigGainCorrUncStr, NULL, 's' },
            };
            UTIL_READ_CONF_LONG(LOC_PATH_ANT_CORR, ant_row_table, array_size_SGC);

            gnssAntennaInfo.signalGainCorrectionDbi.push_back(
                    parseDoublesString(sigGainCorrStr));
            gnssAntennaInfo.signalGainCorrectionUncertaintyDbi.push_back(
                    parseDoublesString(sigGainCorrUncStr));
        }
        gnssAntennaInformations.push_back(std::move(gnssAntennaInfo));
    }
    if (antennaInfoVectorSize > 0 && cb) {
        (*cb)(gnssAntennaInformations);
    }
}

bool GnssAdapter::isStandAloneCDParserPELib() {
    bool standAloneCDParserPELib = false;
    // QDGNSS_3GPP_SOURCE_AVAIL = modem side can supply 3GPP correction data
    // QDGNSS_3GPP_EP_PARSER_AVAIL = QPPE lib can parse 3GPP SSR messages
    if (m3GppSourceMask & (QDGNSS_3GPP_SOURCE_AVAIL | QDGNSS_3GPP_EP_PARSER_AVAIL) &&
            isMlpEnabled() && !isQppeEnabled()) {
        standAloneCDParserPELib = true;
    }

    if (standAloneCDParserPELib) {
        LOC_LOGd("m3GppSourceMask 0x%x, mPpFeatureStatusMask 0x%x, standAloneCDParserPELib %d",
                m3GppSourceMask, mPpFeatureStatusMask, standAloneCDParserPELib);
    } else {
        LOC_LOGv("m3GppSourceMask 0x%x, mPpFeatureStatusMask 0x%x, standAloneCDParserPELib %d",
                m3GppSourceMask, mPpFeatureStatusMask, standAloneCDParserPELib);
    }

    return standAloneCDParserPELib;
}

bool GnssAdapter::isEngineServiceEnable() {
    if (isPreciseEnabled()) {
        return true;
    } else {
        return isStandAloneCDParserPELib();
    }
}

/* ==== DGnss Usable Reporter ========================================================= */
void GnssAdapter::initCDFWServiceCommand() {
    struct MsgInitCDFWService : public LocMsg {
        GnssAdapter* mAdapter;
        inline MsgInitCDFWService(GnssAdapter* adapter) :
            LocMsg(),
            mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter->initCDFWService();
        }
    };

    sendMsg(new MsgInitCDFWService(this));
}
/* ======== UTILITIES ================================================================= */
void GnssAdapter::initCDFWService()
{
    LOC_LOGd("mCdfwInterface %p", mCdfwInterface);
    if (nullptr == mCdfwInterface) {
        void* libHandle = nullptr;
        const char* libName = "libcdfw.so";

        libHandle = nullptr;
        getCdfwInterface getter  = (getCdfwInterface)dlGetSymFromLib(libHandle,
                                    libName, "getQCdfwInterface");
        if (nullptr == getter) {
            LOC_LOGe("dlGetSymFromLib getQCdfwInterface failed");
        } else {
            mCdfwInterface = getter();
        }

        if (nullptr != mCdfwInterface) {
            QDgnssSessionActiveCb qDgnssSessionActiveCb = [this] (bool sessionActive) {
                mDGnssNeedReport = sessionActive;
            };
            QDgnssModem3GppAvailCb qDgneeModem3GppAvailCb =
                    [this] (QDgnss3GppSourceBitMask modem3GppSourceMask) {
                updateModme3GppSourceStatus(modem3GppSourceMask);
            };
            mCdfwInterface->startDgnssApiService(*mMsgTask, qDgneeModem3GppAvailCb);
            mQDgnssListenerHDL = mCdfwInterface->createUsableReporter(qDgnssSessionActiveCb);
        }
    }

    //Read Ntrip params form gps.conf on automobile PLs
#ifdef USE_GLIB
    readPPENtripConfig();
#endif
}

void GnssAdapter::updateModme3GppSourceStatus(QDgnss3GppSourceBitMask modem3GppSourceMask) {
    struct updateModme3GppAvailMsg : public LocMsg {
        GnssAdapter& mAdapter;
        QDgnss3GppSourceBitMask  m3GppSourceMask;

        inline updateModme3GppAvailMsg(GnssAdapter& adapter,
                QDgnss3GppSourceBitMask modem3GppSourceMask) :
            LocMsg(),
            mAdapter(adapter),
            m3GppSourceMask(modem3GppSourceMask) {}

        inline virtual void proc() const {
            if (mAdapter.m3GppSourceMask != m3GppSourceMask) {
                LOC_LOGD("updateModme3GppAvailMsg, m3GppSourceMask %d, m3GppSourceMask %d",
                         mAdapter.m3GppSourceMask, m3GppSourceMask);

                LocationCapabilitiesMask capabilitiesMask = mAdapter.getCapabilities();
                if (m3GppSourceMask & QDGNSS_3GPP_SOURCE_AVAIL) {
                    capabilitiesMask |= LOCATION_CAPABILITIES_MODEM_3GPP_AVAIL;
                } else {
                    capabilitiesMask &= ~LOCATION_CAPABILITIES_MODEM_3GPP_AVAIL;
                }

                mAdapter.m3GppSourceMask = m3GppSourceMask;
                mAdapter.broadcastCapabilities(capabilitiesMask);
                mAdapter.notifyPreciseLocation();
            }
        }
    };
    sendMsg(new updateModme3GppAvailMsg(*this, modem3GppSourceMask));
}

/*==== DGnss Ntrip Source ==========================================================*/
void GnssAdapter::enablePPENtripStreamCommand(const GnssNtripConnectionParams& params,
                                              bool enableRTKEngine) {

    (void)enableRTKEngine; //future parameter, not used
    if (0 == params.size || params.hostNameOrIp.empty() || params.mountPoint.empty() ||
            params.username.empty() || params.password.empty()) {
        LOC_LOGe("Ntrip parameters are invalid!");
        return;
    }

    struct enableNtripMsg : public LocMsg {
        GnssAdapter& mAdapter;
        const GnssNtripConnectionParams mParams;
        bool mEnableRTKEngine;

        inline enableNtripMsg(GnssAdapter& adapter,
                const GnssNtripConnectionParams& params,
                bool enableRTKEngine) :
            LocMsg(),
            mAdapter(adapter),
            mParams(std::move(params)),
            mEnableRTKEngine(enableRTKEngine) {}
        inline virtual void proc() const {
            mAdapter.handleEnablePPENtrip(mParams, mEnableRTKEngine);
        }
    };
    sendMsg(new enableNtripMsg(*this, params, enableRTKEngine));
}

void GnssAdapter::handleEnablePPENtrip(const GnssNtripConnectionParams& params,
        bool enableRTKEngine) {
    uint32_t nmeaUpdateInterval = params.nmeaUpdateInterval ? params.nmeaUpdateInterval :
            DGNSS_RANGE_UPDATE_TIME_10MIN_IN_SEC;
    LOC_LOGd("%d %s %d %s %s %s %d %d mSendNmeaConsent %d",
             params.useSSL, params.hostNameOrIp.data(), params.port,
             params.mountPoint.data(), params.username.data(), params.password.data(),
             params.requiresNmeaLocation, nmeaUpdateInterval, mSendNmeaConsent);

    GnssNtripConnectionParams* pNtripParams = &(mStartDgnssNtripParams.ntripParams);

    if (pNtripParams->useSSL == params.useSSL &&
            0 == pNtripParams->hostNameOrIp.compare(params.hostNameOrIp) &&
            pNtripParams->port == params.port &&
            0 == pNtripParams->mountPoint.compare(params.mountPoint) &&
            0 == pNtripParams->username.compare(params.username) &&
            0 == pNtripParams->password.compare(params.password) &&
            pNtripParams->requiresNmeaLocation == params.requiresNmeaLocation &&
            pNtripParams->nmeaUpdateInterval == nmeaUpdateInterval &&
            mDgnssState & DGNSS_STATE_ENABLE_NTRIP_COMMAND) {
        LOC_LOGd("received same Ntrip param");
        return;
    }

    mDgnssState |= DGNSS_STATE_ENABLE_NTRIP_COMMAND;
    mDgnssState |= DGNSS_STATE_NO_NMEA_PENDING;
    mDgnssState &= ~DGNSS_STATE_NTRIP_SESSION_STARTED;
    getSystemStatus()->eventNtripStarted(true);

    mStartDgnssNtripParams.ntripParams = std::move(params);
    mStartDgnssNtripParams.ntripParams.nmeaUpdateInterval = nmeaUpdateInterval;
    mStartDgnssNtripParams.enableRTKEngine = enableRTKEngine;
    mStartDgnssNtripParams.nmea.clear();
    if (mSendNmeaConsent && pNtripParams->requiresNmeaLocation) {
        mDgnssState &= ~DGNSS_STATE_NO_NMEA_PENDING;
        mDgnssLastNmeaBootTimeMilli = 0;
        return;
    }

    checkUpdateDgnssNtrip(false);
}

void GnssAdapter::disablePPENtripStreamCommand() {
    struct disableNtripMsg : public LocMsg {
        GnssAdapter& mAdapter;

        inline disableNtripMsg(GnssAdapter& adapter) :
            LocMsg(),
            mAdapter(adapter) {}
        inline virtual void proc() const {
            mAdapter.handleDisablePPENtrip();
        }
    };
    sendMsg(new disableNtripMsg(*this));
}

void GnssAdapter::handleDisablePPENtrip() {
    mDgnssState &= ~DGNSS_STATE_ENABLE_NTRIP_COMMAND;
    mDgnssState |= DGNSS_STATE_NO_NMEA_PENDING;
    stopDgnssNtrip();
    getSystemStatus()->eventNtripStarted(false);
}

void GnssAdapter::checkUpdateDgnssNtrip(bool isLocationValid) {
    LOC_LOGd("isInSession %d mDgnssState 0x%x isLocationValid %d isMlpEnabled %d",
            isInSession(), mDgnssState, isLocationValid, isMlpEnabled());
    //Enable edgnss -daemon when isInSession and isMlpEnabled.
    //isMlpEnabled is true when RTK or edgnss feature is enabled.
    if (isInSession() && isMlpEnabled()) {
        uint64_t curBootTime = getBootTimeMilliSec();
        if (mDgnssState == (DGNSS_STATE_ENABLE_NTRIP_COMMAND | DGNSS_STATE_NO_NMEA_PENDING)) {
            mDgnssState |= DGNSS_STATE_NTRIP_SESSION_STARTED;
            mXtraObserver.startDgnssSource(mStartDgnssNtripParams);
            if (isDgnssNmeaRequired()) {
                mDgnssLastNmeaBootTimeMilli = curBootTime;
            }
        } else if ((mDgnssState & DGNSS_STATE_NTRIP_SESSION_STARTED) && isLocationValid &&
            isDgnssNmeaRequired() && curBootTime - mDgnssLastNmeaBootTimeMilli >
            mStartDgnssNtripParams.ntripParams.nmeaUpdateInterval * ONE_SECOND_IN_MS) {
            mXtraObserver.updateNmeaToDgnssServer(mStartDgnssNtripParams.nmea);
            mDgnssLastNmeaBootTimeMilli = curBootTime;
        }
    } else {
        LOC_LOGi("mPpFeatureStatusMask 0x%x", mPpFeatureStatusMask);
    }
}

void GnssAdapter::stopDgnssNtrip() {
    LOC_LOGd("isInSession %d mDgnssState 0x%x", isInSession(), mDgnssState);
    mStartDgnssNtripParams.nmea.clear();
    if (mDgnssState & DGNSS_STATE_NTRIP_SESSION_STARTED) {
        mDgnssState &= ~DGNSS_STATE_NTRIP_SESSION_STARTED;
        mXtraObserver.stopDgnssSource();
    }
}

void GnssAdapter::reportGGAToNtrip(const char* nmea) {

#define POS_OF_GGA (3)  //start position of "GGA"
#define COMMAS_BEFORE_VALID (6) //"$GPGGA,,,,,,0,,,,,,,,*hh"

    if (!isDgnssNmeaRequired()) {
        return;
    }

    if (nullptr == nmea || 0 == strlen(nmea)) {
        return;
    }

    string nmeaString(nmea);
    size_t foundPos = nmeaString.find("GGA");
    size_t foundNth = 0;
    string GGAString;

    if (foundPos != string::npos && foundPos >= POS_OF_GGA) {
        size_t foundNextSentence = nmeaString.find("$", foundPos);
        if (foundNextSentence != string::npos) {
            /* remove other sentences after GGA */
            GGAString = nmeaString.substr(foundPos - POS_OF_GGA, foundNextSentence);
        } else {
            /* GGA is the last sentence */
            GGAString = nmeaString.substr(foundPos - POS_OF_GGA);
        }
        LOC_LOGv("GGAString %s", GGAString.c_str());

        foundPos = GGAString.find(",");
        while (foundPos != string::npos && foundNth < COMMAS_BEFORE_VALID) {
            foundPos++;
            foundNth++;
            foundPos = GGAString.find(",", foundPos);
        }

        if (COMMAS_BEFORE_VALID == foundNth && GGAString.at(foundPos-1) != '0') {
            mDgnssState |= DGNSS_STATE_NO_NMEA_PENDING;
            mStartDgnssNtripParams.nmea = std::move(GGAString);
            checkUpdateDgnssNtrip(true);
        }
    }

    return;
}

void GnssAdapter::readPPENtripConfig() {

    static char NtripParamsString[LOC_MAX_PARAM_STRING];

    if (mDgnssState & DGNSS_STATE_ENABLE_NTRIP_COMMAND) {
        return;
    }

    // A sample Ntrip_Params -> 199.106.116.10 5000 Avante_Ref CV2X 1234 1 0 0 0
    static loc_param_s_type gpsConfParamTable[] = {
        {"Ntrip_Params", &NtripParamsString, nullptr, 's'}
    };
    UTIL_READ_CONF(LOC_PATH_GPS_CONF, gpsConfParamTable);
    LOC_LOGd("Ntrip_Params=%s", NtripParamsString);

    if (0 == strlen(NtripParamsString)) {
        return;
    }

    // assign params to mStartDgnssNtripParams
    GnssNtripConnectionParams* pNtripParams = &(mStartDgnssNtripParams.ntripParams);
    string next(NtripParamsString);
    stringstream ss(next);

#define GET_NEXT() getline(ss, next, ' '); \
        LOC_LOGd("%s", next.c_str());

    GET_NEXT();
    pNtripParams->hostNameOrIp = std::move(next);
    GET_NEXT();
    pNtripParams->port = std::stoi(next);
    GET_NEXT();
    pNtripParams->mountPoint = std::move(next);
    GET_NEXT();
    pNtripParams->username = std::move(next);
    GET_NEXT();
    pNtripParams->password = std::move(next);
    GET_NEXT();
    mSendNmeaConsent = true;
    GET_NEXT();
    pNtripParams->requiresNmeaLocation = next.compare("0") ? true : false;
    GET_NEXT();
    pNtripParams->useSSL = next.compare("0") ? true : false;
    GET_NEXT();
    pNtripParams->nmeaUpdateInterval = std::stoi(next) ? std::stoi(next) :
            DGNSS_RANGE_UPDATE_TIME_10MIN_IN_SEC;

    LOC_LOGd("%d %s %d %s %s %s %d %d",
             pNtripParams->useSSL, pNtripParams->hostNameOrIp.data(), pNtripParams->port,
             pNtripParams->mountPoint.data(), pNtripParams->username.data(),
             pNtripParams->password.data(), pNtripParams->requiresNmeaLocation,
             pNtripParams->nmeaUpdateInterval);

    /* set up state*/
    mDgnssState |= DGNSS_STATE_ENABLE_NTRIP_COMMAND;
    mDgnssState |= DGNSS_STATE_NO_NMEA_PENDING;
    mDgnssState &= ~DGNSS_STATE_NTRIP_SESSION_STARTED;
    getSystemStatus()->eventNtripStarted(true);

    mStartDgnssNtripParams.nmea.clear();
    if (pNtripParams->requiresNmeaLocation) {
        mDgnssState &= ~DGNSS_STATE_NO_NMEA_PENDING;
    }
}

bool GnssAdapter::reportZppBestAvailableFix(LocGpsLocation &zppLoc,
            GpsLocationExtended &location_extended, LocPosTechMask tech_mask) {
    if (sUseZppInDBH && mOdcpiRequest.isEmergencyMode && (mOdcpiStateMask & ODCPI_REQ_ACTIVE)
            && zppLoc.timestamp != 0) {
        LOC_LOGv("report valid ZPP fix to Flp client in DBH");

        struct MsgReportZppPosition : public LocMsg {
            GnssAdapter& mAdapter;
            mutable UlpLocation mUlpLoc;
            mutable GpsLocationExtended mLocationExtended;
            enum loc_sess_status mStatus;

            inline MsgReportZppPosition(GnssAdapter& adapter,
                                        const LocGpsLocation& zppLoc,
                                        const GpsLocationExtended& locationExtended,
                                        enum loc_sess_status status,
                                        LocPosTechMask techMask) :
                    LocMsg(),
                    mAdapter(adapter),
                    mLocationExtended(locationExtended),
                    mStatus(status) {
                memset(&mUlpLoc, 0, sizeof(UlpLocation));
                mUlpLoc.size = sizeof(mUlpLoc);
                mUlpLoc.tech_mask = techMask;
                memcpy(&(mUlpLoc.gpsLocation), &zppLoc, sizeof(LocGpsLocation));
            }
            inline virtual void proc() const {
                mAdapter.reportPosition(mUlpLoc, mLocationExtended, mStatus, mUlpLoc.tech_mask);
            }
        };

        sendMsg(new MsgReportZppPosition(*this,
                    zppLoc, location_extended, LOC_SESS_INTERMEDIATE, tech_mask));
    }
    return true;
}
