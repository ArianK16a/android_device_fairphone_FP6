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

#define LOG_TAG "LocSvc_XtraSystemStatusObs"

#include <sys/stat.h>
#include <sys/un.h>
#include <errno.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <math.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <loc_log.h>
#include <loc_nmea.h>
#include <SystemStatus.h>
#include <vector>
#include <sstream>
#include <LocAdapterBase.h>
#include <DataItemId.h>
#include <DataItemsFactoryProxy.h>
#include <DataItemConcreteTypes.h>
#include <GnssAdapter.h>
#include <XtraSystemStatusObserver.h>

using namespace loc_util;
using namespace loc_core;

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "LocSvc_XSSO"

class XtraIpcListener : public ILocIpcListener {
    IOsObserver*    mSystemStatusObsrvr;
    const MsgTask* mMsgTask;
    XtraSystemStatusObserver& mXSSO;
public:
    inline XtraIpcListener(IOsObserver* observer, const MsgTask* msgTask,
                           XtraSystemStatusObserver& xsso) :
            mSystemStatusObsrvr(observer), mMsgTask(msgTask), mXSSO(xsso) {}
    virtual void onReceive(const char* data, uint32_t length,
                           const LocIpcRecver* recver) override {
#define STRNCMP(str, constStr) strncmp(str, constStr, sizeof(constStr)-1)
        if (!STRNCMP(data, "ping")) {
            LOC_LOGd("ping received");
#ifdef USE_GLIB
        } else if ((!STRNCMP(data, "connectBackhaul")) || (!STRNCMP(data, "disconnectBackhaul"))) {
            char clientName[30] = {0};
            uint16_t prefSub;
            char prefApnName[30] = {0};
            uint16_t prefIpType;
            int ret = sscanf(data, "%*s %29s %u %29s %u",
                             clientName, &prefSub, prefApnName, &prefIpType);
            BackhaulContext ctx = { clientName, prefSub,
                    (0 == STRNCMP(prefApnName, "EMPTY")) ? "" : prefApnName, prefIpType };

            if (!STRNCMP(data, "connectBackhaul")) {
                mSystemStatusObsrvr->connectBackhaul(ctx);
            } else {
                mSystemStatusObsrvr->disconnectBackhaul(ctx);
            }
#endif
        } else if (!STRNCMP(data, "requestStatus")) {
            int32_t xtraStatusUpdated = 0;
            char socketName[40] = {0};
            sscanf(data, "%*s %d %39s", &xtraStatusUpdated, socketName);

            struct HandleStatusRequestMsg : public LocMsg {
                XtraSystemStatusObserver& mXSSO;
                int32_t mStatusUpdated;
                string mSocketName;
                inline HandleStatusRequestMsg(XtraSystemStatusObserver& xsso,
                                              int32_t statusUpdated, string socketName) :
                        mXSSO(xsso), mStatusUpdated(statusUpdated),
                        mSocketName(socketName) {}
                inline void proc() const override {
                    mXSSO.onStatusRequested(mStatusUpdated);
                    /* SSR for DGnss Ntrip Source*/
                    if (0 == mSocketName.compare(LOC_IPC_DGNSS)) {
                        mXSSO.restartDgnssSource();
                    }
                    mXSSO.registerXtraStatusUpdate(0, mXSSO.mRegisterForXtraStatus);
                }
            };
            mMsgTask->sendMsg(new HandleStatusRequestMsg(mXSSO, xtraStatusUpdated, socketName));
        } else if (!STRNCMP(data, "xtraStatusUpdate")) {
            uint32_t sessionId = 0;
            uint8_t downloadReason[XTRA_STATS_DL_REASON_CODE_MAX_LEN];
            XtraStatusUpdateType updateType = XTRA_STATUS_UPDATE_UNDEFINED;
            GnssConfig gnssConfig = {};
            gnssConfig.size = sizeof(gnssConfig);
            gnssConfig.flags = GNSS_CONFIG_FLAGS_XTRA_STATUS_BIT;
            sscanf(data, "%*s %d %d %d %d %d %63s", &sessionId, &updateType,
                   (int *)&gnssConfig.xtraStatus.featureEnabled,
                   &gnssConfig.xtraStatus.xtraDataStatus,
                   &gnssConfig.xtraStatus.xtraValidForHours,
                   &downloadReason[0]);
            std::string lastDownloadReason((char *) &downloadReason[0]);
            gnssConfig.xtraStatus.lastDownloadReasonCode = lastDownloadReason;

            mXSSO.mAdapter->reportGnssConfigEvent(sessionId, gnssConfig);
        } else if (!STRNCMP(data, "xtraMpDisabled")) {
            mXSSO.mAdapter->reportXtraMpDisabledEvent();
        } else if (!STRNCMP(data, "setConstellation")) {
            GnssSvTypeConfig constellationsConfig;
            constellationsConfig.size = sizeof(GnssSvTypeConfig);

            sscanf(data, "%*s %" PRIu64 " %" PRIu64, &constellationsConfig.enabledSvTypesMask,
                    &constellationsConfig.blacklistedSvTypesMask);
            mXSSO.mAdapter->gnssUpdateSvTypeConfigCommand(constellationsConfig,
                    SV_TYPE_CONFIG_FROM_XTRA);
        } else {
            LOC_LOGw("unknown event: %s", data);
        }
    }
};

XtraSystemStatusObserver::XtraSystemStatusObserver(GnssAdapter* adapter,
                                                   IOsObserver* sysStatObs,
                                                   const MsgTask* msgTask) :
        mAdapter(adapter), mSystemStatusObsrvr(sysStatObs), mMsgTask(msgTask),
        mGpsLock(-1), mConnections(~0), mRoaming(false), mXtraThrottle(true),
        mReqStatusReceived(false),
        mIsConnectivityStatusKnown(false),
        mXtraSender(LocIpc::getLocIpcLocalSender(LOC_IPC_XTRA)),
        mDgnssSender(LocIpc::getLocIpcLocalSender(LOC_IPC_DGNSS)),
        mRegisterForXtraStatus(false),
        mDelayLocTimer(*mXtraSender, *mDgnssSender) {
    subscribe(true);
}

void XtraSystemStatusObserver::init() {
    locUtilWaitForDir(SOCKET_DIR_LOCATION);
    auto recver = LocIpc::getLocIpcLocalRecver(
            make_shared<XtraIpcListener>(mSystemStatusObsrvr, mMsgTask, *this),
            LOC_IPC_HAL);
    mIpc.startNonBlockingListening(recver);
    mDelayLocTimer.start(100 /*.1 sec*/,  false);
}

bool XtraSystemStatusObserver::updateLockStatus(GnssConfigGpsLock lock) {
    // mask NI(NFW bit) since from XTRA's standpoint GPS is enabled if
    // MO(AFW bit) is enabled and disabled when MO is disabled
    mGpsLock = lock & ~GNSS_CONFIG_GPS_LOCK_NFW_ALL;

    if (!mReqStatusReceived) {
        return true;
    }

    stringstream ss;
    ss <<  "gpslock";
    ss << " " << mGpsLock;
    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::updateConnections(uint64_t allConnections,
        NetworkInfoType* networkHandleInfo, bool roaming) {
    mIsConnectivityStatusKnown = true;
    mConnections = allConnections;
    mRoaming = roaming;

    LOC_LOGd("updateConnections mConnections:%" PRIx64 " mRoaming:%u",
        mConnections, mRoaming);
    for (uint8_t i = 0; i < MAX_NETWORK_HANDLES; ++i) {
        mNetworkHandle[i] = networkHandleInfo[i];
        LOC_LOGd("updateConnections [%d] networkHandle:%" PRIx64 " networkType:%u",
            i, mNetworkHandle[i].networkHandle, mNetworkHandle[i].networkType);
    }

    if (!mReqStatusReceived) {
        return true;
    }

    stringstream ss;
    ss << "connection" << endl << mConnections << endl
            << mNetworkHandle[0].toString() << endl
            << mNetworkHandle[1].toString() << endl
            << mNetworkHandle[2].toString() << endl
            << mNetworkHandle[3].toString() << endl
            << mNetworkHandle[4].toString() << endl
            << mNetworkHandle[5].toString() << endl
            << mNetworkHandle[6].toString() << endl
            << mNetworkHandle[7].toString() << endl
            << mNetworkHandle[8].toString() << endl
            << mNetworkHandle[MAX_NETWORK_HANDLES-1].toString() << endl
            << (mRoaming ? 1 : 0);
    string s = ss.str();
    LocIpc::send(*mDgnssSender, (const uint8_t*)s.data(), s.size());
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()));
}

bool XtraSystemStatusObserver::updateTac(const string& tac) {
    mTac = tac;

    if (!mReqStatusReceived) {
        return true;
    }

    stringstream ss;
    ss <<  "tac";
    ss << " " << tac.c_str();
    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::updateMccMnc(const string& mccmncCountry) {
    mMccmnc = mccmncCountry;

    if (!mReqStatusReceived) {
        return true;
    }

    stringstream ss;
    ss <<  "mncmcc";
    ss << " " << mccmncCountry.c_str();
    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::updateXtraThrottle(const bool enabled) {
    mXtraThrottle = enabled;

    if (!mReqStatusReceived) {
        return true;
    }

    stringstream ss;
    ss <<  "xtrathrottle";
    ss << " " << (enabled ? 1 : 0);
    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::notifySessionStart() {
    if (!mReqStatusReceived) {
        return true;
    }

    string s = "sessionstart";
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::updatePowerState(const PowerStateType powerState) {

    if (mPowerState == powerState) {
        return true;
    }

    mPowerState = powerState;

    if (!mReqStatusReceived) {
        return true;
    }

    int32_t pState;
    switch (mPowerState) {
        case POWER_STATE_UNKNOWN:
            pState = 0;
            break;
        case POWER_STATE_DEEP_SLEEP_ENTRY:
        case POWER_STATE_SUSPEND:
            pState = 1;
            break;
        case POWER_STATE_DEEP_SLEEP_EXIT:
        case POWER_STATE_RESUME:
            pState = 2;
            break;
        case POWER_STATE_SHUTDOWN:
            pState = 3;
            break;
        default:
            LOC_LOGd("Invalid power state %d", mPowerState);
            break;
    };

    stringstream ss;
    ss <<  "powerstate";
    ss << " " << pState;
    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

inline bool XtraSystemStatusObserver::onStatusRequested(int32_t statusUpdated) {
    mReqStatusReceived = true;

    if (statusUpdated) {
        return true;
    }

    stringstream ss;

    ss << "respondStatus" << endl;
    (mGpsLock == -1 ? ss : ss << mGpsLock) << endl;
    (mConnections == (uint64_t)~0 ? ss : ss << mConnections) << endl
            << mNetworkHandle[0].toString() << endl
            << mNetworkHandle[1].toString() << endl
            << mNetworkHandle[2].toString() << endl
            << mNetworkHandle[3].toString() << endl
            << mNetworkHandle[4].toString() << endl
            << mNetworkHandle[5].toString() << endl
            << mNetworkHandle[6].toString() << endl
            << mNetworkHandle[7].toString() << endl
            << mNetworkHandle[8].toString() << endl
            << mNetworkHandle[MAX_NETWORK_HANDLES-1].toString() << endl
            << mTac << endl << mMccmnc << endl << mIsConnectivityStatusKnown;

    string s = ss.str();
    LocIpc::send(*mDgnssSender, (const uint8_t*)s.data(), s.size());
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

void XtraSystemStatusObserver::startDgnssSource(const StartDgnssNtripParams& params) {
    stringstream ss;
    const GnssNtripConnectionParams* ntripParams = &(params.ntripParams);

    ss <<  "startDgnssSource" << endl;
    ss << ntripParams->useSSL << endl;
    ss << ntripParams->hostNameOrIp.data() << endl;
    ss << ntripParams->port << endl;
    ss << ntripParams->mountPoint.data() << endl;
    ss << ntripParams->username.data() << endl;
    ss << ntripParams->password.data() << endl;
    ss << params.enableRTKEngine << endl;
    if (ntripParams->requiresNmeaLocation && !params.nmea.empty()) {
        ss << params.nmea.data() << endl;
    }
    string s = ss.str();

    LOC_LOGd("%s", s.data());
    LocIpc::send(*mDgnssSender, (const uint8_t*)s.data(), s.size());
    // make a local copy of the string for SSR
    mNtripParamsString.assign(std::move(s));
}

void XtraSystemStatusObserver::restartDgnssSource() {
    if (!mNtripParamsString.empty()) {
        LocIpc::send(*mDgnssSender,
            (const uint8_t*)mNtripParamsString.data(), mNtripParamsString.size());
        LOC_LOGv("Xtra SSR %s", mNtripParamsString.data());
    }
}

void XtraSystemStatusObserver::stopDgnssSource() {
    LOC_LOGv();
    mNtripParamsString.clear();

    const char s[] = "stopDgnssSource";
    LocIpc::send(*mDgnssSender, (const uint8_t*)s, strlen(s));
}

void XtraSystemStatusObserver::updateNmeaToDgnssServer(const string& nmea)
{
    stringstream ss;
    ss <<  "updateDgnssServerNmea" << endl;
    ss << nmea.data() << endl;

    string s = ss.str();
    LOC_LOGd("%s", s.data());
    LocIpc::send(*mDgnssSender, (const uint8_t*)s.data(), s.size());
}

bool XtraSystemStatusObserver::updateXtraConfig(bool enable, const XtraConfigParams& configParams) {
    if (!mReqStatusReceived) {
        return false;
    }

    stringstream ss;
    ss << "xtraConfig" << endl;
    ss << (enable ? 1 : 0) << endl;
    if (enable == true) {
        ss << configParams.xtraDownloadIntervalMinute << endl;
        ss << configParams.xtraDownloadTimeoutSec << endl;
        ss << configParams.xtraDownloadRetryIntervalMinute << endl;
        ss << configParams.xtraDownloadRetryAttempts << endl;
        ss << configParams.xtraCaPath << endl;

        if (configParams.xtraServerURLsCount == 2 ||
                configParams.ntpServerURLsCount == 2) {
             srand(time(0));
        }
        if (configParams.xtraServerURLsCount == 1) {
            ss << configParams.xtraServerURLs[0] << endl;
            ss << configParams.xtraServerURLs[0] << endl;
            ss << configParams.xtraServerURLs[0] << endl;
        } else if (configParams.xtraServerURLsCount == 2) {
            ss << configParams.xtraServerURLs[0] << endl;
            ss << configParams.xtraServerURLs[1] << endl;
            int index = rand() % 2;
            ss << configParams.xtraServerURLs[index] << endl;
        } else {
            ss << configParams.xtraServerURLs[0] << endl;
            ss << configParams.xtraServerURLs[1] << endl;
            ss << configParams.xtraServerURLs[2] << endl;
        }

        if (configParams.ntpServerURLsCount == 1) {
            ss << configParams.ntpServerURLs[0] << endl;
            ss << configParams.ntpServerURLs[0] << endl;
            ss << configParams.ntpServerURLs[0] << endl;
        } else if (configParams.ntpServerURLsCount == 2) {
            ss << configParams.ntpServerURLs[0] << endl;
            ss << configParams.ntpServerURLs[1] << endl;
            int index = rand() % 2;
            ss << configParams.ntpServerURLs[index] << endl;
        } else {
            ss << configParams.ntpServerURLs[0] << endl;
            ss << configParams.ntpServerURLs[1] << endl;
            ss << configParams.ntpServerURLs[2] << endl;
        }
        ss << configParams.xtraIntegrityDownloadEnable << endl;
        ss << configParams.xtraIntegrityDownloadIntervalMinute << endl;
        ss << configParams.xtraDaemonDebugLogLevel << endl;
        ss << configParams.ntsKeServerURL << endl;
        ss << configParams.xtraDaemonDiagLoggingStatus << endl;
    }

    string s = ss.str();
    LOC_LOGd("config params: %s", s.c_str());
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::getXtraStatus(uint32_t sessionId) {
    if (!mReqStatusReceived) {
        return false;
    }

    stringstream ss;
    ss << "getXtraStatus" << endl;
    ss << sessionId <<endl;

    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::registerXtraStatusUpdate(uint32_t sessionId,
                                                        bool registerUpdate) {
    if (!mReqStatusReceived) {
        return false;
    }

    mRegisterForXtraStatus = registerUpdate;

    stringstream ss;
    ss << "registerXtraStatusUpdate" << endl;
    ss << sessionId << endl;
    ss << registerUpdate << endl;

    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

bool XtraSystemStatusObserver::updateXtraDataDeletion() {
    if (!mReqStatusReceived) {
        return false;
    }

    stringstream ss;
    ss << "updateXtraDataDeletion" << endl;

    string s = ss.str();
    return ( LocIpc::send(*mXtraSender, (const uint8_t*)s.data(), s.size()) );
}

void XtraSystemStatusObserver::subscribe(bool yes)
{
    // Subscription data unordered_set
    unordered_set<DataItemId> subItemIdSet = {
            NETWORKINFO_DATA_ITEM_ID,
            MCCMNC_DATA_ITEM_ID,
            TRACKING_STARTED_DATA_ITEM_ID};

    if (yes) {
        mSystemStatusObsrvr->subscribe(subItemIdSet, this);
        unordered_set<DataItemId> reqItemIdSet = {TAC_DATA_ITEM_ID};
        mSystemStatusObsrvr->requestData(reqItemIdSet, this);
    } else {
        mSystemStatusObsrvr->unsubscribe(subItemIdSet, this);
    }
}

// IDataItemObserver overrides
void XtraSystemStatusObserver::getName(string& name)
{
    name = "XtraSystemStatusObserver";
}

void XtraSystemStatusObserver::notify(const unordered_set<IDataItemCore*>& dlist)
{
    struct HandleOsObserverUpdateMsg : public LocMsg {
        XtraSystemStatusObserver* mXtraSysStatObj;
        list <IDataItemCore*> mDataItemList;

        inline HandleOsObserverUpdateMsg(XtraSystemStatusObserver* xtraSysStatObs,
                const unordered_set<IDataItemCore*>& dataItemList) :
                mXtraSysStatObj(xtraSysStatObs) {
            for (auto eachItem : dataItemList) {
                IDataItemCore* dataitem = DataItemsFactoryProxy::createNewDataItem(eachItem);
                if (NULL == dataitem) {
                    break;
                }

                mDataItemList.push_back(dataitem);
            }
        }

        inline ~HandleOsObserverUpdateMsg() {
            for (auto itor = mDataItemList.begin(); itor != mDataItemList.end(); ++itor) {
                if (*itor != nullptr) {
                    delete *itor;
                    *itor = nullptr;
                }
            }
        }

        inline void proc() const {
            for (auto each : mDataItemList) {
                switch (each->getId())
                {
                    case NETWORKINFO_DATA_ITEM_ID:
                    {
                        NetworkInfoDataItem* networkInfo = static_cast<NetworkInfoDataItem*>(each);
                        NetworkInfoType* networkHandleInfo =
                                static_cast<NetworkInfoType*>(networkInfo->getNetworkHandle());
                        mXtraSysStatObj->updateConnections(networkInfo->getAllTypes(),
                                networkHandleInfo, (*networkInfo).mRoaming);
                    }
                    break;

                    case TAC_DATA_ITEM_ID:
                    {
                        TacDataItem* tac = static_cast<TacDataItem*>(each);
                        mXtraSysStatObj->updateTac(tac->mValue);
                    }
                    break;

                    case MCCMNC_DATA_ITEM_ID:
                    {
                        MccmncDataItem* mccmnc = static_cast<MccmncDataItem*>(each);
                        mXtraSysStatObj->updateMccMnc(mccmnc->mValue);
                    }
                    break;

                    case TRACKING_STARTED_DATA_ITEM_ID:
                    {
                        TrackingStartedDataItem* trackingStarted =
                                static_cast<TrackingStartedDataItem*>(each);
                        if (trackingStarted->mTrackingStarted) {
                            mXtraSysStatObj->notifySessionStart();
                        }
                    }
                    break;

                    default:
                    break;
                }
            }
        }
    };
    mMsgTask->sendMsg(new (nothrow) HandleOsObserverUpdateMsg(this, dlist));
}
