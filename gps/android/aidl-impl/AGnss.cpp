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
/*
Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/
#include <aidl/android/hardware/gnss/BnAGnss.h>
#include <aidl/android/hardware/gnss/IAGnssCallback.h>
#include "Gnss.h"
#include "AGnss.h"

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
static AGnss* spAGnss = nullptr;

void agnssServiceDied(void* cookie) {
    LOC_LOGe("IAGnss AIDL service died");
    AGnss* iface = static_cast<AGnss*>(cookie);
    if (iface != nullptr) {
        iface->setCallback(nullptr);
        iface = nullptr;
    }
}
AGnss::AGnss(Gnss* gnss) : mGnss(gnss), mType(LOC_AGPS_TYPE_INVALID),
    mDeathRecipient(AIBinder_DeathRecipient_new(&agnssServiceDied)) {
    spAGnss = this;
    LocationControlCallbacks locCtrlCbs;
    memset(&locCtrlCbs, 0, sizeof(locCtrlCbs));
    locCtrlCbs.size = sizeof(LocationControlCallbacks);

    locCtrlCbs.agpsStatusIpV4Cb = [this](AGnssExtStatusIpV4 status) {
            statusCb(status.type, status.status);
    };

    mGnss->getLocationControlApi()->updateCallbacks(locCtrlCbs);
}

AGnss::~AGnss() {
    spAGnss = nullptr;
}

void AGnss::statusCb(AGpsExtType type, LocAGpsStatusValue status) {

    IAGnssCallback::AGnssType  aType;
    IAGnssCallback::AGnssStatusValue aStatus;
    // cache the AGps Type
    if (type > LOC_AGPS_TYPE_INVALID && type <= LOC_AGPS_TYPE_SUPL_ES) {
        mMutex.lock();
        mType = type;
        mMutex.unlock();
    }

    switch (type) {
    case LOC_AGPS_TYPE_SUPL:
        aType = IAGnssCallback::AGnssType::SUPL;
        break;
    case LOC_AGPS_TYPE_SUPL_ES:
        aType = IAGnssCallback::AGnssType::SUPL_EIMS;
        break;
    default:
        LOC_LOGe("invalid type: %d", type);
        return;
    }

    switch (status) {
    case LOC_GPS_REQUEST_AGPS_DATA_CONN:
        aStatus = IAGnssCallback::AGnssStatusValue::REQUEST_AGNSS_DATA_CONN;
        break;
    case LOC_GPS_RELEASE_AGPS_DATA_CONN:
        aStatus = IAGnssCallback::AGnssStatusValue::RELEASE_AGNSS_DATA_CONN;
        break;
    case LOC_GPS_AGPS_DATA_CONNECTED:
        aStatus = IAGnssCallback::AGnssStatusValue::AGNSS_DATA_CONNECTED;
        break;
    case LOC_GPS_AGPS_DATA_CONN_DONE:
        aStatus = IAGnssCallback::AGnssStatusValue::AGNSS_DATA_CONN_DONE;
        break;
    case LOC_GPS_AGPS_DATA_CONN_FAILED:
        aStatus = IAGnssCallback::AGnssStatusValue::AGNSS_DATA_CONN_FAILED;
        break;
    default:
        LOC_LOGe("invalid status: %d", status);
        return;
    }

    mMutex.lock();
    auto aGnssCbIface = mAGnssCbIface;
    mMutex.unlock();
    if (aGnssCbIface != nullptr) {
        auto r = aGnssCbIface->agnssStatusCb(aType, aStatus);
        if (!r.isOk()) {
            LOC_LOGw("Error invoking AGNSS status cb");
        }
    }
    else {
        LOC_LOGw("setCallback has not been called yet");
    }
}

ScopedAStatus AGnss::setCallback(const shared_ptr<IAGnssCallback>& callback) {
    if (mGnss == nullptr || mGnss->getLocationControlApi() == nullptr) {
        LOC_LOGe("Null GNSS interface");
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    if (mAGnssCbIface != nullptr) {
        AIBinder_unlinkToDeath(mAGnssCbIface->asBinder().get(), mDeathRecipient, this);
    }

    // Save the interface
    mMutex.lock();
    mAGnssCbIface = callback;
    mMutex.unlock();
    if (mAGnssCbIface != nullptr) {
        AIBinder_linkToDeath(mAGnssCbIface->asBinder().get(), mDeathRecipient, this);
    }

    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::dataConnClosed() {
    if (mGnss == nullptr || mGnss->getLocationControlApi() == nullptr) {
        LOC_LOGe("Null GNSS interface");
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    mGnss->getLocationControlApi()->agpsDataConnClosed(AGPS_TYPE_SUPL);
    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::dataConnFailed() {
    if (mGnss == nullptr || mGnss->getLocationControlApi() == nullptr) {
        LOC_LOGe("Null GNSS interface");
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    mGnss->getLocationControlApi()->agpsDataConnFailed(AGPS_TYPE_SUPL);
    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::dataConnOpen(int64_t networkHandle, const std::string& apn,
        ::aidl::android::hardware::gnss::IAGnss::ApnIpType apnIpType) {
    if (mGnss == nullptr || mGnss->getLocationControlApi() == nullptr) {
        LOC_LOGe("Null GNSS interface");
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    std::string apnString(apn.c_str());
    mMutex.lock();
    auto agpsType = mType;
    mMutex.unlock();
    // During Emergency SUPL, an apn name of "sos" means that no
    // apn was found, like in the simless case, so apn is cleared
    if (LOC_AGPS_TYPE_SUPL_ES == agpsType && "sos" == apnString) {
        LOC_LOGd("dataConnOpen APN name = [sos] cleared");
        apnString.clear();
    }

    LOC_LOGd("dataConnOpen APN name = [%s]", apnString.c_str());

    AGpsBearerType bearerType;
    switch (apnIpType) {
    case IAGnss::ApnIpType::IPV4:
        bearerType = AGPS_APN_BEARER_IPV4;
        break;
    case IAGnss::ApnIpType::IPV6:
        bearerType = AGPS_APN_BEARER_IPV6;
        break;
    case IAGnss::ApnIpType::IPV4V6:
        bearerType = AGPS_APN_BEARER_IPV4V6;
        break;
    default:
        bearerType = AGPS_APN_BEARER_IPV4;
        break;
    }

    mGnss->getLocationControlApi()->agpsDataConnOpen(
        AGPS_TYPE_SUPL, apnString.c_str(), apnString.size(), (int)bearerType);
    return ScopedAStatus::ok();
}
ScopedAStatus AGnss::setServer(::aidl::android::hardware::gnss::IAGnssCallback::AGnssType type,
        const std::string& hostname, int32_t port) {
    if (mGnss == nullptr) {
        LOC_LOGe("]: mGnss is nullptr");
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    GnssConfig config;
    memset(&config, 0, sizeof(GnssConfig));
    config.size = sizeof(GnssConfig);
    config.flags = GNSS_CONFIG_FLAGS_SET_ASSISTANCE_DATA_VALID_BIT;
    config.assistanceServer.size = sizeof(GnssConfigSetAssistanceServer);
    if (type == IAGnssCallback::AGnssType::SUPL) {
        config.assistanceServer.type = GNSS_ASSISTANCE_TYPE_SUPL;
    } else if (type == IAGnssCallback::AGnssType::C2K) {
        config.assistanceServer.type = GNSS_ASSISTANCE_TYPE_C2K;
    } else if (type == IAGnssCallback::AGnssType::SUPL_EIMS) {
        config.assistanceServer.type = GNSS_ASSISTANCE_TYPE_SUPL_EIMS;
    } else if (type == IAGnssCallback::AGnssType::SUPL_IMS) {
        config.assistanceServer.type = GNSS_ASSISTANCE_TYPE_SUPL_IMS;
    } else {
        LOC_LOGe("]: invalid AGnssType: %d", static_cast<uint8_t>(type));
        return ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }
    config.assistanceServer.hostName = strdup(hostname.c_str());
    config.assistanceServer.port = port;
    mGnss->updateConfiguration(config);
    return ScopedAStatus::ok();
}
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
