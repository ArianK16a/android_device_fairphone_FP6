/* Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
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
#define LOG_TAG "LocSvc_SystemStatusOsObserver"

#include <algorithm>
#include <SystemStatus.h>
#include <SystemStatusOsObserver.h>
#include <IDataItemCore.h>
#include <DataItemsFactoryProxy.h>

namespace loc_core
{
template <typename CINT, typename COUT>
COUT SystemStatusOsObserver::containerTransfer(CINT& inContainer) {
    COUT outContainer = {};
    for (auto item : inContainer) {
        outContainer.insert(outContainer.begin(), item);
    }
    return outContainer;
}

SystemStatusOsObserver::~SystemStatusOsObserver() {
    // Destroy cache
    for (auto each : mDataItemCache) {
        if (nullptr != each.second) {
            delete each.second;
        }
    }

    mDataItemCache.clear();
}

void SystemStatusOsObserver::setSubscriptionObj(IDataItemSubscription* subscriptionObj)
{
    struct SetSubsObj : public LocMsg {
        ObserverContext& mContext;
        IDataItemSubscription* mSubsObj;
        inline SetSubsObj(ObserverContext& context, IDataItemSubscription* subscriptionObj) :
                mContext(context), mSubsObj(subscriptionObj) {}
        void proc() const {
            LOC_LOGi("SetSubsObj::enter");
            mContext.mSubscriptionObj = mSubsObj;

            if (!mContext.mSSObserver->mDataItemToClients.empty()) {
                unordered_set<DataItemId> dis(mContext.mSSObserver->mDataItemToClients.getKeys());
                mContext.mSubscriptionObj->subscribe(dis, mContext.mSSObserver);
                mContext.mSubscriptionObj->requestData(dis, mContext.mSSObserver);
            }
            LOC_LOGi("SetSubsObj::exit");
        }
    };

    if (nullptr == subscriptionObj) {
        LOC_LOGw("subscriptionObj is NULL");
    } else {
        mContext.mMsgTask->sendMsg(new SetSubsObj(mContext, subscriptionObj));
    }
}

/******************************************************************************
 IDataItemSubscription Overrides
******************************************************************************/
void SystemStatusOsObserver::subscribe(const unordered_set<DataItemId>& l, IDataItemObserver* client,
                                       bool toRequestData)
{
    struct HandleSubscribeReq : public LocMsg {
        inline HandleSubscribeReq(SystemStatusOsObserver* parent,
                const unordered_set<DataItemId>& l, IDataItemObserver* client, bool requestData) :
                mParent(parent), mClient(client),
                mDataItemSet(std::move(l)),
                diItemlist(containerTransfer<const unordered_set<DataItemId>, list<DataItemId>>(l)),
                mToRequestData(requestData) {}

        void proc() const {
            unordered_set<DataItemId> dataItemsToSubscribe = {};
            mParent->mDataItemToClients.add(mDataItemSet, {mClient}, &dataItemsToSubscribe);
            mParent->mClientToDataItems.add(mClient, mDataItemSet);

            mParent->sendCachedDataItems(mDataItemSet, mClient);

            // Send subscription set to framework
            if (nullptr != mParent->mContext.mSubscriptionObj) {
                if (mToRequestData) {
                    LOC_LOGd("Request Data sent to framework for the following");
                    mParent->mContext.mSubscriptionObj->requestData(dataItemsToSubscribe, mParent);
                } else if (!dataItemsToSubscribe.empty()) {
                    LOC_LOGd("Subscribe Request sent to framework for the following");
                    mParent->logMe(dataItemsToSubscribe);
                    mParent->mContext.mSubscriptionObj->subscribe(dataItemsToSubscribe, mParent);
                }
            }
        }
        mutable SystemStatusOsObserver* mParent;
        IDataItemObserver* mClient;
        const unordered_set<DataItemId> mDataItemSet;
        const list<DataItemId> diItemlist;
        bool mToRequestData;
    };

    if (l.empty() || nullptr == client) {
        LOC_LOGw("Data item set is empty or client is nullptr");
    } else {
        mContext.mMsgTask->sendMsg(
                new HandleSubscribeReq(this, l, client, toRequestData));
    }
}

void SystemStatusOsObserver::updateSubscription(
        const unordered_set<DataItemId>& l, IDataItemObserver* client)
{
    struct HandleUpdateSubscriptionReq : public LocMsg {
        HandleUpdateSubscriptionReq(SystemStatusOsObserver* parent,
                                    const unordered_set<DataItemId>& l, IDataItemObserver* client) :
                mParent(parent), mClient(client), mDataItemSet(l) {}

        void proc() const {
            unordered_set<DataItemId> dataItemsToSubscribe = {};
            unordered_set<DataItemId> dataItemsToUnsubscribe = {};
            unordered_set<IDataItemObserver*> clients({mClient});
            // below removes clients from all entries keyed with the return of the
            // mClientToDataItems.update() call. If leaving an empty set of clients as the
            // result, the entire entry will be removed. dataItemsToUnsubscribe will be
            // populated to keep the keys of the removed entries.
            mParent->mDataItemToClients.trimOrRemove(
                    // this call updates <IDataItemObserver*, DataItemId> map; removes
                    // the DataItemId's that are not new to the clietn from mDataItemSet;
                    // and returns a set of mDataItemSet's that are no longer used by client.
                    // This unused set of mDataItemSet's is passed to trimOrRemove method of
                    // <DataItemId, IDataItemObserver*> map to remove the client from the
                    // corresponding entries, and gets a set of the entries that are
                    // removed from the <DataItemId, IDataItemObserver*> map as a result.
                    mParent->mClientToDataItems.update(mClient,
                                                       (unordered_set<DataItemId>&)mDataItemSet),
                    clients, &dataItemsToUnsubscribe, nullptr);
            // below adds mClient to <DataItemId, IDataItemObserver*> map, and populates
            // new keys added to that map, which are DataItemIds to be subscribed.
            mParent->mDataItemToClients.add(mDataItemSet, clients, &dataItemsToSubscribe);

            // Send First Response
            mParent->sendCachedDataItems(mDataItemSet, mClient);

            if (nullptr != mParent->mContext.mSubscriptionObj) {
                // Send subscription set to framework
                if (!dataItemsToSubscribe.empty()) {
                    LOC_LOGd("Subscribe Request sent to framework for the following");
                    mParent->logMe(dataItemsToSubscribe);

                    mParent->mContext.mSubscriptionObj->subscribe(dataItemsToSubscribe, mParent);
                }

                // Send unsubscribe to framework
                if (!dataItemsToUnsubscribe.empty()) {
                    LOC_LOGd("Unsubscribe Request sent to framework for the following");
                    mParent->logMe(dataItemsToUnsubscribe);

                    mParent->mContext.mSubscriptionObj->unsubscribe(dataItemsToUnsubscribe,
                            mParent);
                }
            }
        }
        SystemStatusOsObserver* mParent;
        IDataItemObserver* mClient;
        unordered_set<DataItemId> mDataItemSet;
    };

    if (l.empty() || nullptr == client) {
        LOC_LOGw("Data item set is empty or client is nullptr");
    } else {
        mContext.mMsgTask->sendMsg(
                new HandleUpdateSubscriptionReq(this, l, client));
    }
}

void SystemStatusOsObserver::unsubscribe(
        const unordered_set<DataItemId>& l, IDataItemObserver* client)
{
    struct HandleUnsubscribeReq : public LocMsg {
        HandleUnsubscribeReq(SystemStatusOsObserver* parent,
                const unordered_set<DataItemId>& l, IDataItemObserver* client) :
                mParent(parent), mClient(client),
                mDataItemSet(std::move(l)) {}

        void proc() const {
            unordered_set<DataItemId> dataItemsUnusedByClient = {};
            unordered_set<IDataItemObserver*> clientToRemove = {};
            unordered_set<DataItemId> dataItemsToUnsubscribe = {};
            mParent->mClientToDataItems.trimOrRemove({mClient}, mDataItemSet,  &clientToRemove,
                                                     &dataItemsUnusedByClient);
            mParent->mDataItemToClients.trimOrRemove(dataItemsUnusedByClient, {mClient},
                                                     &dataItemsToUnsubscribe, nullptr);

            if (nullptr != mParent->mContext.mSubscriptionObj && !dataItemsToUnsubscribe.empty()) {
                LOC_LOGd("Unsubscribe Request sent to framework for the following data items");
                mParent->logMe(dataItemsToUnsubscribe);

                // Send unsubscribe to framework
                mParent->mContext.mSubscriptionObj->unsubscribe(dataItemsToUnsubscribe, mParent);
            }
        }
        SystemStatusOsObserver* mParent;
        IDataItemObserver* mClient;
        unordered_set<DataItemId> mDataItemSet;
    };

    if (l.empty() || nullptr == client) {
        LOC_LOGw("Data item set is empty or client is nullptr");
    } else {
        mContext.mMsgTask->sendMsg(new HandleUnsubscribeReq(this, l, client));
    }
}

void SystemStatusOsObserver::unsubscribeAll(IDataItemObserver* client)
{
    struct HandleUnsubscribeAllReq : public LocMsg {
        HandleUnsubscribeAllReq(SystemStatusOsObserver* parent,
                IDataItemObserver* client) :
                mParent(parent), mClient(client) {}

        void proc() const {
            unordered_set<DataItemId> diByClient = mParent->mClientToDataItems.getValSet(mClient);

            if (!diByClient.empty()) {
                unordered_set<DataItemId> dataItemsToUnsubscribe;
                mParent->mClientToDataItems.remove(mClient);
                mParent->mDataItemToClients.trimOrRemove(diByClient, {mClient},
                                                         &dataItemsToUnsubscribe, nullptr);

                if (!dataItemsToUnsubscribe.empty() &&
                    nullptr != mParent->mContext.mSubscriptionObj) {

                    LOC_LOGd("Unsubscribe Request sent to framework for the following data items");
                    mParent->logMe(dataItemsToUnsubscribe);

                    // Send unsubscribe to framework
                    mParent->mContext.mSubscriptionObj->unsubscribe(dataItemsToUnsubscribe,
                            mParent);
                }
            }
        }
        SystemStatusOsObserver* mParent;
        IDataItemObserver* mClient;
    };

    if (nullptr == client) {
        LOC_LOGw("Data item set is empty or client is nullptr");
    } else {
        mContext.mMsgTask->sendMsg(new HandleUnsubscribeAllReq(this, client));
    }
}

/******************************************************************************
 IDataItemObserver Overrides
******************************************************************************/
void SystemStatusOsObserver::notify(const unordered_set<IDataItemCore*>& dlist)
{
    struct HandleNotify : public LocMsg {
        HandleNotify(SystemStatusOsObserver* parent, vector<IDataItemCore*>& v) :
                mParent(parent), mDiVec(std::move(v)) {}

        inline virtual ~HandleNotify() {
            for (auto item : mDiVec) {
                delete item;
            }
        }

        void proc() const {
            // Update Cache with received data items and prepare
            // list of data items to be sent.
            unordered_set<DataItemId> dataItemIdsToBeSent = {};
            for (auto item : mDiVec) {
                if (mParent->updateCache(item)) {
                    dataItemIdsToBeSent.insert(item->getId());
                }
            }

            // Send data item to all subscribed clients
            unordered_set<IDataItemObserver*> clientSet = {};
            for (auto each : dataItemIdsToBeSent) {
                auto clients = mParent->mDataItemToClients.getValSetPtr(each);
                if (nullptr != clients) {
                    clientSet.insert(clients->begin(), clients->end());
                }
            }

            for (auto client : clientSet) {
                unordered_set<DataItemId> dataItemIdsForThisClient(
                        mParent->mClientToDataItems.getValSet(client));
                for (auto itr = dataItemIdsForThisClient.begin();
                        itr != dataItemIdsForThisClient.end(); ) {
                    if (dataItemIdsToBeSent.find(*itr) == dataItemIdsToBeSent.end()) {
                        itr = dataItemIdsForThisClient.erase(itr);
                    } else {
                        itr++;
                    }
                }

                mParent->sendCachedDataItems(dataItemIdsForThisClient, client);
            }
        }
        SystemStatusOsObserver* mParent;
        const vector<IDataItemCore*> mDiVec;
    };

    if (!dlist.empty()) {
        vector<IDataItemCore*> dataItemVec;

        for (auto each : dlist) {

            IDataItemCore* di = DataItemsFactoryProxy::createNewDataItem(each);
            if (nullptr == di) {
                LOC_LOGw("Unable to create dataitem:%d", each->getId());
                continue;
            }

            // add this dataitem if updated from last one
            dataItemVec.push_back(di);
            IF_LOC_LOGD {
                string dv;
                di->stringify(dv);
                LOC_LOGd("notify: DataItem In Value:%s", dv.c_str());
            }
        }

        if (!dataItemVec.empty()) {
            mContext.mMsgTask->sendMsg(new HandleNotify(this, dataItemVec));
        }
    }
}

#ifdef USE_GLIB
bool SystemStatusOsObserver::connectBackhaul(const BackhaulContext& ctx)
{
    bool result = false;

    if (mContext.mFrameworkActionReqObj != NULL) {
        struct HandleConnectBackhaul : public LocMsg {
            HandleConnectBackhaul(IFrameworkActionReq* fwkActReq, const BackhaulContext& ctx) :
                    mFwkActionReqObj(fwkActReq), mCtx(ctx) {}
            virtual ~HandleConnectBackhaul() {}
            void proc() const {
                LOC_LOGi("HandleConnectBackhaul::enter");
                mFwkActionReqObj->connectBackhaul(mCtx);
                LOC_LOGi("HandleConnectBackhaul::exit");
            }
            IFrameworkActionReq* mFwkActionReqObj;
            BackhaulContext mCtx;
        };
        mContext.mMsgTask->sendMsg(
                new (nothrow) HandleConnectBackhaul(mContext.mFrameworkActionReqObj, ctx));
        result = true;
    } else {
        LOC_LOGe("Framework action request object is NULL.Caching connect request: %s",
                 ctx.clientName.c_str());
        LOC_LOGd("Adding client context to BackHaulConnReqCache list");
        mBackHaulConnReqCache[ctx.clientName] = ctx;
        result = false;
    }
    return result;
}

bool SystemStatusOsObserver::disconnectBackhaul(const BackhaulContext& ctx)
{
    bool result = false;

    if (mContext.mFrameworkActionReqObj != NULL) {
        struct HandleDisconnectBackhaul : public LocMsg {
            HandleDisconnectBackhaul(IFrameworkActionReq* fwkActReq, const BackhaulContext& ctx) :
                    mFwkActionReqObj(fwkActReq), mCtx(ctx) {}
            virtual ~HandleDisconnectBackhaul() {}
            void proc() const {
                LOC_LOGi("HandleDisconnectBackhaul::enter");
                mFwkActionReqObj->disconnectBackhaul(mCtx);
                LOC_LOGi("HandleDisconnectBackhaul::exit");
            }
            IFrameworkActionReq* mFwkActionReqObj;
            BackhaulContext mCtx;
        };
        mContext.mMsgTask->sendMsg(
                new (nothrow) HandleDisconnectBackhaul(mContext.mFrameworkActionReqObj, ctx));
    }
    else {
        LOC_LOGe("Framework action request object is NULL.Caching disconnect request: %s",
                 ctx.clientName.c_str());
        // Check if client has requested for backhaul connection.
        LOC_LOGd("Removing client from BackHaulConnReqCache list");
        mBackHaulConnReqCache.erase(ctx.clientName);
        result = false;
    }
    return result;
}
#endif
/******************************************************************************
 Helpers
******************************************************************************/
void SystemStatusOsObserver::sendCachedDataItems(
        const unordered_set<DataItemId>& s, IDataItemObserver* to)
{
    if (nullptr == to) {
        LOC_LOGv("client pointer is NULL.");
    } else {
        string clientName;
        to->getName(clientName);
        unordered_set<IDataItemCore*> dataItems = {};

        for (auto each : s) {
            auto citer = mDataItemCache.find(each);
            if (citer != mDataItemCache.end()) {
                string dv;
                citer->second->stringify(dv);
                LOC_LOGi("DataItem: %s >> %s", dv.c_str(), clientName.c_str());
                dataItems.insert(citer->second);
            }
        }

        if (dataItems.empty()) {
            LOC_LOGv("No items to notify.");
        } else {
            to->notify(dataItems);
        }
    }
}

bool SystemStatusOsObserver::updateCache(IDataItemCore* d)
{
    bool dataItemUpdated = false;

    // Request systemstatus to record this dataitem in its cache
    // if the return is false, it means that SystemStatus is not
    // handling it, so SystemStatusOsObserver also doesn't.
    // So it has to be true to proceed.
    if (nullptr != d && mSystemStatus->eventDataItemNotify(d)) {
        dataItemUpdated = true;
        auto citer = mDataItemCache.find(d->getId());
        if (citer == mDataItemCache.end()) {
            // New data item; not found in cache
            IDataItemCore* dataitem = DataItemsFactoryProxy::createNewDataItem(d);
            if (nullptr != dataitem) {
                // Insert in mDataItemCache
                mDataItemCache.insert(std::make_pair(d->getId(), dataitem));
            }
        } else {
            // Found in cache; Update cache if necessary
            citer->second->copyFrom(d);
        }
    }

    if (nullptr != d) {
        LOC_LOGd("DataItem:%d updated:%d", d->getId(), dataItemUpdated);
    }
    return dataItemUpdated;
}

} // namespace loc_core

