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

#define LOG_NDEBUG 0
#define LOG_TAG "LocSvc_BatchingAPIClient"

#include <inttypes.h>
#include <log_util.h>
#include <loc_cfg.h>
#include <thread>
#include "LocationUtil.h"
#include "BatchingAPIClient.h"

#include "limits.h"


namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

using ::aidl::android::hardware::gnss::IGnssBatching;
using ::aidl::android::hardware::gnss::IGnssBatchingCallback;

static void convertBatchOption(const IGnssBatching::Options& in, LocationOptions& out,
        LocationCapabilitiesMask mask);

BatchingAPIClient::BatchingAPIClient(const shared_ptr<IGnssBatchingCallback>& callback) :
    LocationAPIClientBase(),
    mGnssBatchingCbIface(nullptr),
    mDefaultId(UINT_MAX),
    mLocationCapabilitiesMask(0) {
    LOC_LOGd("]: (%p)", &callback);
    gnssUpdateCallbacks(callback);
}

BatchingAPIClient::~BatchingAPIClient() {
    LOC_LOGd("]: ()");
}

int BatchingAPIClient::getBatchSize() {
    int batchSize = locAPIGetBatchSize();
    LOC_LOGd("batchSize: %d", batchSize);
    return batchSize;
}

void BatchingAPIClient::setCallbacks() {
    LocationCallbacks locationCallbacks;
    memset(&locationCallbacks, 0, sizeof(LocationCallbacks));
    locationCallbacks.size = sizeof(LocationCallbacks);

    locationCallbacks.trackingCb = nullptr;
    locationCallbacks.batchingCb = nullptr;
    locationCallbacks.batchingCb = [this](size_t count, Location* location,
        const BatchingOptions& batchOptions) {
        onBatchingCb(count, location, batchOptions);
    };
    locationCallbacks.geofenceBreachCb = nullptr;
    locationCallbacks.geofenceStatusCb = nullptr;
    locationCallbacks.gnssLocationInfoCb = nullptr;
    locationCallbacks.gnssNiCb = nullptr;
    locationCallbacks.gnssSvCb = nullptr;
    locationCallbacks.gnssNmeaCb = nullptr;
    locationCallbacks.gnssMeasurementsCb = nullptr;

    locAPISetCallbacks(locationCallbacks);
}

void BatchingAPIClient::gnssUpdateCallbacks(const shared_ptr<IGnssBatchingCallback>& callback) {
    mMutex.lock();
    bool cbWasNull = (mGnssBatchingCbIface == nullptr);
    mGnssBatchingCbIface = callback;
    mMutex.unlock();

    if (cbWasNull) {
        setCallbacks();
    }
}

int BatchingAPIClient::startSession(const IGnssBatching::Options& opts) {
    mMutex.lock();
    mState = STARTED;
    mMutex.unlock();
    LOC_LOGd("]: (%lld %d)",
            static_cast<long long>(opts.periodNanos), static_cast<uint8_t>(opts.flags));
    int retVal = -1;
    LocationOptions options;
    convertBatchOption(opts, options, mLocationCapabilitiesMask);
    uint32_t mode = 0;
    if (opts.flags == static_cast<uint8_t>(IGnssBatching::WAKEUP_ON_FIFO_FULL)) {
        mode = SESSION_MODE_ON_FULL;
    }
    if (locAPIStartSession(mDefaultId, mode, options) == LOCATION_ERROR_SUCCESS) {
        retVal = 1;
    }
    return retVal;
}

int BatchingAPIClient::updateSessionOptions(const IGnssBatching::Options& opts) {
    LOC_LOGd("]: (%lld %d)",
            static_cast<long long>(opts.periodNanos), static_cast<uint8_t>(opts.flags));
    int retVal = -1;
    LocationOptions options;
    convertBatchOption(opts, options, mLocationCapabilitiesMask);

    uint32_t mode = 0;
    if (opts.flags == static_cast<uint8_t>(IGnssBatching::WAKEUP_ON_FIFO_FULL)) {
        mode = SESSION_MODE_ON_FULL;
    }
    if (locAPIUpdateSessionOptions(mDefaultId, mode, options) == LOCATION_ERROR_SUCCESS) {
        retVal = 1;
    }
    return retVal;
}

int BatchingAPIClient::stopSession() {
    mMutex.lock();
    mState = STOPPING;
    mMutex.unlock();
    LOC_LOGd("]: ");
    int retVal = -1;
    locAPIGetBatchedLocations(mDefaultId, SIZE_MAX);
    if (locAPIStopSession(mDefaultId) == LOCATION_ERROR_SUCCESS) {
        retVal = 1;
    }
    return retVal;
}

void BatchingAPIClient::getBatchedLocation(int last_n_locations) {
    LOC_LOGd("]: (%d)", last_n_locations);
    locAPIGetBatchedLocations(mDefaultId, last_n_locations);
}

void BatchingAPIClient::flushBatchedLocations() {
    LOC_LOGd("]: ()");
    uint32_t retVal = locAPIGetBatchedLocations(mDefaultId, SIZE_MAX);
    // when flush a stopped session or one doesn't exist, just report an empty batch.
    if (LOCATION_ERROR_ID_UNKNOWN == retVal) {
        BatchingOptions opt = {};
        ::std::thread thd(&BatchingAPIClient::onBatchingCb, this, 0, nullptr, opt);
        thd.detach();
    }
}

void BatchingAPIClient::onCapabilitiesCb(LocationCapabilitiesMask capabilitiesMask) {
    LOC_LOGd("]: (%" PRIu64 ")", capabilitiesMask);
    mLocationCapabilitiesMask = capabilitiesMask;
}

void BatchingAPIClient::onBatchingCb(size_t count, Location* location,
        const BatchingOptions& /*batchOptions*/) {
    bool processReport = false;
    LOC_LOGd("(count: %zu)", count);
    mMutex.lock();
    // back to back stop() and flush() could bring twice onBatchingCb(). Each one might come first.
    // Combine them both (the first goes to cache, the second in location*) before report to FW
    switch (mState) {
        case STOPPING:
            mState = STOPPED;
            for (size_t i = 0; i < count; i++) {
                mBatchedLocationInCache.push_back(location[i]);
            }
            break;
        case STARTED:
        case STOPPED: // flush() always trigger report, even on a stopped session
            processReport = true;
            break;
        default:
            break;
    }
    // report location batch when in STARTED state or flush(), combined with cache in last stop()
    if (processReport) {
        auto gnssBatchingCbIface(mGnssBatchingCbIface);
        size_t batchCacheCnt = mBatchedLocationInCache.size();
        LOC_LOGd("(batchCacheCnt: %zu)", batchCacheCnt);
        if (gnssBatchingCbIface != nullptr) {
            std::vector<GnssLocation> locationVec;
            if (count+batchCacheCnt > 0) {
                locationVec.resize(count+batchCacheCnt);
                for (size_t i = 0; i < batchCacheCnt; ++i) {
                    convertGnssLocation(mBatchedLocationInCache[i], locationVec[i]);
                }
                for (size_t i = 0; i < count; i++) {
                    convertGnssLocation(location[i], locationVec[i+batchCacheCnt]);
                }
            }
            mMutex.unlock();
            auto r = gnssBatchingCbIface->gnssLocationBatchCb(locationVec);
            if (!r.isOk()) {
                LOC_LOGe("] Error from gnssLocationBatchCb");
            }
        } else {
            mMutex.unlock();
        }
        mMutex.lock();
        mBatchedLocationInCache.clear();
        mMutex.unlock();
    } else {
        mMutex.unlock();
    }
}

static void convertBatchOption(const IGnssBatching::Options& in, LocationOptions& out,
        LocationCapabilitiesMask mask) {
    memset(&out, 0, sizeof(LocationOptions));
    out.size = sizeof(LocationOptions);
    out.minInterval = (uint32_t)(in.periodNanos / 1000000L);
    out.minDistance = 0;
    out.mode = GNSS_SUPL_MODE_STANDALONE;
    if (mask & LOCATION_CAPABILITIES_GNSS_MSA_BIT)
        out.mode = GNSS_SUPL_MODE_MSA;
    if (mask & LOCATION_CAPABILITIES_GNSS_MSB_BIT)
        out.mode = GNSS_SUPL_MODE_MSB;
}

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
