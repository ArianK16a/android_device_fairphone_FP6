/* Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
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

#ifndef __SYSTEM_STATUS__
#define __SYSTEM_STATUS__

#include <stdint.h>
#include <sys/time.h>
#include <vector>
#include <algorithm>
#include <iterator>
#include <loc_pla.h>
#include <log_util.h>
#include <MsgTask.h>
#include <IDataItemCore.h>
#include <IOsObserver.h>
#include <DataItemConcreteTypes.h>
#include <SystemStatusOsObserver.h>

#include <gps_extended_c.h>

#define GPS_MIN    (1)   //1-32
#define SBAS_MIN   (33)
#define GLO_MIN    (65)  //65-88
#define QZSS_MIN   (193) //193-197
#define BDS_MIN    (201) //201-237
#define GAL_MIN    (301) //301-336
#define NAVIC_MIN  (401) //401-414

#define GPS_NUM     (32)
#define SBAS_NUM    (32)
#define GLO_NUM     (24)
#define QZSS_NUM    (5)
#define BDS_NUM     (37)
#define GAL_NUM     (36)
#define NAVIC_NUM   (14)
#define SV_ALL_NUM_MIN  (GPS_NUM + GLO_NUM + QZSS_NUM + BDS_NUM + GAL_NUM) //=134
#define SV_ALL_NUM      (SV_ALL_NUM_MIN + NAVIC_NUM) //=148

namespace loc_core
{

/******************************************************************************
 SystemStatus report data structure
******************************************************************************/
class SystemStatusItemBase
{
public:
    timespec  mUtcTime;
    timespec  mUtcReported;
    static const uint32_t maxItem = 5;

    SystemStatusItemBase() {
        timeval tv;
        gettimeofday(&tv, NULL);
        mUtcTime.tv_sec  = tv.tv_sec;
        mUtcTime.tv_nsec = tv.tv_usec*1000ULL;
        mUtcReported = mUtcTime;
    };
    virtual ~SystemStatusItemBase() {};
    inline virtual SystemStatusItemBase& collate(SystemStatusItemBase&) {
        return *this;
    }
    virtual void dump(void) {};
    inline virtual bool ignore() { return false; };
    virtual bool equals(const SystemStatusItemBase& peer) { return false; }
};

class SystemStatusLocation : public SystemStatusItemBase
{
public:
    bool mValid;
    UlpLocation mLocation;
    GpsLocationExtended mLocationEx;
    inline SystemStatusLocation() :
        mValid(false) {}
    inline SystemStatusLocation(const UlpLocation& location,
                         const GpsLocationExtended& locationEx) :
        mValid(true),
        mLocation(location),
        mLocationEx(locationEx) {}
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWM1;
class SystemStatusTimeAndClock : public SystemStatusItemBase
{
public:
    uint16_t mGpsWeek;
    uint32_t mGpsTowMs;
    uint8_t  mTimeValid;
    uint8_t  mTimeSource;
    int32_t  mTimeUnc;
    int32_t  mClockFreqBias;
    int32_t  mClockFreqBiasUnc;
    int32_t  mLeapSeconds;
    int32_t  mLeapSecUnc;
    uint64_t mTimeUncNs;
    inline SystemStatusTimeAndClock() :
        mGpsWeek(0),
        mGpsTowMs(0),
        mTimeValid(0),
        mTimeSource(0),
        mTimeUnc(0),
        mClockFreqBias(0),
        mClockFreqBiasUnc(0),
        mLeapSeconds(0),
        mLeapSecUnc(0),
        mTimeUncNs(0ULL) {}
    inline SystemStatusTimeAndClock(const SystemStatusPQWM1& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusXoState : public SystemStatusItemBase
{
public:
    uint8_t  mXoState;
    inline SystemStatusXoState() :
        mXoState(0) {}
    inline SystemStatusXoState(const SystemStatusPQWM1& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusRfAndParams : public SystemStatusItemBase
{
public:
    int32_t  mPgaGain;
    int32_t mGpsBpAmpI;
    int32_t mGpsBpAmpQ;
    int32_t mAdcI;
    int32_t mAdcQ;
    uint32_t mJammerGps;
    uint32_t mJammerGlo;
    uint32_t mJammerBds;
    uint32_t mJammerGal;
    int32_t mGloBpAmpI;
    int32_t mGloBpAmpQ;
    int32_t mBdsBpAmpI;
    int32_t mBdsBpAmpQ;
    int32_t mGalBpAmpI;
    int32_t mGalBpAmpQ;
    uint32_t mJammedSignalsMask;
    std::vector<int32_t> mJammerInd;
    inline SystemStatusRfAndParams() :
        mPgaGain(INT32_MIN),
        mGpsBpAmpI(INT32_MIN),
        mGpsBpAmpQ(INT32_MIN),
        mAdcI(INT32_MIN),
        mAdcQ(INT32_MIN),
        mJammerGps(0),
        mJammerGlo(0),
        mJammerBds(0),
        mJammerGal(0),
        mGloBpAmpI(INT32_MIN),
        mGloBpAmpQ(INT32_MIN),
        mBdsBpAmpI(INT32_MIN),
        mBdsBpAmpQ(INT32_MIN),
        mGalBpAmpI(INT32_MIN),
        mGalBpAmpQ(INT32_MIN),
        mJammedSignalsMask(0) {}
    inline SystemStatusRfAndParams(const SystemStatusPQWM1& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusErrRecovery : public SystemStatusItemBase
{
public:
    uint32_t mRecErrorRecovery;
    inline SystemStatusErrRecovery() :
        mRecErrorRecovery(0) {};
    inline SystemStatusErrRecovery(const SystemStatusPQWM1& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    inline bool ignore() override { return 0 == mRecErrorRecovery; };
    void dump(void) override;
};

class SystemStatusPQWP1;
class SystemStatusInjectedPosition : public SystemStatusItemBase
{
public:
    uint8_t  mEpiValidity;
    float    mEpiLat;
    float    mEpiLon;
    float    mEpiAlt;
    float    mEpiHepe;
    float    mEpiAltUnc;
    uint8_t  mEpiSrc;
    inline SystemStatusInjectedPosition() :
        mEpiValidity(0),
        mEpiLat(0),
        mEpiLon(0),
        mEpiAlt(0),
        mEpiHepe(0),
        mEpiAltUnc(0),
        mEpiSrc(0) {}
    inline SystemStatusInjectedPosition(const SystemStatusPQWP1& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWP2;
class SystemStatusBestPosition : public SystemStatusItemBase
{
public:
    bool     mValid;
    float    mBestLat;
    float    mBestLon;
    float    mBestAlt;
    float    mBestHepe;
    float    mBestAltUnc;
    inline SystemStatusBestPosition() :
        mValid(false),
        mBestLat(0),
        mBestLon(0),
        mBestAlt(0),
        mBestHepe(0),
        mBestAltUnc(0) {}
    inline SystemStatusBestPosition(const SystemStatusPQWP2& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWP3;
class SystemStatusXtra : public SystemStatusItemBase
{
public:
    uint8_t   mXtraValidMask;
    uint32_t  mGpsXtraAge;
    uint32_t  mGloXtraAge;
    uint32_t  mBdsXtraAge;
    uint32_t  mGalXtraAge;
    uint32_t  mQzssXtraAge;
    uint32_t  mNavicXtraAge;
    uint32_t  mGpsXtraValid;
    uint32_t  mGloXtraValid;
    uint64_t  mBdsXtraValid;
    uint64_t  mGalXtraValid;
    uint8_t   mQzssXtraValid;
    uint32_t  mNavicXtraValid;
    inline SystemStatusXtra() :
        mXtraValidMask(0),
        mGpsXtraAge(0),
        mGloXtraAge(0),
        mBdsXtraAge(0),
        mGalXtraAge(0),
        mQzssXtraAge(0),
        mNavicXtraAge(0),
        mGpsXtraValid(0),
        mGloXtraValid(0),
        mBdsXtraValid(0ULL),
        mGalXtraValid(0ULL),
        mQzssXtraValid(0),
        mNavicXtraValid(0) {}
    inline SystemStatusXtra(const SystemStatusPQWP3& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWP4;
class SystemStatusEphemeris : public SystemStatusItemBase
{
public:
    uint32_t  mGpsEpheValid;
    uint32_t  mGloEpheValid;
    uint64_t  mBdsEpheValid;
    uint64_t  mGalEpheValid;
    uint8_t   mQzssEpheValid;
    inline SystemStatusEphemeris() :
        mGpsEpheValid(0),
        mGloEpheValid(0),
        mBdsEpheValid(0ULL),
        mGalEpheValid(0ULL),
        mQzssEpheValid(0) {}
    inline SystemStatusEphemeris(const SystemStatusPQWP4& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWP5;
class SystemStatusSvHealth : public SystemStatusItemBase
{
public:
    uint32_t  mGpsUnknownMask;
    uint32_t  mGloUnknownMask;
    uint64_t  mBdsUnknownMask;
    uint64_t  mGalUnknownMask;
    uint8_t   mQzssUnknownMask;
    uint32_t  mNavicUnknownMask;
    uint32_t  mGpsGoodMask;
    uint32_t  mGloGoodMask;
    uint64_t  mBdsGoodMask;
    uint64_t  mGalGoodMask;
    uint8_t   mQzssGoodMask;
    uint32_t  mNavicGoodMask;
    uint32_t  mGpsBadMask;
    uint32_t  mGloBadMask;
    uint64_t  mBdsBadMask;
    uint64_t  mGalBadMask;
    uint8_t   mQzssBadMask;
    uint32_t  mNavicBadMask;
    inline SystemStatusSvHealth() :
        mGpsUnknownMask(0),
        mGloUnknownMask(0),
        mBdsUnknownMask(0ULL),
        mGalUnknownMask(0ULL),
        mQzssUnknownMask(0),
        mNavicUnknownMask(0),
        mGpsGoodMask(0),
        mGloGoodMask(0),
        mBdsGoodMask(0ULL),
        mGalGoodMask(0ULL),
        mQzssGoodMask(0),
        mNavicGoodMask(0),
        mGpsBadMask(0),
        mGloBadMask(0),
        mBdsBadMask(0ULL),
        mGalBadMask(0ULL),
        mQzssBadMask(0),
        mNavicBadMask(0) {}
    inline SystemStatusSvHealth(const SystemStatusPQWP5& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWP6;
class SystemStatusPdr : public SystemStatusItemBase
{
public:
    uint32_t  mFixInfoMask;
    inline SystemStatusPdr() :
        mFixInfoMask(0) {}
    inline SystemStatusPdr(const SystemStatusPQWP6& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWP7;
struct SystemStatusNav
{
    GnssEphemerisType   mType;
    GnssEphemerisSource mSource;
    int32_t             mAgeSec;
};

class SystemStatusNavData : public SystemStatusItemBase
{
public:
    SystemStatusNav mNav[SV_ALL_NUM];
    inline SystemStatusNavData() {
        for (uint32_t i=0; i<SV_ALL_NUM; i++) {
            mNav[i].mType = GNSS_EPH_TYPE_UNKNOWN;
            mNav[i].mSource = GNSS_EPH_SOURCE_UNKNOWN;
            mNav[i].mAgeSec = 0;
        }
    }
    inline SystemStatusNavData(const SystemStatusPQWP7& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

class SystemStatusPQWS1;
class SystemStatusPositionFailure : public SystemStatusItemBase
{
public:
    uint32_t  mFixInfoMask;
    uint32_t  mHepeLimit;
    inline SystemStatusPositionFailure() :
        mFixInfoMask(0),
        mHepeLimit(0) {}
    inline SystemStatusPositionFailure(const SystemStatusPQWS1& nmea);
    bool equals(const SystemStatusItemBase& peer) override;
    void dump(void) override;
};

/******************************************************************************
 SystemStatus report data structure - from DataItem observer
******************************************************************************/
class SystemStatusAirplaneMode : public SystemStatusItemBase {
public:
    AirplaneModeDataItem mDataItem;
    inline SystemStatusAirplaneMode(bool mode=false): mDataItem(mode) {}
    inline SystemStatusAirplaneMode(const AirplaneModeDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mMode == ((const SystemStatusAirplaneMode&)peer).mDataItem.mMode;
    }
};

class SystemStatusENH : public SystemStatusItemBase {
public:
    ENHDataItem mDataItem;
    inline SystemStatusENH(bool enabled, ENHDataItem::Fields updateBit = ENHDataItem::FIELD_MAX):
            mDataItem(enabled, updateBit) {}
    inline SystemStatusENH(const ENHDataItem& itemBase): mDataItem(itemBase) {}
    inline virtual SystemStatusItemBase& collate(SystemStatusItemBase& peer) {
        mDataItem.mEnhFields = ((const SystemStatusENH&)peer).mDataItem.mEnhFields;
        mDataItem.updateFields();
        return *this;
    }
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mEnhFields == ((const SystemStatusENH&)peer).mDataItem.mEnhFields;
    }
};

class SystemStatusGpsState : public SystemStatusItemBase {
public:
    GPSStateDataItem mDataItem;
    inline SystemStatusGpsState(bool enabled=false): mDataItem(enabled) {}
    inline SystemStatusGpsState(const GPSStateDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mEnabled == ((const SystemStatusGpsState&)peer).mDataItem.mEnabled;
    }
    inline void dump(void) override {
        LOC_LOGD("GpsState: state=%u", mDataItem.mEnabled);
    }
};

class SystemStatusNLPStatus : public SystemStatusItemBase {
public:
    NLPStatusDataItem mDataItem;
    inline SystemStatusNLPStatus(bool enabled=false): mDataItem(enabled) {}
    inline SystemStatusNLPStatus(const NLPStatusDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mEnabled == ((const SystemStatusNLPStatus&)peer).mDataItem.mEnabled;
    }
};

class SystemStatusWifiHardwareState : public SystemStatusItemBase {
public:
    WifiHardwareStateDataItem mDataItem;
    inline SystemStatusWifiHardwareState(bool enabled=false): mDataItem(enabled) {}
    inline SystemStatusWifiHardwareState(const WifiHardwareStateDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mEnabled == ((const SystemStatusWifiHardwareState&)peer).mDataItem.mEnabled;
    }
};

class SystemStatusNetworkInfo : public SystemStatusItemBase {
public:
    NetworkInfoDataItem mDataItem;
    inline SystemStatusNetworkInfo(int32_t type=0, std::string typeName="", string subTypeName="",
            bool connected=false, bool roaming=false,
            uint64_t networkHandle=NETWORK_HANDLE_UNKNOWN, string apn = "") :
                mDataItem((NetworkType)type, type, typeName, subTypeName, connected && (!roaming),
                          connected, roaming, networkHandle, apn) {}
    inline SystemStatusNetworkInfo(const NetworkInfoDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        const NetworkInfoDataItem peerDI = ((const SystemStatusNetworkInfo&)peer).mDataItem;
        bool rtv = (mDataItem.mAllTypes == peerDI.mAllTypes) &&
                (mDataItem.mConnected == peerDI.mConnected);
        for (uint8_t i = 0; rtv && i < MAX_NETWORK_HANDLES; ++i) {
            rtv = (mDataItem.mAllNetworkHandles[i] == peerDI.mAllNetworkHandles[i]) && rtv;
        }
        rtv = rtv && !peerDI.mApn.compare(mDataItem.mApn);
        LOC_LOGv("NetworkInfoDataItem quals: %d", rtv);
        return rtv;
    }
    inline virtual SystemStatusItemBase& collate(SystemStatusItemBase& curInfo) {
        LOC_LOGv("NetworkInfo: mAllTypes=%" PRIx64 " connected=%u mType=%x mApn=%s",
                 mDataItem.mAllTypes, mDataItem.mConnected, mDataItem.mType,
                 mDataItem.mApn.c_str());
        uint64_t allTypes = (static_cast<SystemStatusNetworkInfo&>(curInfo)).mDataItem.mAllTypes;
        string& apn = (static_cast<SystemStatusNetworkInfo&>(curInfo)).mDataItem.mApn;
        // Replace current with cached table for now and then update
        memcpy(mDataItem.mAllNetworkHandles,
               static_cast<SystemStatusNetworkInfo&>(curInfo).mDataItem.getNetworkHandle(),
               sizeof(mDataItem.mAllNetworkHandles));
        // Update the apn for non-mobile type connections.
        if (TYPE_MOBILE != mDataItem.mType && apn.compare("") != 0) {
            mDataItem.mApn = apn;
        }
        if (mDataItem.mConnected) {
            mDataItem.mAllTypes |= allTypes;
            for (uint8_t i = 0; i < MAX_NETWORK_HANDLES; ++i) {
                if (mDataItem.mNetworkHandle ==
                        mDataItem.mAllNetworkHandles[i].networkHandle) {
                    LOC_LOGD("collate duplicate detected, not updating");
                    break;
                }
                if (NETWORK_HANDLE_UNKNOWN ==
                        mDataItem.mAllNetworkHandles[i].networkHandle) {
                    mDataItem.mAllNetworkHandles[i].networkHandle =
                            mDataItem.mNetworkHandle;
                    mDataItem.mAllNetworkHandles[i].networkType =
                            (loc_core::NetworkType) mDataItem.mType;
                    break;
                }
            }
        } else if (0 != mDataItem.mAllTypes) {
            uint8_t deletedIndex = MAX_NETWORK_HANDLES;
            uint8_t lastValidIndex = 0;
            uint8_t typeCount = 0;
            for (; lastValidIndex < MAX_NETWORK_HANDLES && NETWORK_HANDLE_UNKNOWN !=
                    mDataItem.mAllNetworkHandles[lastValidIndex].networkHandle;
                 ++lastValidIndex) {
                // Maintain count for number of network handles still
                // connected for given type
                if (mDataItem.mType ==
                        mDataItem.mAllNetworkHandles[lastValidIndex].networkType) {
                    if (mDataItem.mNetworkHandle ==
                            mDataItem.mAllNetworkHandles[lastValidIndex].networkHandle) {
                        deletedIndex = lastValidIndex;
                    } else {
                        typeCount++;
                    }
                }

            }
            if (lastValidIndex > 0) {
                --lastValidIndex;
            }

            if (MAX_NETWORK_HANDLES != deletedIndex) {
                LOC_LOGd("deletedIndex:%u, lastValidIndex:%u, typeCount:%u",
                        deletedIndex, lastValidIndex, typeCount);
                mDataItem.mAllNetworkHandles[deletedIndex] =
                        mDataItem.mAllNetworkHandles[lastValidIndex];
                mDataItem.mAllNetworkHandles[lastValidIndex].networkHandle =
                        NETWORK_HANDLE_UNKNOWN;
                mDataItem.mAllNetworkHandles[lastValidIndex].networkType = TYPE_UNKNOWN;
            }

            // If no more handles of given type, set bitmask
            if (0 == typeCount) {
                mDataItem.mAllTypes = (allTypes & (~mDataItem.mAllTypes));
                LOC_LOGD("mAllTypes:%" PRIx64, mDataItem.mAllTypes);
            }
        } // else (mDataItem.mConnected == false && mDataItem.mAllTypes == 0)
          // we keep mDataItem->mAllTypes as 0, which means no more connections.
        return *this;
    }
    inline void dump(void) override {
        LOC_LOGD("NetworkInfo: mAllTypes=%" PRIx64 " connected=%u mType=%x mApn=%s",
                 mDataItem.mAllTypes, mDataItem.mConnected, mDataItem.mType,
                 mDataItem.mApn.c_str());
    }
};

class SystemStatusServiceInfo : public SystemStatusItemBase {
public:
    RilServiceInfoDataItem mDataItem;
    inline SystemStatusServiceInfo(): mDataItem() {}
    inline SystemStatusServiceInfo(const RilServiceInfoDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return ((const SystemStatusServiceInfo&)peer).mDataItem == mDataItem;
    }
};

class SystemStatusRilCellInfo : public SystemStatusItemBase {
public:
    RilCellInfoDataItem mDataItem;
    inline SystemStatusRilCellInfo(): mDataItem() {}
    inline SystemStatusRilCellInfo(const RilCellInfoDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return ((const SystemStatusRilCellInfo&)peer).mDataItem == mDataItem;
    }
};

class SystemStatusServiceStatus : public SystemStatusItemBase {
public:
    ServiceStatusDataItem mDataItem;
    inline SystemStatusServiceStatus(int32_t mServiceState=0): mDataItem(mServiceState) {}
    inline SystemStatusServiceStatus(const ServiceStatusDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mServiceState ==
                ((const SystemStatusServiceStatus&)peer).mDataItem.mServiceState;
    }
};

class SystemStatusModel : public SystemStatusItemBase {
public:
    ModelDataItem mDataItem;
    inline SystemStatusModel(string name=""): mDataItem(name) {}
    inline SystemStatusModel(const ModelDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mModel == ((const SystemStatusModel&)peer).mDataItem.mModel;
    }
};

class SystemStatusManufacturer : public SystemStatusItemBase {
public:
    ManufacturerDataItem mDataItem;
    inline SystemStatusManufacturer(string name=""): mDataItem(name) {}
    inline SystemStatusManufacturer(const ManufacturerDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mManufacturer ==
                ((const SystemStatusManufacturer&)peer).mDataItem.mManufacturer;
    }
};

class SystemStatusAssistedGps : public SystemStatusItemBase {
public:
    AssistedGpsDataItem mDataItem;
    inline SystemStatusAssistedGps(bool enabled=false): mDataItem(enabled) {}
    inline SystemStatusAssistedGps(const AssistedGpsDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mEnabled == ((const SystemStatusAssistedGps&)peer).mDataItem.mEnabled;
    }
};

class SystemStatusScreenState : public SystemStatusItemBase {
public:
    ScreenStateDataItem mDataItem;
    inline SystemStatusScreenState(bool state=false): mDataItem(state) {}
    inline SystemStatusScreenState(const ScreenStateDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mState == ((const SystemStatusScreenState&)peer).mDataItem.mState;
    }
};

class SystemStatusPowerConnectState : public SystemStatusItemBase {
public:
    PowerConnectStateDataItem mDataItem;
    inline SystemStatusPowerConnectState(bool state=false): mDataItem(state) {}
    inline SystemStatusPowerConnectState(const PowerConnectStateDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mState == ((const SystemStatusPowerConnectState&)peer).mDataItem.mState;
    }
};

class SystemStatusTimeZoneChange : public SystemStatusItemBase {
public:
    TimeZoneChangeDataItem mDataItem;
    inline SystemStatusTimeZoneChange(int64_t currTimeMillis=0ULL, int32_t rawOffset=0,
            int32_t dstOffset=0): mDataItem(currTimeMillis, rawOffset, dstOffset) {}
    inline SystemStatusTimeZoneChange(const TimeZoneChangeDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mCurrTimeMillis ==
                ((const SystemStatusTimeZoneChange&)peer).mDataItem.mCurrTimeMillis &&
                mDataItem.mRawOffsetTZ ==
                ((const SystemStatusTimeZoneChange&)peer).mDataItem.mRawOffsetTZ &&
                mDataItem.mDstOffsetTZ ==
                ((const SystemStatusTimeZoneChange&)peer).mDataItem.mDstOffsetTZ;
    }
};

class SystemStatusTimeChange : public SystemStatusItemBase {
public:
    TimeChangeDataItem mDataItem;
    inline SystemStatusTimeChange(
        int64_t currTimeMillis=0ULL, int32_t rawOffset=0, int32_t dstOffset=0):
        mDataItem(currTimeMillis, rawOffset, dstOffset) {}
    inline SystemStatusTimeChange(const TimeChangeDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mCurrTimeMillis ==
                ((const SystemStatusTimeChange&)peer).mDataItem.mCurrTimeMillis &&
                mDataItem.mRawOffsetTZ ==
                ((const SystemStatusTimeChange&)peer).mDataItem.mRawOffsetTZ &&
                mDataItem.mDstOffsetTZ ==
                ((const SystemStatusTimeChange&)peer).mDataItem.mDstOffsetTZ;
    }
};

class SystemStatusWifiSupplicantStatus : public SystemStatusItemBase {
public:
    WifiSupplicantStatusDataItem mDataItem;
    inline SystemStatusWifiSupplicantStatus(): mDataItem() {}
    inline SystemStatusWifiSupplicantStatus(const WifiSupplicantStatusDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mState ==
                ((const SystemStatusWifiSupplicantStatus&)peer).mDataItem.mState &&
                mDataItem.mApMacAddressValid ==
                ((const SystemStatusWifiSupplicantStatus&)peer).mDataItem.mApMacAddressValid &&
                mDataItem.mWifiApSsidValid ==
                ((const SystemStatusWifiSupplicantStatus&)peer).mDataItem.mWifiApSsidValid &&
                mDataItem.mWifiApSsid ==
                ((const SystemStatusWifiSupplicantStatus&)peer).mDataItem.mWifiApSsid;
        }
};

class SystemStatusShutdownState : public SystemStatusItemBase {
public:
    ShutdownStateDataItem mDataItem;
    inline SystemStatusShutdownState(bool state=false): mDataItem(state) {}
    inline SystemStatusShutdownState(const ShutdownStateDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mState == ((const SystemStatusShutdownState&)peer).mDataItem.mState;
    }
};

class SystemStatusTac : public SystemStatusItemBase {
public:
    TacDataItem mDataItem;
    inline SystemStatusTac(std::string value=""): mDataItem(value) {}
    inline SystemStatusTac(const TacDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mValue == ((const SystemStatusTac&)peer).mDataItem.mValue;
    }
    inline void dump(void) override {
        LOC_LOGD("Tac: value=%s", mDataItem.mValue.c_str());
    }
};

class SystemStatusMccMnc : public SystemStatusItemBase {
public:
    MccmncDataItem mDataItem;
    inline SystemStatusMccMnc(std::string value=""): mDataItem(value) {}
    inline SystemStatusMccMnc(const MccmncDataItem& itemBase): mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mValue == ((const SystemStatusMccMnc&)peer).mDataItem.mValue;
    }
    inline void dump(void) override {
        LOC_LOGD("TacMccMncCountry value=%s", mDataItem.mValue.c_str());
    }
};

class SystemStatusInEmergencyCall : public SystemStatusItemBase {
public:
    InEmergencyCallDataItem mDataItem;
    inline SystemStatusInEmergencyCall(bool value = false): mDataItem(value) {}
    inline SystemStatusInEmergencyCall(const InEmergencyCallDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mIsEmergency ==
                    ((const SystemStatusInEmergencyCall&)peer).mDataItem.mIsEmergency;
    }
    inline void dump(void) override {
        LOC_LOGd("In Emergency Call: %d", mDataItem.mIsEmergency);
    }
};

class SystemStatusPreciseLocationEnabled : public SystemStatusItemBase {
public:
    PreciseLocationEnabledDataItem mDataItem;
    inline SystemStatusPreciseLocationEnabled(bool value = false): mDataItem(value) {}
    inline SystemStatusPreciseLocationEnabled(const PreciseLocationEnabledDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mPreciseLocationEnabled ==
            ((const SystemStatusPreciseLocationEnabled&)peer).mDataItem.mPreciseLocationEnabled;
    }
    inline void dump(void) override {
        LOC_LOGd("Precise Location Enabled: %d", mDataItem.mPreciseLocationEnabled);
    }
};

class SystemStatusTrackingStarted : public SystemStatusItemBase {
public:
    TrackingStartedDataItem mDataItem;
    inline SystemStatusTrackingStarted(bool value = false): mDataItem(value) {}
    inline SystemStatusTrackingStarted(const TrackingStartedDataItem& itemBase):
        mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mTrackingStarted ==
            ((const SystemStatusTrackingStarted&)peer).mDataItem.mTrackingStarted;
    }
    inline void dump(void) override {
        LOC_LOGd("Tracking started: %d", mDataItem.mTrackingStarted);
    }
};

class SystemStatusNtripStarted : public SystemStatusItemBase {
public:
    NtripStartedDataItem mDataItem;
    inline SystemStatusNtripStarted(bool value = false): mDataItem(value) {}
    inline SystemStatusNtripStarted(const NtripStartedDataItem& itemBase):
        mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mNtripStarted ==
            ((const SystemStatusNtripStarted&)peer).mDataItem.mNtripStarted;
    }
    inline void dump(void) override {
        LOC_LOGd("Ntrip started: %d", mDataItem.mNtripStarted);
    }
};

class SystemStatusLocFeatureStatus : public SystemStatusItemBase {
public:
    LocFeatureStatusDataItem mDataItem;
    inline SystemStatusLocFeatureStatus(std::unordered_set<int> fids) : mDataItem(fids) {}
    inline SystemStatusLocFeatureStatus(const LocFeatureStatusDataItem& itemBase):
            mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mFids ==
            ((const SystemStatusLocFeatureStatus&)peer).mDataItem.mFids;
    }
    inline void dump(void) override {
        string str;
        mDataItem.stringify(str);
        LOC_LOGd("Location feature qwes status: %s", str.c_str());
    }
};

class SystemStatusNlpSessionStarted : public SystemStatusItemBase {
public:
    NlpSessionStartedDataItem mDataItem;
    inline SystemStatusNlpSessionStarted(bool value = false): mDataItem(value) {}
    inline SystemStatusNlpSessionStarted(const NlpSessionStartedDataItem& itemBase):
        mDataItem(itemBase) {}
    inline bool equals(const SystemStatusItemBase& peer) override {
        return mDataItem.mNlpStarted ==
            ((const SystemStatusNlpSessionStarted&)peer).mDataItem.mNlpStarted;
    }
    inline void dump(void) override {
        LOC_LOGd("NLP Session started: %d", mDataItem.mNlpStarted);
    }
};

/******************************************************************************
 SystemStatusReports
******************************************************************************/
class SystemStatusReports
{
public:
    // from QMI_LOC indication
    std::vector<SystemStatusLocation>         mLocation;

    // from ME debug NMEA
    std::vector<SystemStatusTimeAndClock>     mTimeAndClock;
    std::vector<SystemStatusXoState>          mXoState;
    std::vector<SystemStatusRfAndParams>      mRfAndParams;
    std::vector<SystemStatusErrRecovery>      mErrRecovery;

    // from PE debug NMEA
    std::vector<SystemStatusInjectedPosition> mInjectedPosition;
    std::vector<SystemStatusBestPosition>     mBestPosition;
    std::vector<SystemStatusXtra>             mXtra;
    std::vector<SystemStatusEphemeris>        mEphemeris;
    std::vector<SystemStatusSvHealth>         mSvHealth;
    std::vector<SystemStatusPdr>              mPdr;
    std::vector<SystemStatusNavData>          mNavData;

    // from SM debug NMEA
    std::vector<SystemStatusPositionFailure>  mPositionFailure;

    // from dataitems observer
    std::vector<SystemStatusAirplaneMode>     mAirplaneMode;
    std::vector<SystemStatusENH>              mENH;
    std::vector<SystemStatusGpsState>         mGPSState;
    std::vector<SystemStatusNLPStatus>        mNLPStatus;
    std::vector<SystemStatusWifiHardwareState> mWifiHardwareState;
    std::vector<SystemStatusNetworkInfo>      mNetworkInfo;
    std::vector<SystemStatusServiceInfo>      mRilServiceInfo;
    std::vector<SystemStatusRilCellInfo>      mRilCellInfo;
    std::vector<SystemStatusServiceStatus>    mServiceStatus;
    std::vector<SystemStatusModel>            mModel;
    std::vector<SystemStatusManufacturer>     mManufacturer;
    std::vector<SystemStatusInEmergencyCall>  mInEmergencyCall;
    std::vector<SystemStatusAssistedGps>      mAssistedGps;
    std::vector<SystemStatusScreenState>      mScreenState;
    std::vector<SystemStatusPowerConnectState> mPowerConnectState;
    std::vector<SystemStatusTimeZoneChange>   mTimeZoneChange;
    std::vector<SystemStatusTimeChange>       mTimeChange;
    std::vector<SystemStatusWifiSupplicantStatus> mWifiSupplicantStatus;
    std::vector<SystemStatusShutdownState>    mShutdownState;
    std::vector<SystemStatusTac>              mTac;
    std::vector<SystemStatusMccMnc>           mMccMnc;
    std::vector<SystemStatusPreciseLocationEnabled>  mPreciseLocationEnabled;
    std::vector<SystemStatusTrackingStarted>  mTrackingStarted;
    std::vector<SystemStatusNtripStarted>  mNtripStarted;
    std::vector<SystemStatusLocFeatureStatus>  mLocFeatureStatus;
    std::vector<SystemStatusNlpSessionStarted>  mNlpSessionStarted;
};

/******************************************************************************
 SystemStatus
******************************************************************************/
class SystemStatus
{
private:
    static SystemStatus                       *mInstance;
    SystemStatusOsObserver                    mSysStatusObsvr;
    // ctor
    SystemStatus(const MsgTask* msgTask);
    // dtor
    inline ~SystemStatus() {}

    // Data members
    static pthread_mutex_t                    mMutexSystemStatus;
    SystemStatusReports mCache;
    bool mTracking;

    template <typename TYPE_REPORT, typename TYPE_ITEM>
    bool setIteminReport(TYPE_REPORT& report, TYPE_ITEM&& s);

    // set default dataitem derived item in report cache
    template <typename TYPE_REPORT, typename TYPE_ITEM>
    void setDefaultIteminReport(TYPE_REPORT& report, const TYPE_ITEM& s);

    template <typename TYPE_REPORT, typename TYPE_ITEM>
    void getIteminReport(TYPE_REPORT& reportout, const TYPE_ITEM& c) const;

public:
    // Static methods
    static SystemStatus* getInstance(const MsgTask* msgTask);
    static void destroyInstance();
    IOsObserver* getOsObserver();

    // Helpers
    bool eventPosition(const UlpLocation& location,const GpsLocationExtended& locationEx);
    bool eventDataItemNotify(IDataItemCore* dataitem);
    bool setNmeaString(const char *data, uint32_t len);
    void setEngineDebugDataInfo(const GnssEngineDebugDataInfo& gnssEngineDebugDataInfo);
    bool getReport(SystemStatusReports& reports, bool isLatestonly = false,
            bool inSessionOnly = true) const;
    bool setDefaultGnssEngineStates(void);
    bool eventConnectionStatus(bool connected, int8_t type,
                               bool roaming, NetworkHandle networkHandle, const string& apn);
    bool updatePowerConnectState(bool charging);
    void resetNetworkInfo();
    bool eventOptInStatus(bool userConsent);
    bool eventRegionStatus(bool region);
    bool eventInEmergencyCall(bool isEmergency);
    bool eventSetTracking(bool tracking, bool updateSysStatusTrkState);
    bool eventNtripStarted(bool ntripStarted);
    bool eventPreciseLocation(bool preciseLocation);
    bool eventLocFeatureStatus(std::unordered_set<int> fids);
    bool eventNlpSessionStatus(bool nlpStarted);
    bool eventGpsEnabled(bool gpsEnabled);
};

} // namespace loc_core

#endif //__SYSTEM_STATUS__

