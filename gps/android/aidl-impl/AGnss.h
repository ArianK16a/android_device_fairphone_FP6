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
#ifndef ANDROID_HARDWARE_GNSS_AIDL_AGNSS_H
#define ANDROID_HARDWARE_GNSS_AIDL_AGNSS_H
#include <aidl/android/hardware/gnss/BnAGnss.h>
#include <aidl/android/hardware/gnss/IAGnssCallback.h>
#include <gps_extended_c.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
using ::aidl::android::hardware::gnss::BnAGnss;
using ::aidl::android::hardware::gnss::IAGnssCallback;
using ::std::shared_ptr;
using ::ndk::ScopedAStatus;
class AGnss : public BnAGnss {
public:
    AGnss(Gnss* gnss);
    virtual ~AGnss();

    ScopedAStatus setCallback(const shared_ptr<IAGnssCallback>& callback) override;
    ScopedAStatus dataConnClosed() override;
    ScopedAStatus dataConnFailed() override;
    ScopedAStatus setServer(IAGnssCallback::AGnssType type, const std::string& hostname,
            int32_t port) override;
    ScopedAStatus dataConnOpen(int64_t networkHandle, const std::string& apn,
            ApnIpType apnIpType) override;
    /* Data call setup callback passed down to GNSS HAL implementation */
    void statusCb(AGpsExtType type, LocAGpsStatusValue status);
private:
    Gnss* mGnss = nullptr;
    std::mutex mMutex;
    shared_ptr<IAGnssCallback> mAGnssCbIface = nullptr;
    AIBinder_DeathRecipient *mDeathRecipient = nullptr;

    AGpsExtType mType;
};
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif //ANDROID_HARDWARE_GNSS_AIDL_AGNSS_H
