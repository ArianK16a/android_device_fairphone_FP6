/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Not a Contribution
 */
/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "LocSvc_GnssBatchingInterface"

#include <log_util.h>
#include <BatchingAPIClient.h>
#include "GnssBatching.h"

namespace android {
namespace hardware {
namespace gnss {
namespace V1_1 {
namespace implementation {

void GnssBatching::GnssBatchingDeathRecipient::serviceDied(
        uint64_t cookie, const wp<IBase>& who) {
    LOC_LOGE("%s] service died. cookie: %llu, who: %p",
            __FUNCTION__, static_cast<unsigned long long>(cookie), &who);
    auto gnssBatching = mGnssBatching.promote();
    if (gnssBatching != nullptr) {
        gnssBatching->handleClientDeath();
    }
}

GnssBatching::~GnssBatching() {
    if (mApi != nullptr) {
        mApi->destroy();
        mApi = nullptr;
    }
}

void GnssBatching::handleClientDeath() {
    stop();
    cleanup();
    if (mApi != nullptr) {
        mApi->gnssUpdateCallbacks(nullptr);
    }
    mGnssBatchingCbIface = nullptr;
}

// Methods from ::android::hardware::gnss::V1_0::IGnssBatching follow.
Return<bool> GnssBatching::init(const sp<IGnssBatchingCallback>& callback) {
    if (mGnssBatchingDeathRecipient == nullptr) {
        mGnssBatchingDeathRecipient = new GnssBatchingDeathRecipient(mSelf);
    }

    if (mApi != nullptr) {
        mApi->gnssUpdateCallbacks(callback);
    } else {
        mApi = new BatchingAPIClient(callback);
    }

    if (mGnssBatchingCbIface != nullptr) {
        mGnssBatchingCbIface->unlinkToDeath(mGnssBatchingDeathRecipient);
    }
    mGnssBatchingCbIface = callback;
    if (mGnssBatchingCbIface != nullptr) {
        mGnssBatchingCbIface->linkToDeath(mGnssBatchingDeathRecipient, 0 /*cookie*/);
    }

    return true;
}

Return<uint16_t> GnssBatching::getBatchSize() {
    uint16_t ret = 0;
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        ret = mApi->getBatchSize();
    }
    return ret;
}

Return<bool> GnssBatching::start(const IGnssBatching::Options& options) {
    bool ret = false;
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        ret = mApi->startSession(options);
    }
    return ret;
}

Return<void> GnssBatching::flush() {
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        mApi->flushBatchedLocations();
    }
    return Void();
}

Return<bool> GnssBatching::stop() {
    bool ret = false;
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        ret = mApi->stopSession();
    }
    return ret;
}

Return<void> GnssBatching::cleanup() {
    if (mGnssBatchingCbIface != nullptr) {
        mGnssBatchingCbIface->unlinkToDeath(mGnssBatchingDeathRecipient);
    }
    return Void();
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace gnss
}  // namespace hardware
}  // namespace android
