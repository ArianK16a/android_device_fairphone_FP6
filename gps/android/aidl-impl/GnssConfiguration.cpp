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


#define LOG_TAG "GnssConfigurationAidl"

#include "Gnss.h"
#include "GnssConfiguration.h"
#include <log_util.h>

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

GnssConfiguration::GnssConfiguration(Gnss* gnss) : mGnss(gnss) {
}

ScopedAStatus GnssConfiguration::setSuplVersion(int version) {
    if (mGnss == nullptr) {
        LOC_LOGe("]: mGnss is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    GnssConfig config;
    memset(&config, 0, sizeof(GnssConfig));
    config.size = sizeof(GnssConfig);
    config.flags = GNSS_CONFIG_FLAGS_SUPL_VERSION_VALID_BIT;
    switch (version) {
        case 0x00020004:
            config.suplVersion = GNSS_CONFIG_SUPL_VERSION_2_0_4;
            break;
        case 0x00020002:
            config.suplVersion = GNSS_CONFIG_SUPL_VERSION_2_0_2;
            break;
        case 0x00020000:
            config.suplVersion = GNSS_CONFIG_SUPL_VERSION_2_0_0;
            break;
        case 0x00010000:
            config.suplVersion = GNSS_CONFIG_SUPL_VERSION_1_0_0;
            break;
        default:
            LOC_LOGe("]: invalid version: 0x%x.", version);
            return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    return mGnss->updateConfiguration(config);
}

ScopedAStatus GnssConfiguration::setSuplMode(int mode) {
    if (mGnss == nullptr) {
        LOC_LOGe("]: mGnss is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    GnssConfig config;
    memset(&config, 0, sizeof(GnssConfig));
    config.size = sizeof(GnssConfig);
    config.flags = GNSS_CONFIG_FLAGS_SUPL_MODE_BIT;
    switch (mode) {
        case 0:
            config.suplModeMask = 0; // STANDALONE ONLY
            break;
        case 1:
            config.suplModeMask = GNSS_CONFIG_SUPL_MODE_MSB_BIT;
            break;
        case 2:
            config.suplModeMask = GNSS_CONFIG_SUPL_MODE_MSA_BIT;
            break;
        case 3:
            config.suplModeMask = GNSS_CONFIG_SUPL_MODE_MSB_BIT | GNSS_CONFIG_SUPL_MODE_MSA_BIT;
            break;
        default:
            LOC_LOGe("]: invalid mode: %d.", mode);
            return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    return mGnss->updateConfiguration(config);
}

ScopedAStatus GnssConfiguration::setLppProfile(int lppProfileMask) {
    if (mGnss == nullptr) {
        LOC_LOGe("]: mGnss is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    GnssConfig config = {};
    config.size = sizeof(GnssConfig);
    config.flags = GNSS_CONFIG_FLAGS_LPP_PROFILE_VALID_BIT;
    config.lppProfileMask = GNSS_CONFIG_LPP_PROFILE_RRLP_ON_LTE; //default

    if (lppProfileMask & (1<<0)) {
        config.lppProfileMask |= GNSS_CONFIG_LPP_PROFILE_USER_PLANE_BIT;
    }
    if (lppProfileMask & (1<<1)) {
        config.lppProfileMask |= GNSS_CONFIG_LPP_PROFILE_CONTROL_PLANE_BIT;
    }
    if (lppProfileMask & (1<<2)) {
        config.lppProfileMask |= GNSS_CONFIG_LPP_PROFILE_USER_PLANE_OVER_NR5G_SA_BIT;
    }
    if (lppProfileMask & (1<<3)) {
        config.lppProfileMask |= GNSS_CONFIG_LPP_PROFILE_CONTROL_PLANE_OVER_NR5G_SA_BIT;
    }

    return mGnss->updateConfiguration(config);
}

ScopedAStatus GnssConfiguration::setGlonassPositioningProtocol(int protocol) {
    if (mGnss == nullptr) {
        LOC_LOGe("]: mGnss is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    GnssConfig config;
    memset(&config, 0, sizeof(GnssConfig));
    config.size = sizeof(GnssConfig);

    config.flags = GNSS_CONFIG_FLAGS_AGLONASS_POSITION_PROTOCOL_VALID_BIT;
    if (protocol & (1<<0)) {
        config.aGlonassPositionProtocolMask |= GNSS_CONFIG_RRC_CONTROL_PLANE_BIT;
    }
    if (protocol & (1<<1)) {
        config.aGlonassPositionProtocolMask |= GNSS_CONFIG_RRLP_USER_PLANE_BIT;
    }
    if (protocol & (1<<2)) {
        config.aGlonassPositionProtocolMask |= GNSS_CONFIG_LLP_USER_PLANE_BIT;
    }
    if (protocol & (1<<3)) {
        config.aGlonassPositionProtocolMask |= GNSS_CONFIG_LLP_CONTROL_PLANE_BIT;
    }

    return mGnss->updateConfiguration(config);
}

ScopedAStatus GnssConfiguration::setEmergencySuplPdn(bool enabled) {
    if (mGnss == nullptr) {
        LOC_LOGe("]: mGnss is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    GnssConfig config;
    memset(&config, 0, sizeof(GnssConfig));
    config.size = sizeof(GnssConfig);
    config.flags = GNSS_CONFIG_FLAGS_EM_PDN_FOR_EM_SUPL_VALID_BIT;
    config.emergencyPdnForEmergencySupl = (enabled ?
            GNSS_CONFIG_EMERGENCY_PDN_FOR_EMERGENCY_SUPL_YES :
            GNSS_CONFIG_EMERGENCY_PDN_FOR_EMERGENCY_SUPL_NO);

    return mGnss->updateConfiguration(config);
}

ScopedAStatus GnssConfiguration::setEsExtensionSec(int emergencyExtensionSeconds) {
    ENTRY_LOG_CALLFLOW();
    if (mGnss == nullptr) {
        LOC_LOGe("mGnss is nullptr");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    GnssConfig config;
    memset(&config, 0, sizeof(GnssConfig));
    config.size = sizeof(GnssConfig);
    config.flags = GNSS_CONFIG_FLAGS_EMERGENCY_EXTENSION_SECONDS_BIT;
    config.emergencyExtensionSeconds = emergencyExtensionSeconds;

    return mGnss->updateConfiguration(config);
}

ScopedAStatus GnssConfiguration::setBlocklist(const vector<BlocklistedSource>& sourceList) {
    ENTRY_LOG_CALLFLOW();
    if (nullptr == mGnss) {
        LOC_LOGe("mGnss is null");
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }

    // blValid is true if sourceList is empty, i.e. clearing the BL;
    // if sourceList is not empty, blValid is initialied to false, and later
    // updated in the for loop to become true only if there is at least
    // one {constellation, svid} in the list that is valid.
    bool blValid = (0 == sourceList.size());
    GnssConfig config;
    memset(&config, 0, sizeof(GnssConfig));
    config.size = sizeof(GnssConfig);
    config.flags = GNSS_CONFIG_FLAGS_BLACKLISTED_SV_IDS_BIT;
    config.blacklistedSvIds.clear();

    GnssSvIdSource source = {};
    for (int idx = 0; idx < (int)sourceList.size(); idx++) {
        // Set blValid true if any one source is valid
        blValid = setBlocklistedSource(source, sourceList[idx]) || blValid;
        config.blacklistedSvIds.push_back(source);
    }

    // Update configuration only if blValid is true
    // i.e. only if atleast one source is valid for sourceListing
    if (!blValid) {
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    return mGnss->updateConfiguration(config);
}

bool GnssConfiguration::setBlocklistedSource(GnssSvIdSource& copyToSource,
        const BlocklistedSource& copyFromSource) {

    bool retVal = true;
    uint16_t svIdOffset = 0;
    copyToSource.size = sizeof(GnssSvIdSource);
    copyToSource.svId = copyFromSource.svid;

    switch (copyFromSource.constellation) {
    case GnssConstellationType::GPS:
        copyToSource.constellation = GNSS_SV_TYPE_GPS;
        svIdOffset = GNSS_SV_CONFIG_GPS_INITIAL_SV_ID - 1;
        break;
    case GnssConstellationType::SBAS:
        copyToSource.constellation = GNSS_SV_TYPE_SBAS;
        LOC_LOGe("SBAS SVs can't be blocklisted.");
        retVal = false;
        break;
    case GnssConstellationType::GLONASS:
        copyToSource.constellation = GNSS_SV_TYPE_GLONASS;
        svIdOffset = GNSS_SV_CONFIG_GLO_INITIAL_SV_ID - 1;
        break;
    case GnssConstellationType::QZSS:
        copyToSource.constellation = GNSS_SV_TYPE_QZSS;
        svIdOffset = GNSS_SV_CONFIG_QZSS_INITIAL_SV_ID - 1;
        break;
    case GnssConstellationType::BEIDOU:
        copyToSource.constellation = GNSS_SV_TYPE_BEIDOU;
        svIdOffset = GNSS_SV_CONFIG_BDS_INITIAL_SV_ID - 1;
        break;
    case GnssConstellationType::GALILEO:
        copyToSource.constellation = GNSS_SV_TYPE_GALILEO;
        svIdOffset = GNSS_SV_CONFIG_GAL_INITIAL_SV_ID - 1;
        break;
    case GnssConstellationType::IRNSS:
        copyToSource.constellation = GNSS_SV_TYPE_NAVIC;
        svIdOffset = GNSS_SV_CONFIG_NAVIC_INITIAL_SV_ID - 1;
        break;
    default:
        copyToSource.constellation = GNSS_SV_TYPE_UNKNOWN;
        LOC_LOGe("Invalid constellation %d", copyFromSource.constellation);
        retVal = false;
        break;
    }

    if (copyToSource.svId > 0 && svIdOffset > 0) {
        copyToSource.svId += svIdOffset;
    }

    return retVal;
}

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
