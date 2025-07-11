/* Copyright (c) 2015-2017, 2020 The Linux Foundation. All rights reserved.
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
#ifndef __SYSTEM_STATUS_OSOBSERVER__
#define __SYSTEM_STATUS_OSOBSERVER__

#include <cinttypes>
#include <string>
#include <map>
#include <new>
#include <vector>

#include <MsgTask.h>
#include <DataItemId.h>
#include <IOsObserver.h>
#include <loc_pla.h>
#include <log_util.h>
#include <gps_extended.h>
#include <LocUnorderedSetMap.h>

namespace loc_core
{
/******************************************************************************
 SystemStatusOsObserver
******************************************************************************/
using namespace std;
using namespace loc_util;

// Forward Declarations
class IDataItemCore;
class SystemStatus;
class SystemStatusOsObserver;
typedef LocUnorderedSetMap<IDataItemObserver*, DataItemId> ClientToDataItems;
typedef LocUnorderedSetMap<DataItemId, IDataItemObserver*> DataItemToClients;
typedef unordered_map<DataItemId, IDataItemCore*> DataItemIdToCore;
typedef unordered_map<DataItemId, int> DataItemIdToInt;
#ifdef USE_GLIB
// Cache details of backhaul client requests
typedef std::map<string, BackhaulContext> ClientBackhaulReqCache;
#endif

struct ObserverContext {
    IDataItemSubscription* mSubscriptionObj;
    IFrameworkActionReq* mFrameworkActionReqObj;
    const MsgTask* mMsgTask;
    SystemStatusOsObserver* mSSObserver;

    inline ObserverContext(const MsgTask* msgTask, SystemStatusOsObserver* observer) :
            mSubscriptionObj(NULL), mFrameworkActionReqObj(NULL),
            mMsgTask(msgTask), mSSObserver(observer) {}
};

// Clients wanting to get data from OS/Framework would need to
// subscribe with OSObserver using IDataItemSubscription interface.
// Such clients would need to implement IDataItemObserver interface
// to receive data when it becomes available.
class SystemStatusOsObserver : public IOsObserver {

public:
    // ctor
    inline SystemStatusOsObserver(SystemStatus* systemstatus, const MsgTask* msgTask) :
            mSystemStatus(systemstatus), mContext(msgTask, this),
            mAddress("SystemStatusOsObserver"),
            mClientToDataItems(MAX_DATA_ITEM_ID), mDataItemToClients(MAX_DATA_ITEM_ID) {}

    // dtor
    ~SystemStatusOsObserver();

    template <typename CINT, typename COUT>
    static COUT containerTransfer(CINT& s);
    template <typename CINT, typename COUT>
    inline static COUT containerTransfer(CINT&& s) {
        return containerTransfer<CINT, COUT>(s);
    }

    // To set the subscription object
    virtual void setSubscriptionObj(IDataItemSubscription* subscriptionObj);

    // To set the framework action request object
    inline void setFrameworkActionReqObj(IFrameworkActionReq* frameworkActionReqObj) {
        mContext.mFrameworkActionReqObj = frameworkActionReqObj;
#ifdef USE_GLIB
        uint32_t numBackHaulClients = mBackHaulConnReqCache.size();
        if (numBackHaulClients > 0) {
            // For each client, invoke connectbackhaul.
            for (auto clientContext : mBackHaulConnReqCache) {
                LOC_LOGd("Invoke connectBackhaul for client: %s Sub: %d Apn: %s IpType: %d",
                         clientContext.second.clientName.c_str(), clientContext.second.prefSub,
                         clientContext.second.prefApn.c_str(), clientContext.second.prefIpType);
                BackhaulContext ctx = { clientContext.second.clientName,
                                        clientContext.second.prefSub,
                                        clientContext.second.prefApn,
                                        clientContext.second.prefIpType };
                connectBackhaul(ctx);
            }
            // Clear the set
            mBackHaulConnReqCache.clear();
        }
#endif
    }

    // IDataItemSubscription Overrides
    inline virtual void subscribe(const unordered_set<DataItemId>& l, IDataItemObserver* client) override {
        subscribe(l, client, false);
    }
    virtual void updateSubscription(const unordered_set<DataItemId>& l, IDataItemObserver* client) override;
    inline virtual void requestData(const unordered_set<DataItemId>& l, IDataItemObserver* client) override {
        subscribe(l, client, true);
    }
    virtual void unsubscribe(const unordered_set<DataItemId>& l, IDataItemObserver* client) override;
    virtual void unsubscribeAll(IDataItemObserver* client) override;

    // IDataItemObserver Overrides
    virtual void notify(const unordered_set<IDataItemCore*>& dlist) override;
    inline virtual void getName(string& name) override {
        name = mAddress;
    }

#ifdef USE_GLIB
    virtual bool connectBackhaul(const BackhaulContext& ctx) override;
    virtual bool disconnectBackhaul(const BackhaulContext& ctx) override;
#endif

private:
    SystemStatus*                                    mSystemStatus;
    ObserverContext                                  mContext;
    const string                                     mAddress;
    ClientToDataItems                                mClientToDataItems;
    DataItemToClients                                mDataItemToClients;
    DataItemIdToCore                                 mDataItemCache;

#ifdef USE_GLIB
    // Cache the framework action request for connect/disconnect
    ClientBackhaulReqCache  mBackHaulConnReqCache;
#endif

    void subscribe(const unordered_set<DataItemId>& l, IDataItemObserver* client, bool toRequestData);

    // Helpers
    void sendCachedDataItems(const unordered_set<DataItemId>& s, IDataItemObserver* to);
    bool updateCache(IDataItemCore* d);
    inline void logMe(const unordered_set<DataItemId>& l) {
        IF_LOC_LOGD {
            for (auto id : l) {
                LOC_LOGD("DataItem %d", id);
            }
        }
    }
};

} // namespace loc_core

#endif //__SYSTEM_STATUS__

