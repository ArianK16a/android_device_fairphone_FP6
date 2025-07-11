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

#define LOG_TAG "GnssBatchingAidl"

#include "GnssBatching.h"
#include <android/binder_auto_utils.h>
#include <log_util.h>
#include <inttypes.h>
#include "loc_misc_utils.h"


namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
void gnssBatchingDied(void* cookie) {
    LOC_LOGe("IGnssBatchingCallback service died");
    GnssBatching* iface = static_cast<GnssBatching*>(cookie);
    if (iface != nullptr) {
        iface->cleanup();
        iface = nullptr;
    }
}
GnssBatching::GnssBatching(): mBatchSize(0),
    mDeathRecipient(AIBinder_DeathRecipient_new(gnssBatchingDied)) {
}

GnssBatching::~GnssBatching() {
}


ScopedAStatus GnssBatching::init(const shared_ptr<IGnssBatchingCallback>& callback) {
    if (mApi != nullptr) {
        mApi->gnssUpdateCallbacks(callback);
    } else {
        mApi = new BatchingAPIClient(callback);
    }

    mMutex.lock();
    if (mGnssBatchingCbIface != nullptr) {
        AIBinder_unlinkToDeath(mGnssBatchingCbIface->asBinder().get(), mDeathRecipient, this);
    }

    mGnssBatchingCbIface = callback;
    if (mGnssBatchingCbIface != nullptr) {
        AIBinder_linkToDeath(mGnssBatchingCbIface->asBinder().get(), mDeathRecipient, this);
    }
    mMutex.unlock();
    return ScopedAStatus::ok();
}
ScopedAStatus GnssBatching::getBatchSize(int32_t* _aidl_return) {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr");
    } else {
        mBatchSize = mApi->getBatchSize();
    }
    *_aidl_return = mBatchSize;
    return ScopedAStatus::ok();
}
ScopedAStatus GnssBatching::start(const IGnssBatching::Options& options) {
    if (nullptr != mApi) {
        mApi->startSession(options);
    } else {
        LOC_LOGe("mGnssInterface is nullptr");
    }

    return ScopedAStatus::ok();
}
ScopedAStatus GnssBatching::flush() {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr");
    } else {
        mApi->flushBatchedLocations();
    }
    return ScopedAStatus::ok();
}
ScopedAStatus GnssBatching::stop() {
    if (mApi == nullptr) {
        LOC_LOGe("]: mApi is nullptr");
    } else {
        mApi->stopSession();
    }
    return ScopedAStatus::ok();
}
ScopedAStatus GnssBatching::cleanup() {
    if (mApi != nullptr) {
        mApi->gnssUpdateCallbacks(nullptr);
        mApi->stopSession();
    }
    mMutex.lock();
    if (mGnssBatchingCbIface != nullptr) {
        mGnssBatchingCbIface = nullptr;
    }
    mMutex.unlock();
    return ScopedAStatus::ok();
}

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
