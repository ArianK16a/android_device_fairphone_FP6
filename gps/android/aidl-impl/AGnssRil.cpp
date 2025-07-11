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
#include <aidl/android/hardware/gnss/IAGnssRil.h>
#include <aidl/android/hardware/gnss/IAGnssRilCallback.h>
#include <aidl/android/hardware/gnss/BnAGnssRil.h>
#include "Gnss.h"
#include "AGnssRil.h"
#include <DataItemConcreteTypes.h>
#include <log_util.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {
AGnssRil::AGnssRil(Gnss* gnss) : mGnss(gnss) {
    ENTRY_LOG_CALLFLOW();
}

AGnssRil::~AGnssRil() {
    ENTRY_LOG_CALLFLOW();
}

ScopedAStatus AGnssRil::updateNetworkState(const IAGnssRil::NetworkAttributes& attributes) {
    ENTRY_LOG_CALLFLOW();
    std::string apn = attributes.apn;
    if (nullptr != mGnss && (nullptr != mGnss->getLocationControlApi())) {
        int8_t typeout = loc_core::TYPE_UNKNOWN;
        bool roaming = false;
        if (attributes.capabilities & IAGnssRil::NETWORK_CAPABILITY_NOT_METERED) {
            typeout = loc_core::TYPE_WIFI;
        } else {
            typeout = loc_core::TYPE_MOBILE;
        }
        if (attributes.capabilities & IAGnssRil::NETWORK_CAPABILITY_NOT_ROAMING) {
            roaming = false;
        }
        LOC_LOGd("apn string received is: %s", apn.c_str());
        mGnss->getLocationControlApi()->updateConnectionStatus(attributes.isConnected,
                typeout, roaming, (NetworkHandle) attributes.networkHandle, apn);
    }
    return ScopedAStatus::ok();
}
}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
