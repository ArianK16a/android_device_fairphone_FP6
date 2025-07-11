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
#define LOG_TAG "LocSvc_GeofenceApiClient"

#include <log_util.h>
#include <loc_cfg.h>

#include "LocationUtil.h"
#include "GeofenceAPIClient.h"

namespace android {
namespace hardware {
namespace gnss {
namespace aidl {
namespace implementation {

using ::aidl::android::hardware::gnss::GnssLocation;

GeofenceAPIClient::GeofenceAPIClient(const shared_ptr<IGnssGeofenceCallback>& callback) :
    LocationAPIClientBase(),
    mGnssGeofencingCbIface(callback)
{
    LOC_LOGd("callback: %p", &callback);

    LocationCallbacks locationCallbacks;
    memset(&locationCallbacks, 0, sizeof(LocationCallbacks));
    locationCallbacks.size = sizeof(LocationCallbacks);

    locationCallbacks.trackingCb = nullptr;
    locationCallbacks.batchingCb = nullptr;

    locationCallbacks.geofenceBreachCb = nullptr;
    locationCallbacks.geofenceBreachCb =
            [this](const GeofenceBreachNotification& geofenceBreachNotification) {
        onGeofenceBreachCb(geofenceBreachNotification);
    };

    locationCallbacks.geofenceStatusCb =
            [this](const GeofenceStatusNotification& geofenceStatusNotification) {
        onGeofenceStatusCb(geofenceStatusNotification);
    };

    locationCallbacks.gnssLocationInfoCb = nullptr;
    locationCallbacks.gnssNiCb = nullptr;
    locationCallbacks.gnssSvCb = nullptr;
    locationCallbacks.gnssNmeaCb = nullptr;
    locationCallbacks.gnssMeasurementsCb = nullptr;

    locAPISetCallbacks(locationCallbacks);
}

void GeofenceAPIClient::upcateCallback(const shared_ptr<IGnssGeofenceCallback>& callback) {
    mMutex.lock();
    mGnssGeofencingCbIface = callback;
    mMutex.unlock();
}

void GeofenceAPIClient::geofenceAdd(uint32_t geofence_id, double latitude, double longitude,
        double radius_meters, int32_t last_transition, int32_t monitor_transitions,
        uint32_t notification_responsiveness_ms, uint32_t unknown_timer_ms)
{
    LOC_LOGd("geofence id: %d, geofence info: %f %f %f %d %d %d %d",
            geofence_id, latitude, longitude, radius_meters,
            last_transition, monitor_transitions, notification_responsiveness_ms, unknown_timer_ms);

    GeofenceOption options;
    memset(&options, 0, sizeof(GeofenceOption));
    options.size = sizeof(GeofenceOption);
    if (monitor_transitions & IGnssGeofenceCallback::ENTERED)
        options.breachTypeMask |= GEOFENCE_BREACH_ENTER_BIT;
    if (monitor_transitions & IGnssGeofenceCallback::EXITED)
        options.breachTypeMask |=  GEOFENCE_BREACH_EXIT_BIT;
    options.responsiveness = notification_responsiveness_ms;
    options.confidence = GEOFENCE_CONFIDENCE_HIGH;

    GeofenceInfo data;
    data.size = sizeof(GeofenceInfo);
    data.latitude = latitude;
    data.longitude = longitude;
    data.radius = radius_meters;

    LocationError err = (LocationError)locAPIAddGeofences(1, &geofence_id, &options, &data);
    if (LOCATION_ERROR_SUCCESS != err) {
        onAddGeofencesCb(1, &err, &geofence_id);
    }
}

void GeofenceAPIClient::geofencePause(uint32_t geofence_id)
{
    LOC_LOGd("geofence id %d", geofence_id);
    locAPIPauseGeofences(1, &geofence_id);
}

void GeofenceAPIClient::geofenceResume(uint32_t geofence_id, int32_t monitor_transitions)
{
    LOC_LOGd("geofence_id: %d monitor_transitions: %d", geofence_id, monitor_transitions);
    GeofenceBreachTypeMask mask = 0;
    if (monitor_transitions & IGnssGeofenceCallback::ENTERED)
        mask |= GEOFENCE_BREACH_ENTER_BIT;
    if (monitor_transitions & IGnssGeofenceCallback::EXITED)
        mask |=  GEOFENCE_BREACH_EXIT_BIT;
    locAPIResumeGeofences(1, &geofence_id, &mask);
}

void GeofenceAPIClient::geofenceRemove(uint32_t geofence_id)
{
    LOC_LOGd("geofence_id: %d", geofence_id);
    locAPIRemoveGeofences(1, &geofence_id);
}

void GeofenceAPIClient::geofenceRemoveAll()
{
    LOC_LOGd("");
    locAPIRemoveAllGeofences();
}

// callbacks
void GeofenceAPIClient::onGeofenceBreachCb(
        const GeofenceBreachNotification& geofenceBreachNotification)
{
    LOC_LOGd("BreachNotification.count %d", geofenceBreachNotification.count);
    mMutex.lock();
    auto cbIface = mGnssGeofencingCbIface;
    mMutex.unlock();
    if (cbIface != nullptr) {
        for (size_t i = 0; i < geofenceBreachNotification.count; i++) {
            GnssLocation gnssLocation;
            convertGnssLocation(geofenceBreachNotification.location, gnssLocation);

            int transition;
            if (geofenceBreachNotification.type == GEOFENCE_BREACH_ENTER)
                transition = IGnssGeofenceCallback::ENTERED;
            else if (geofenceBreachNotification.type == GEOFENCE_BREACH_EXIT)
                transition = IGnssGeofenceCallback::EXITED;
            else {
                // continue with other breach if transition is
                // nether GPS_GEOFENCE_ENTERED nor GPS_GEOFENCE_EXITED
                continue;
            }

            auto r = cbIface->gnssGeofenceTransitionCb(
                    geofenceBreachNotification.ids[i], gnssLocation, transition,
                    static_cast<long>(geofenceBreachNotification.timestamp));
            if (!r.isOk()) {
                LOC_LOGe("Error from gnssGeofenceTransitionCb");
            }
        }
    }
}

void GeofenceAPIClient::onGeofenceStatusCb(
        const GeofenceStatusNotification& geofenceStatusNotification) {
    LOC_LOGd("geofenceStatusNotification: %d", geofenceStatusNotification.available);
    mMutex.lock();
    auto cbIface = mGnssGeofencingCbIface;
    mMutex.unlock();
    if (cbIface != nullptr) {
        int status = IGnssGeofenceCallback::UNAVAILABLE;
        if (geofenceStatusNotification.available == GEOFENCE_STATUS_AVAILABILE_YES) {
            status = IGnssGeofenceCallback::AVAILABLE;
        }
        GnssLocation gnssLocation;
        memset(&gnssLocation, 0, sizeof(GnssLocation));
        auto r = cbIface->gnssGeofenceStatusCb(status, gnssLocation);
        if (!r.isOk()) {
            LOC_LOGe("Error from gnssGeofenceStatusCb");
        }
    }
}

void GeofenceAPIClient::onAddGeofencesCb(size_t count, LocationError* errors, uint32_t* ids)
{
    LOC_LOGd("count: %zu", count);
    mMutex.lock();
    auto cbIface = mGnssGeofencingCbIface;
    mMutex.unlock();
    if (cbIface != nullptr) {
        for (size_t i = 0; i < count; i++) {
            int status = IGnssGeofenceCallback::ERROR_GENERIC;
            if (errors[i] == LOCATION_ERROR_SUCCESS)
                status = IGnssGeofenceCallback::OPERATION_SUCCESS;
            else if (errors[i] == LOCATION_ERROR_ID_EXISTS)
                status = IGnssGeofenceCallback::ERROR_ID_EXISTS;
            auto r = cbIface->gnssGeofenceAddCb(ids[i], status);
            if (!r.isOk()) {
                LOC_LOGe("Error from gnssGeofenceAddCb");
            }
        }
    }
}

void GeofenceAPIClient::onRemoveGeofencesCb(size_t count, LocationError* errors, uint32_t* ids)
{
    LOC_LOGd("count: %zu", count);
    mMutex.lock();
    auto cbIface = mGnssGeofencingCbIface;
    mMutex.unlock();
    if (cbIface != nullptr) {
        for (size_t i = 0; i < count; i++) {
            int status = IGnssGeofenceCallback::ERROR_GENERIC;
            if (errors[i] == LOCATION_ERROR_SUCCESS)
                status = IGnssGeofenceCallback::OPERATION_SUCCESS;
            else if (errors[i] == LOCATION_ERROR_ID_UNKNOWN)
                status = IGnssGeofenceCallback::ERROR_ID_UNKNOWN;
            auto r = cbIface->gnssGeofenceRemoveCb(ids[i], status);
            if (!r.isOk()) {
                LOC_LOGe("Error from gnssGeofenceRemoveCb");
            }
        }
    }
}

void GeofenceAPIClient::onPauseGeofencesCb(size_t count, LocationError* errors, uint32_t* ids)
{
    LOC_LOGd("count: %zu", count);
    mMutex.lock();
    auto cbIface = mGnssGeofencingCbIface;
    mMutex.unlock();
    if (cbIface != nullptr) {
        for (size_t i = 0; i < count; i++) {
            int status = IGnssGeofenceCallback::ERROR_GENERIC;
            if (errors[i] == LOCATION_ERROR_SUCCESS)
                status = IGnssGeofenceCallback::OPERATION_SUCCESS;
            else if (errors[i] == LOCATION_ERROR_ID_UNKNOWN)
                status = IGnssGeofenceCallback::ERROR_ID_UNKNOWN;
            auto r = cbIface->gnssGeofencePauseCb(ids[i], status);
            if (!r.isOk()) {
                LOC_LOGe("Error from gnssGeofencePauseCb");
            }
        }
    }
}

void GeofenceAPIClient::onResumeGeofencesCb(size_t count, LocationError* errors, uint32_t* ids)
{
    LOC_LOGd("count: %zu", count);
    mMutex.lock();
    auto cbIface = mGnssGeofencingCbIface;
    mMutex.unlock();
    if (cbIface != nullptr) {
        for (size_t i = 0; i < count; i++) {
            int status = IGnssGeofenceCallback::ERROR_GENERIC;
            if (errors[i] == LOCATION_ERROR_SUCCESS)
                status = IGnssGeofenceCallback::OPERATION_SUCCESS;
            else if (errors[i] == LOCATION_ERROR_ID_UNKNOWN)
                status = IGnssGeofenceCallback::ERROR_ID_UNKNOWN;
            auto r = cbIface->gnssGeofenceResumeCb(ids[i], status);
            if (!r.isOk()) {
                LOC_LOGe("Error from gnssGeofenceResumeCb");
            }
        }
    }
}

}  // namespace implementation
}  // namespace aidl
}  // namespace gnss
}  // namespace hardware
}  // namespace android
