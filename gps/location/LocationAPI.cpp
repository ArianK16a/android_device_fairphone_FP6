/* Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 *     * Neither the name of The Linux Foundation nor the names of its
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

#define LOG_NDEBUG 0
#define LOG_TAG "LocSvc_LocationAPI"

#include <location_interface.h>
#include <dlfcn.h>
#include <loc_pla.h>
#include <log_util.h>
#include <pthread.h>
#include <map>
#include <loc_misc_utils.h>
#include <loc_cfg.h>

typedef const GnssInterface* (getGnssInterface)();
typedef const GeofenceInterface* (getGeofenceInterface)();
typedef const BatchingInterface* (getBatchingInterface)();

// GTP services
typedef void (enableProviderGetter)();
typedef void (disableProviderGetter)();
typedef void (getSingleNetworkLocationGetter)(trackingCallback* callback);
typedef void (stopNetworkLocationGetter)(trackingCallback* callback);

typedef ILocationAPI* (*getLocationClientApiImpl)(capabilitiesCallback capabitiescb);
typedef ILocationControlAPI* (*getLocationIntegrationApiImpl) ();

typedef struct {
    // bit mask of the adpaters that we need to wait for the removeClientCompleteCallback
    // before we invoke the registered locationApiDestroyCompleteCallback
    LocationAdapterTypeMask waitAdapterMask;
    locationApiDestroyCompleteCallback destroyCompleteCb;
} LocationAPIDestroyCbData;

// This is the map for the client that has requested destroy with
// destroy callback provided.
typedef std::map<LocationAPI*, LocationAPIDestroyCbData>
    LocationClientDestroyCbMap;

typedef std::map<LocationAPI*, LocationCallbacks> LocationClientMap;
typedef struct {
    LocationClientMap clientData;
    LocationClientDestroyCbMap destroyClientData;
    ILocationControlAPI* controlAPI;
    LocationControlCallbacks controlCallbacks;
    GnssInterface* gnssInterface;
    GeofenceInterface* geofenceInterface;
    BatchingInterface* batchingInterface;
} LocationAPIData;

static LocationAPIData gData = {};
static pthread_mutex_t gDataMutex = PTHREAD_MUTEX_INITIALIZER;
static bool gGnssLoadFailed = false;
static bool gBatchingLoadFailed = false;
static bool gGeofenceLoadFailed = false;
static uint32_t gEnableMDMGnssHal = 0;
static bool gReadGnssDeploymentConfigOnce = false;

const loc_param_s_type gps_conf_params[] = {
    {"GNSS_DEPLOYMENT", &gEnableMDMGnssHal, nullptr, 'n'}
};

template <typename T1, typename T2>
static const T1* loadLocationInterface(const char* library, const char* name) {
    void* libhandle = nullptr;
    T2* getter = (T2*)dlGetSymFromLib(libhandle, library, name);
    if (nullptr == getter) {
        return (const T1*) getter;
    }else {
        return (*getter)();
    }
}

bool LocationAPI::isInfotainmentHalConfigured() {
    if (!gReadGnssDeploymentConfigOnce) {
        UTIL_READ_CONF(LOC_PATH_GPS_CONF, gps_conf_params);
        gReadGnssDeploymentConfigOnce = true;
    }
    return (gEnableMDMGnssHal == QTI_MDM_GNSS_ENABLED);
}

static void loadLibGnss() {

    if (NULL == gData.gnssInterface && !gGnssLoadFailed) {
        gData.gnssInterface =
            (GnssInterface*)loadLocationInterface<GnssInterface,
                getGnssInterface>("libgnss.so", "getGnssInterface");
        if (NULL == gData.gnssInterface) {
            gGnssLoadFailed = true;
            LOC_LOGW("%s:%d]: No gnss interface available", __func__, __LINE__);
        } else {
            gData.gnssInterface->initialize();
        }
    }
}

static void loadLibBatching() {

    if (NULL == gData.batchingInterface && !gBatchingLoadFailed) {
        gData.batchingInterface =
            (BatchingInterface*)loadLocationInterface<BatchingInterface,
             getBatchingInterface>("libbatching.so", "getBatchingInterface");
        if (NULL == gData.batchingInterface) {
            gBatchingLoadFailed = true;
            LOC_LOGW("%s:%d]: No batching interface available", __func__, __LINE__);
        } else {
            gData.batchingInterface->initialize();
        }
    }
}

static void loadLibGeofencing() {

    if (NULL == gData.geofenceInterface && !gGeofenceLoadFailed) {
        gData.geofenceInterface =
           (GeofenceInterface*)loadLocationInterface<GeofenceInterface,
           getGeofenceInterface>("libgeofencing.so", "getGeofenceInterface");
        if (NULL == gData.geofenceInterface) {
            gGeofenceLoadFailed = true;
            LOC_LOGW("%s:%d]: No geofence interface available", __func__, __LINE__);
        } else {
            gData.geofenceInterface->initialize();
        }
    }
}

static bool isGnssClient(LocationCallbacks& locationCallbacks)
{
    return (locationCallbacks.gnssNiCb != nullptr ||
            locationCallbacks.trackingCb != nullptr ||
            locationCallbacks.gnssLocationInfoCb != nullptr ||
            locationCallbacks.engineLocationsInfoCb != nullptr ||
            locationCallbacks.gnssSvCb != nullptr ||
            locationCallbacks.gnssNmeaCb != nullptr ||
            locationCallbacks.engineNmeaCb != nullptr ||
            locationCallbacks.gnssDataCb != nullptr ||
            locationCallbacks.gnssMeasurementsCb != nullptr ||
            locationCallbacks.gnssNHzMeasurementsCb != nullptr ||
            locationCallbacks.locationSystemInfoCb != nullptr ||
            locationCallbacks.gnssDcReportCb != nullptr ||
            locationCallbacks.gnssSignalTypesCb != nullptr ||
            locationCallbacks.svEphemerisCb != nullptr);
}

static bool isBatchingClient(LocationCallbacks& locationCallbacks)
{
    return (locationCallbacks.batchingCb != nullptr);
}

static bool isGeofenceClient(LocationCallbacks& locationCallbacks)
{
    return (locationCallbacks.geofenceBreachCb != nullptr ||
            locationCallbacks.geofenceStatusCb != nullptr);
}


void LocationAPI::onRemoveClientCompleteCb (LocationAdapterTypeMask adapterType)
{
    bool invokeCallback = false;
    locationApiDestroyCompleteCallback destroyCompleteCb;
    LOC_LOGd("adatper type %x", adapterType);
    pthread_mutex_lock(&gDataMutex);
    auto it = gData.destroyClientData.find(this);
    if (it != gData.destroyClientData.end()) {
        it->second.waitAdapterMask &= ~adapterType;
        if (it->second.waitAdapterMask == 0) {
            invokeCallback = true;
            destroyCompleteCb = it->second.destroyCompleteCb;
            gData.destroyClientData.erase(it);
        }
    }
    pthread_mutex_unlock(&gDataMutex);

    if (invokeCallback) {
        LOC_LOGd("invoke client destroy cb");
        if (!destroyCompleteCb) {
            (destroyCompleteCb) ();
        }

        delete this;
    }
}

void onGnssRemoveClientCompleteCb (LocationAPI* client)
{
    client->onRemoveClientCompleteCb (LOCATION_ADAPTER_GNSS_TYPE_BIT);
}

void onBatchingRemoveClientCompleteCb (LocationAPI* client)
{
    client->onRemoveClientCompleteCb (LOCATION_ADAPTER_BATCHING_TYPE_BIT);
}

void onGeofenceRemoveClientCompleteCb (LocationAPI* client)
{
    client->onRemoveClientCompleteCb (LOCATION_ADAPTER_GEOFENCE_TYPE_BIT);
}

ILocationAPI*
LocationAPI::createInstance (LocationCallbacks& locationCallbacks)
{
    ILocationAPI* locationClientApiImpl = nullptr;
    LocationAPI* locationApiObj = nullptr;

    if (nullptr == locationCallbacks.capabilitiesCb ||
        nullptr == locationCallbacks.responseCb ||
        nullptr == locationCallbacks.collectiveResponseCb) {
        LOC_LOGe("missing mandatory callback, return null");
        return NULL;
    }

    if (isInfotainmentHalConfigured()) {
        void *handle = nullptr;
        getLocationClientApiImpl getter = (getLocationClientApiImpl)dlGetSymFromLib(handle,
                "liblocation_client_api.so", "getLocationClientApiImpl");
        if (nullptr == getter) {
            LOC_LOGe("Failed to load LocationClientApi implementation.");
        } else {
            locationClientApiImpl = getter(locationCallbacks.capabilitiesCb);
            locationClientApiImpl->updateCallbacks(locationCallbacks);
            LOC_LOGi("Succesfully loaded LocationClientApi implementation.");
        }
        return locationClientApiImpl;
    }

    locationApiObj = new LocationAPI();

    bool requestedCapabilities = false;
    pthread_mutex_lock(&gDataMutex);

    if (isGnssClient(locationCallbacks)) {
        loadLibGnss();
        if (NULL != gData.gnssInterface) {
            gData.gnssInterface->addClient(locationApiObj, locationCallbacks);
            if (!requestedCapabilities) {
                gData.gnssInterface->requestCapabilities(locationApiObj);
                requestedCapabilities = true;
            }
        }
    }

    if (isBatchingClient(locationCallbacks)) {
        loadLibBatching();
        if (NULL != gData.batchingInterface) {
            gData.batchingInterface->addClient(locationApiObj, locationCallbacks);
            if (!requestedCapabilities) {
                gData.batchingInterface->requestCapabilities(locationApiObj);
                requestedCapabilities = true;
            }
        }
    }

    if (isGeofenceClient(locationCallbacks)) {
        loadLibGeofencing();
        if (NULL != gData.geofenceInterface) {
            gData.geofenceInterface->addClient(locationApiObj, locationCallbacks);
            if (!requestedCapabilities) {
                gData.geofenceInterface->requestCapabilities(locationApiObj);
                requestedCapabilities = true;
            }
        }
    }

    if (!requestedCapabilities && locationCallbacks.capabilitiesCb != nullptr) {
        loadLibGnss();
        if (NULL != gData.gnssInterface) {
            gData.gnssInterface->addClient(locationApiObj, locationCallbacks);
            gData.gnssInterface->requestCapabilities(locationApiObj);
            requestedCapabilities = true;
        }
    }

    gData.clientData[locationApiObj] = locationCallbacks;

    pthread_mutex_unlock(&gDataMutex);

    return locationApiObj;
}

void
LocationAPI::destroy(locationApiDestroyCompleteCallback destroyCompleteCb)
{
    bool invokeDestroyCb = false;

    pthread_mutex_lock(&gDataMutex);
    auto it = gData.clientData.find(this);
    if (it != gData.clientData.end()) {
        bool removeFromGnssInf = (NULL != gData.gnssInterface);
        bool removeFromBatchingInf = (NULL != gData.batchingInterface);
        bool removeFromGeofenceInf = (NULL != gData.geofenceInterface);
        bool needToWait = (removeFromGnssInf || removeFromBatchingInf || removeFromGeofenceInf);
        LOC_LOGe("removeFromGnssInf: %d, removeFromBatchingInf: %d, removeFromGeofenceInf: %d,"
                 "needToWait: %d", removeFromGnssInf, removeFromBatchingInf, removeFromGeofenceInf,
                 needToWait);

        if ((NULL != destroyCompleteCb) && (true == needToWait)) {
            LocationAPIDestroyCbData destroyCbData = {};
            destroyCbData.destroyCompleteCb = destroyCompleteCb;
            // record down from which adapter we need to wait for the destroy complete callback
            // only when we have received all the needed callbacks from all the associated stacks,
            // we shall notify the client.
            destroyCbData.waitAdapterMask =
                    (removeFromGnssInf ? LOCATION_ADAPTER_GNSS_TYPE_BIT : 0);
            destroyCbData.waitAdapterMask |=
                    (removeFromBatchingInf ? LOCATION_ADAPTER_BATCHING_TYPE_BIT : 0);
            destroyCbData.waitAdapterMask |=
                    (removeFromGeofenceInf ? LOCATION_ADAPTER_GEOFENCE_TYPE_BIT : 0);
            gData.destroyClientData[this] = destroyCbData;
            LOC_LOGi("destroy data stored in the map: 0x%x", destroyCbData.waitAdapterMask);
        }

        if (removeFromGnssInf) {
            gData.gnssInterface->removeClient(it->first,
                                              onGnssRemoveClientCompleteCb);
        }
        if (removeFromBatchingInf) {
            gData.batchingInterface->removeClient(it->first,
                                             onBatchingRemoveClientCompleteCb);
        }
        if (removeFromGeofenceInf) {
            gData.geofenceInterface->removeClient(it->first,
                                                  onGeofenceRemoveClientCompleteCb);
        }

        gData.clientData.erase(it);

        if (!needToWait) {
            invokeDestroyCb = true;
        }
    } else {
        LOC_LOGE("%s:%d]: Location API client %p not found in client data",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
    if (invokeDestroyCb) {
        if (!destroyCompleteCb) {
            (destroyCompleteCb) ();
        }
        delete this;
    }
}

LocationAPI::LocationAPI()
{
    LOC_LOGD("LOCATION API CONSTRUCTOR");
}

// private destructor
LocationAPI::~LocationAPI()
{
    LOC_LOGD("LOCATION API DESTRUCTOR");
}

void
LocationAPI::updateCallbacks(LocationCallbacks& locationCallbacks)
{
    if (nullptr == locationCallbacks.capabilitiesCb ||
        nullptr == locationCallbacks.responseCb ||
        nullptr == locationCallbacks.collectiveResponseCb) {
        return;
    }

    pthread_mutex_lock(&gDataMutex);

    LocationCallbacks currentCallbacks = {};
    auto it = gData.clientData.find(this);
    if (it != gData.clientData.end()) {
        currentCallbacks = gData.clientData[this];
    }

    if (isGnssClient(locationCallbacks)) {
        loadLibGnss();
        if (NULL != gData.gnssInterface) {
            // either adds new Client or updates existing Client
            gData.gnssInterface->addClient(this, locationCallbacks);
        }
    } else if (NULL != gData.gnssInterface && isGnssClient(currentCallbacks)) {
        gData.gnssInterface->removeClient(this, nullptr);
    }

    if (isBatchingClient(locationCallbacks)) {
        loadLibBatching();
        if (NULL != gData.batchingInterface) {
            // either adds new Client or updates existing Client
            gData.batchingInterface->addClient(this, locationCallbacks);
        }
    } else if (NULL != gData.batchingInterface && isBatchingClient(currentCallbacks)) {
        gData.batchingInterface->removeClient(this, nullptr);
    }

    if (isGeofenceClient(locationCallbacks)) {
        loadLibGeofencing();
        if (NULL != gData.geofenceInterface) {
            // either adds new Client or updates existing Client
            gData.geofenceInterface->addClient(this, locationCallbacks);
        }
    } else if (NULL != gData.geofenceInterface && isGeofenceClient(currentCallbacks)) {
        gData.geofenceInterface->removeClient(this, nullptr);
    }

    gData.clientData[this] = locationCallbacks;

    pthread_mutex_unlock(&gDataMutex);
}

uint32_t
LocationAPI::startTracking(const TrackingOptions& trackingOptions)
{
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    auto it = gData.clientData.find(this);
    if (it != gData.clientData.end()) {
        if (NULL != gData.gnssInterface) {
            id = gData.gnssInterface->startTracking(this, trackingOptions);
        } else {
            LOC_LOGE("%s:%d]: No gnss interface available for Location API client %p ",
                     __func__, __LINE__, this);
        }
    } else {
        LOC_LOGE("%s:%d]: Location API client %p not found in client data",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

void
LocationAPI::stopTracking(uint32_t id)
{
    pthread_mutex_lock(&gDataMutex);

    auto it = gData.clientData.find(this);
    if (it != gData.clientData.end()) {
        if (gData.gnssInterface != NULL) {
            gData.gnssInterface->stopTracking(this, id);
        } else {
            LOC_LOGE("%s:%d]: No gnss interface available for Location API client %p ",
                     __func__, __LINE__, this);
        }
    } else {
        LOC_LOGE("%s:%d]: Location API client %p not found in client data",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void
LocationAPI::updateTrackingOptions(
        uint32_t id, const TrackingOptions& trackingOptions)
{
    pthread_mutex_lock(&gDataMutex);

    auto it = gData.clientData.find(this);
    if (it != gData.clientData.end()) {
        if (gData.gnssInterface != NULL) {
            gData.gnssInterface->updateTrackingOptions(this, id, trackingOptions);
        } else {
            LOC_LOGE("%s:%d]: No gnss interface available for Location API client %p ",
                     __func__, __LINE__, this);
        }
    } else {
        LOC_LOGE("%s:%d]: Location API client %p not found in client data",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

uint32_t
LocationAPI::startBatching(const BatchingOptions &batchingOptions)
{
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (NULL != gData.batchingInterface) {
        id = gData.batchingInterface->startBatching(this, batchingOptions);
    } else {
        LOC_LOGE("%s:%d]: No batching interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

void
LocationAPI::stopBatching(uint32_t id)
{
    pthread_mutex_lock(&gDataMutex);

    if (NULL != gData.batchingInterface) {
        gData.batchingInterface->stopBatching(this, id);
    } else {
        LOC_LOGE("%s:%d]: No batching interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void
LocationAPI::updateBatchingOptions(uint32_t id, const BatchingOptions& batchOptions)
{
    pthread_mutex_lock(&gDataMutex);

    if (NULL != gData.batchingInterface) {
        gData.batchingInterface->updateBatchingOptions(this, id, batchOptions);
    } else {
        LOC_LOGE("%s:%d]: No batching interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void
LocationAPI::getBatchedLocations(uint32_t id, size_t count)
{
    pthread_mutex_lock(&gDataMutex);

    if (gData.batchingInterface != NULL) {
        gData.batchingInterface->getBatchedLocations(this, id, count);
    } else {
        LOC_LOGE("%s:%d]: No batching interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

uint32_t*
LocationAPI::addGeofences(size_t count, GeofenceOption* options, GeofenceInfo* info)
{
    uint32_t* ids = NULL;
    pthread_mutex_lock(&gDataMutex);

    if (gData.geofenceInterface != NULL) {
        ids = gData.geofenceInterface->addGeofences(this, count, options, info);
    } else {
        LOC_LOGE("%s:%d]: No geofence interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
    return ids;
}

void
LocationAPI::removeGeofences(size_t count, uint32_t* ids)
{
    pthread_mutex_lock(&gDataMutex);

    if (gData.geofenceInterface != NULL) {
        gData.geofenceInterface->removeGeofences(this, count, ids);
    } else {
        LOC_LOGE("%s:%d]: No geofence interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void
LocationAPI::modifyGeofences(size_t count, uint32_t* ids, GeofenceOption* options)
{
    pthread_mutex_lock(&gDataMutex);

    if (gData.geofenceInterface != NULL) {
        gData.geofenceInterface->modifyGeofences(this, count, ids, options);
    } else {
        LOC_LOGE("%s:%d]: No geofence interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void
LocationAPI::pauseGeofences(size_t count, uint32_t* ids)
{
    pthread_mutex_lock(&gDataMutex);

    if (gData.geofenceInterface != NULL) {
        gData.geofenceInterface->pauseGeofences(this, count, ids);
    } else {
        LOC_LOGE("%s:%d]: No geofence interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void
LocationAPI::resumeGeofences(size_t count, uint32_t* ids)
{
    pthread_mutex_lock(&gDataMutex);

    if (gData.geofenceInterface != NULL) {
        gData.geofenceInterface->resumeGeofences(this, count, ids);
    } else {
        LOC_LOGE("%s:%d]: No geofence interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void
LocationAPI::gnssNiResponse(uint32_t id, GnssNiResponse response)
{
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->gnssNiResponse(this, id, response);
    } else {
        LOC_LOGE("%s:%d]: No gnss interface available for Location API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

void LocationAPI::enableNetworkProvider() {
    void* libHandle = nullptr;
    enableProviderGetter* setter = (enableProviderGetter*)dlGetSymFromLib(libHandle,
            "liblocationservice_glue.so", "enableNetworkProvider");
    if (setter != nullptr) {
        (*setter)();
    } else {
        LOC_LOGe("dlGetSymFromLib failed for liblocationservice_glue.so");
    }
}

void LocationAPI::disableNetworkProvider() {
    void* libHandle = nullptr;
    disableProviderGetter* setter = (disableProviderGetter*)dlGetSymFromLib(libHandle,
            "liblocationservice_glue.so", "disableNetworkProvider");
    if (setter != nullptr) {
        (*setter)();
    } else {
        LOC_LOGe("dlGetSymFromLib failed for liblocationservice_glue.so");
    }
}

void LocationAPI::startNetworkLocation(trackingCallback* callback) {
    void* libHandle = nullptr;
    getSingleNetworkLocationGetter* setter =
            (getSingleNetworkLocationGetter*)dlGetSymFromLib(libHandle,
            "liblocationservice_glue.so", "startNetworkLocation");
    if (setter != nullptr) {
        (*setter)(callback);
    } else {
        LOC_LOGe("dlGetSymFromLib failed for liblocationservice_glue.so");
    }
}

void LocationAPI::stopNetworkLocation(trackingCallback* callback) {
    void* libHandle = nullptr;
    stopNetworkLocationGetter* setter = (stopNetworkLocationGetter*)dlGetSymFromLib(libHandle,
            "liblocationservice_glue.so", "stopNetworkLocation");
    if (setter != nullptr) {
        LOC_LOGe("called");
        (*setter)(callback);
    } else {
        LOC_LOGe("dlGetSymFromLib failed for liblocationservice_glue.so");
    }
}

void LocationAPI::getDebugReport(GnssDebugReport& report) {
    if (gData.gnssInterface != NULL) {
        pthread_mutex_lock(&gDataMutex);
        gData.gnssInterface->getDebugReport(report);
        pthread_mutex_unlock(&gDataMutex);
    } else {
        LOC_LOGe("No gnss interface available for Location API");
    }
}

uint32_t LocationAPI::getAntennaInfo(AntennaInfoCallback* cb) {
    if (gData.gnssInterface != NULL) {
        return gData.gnssInterface->getAntennaInfo(cb);
    } else {
        LOC_LOGe("No gnss interface available for Location API");
        return LOCATION_ERROR_GENERAL_FAILURE;
    }
}

ILocationControlAPI*
LocationControlAPI::getInstance(LocationControlCallbacks& locationControlCallbacks)
{
    pthread_mutex_lock(&gDataMutex);
    void *handle = nullptr;

    if (NULL == gData.controlAPI) {
        if (LocationAPI::isInfotainmentHalConfigured()) {
            getLocationIntegrationApiImpl getter =
                (getLocationIntegrationApiImpl)dlGetSymFromLib(handle,
                "liblocation_integration_api.so", "getLocationIntegrationApiImpl");
            if (nullptr == getter) {
                LOC_LOGe("Failed to load LocationIntegrationApi implementation.");
            } else {
                gData.controlAPI = getter();
                LOC_LOGi("Succesfully loaded LocationIntegrationApi implementation.");
            }
        } else {
            loadLibGnss();
            if (!gGnssLoadFailed) {
                gData.controlAPI = new LocationControlAPI();
            }
        }
    }

    if (NULL != gData.gnssInterface) {
        if (locationControlCallbacks.responseCb != NULL)
            gData.controlCallbacks.responseCb = locationControlCallbacks.responseCb;
        if (locationControlCallbacks.collectiveResponseCb != NULL)
            gData.controlCallbacks.collectiveResponseCb =
                    locationControlCallbacks.collectiveResponseCb;
        if (locationControlCallbacks.gnssConfigCb != NULL)
            gData.controlCallbacks.gnssConfigCb = locationControlCallbacks.gnssConfigCb;
        if (locationControlCallbacks.odcpiReqCb != NULL)
            gData.controlCallbacks.odcpiReqCb = locationControlCallbacks.odcpiReqCb;

        gData.gnssInterface->setControlCallbacks(gData.controlCallbacks);
    }

    pthread_mutex_unlock(&gDataMutex);
    return gData.controlAPI;
}

ILocationControlAPI*
LocationControlAPI::getInstance()
{
    ILocationControlAPI* controlAPI = NULL;

    pthread_mutex_lock(&gDataMutex);
    controlAPI = gData.controlAPI;
    pthread_mutex_unlock(&gDataMutex);
    return controlAPI;
}

void
LocationControlAPI::destroy()
{
    delete this;
}

LocationControlAPI::LocationControlAPI()
{
    LOC_LOGD("LOCATION CONTROL API CONSTRUCTOR");
}

LocationControlAPI::~LocationControlAPI()
{
    LOC_LOGD("LOCATION CONTROL API DESTRUCTOR");
    pthread_mutex_lock(&gDataMutex);

    gData.controlAPI = NULL;

    pthread_mutex_unlock(&gDataMutex);
}

uint32_t
LocationControlAPI::enable(LocationTechnologyType techType)
{
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->enable(techType);
    } else {
        LOC_LOGE("%s:%d]: No gnss interface available for Location Control API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

void
LocationControlAPI::disable(uint32_t id)
{
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->disable(id);
    } else {
        LOC_LOGE("%s:%d]: No gnss interface available for Location Control API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
}

uint32_t*
LocationControlAPI::gnssUpdateConfig(const GnssConfig& config)
{
    uint32_t* ids = NULL;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        ids = gData.gnssInterface->gnssUpdateConfig(config);
    } else {
        LOC_LOGE("%s:%d]: No gnss interface available for Location Control API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
    return ids;
}

uint32_t* LocationControlAPI::gnssGetConfig(GnssConfigFlagsMask mask) {

    uint32_t* ids = NULL;
    pthread_mutex_lock(&gDataMutex);

    if (NULL != gData.gnssInterface) {
        ids = gData.gnssInterface->gnssGetConfig(mask);
    } else {
        LOC_LOGe("No gnss interface available for Control API client %p", this);
    }

    pthread_mutex_unlock(&gDataMutex);
    return ids;
}

uint32_t
LocationControlAPI::gnssDeleteAidingData(const GnssAidingData& data)
{
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->gnssDeleteAidingData(data);
    } else {
        LOC_LOGE("%s:%d]: No gnss interface available for Location Control API client %p ",
                 __func__, __LINE__, this);
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configConstellations(
        const GnssSvTypeConfig& constellationEnablementConfig,
        const GnssSvIdConfig&   blacklistSvConfig) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->gnssUpdateSvConfig(
                constellationEnablementConfig, blacklistSvConfig);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configConstellationSecondaryBand(
        const GnssSvTypeConfig& secondaryBandConfig) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->gnssUpdateSecondaryBandConfig(secondaryBandConfig);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configConstrainedTimeUncertainty(
            bool enable, float tuncThreshold, uint32_t energyBudget) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->setConstrainedTunc(enable,
                                                     tuncThreshold,
                                                     energyBudget);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configPositionAssistedClockEstimator(bool enable) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->setPositionAssistedClockEstimator(enable);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configLeverArm(const LeverArmConfigInfo& configInfo) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configLeverArm(configInfo);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configRobustLocation(bool enable, bool enableForE911) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configRobustLocation(enable, enableForE911);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configMinGpsWeek(uint16_t minGpsWeek) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configMinGpsWeek(minGpsWeek);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configDeadReckoningEngineParams(
        const DeadReckoningEngineConfig& dreConfig) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configDeadReckoningEngineParams(dreConfig);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configEngineRunState(
        PositioningEngineMask engType, LocEngineRunState engState) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configEngineRunState(engType, engState);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::setOptInStatus(bool userConsent) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->setOptInStatus(userConsent);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configOutputNmeaTypes(
            GnssNmeaTypesMask enabledNmeaTypes,
            GnssGeodeticDatumType nmeaDatumType,
            LocReqEngineTypeMask locReqEngTypeMask) {

    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configOutputNmeaTypes(enabledNmeaTypes,
                                                        nmeaDatumType,
                                                        locReqEngTypeMask);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

void LocationControlAPI::powerStateEvent(PowerStateType powerState) {
    pthread_mutex_lock(&gDataMutex);

    LOC_LOGv("--> entry, powerState: %d", powerState);

    if (NULL != gData.gnssInterface) {
        gData.gnssInterface->updateSystemPowerState(powerState);
    } else {
        LOC_LOGv("No gnss interface available.");
    }

    if (NULL != gData.geofenceInterface) {
            gData.geofenceInterface->updateSystemPowerState(powerState);
        } else {
            LOC_LOGv("No geofence interface available.");
    }

    if (NULL != gData.batchingInterface) {
        gData.batchingInterface->updateSystemPowerState(powerState);
    } else {
        LOC_LOGv("No batching interface available.");
    }

    pthread_mutex_unlock(&gDataMutex);
}

uint32_t LocationControlAPI::updateCallbacks(LocationControlCallbacks& callbacks)
{
    uint32_t retVal = 0;

    if (gData.gnssInterface == NULL) return retVal;

    pthread_mutex_lock(&gDataMutex);

    // Some of the init API's like antennaInfoInit , measCorrInit, have an explicit defined
    // uint32_t return value else we can assume the return is non-zero i.e. success.
    retVal = 1;

    if (callbacks.odcpiReqCb) {
        gData.gnssInterface->odcpiInit(callbacks.odcpiReqCb, ODCPI_HANDLER_PRIORITY_DEFAULT,
                (EMERGENCY_ODCPI | NON_EMERGENCY_ODCPI));
    } else if (callbacks.agpsStatusIpV4Cb) {
        AgpsCbInfo cbInfo {callbacks.agpsStatusIpV4Cb,
                           AGPS_ATL_TYPE_SUPL | AGPS_ATL_TYPE_SUPL_ES, AGPS_CB_PRIORITY_LOW};
        gData.gnssInterface->agpsInit(cbInfo);
    } else if (callbacks.measCorrSetCapabilitiesCb) {
        retVal = gData.gnssInterface->measCorrInit(callbacks.measCorrSetCapabilitiesCb);
    } else if ((callbacks.nfwStatusCb) || (callbacks.isInEmergencyStatusCb)) {
        NfwCbInfo cbInfo {callbacks.nfwStatusCb, callbacks.isInEmergencyStatusCb};
        gData.gnssInterface->nfwInit(cbInfo);
    }

    pthread_mutex_unlock(&gDataMutex);
    return retVal;
}

void LocationControlAPI::odcpiInject(const ::Location& location) {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->odcpiInject(location);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

void LocationControlAPI::resetNetworkInfo() {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->resetNetworkInfo();
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

void LocationControlAPI::updateBatteryStatus(bool charging) {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->updateBatteryStatus(charging);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

void LocationControlAPI::injectLocation(double latitude, double longitude, float accuracy) {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->injectLocation(latitude, longitude, accuracy);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

void LocationControlAPI::agpsDataConnOpen(AGpsType agpsType, const char* apnName,
        int apnLen, int ipType) {

    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->agpsDataConnOpen(agpsType, apnName, apnLen, ipType);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

void LocationControlAPI::agpsDataConnClosed(AGpsType agpsType) {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->agpsDataConnClosed(agpsType);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    }

void LocationControlAPI::agpsDataConnFailed(AGpsType agpsType) {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->agpsDataConnFailed(agpsType);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

void LocationControlAPI::updateConnectionStatus(bool connected, int8_t type, bool roaming,
        NetworkHandle networkHandle, const std::string& apn) {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->updateConnectionStatus(connected, type, roaming,
                networkHandle, apn);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

bool LocationControlAPI::measCorrSetCorrections(
        const GnssMeasurementCorrections& gnssMeasCorr) {
    pthread_mutex_lock(&gDataMutex);
    bool retVal = 0;

    if (gData.gnssInterface != NULL) {
        retVal = gData.gnssInterface->measCorrSetCorrections(gnssMeasCorr);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return retVal;
}

void LocationControlAPI::measCorrClose() {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->measCorrClose();
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);

}

void LocationControlAPI::enableNfwLocationAccess(const std::vector<std::string>& enabledNfws) {
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        gData.gnssInterface->enableNfwLocationAccess(enabledNfws);
    }
    else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
}

uint32_t LocationControlAPI::configEngineIntegrityRisk(
            PositioningEngineMask engType, uint32_t integrityRisk) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configEngineIntegrityRisk(engType, integrityRisk);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configXtraParams(
        bool enable, const XtraConfigParams& xtraParams) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configXtraParams(enable, xtraParams);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configMerkleTree(const char * merkleTreeXml, int xmlSize) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configMerkleTree(merkleTreeXml, xmlSize);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}

uint32_t LocationControlAPI::configOsnmaEnablement(bool enable) {
    uint32_t id = 0;
    pthread_mutex_lock(&gDataMutex);

    if (gData.gnssInterface != NULL) {
        id = gData.gnssInterface->configOsnmaEnablement(enable);
    } else {
        LOC_LOGe("No gnss interface available for Location Control API");
    }

    pthread_mutex_unlock(&gDataMutex);
    return id;
}
