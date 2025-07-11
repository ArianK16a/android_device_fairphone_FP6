/* Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef LOC_GLINK_BASE_H
#define LOC_GLINK_BASE_H

#include <loc_pla.h>
#include <loc_gps.h>

using namespace loc_util;
namespace loc_core {

class LocGlinkBase {
    public:
        LocGlinkBase() = default;

        virtual ~LocGlinkBase() = default;

        // gnss session related functions
        inline virtual void injectLocation(LocGpsLocation gpsLocation) {
            (void) gpsLocation;
        }
        inline virtual void getPropogatedPuncFromSlate(){}

        typedef std::function<void(LocGpsLocation location)>GnssAdapterReportPuncEventCb;

        typedef LocGlinkBase* (getLocGlinkProxyFn)(
                                                const MsgTask * msgTask,
                                                GnssAdapterReportPuncEventCb event_cb);
};
} // namespace loc_core

#endif // LOC_GLINK__BASE_H
