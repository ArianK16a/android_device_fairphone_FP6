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
#ifndef BATCHING_API_CLINET_H
#define BATCHING_API_CLINET_H

#include <mutex>
#include <pthread.h>

#include <LocationAPIClientBase.h>
#include <aidl/android/hardware/gnss/BnGnssBatching.h>
#include <aidl/android/hardware/gnss/IGnssBatchingCallback.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

using ::std::shared_ptr;
using ::aidl::android::hardware::gnss::IGnssBatching;
using ::aidl::android::hardware::gnss::IGnssBatchingCallback;

enum BATCHING_STATE { STARTED, STOPPING, STOPPED };

class BatchingAPIClient : public LocationAPIClientBase
{
public:
    BatchingAPIClient(const shared_ptr<IGnssBatchingCallback>& callback);
    void gnssUpdateCallbacks(const shared_ptr<IGnssBatchingCallback>& callback);
    int getBatchSize();
    int startSession(const IGnssBatching::Options& options);
    int updateSessionOptions(const IGnssBatching::Options& options);
    int stopSession();
    void getBatchedLocation(int last_n_locations);
    void flushBatchedLocations();

    inline LocationCapabilitiesMask getCapabilities() { return mLocationCapabilitiesMask; }

    // callbacks
    void onCapabilitiesCb(LocationCapabilitiesMask capabilitiesMask) final;
    void onBatchingCb(size_t count, Location* location, const BatchingOptions& batchOptions) final;

private:
    ~BatchingAPIClient();

    void setCallbacks();
    std::mutex mMutex;
    shared_ptr<IGnssBatchingCallback> mGnssBatchingCbIface;
    uint32_t mDefaultId;
    LocationCapabilitiesMask mLocationCapabilitiesMask;
    volatile BATCHING_STATE mState = STOPPED;

    std::vector<Location> mBatchedLocationInCache;
};

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif // BATCHING_API_CLINET_H
