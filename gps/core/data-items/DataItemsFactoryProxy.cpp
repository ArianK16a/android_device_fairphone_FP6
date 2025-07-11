/* Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
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

Copyright (c) 2022, 2023 Qualcomm Innovation Center, Inc. All rights reserved.

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

#define LOG_TAG "DataItemsFactoryProxy"

#include <dlfcn.h>
#include <DataItemId.h>
#include <IDataItemCore.h>
#include <DataItemsFactoryProxy.h>
#include <DataItemConcreteTypes.h>
#include <loc_pla.h>
#include <log_util.h>
#include "loc_misc_utils.h"

namespace loc_core
{

IDataItemCore* DataItemsFactoryProxy::createNewDataItem(IDataItemCore* dataItem) {
    IDataItemCore *mydi = nullptr;

    switch (dataItem->getId()) {
    case AIRPLANEMODE_DATA_ITEM_ID:
        mydi = new AirplaneModeDataItem(*((AirplaneModeDataItem*)dataItem));
        break;
    case ENH_DATA_ITEM_ID:
        mydi = new ENHDataItem(*((ENHDataItem*)dataItem));
        break;
    case GPSSTATE_DATA_ITEM_ID:
        mydi = new GPSStateDataItem(*((GPSStateDataItem*)dataItem));
        break;
    case NLPSTATUS_DATA_ITEM_ID:
        mydi = new NLPStatusDataItem(*((NLPStatusDataItem*)dataItem));
        break;
    case WIFIHARDWARESTATE_DATA_ITEM_ID:
        mydi = new WifiHardwareStateDataItem(*((WifiHardwareStateDataItem*)dataItem));
        break;
    case NETWORKINFO_DATA_ITEM_ID:
        mydi = new NetworkInfoDataItem(*((NetworkInfoDataItem*)dataItem));
        break;
    case SERVICESTATUS_DATA_ITEM_ID:
       mydi = new ServiceStatusDataItem(*((ServiceStatusDataItem*)dataItem));
        break;
    case RILCELLINFO_DATA_ITEM_ID:
        mydi = new RilCellInfoDataItem(*((RilCellInfoDataItem*)dataItem));
        break;
    case RILSERVICEINFO_DATA_ITEM_ID:
        mydi = new RilServiceInfoDataItem(*((RilServiceInfoDataItem*)dataItem));
        break;
    case MODEL_DATA_ITEM_ID:
        mydi = new ModelDataItem(*((ModelDataItem*)dataItem));
        break;
    case MANUFACTURER_DATA_ITEM_ID:
        mydi = new ManufacturerDataItem(*((ManufacturerDataItem*)dataItem));
        break;
    case IN_EMERGENCY_CALL_DATA_ITEM_ID:
        mydi = new InEmergencyCallDataItem(*((InEmergencyCallDataItem*)dataItem));
        break;
    case ASSISTED_GPS_DATA_ITEM_ID:
        mydi = new AssistedGpsDataItem(*((AssistedGpsDataItem*)dataItem));
        break;
    case SCREEN_STATE_DATA_ITEM_ID:
        mydi = new ScreenStateDataItem(*((ScreenStateDataItem*)dataItem));
        break;
    case POWER_CONNECTED_STATE_DATA_ITEM_ID:
        mydi = new PowerConnectStateDataItem(*((PowerConnectStateDataItem*)dataItem));
        break;
    case TIMEZONE_CHANGE_DATA_ITEM_ID:
        mydi = new TimeZoneChangeDataItem(*((TimeZoneChangeDataItem*)dataItem));
        break;
    case TIME_CHANGE_DATA_ITEM_ID:
        mydi = new TimeChangeDataItem(*((TimeChangeDataItem*)dataItem));
        break;
    case WIFI_SUPPLICANT_STATUS_DATA_ITEM_ID:
        mydi = new WifiSupplicantStatusDataItem(*((WifiSupplicantStatusDataItem*)dataItem));
        break;
    case SHUTDOWN_STATE_DATA_ITEM_ID:
        mydi = new ShutdownStateDataItem(*((ShutdownStateDataItem*)dataItem));
        break;
    case TAC_DATA_ITEM_ID:
        mydi = new TacDataItem(*((TacDataItem*)dataItem));
        break;
    case MCCMNC_DATA_ITEM_ID:
        mydi = new MccmncDataItem(*((MccmncDataItem*)dataItem));
        break;
    case BATTERY_LEVEL_DATA_ITEM_ID:
        mydi = new BatteryLevelDataItem(*((BatteryLevelDataItem*)dataItem));
        break;
    case PRECISE_LOCATION_ENABLED_DATA_ITEM_ID:
        mydi = new PreciseLocationEnabledDataItem(*((PreciseLocationEnabledDataItem*)dataItem));
        break;
    case TRACKING_STARTED_DATA_ITEM_ID:
        mydi = new TrackingStartedDataItem(*((TrackingStartedDataItem*)dataItem));
        break;
    case NTRIP_STARTED_DATA_ITEM_ID:
        mydi = new NtripStartedDataItem(*((NtripStartedDataItem*)dataItem));
        break;
    case LOC_FEATURE_STATUS_DATA_ITEM_ID:
        mydi = new LocFeatureStatusDataItem(*((LocFeatureStatusDataItem*)dataItem));
        break;
    case NETWORK_POSITIONING_STARTED_DATA_ITEM_ID:
        mydi = new NlpSessionStartedDataItem(*((NlpSessionStartedDataItem*)dataItem));
        break;
    case INVALID_DATA_ITEM_ID:
    case MAX_DATA_ITEM_ID:
    default:
        break;
    };
    return mydi;
}


} // namespace loc_core


