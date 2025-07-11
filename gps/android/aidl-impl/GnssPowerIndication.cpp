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

#define LOG_TAG "GnssPowerIndicationAidl"

#include "GnssPowerIndication.h"
#include <android/binder_auto_utils.h>
#include <log_util.h>
#include <inttypes.h>
#include "loc_misc_utils.h"

typedef const GnssInterface* (getLocationInterface)();

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

static GnssPowerIndication* spGnssPowerIndication = nullptr;

GnssPowerIndication::GnssPowerIndication() :
    mDeathRecipient(AIBinder_DeathRecipient_new(GnssPowerIndication::gnssPowerIndicationDied)) {
    spGnssPowerIndication = this;
}

GnssPowerIndication::~GnssPowerIndication() {
    spGnssPowerIndication = nullptr;
}

ScopedAStatus GnssPowerIndication::setCallback(
        const shared_ptr<IGnssPowerIndicationCallback>& callback) {

    if (nullptr == callback) {
        LOC_LOGe("callback is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient, this);
    std::unique_lock<std::mutex> lock(mMutex);
    mGnssPowerIndicationCb = callback;
    lock.unlock();
    static bool getGnssInterfaceFailed = false;

    if (nullptr == mGnssInterface && !getGnssInterfaceFailed) {
        void * libHandle = nullptr;
        getLocationInterface* getter = (getLocationInterface*)
                dlGetSymFromLib(libHandle, "libgnss.so", "getGnssInterface");

        if (nullptr == getter) {
            getGnssInterfaceFailed = true;
        } else {
            mGnssInterface = (GnssInterface*)(*getter)();
        }
    }

    if (nullptr != mGnssInterface) {
        mGnssInterface->powerIndicationInit(piGnssPowerIndicationCb);
    } else {
        LOC_LOGe("mGnssInterface is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    if (callback != nullptr) {
        callback->setCapabilitiesCb(IGnssPowerIndicationCallback::CAPABILITY_TOTAL);
    }
    return ScopedAStatus::ok();
}

void GnssPowerIndication::cleanup() {
    LOC_LOGd("()");
    std::unique_lock<std::mutex> lock(mMutex);
    if (nullptr != mGnssPowerIndicationCb) {
        AIBinder_unlinkToDeath(mGnssPowerIndicationCb->asBinder().get(), mDeathRecipient, this);
        mGnssPowerIndicationCb = nullptr;
    }
}

void GnssPowerIndication::gnssPowerIndicationDied(void* cookie) {
    LOC_LOGe("IGnssPowerIndicationCallback service died");
    GnssPowerIndication* iface = static_cast<GnssPowerIndication*>(cookie);
    if (iface != nullptr) {
        iface->cleanup();
    }
}

ScopedAStatus GnssPowerIndication::requestGnssPowerStats() {
    LOC_LOGd("requestGnssPowerStats");
    std::unique_lock<std::mutex> lock(mMutex);

    if (nullptr != mGnssInterface) {
        lock.unlock();
        mGnssInterface->powerIndicationRequest();
    } else {
        LOC_LOGe("mGnssInterface is nullptr");
    }

    return ScopedAStatus::ok();
}

void GnssPowerIndication::piGnssPowerIndicationCb(GnssPowerStatistics gnssPowerStatistics) {
    if (nullptr != spGnssPowerIndication) {
        spGnssPowerIndication->gnssPowerIndicationCb(gnssPowerStatistics);
    } else {
        LOC_LOGe("spGnssPowerIndication is nullptr");
    }
}

void GnssPowerIndication::gnssPowerIndicationCb(GnssPowerStatistics gnssPowerStatistics) {

    ElapsedRealtime elapsedRealtime = {
            .flags = ElapsedRealtime::HAS_TIMESTAMP_NS | ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS,
            .timestampNs = (int64_t)gnssPowerStatistics.elapsedRealTime,
            .timeUncertaintyNs = (double)gnssPowerStatistics.elapsedRealTimeUnc,
    };
    GnssPowerStats gnssPowerStats = {
            .elapsedRealtime = elapsedRealtime,
            .totalEnergyMilliJoule = gnssPowerStatistics.totalEnergyMilliJoule,
            .singlebandTrackingModeEnergyMilliJoule = 0.0,
            .multibandTrackingModeEnergyMilliJoule = 0.0,
            .singlebandAcquisitionModeEnergyMilliJoule = 0.0,
            .multibandAcquisitionModeEnergyMilliJoule = 0.0,
            .otherModesEnergyMilliJoule = {0},
    };

    LOC_LOGd("gnssPowerStats.elapsedRealtime.flags: 0x%08X"
             " gnssPowerStats.elapsedRealtime.timestampNs: %" PRId64", "
             " gnssPowerStats.elapsedRealtime.timeUncertaintyNs: %.2f,"
             " gnssPowerStatistics.totalEnergyMilliJoule = %.2f",
             gnssPowerStats.elapsedRealtime.flags,
             gnssPowerStats.elapsedRealtime.timestampNs,
             gnssPowerStats.elapsedRealtime.timeUncertaintyNs,
             gnssPowerStats.totalEnergyMilliJoule);

    std::unique_lock<std::mutex> lock(mMutex);
    auto gnssPowerIndicationCb = mGnssPowerIndicationCb;
    lock.unlock();
    if (nullptr != gnssPowerIndicationCb) {
        gnssPowerIndicationCb->gnssPowerStatsCb(gnssPowerStats);
    }
}

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
