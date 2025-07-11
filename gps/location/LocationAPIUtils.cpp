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
Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#define LOG_NDEBUG 0
#define LOG_TAG "LocSvc_LocationAPIUtils"

#include <LocationAPIUtils.h>
#include <cmath>
#include <random>
#include <chrono>

// Fudge fine location to coarse:
// value need to match with Android Framework default
static const float MIN_ACCURACY_M = 2000.0f;
static const long OFFSET_UPDATE_INTERVAL_MS = 60 * 60 * 1000;
static const double CHANGE_PER_INTERVAL = 0.03;
static const double NEW_WEIGHT = CHANGE_PER_INTERVAL;
static const double OLD_WEIGHT = std::sqrt(1 - NEW_WEIGHT * NEW_WEIGHT);
static const int APPROXIMATE_METERS_PER_DEGREE_AT_EQUATOR = 111'000;
static const double MAX_LATITUDE = 90.0 - (1.0 / APPROXIMATE_METERS_PER_DEGREE_AT_EQUATOR);
static const float sAccuracyM = MIN_ACCURACY_M;

static double sLatitudeOffsetM;
static double sLongitudeOffsetM;
static long sNextUpdateRealtimeMs;

using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::system_clock;

static double nextRandom() {
    long now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    srand(now);
    int r = rand() % 10000;
    return (double)(r - 5000) / 5000;
}

static double nextRandomOffset() {
    return nextRandom() * sAccuracyM;
}

static void updateOffsets() {
    long now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    if (now < sNextUpdateRealtimeMs) {
        return;
    }
    sLatitudeOffsetM = (OLD_WEIGHT * sLatitudeOffsetM) + (NEW_WEIGHT * nextRandomOffset());
    sLongitudeOffsetM = (OLD_WEIGHT * sLongitudeOffsetM) + (NEW_WEIGHT * nextRandomOffset());
    sNextUpdateRealtimeMs = now + OFFSET_UPDATE_INTERVAL_MS;
}

static double wrapLatitude(double lat) {
    if (lat > MAX_LATITUDE) {
        lat = MAX_LATITUDE;
    }
    if (lat < -MAX_LATITUDE) {
        lat = -MAX_LATITUDE;
    }
    return lat;
}
static double wrapLongitude(double lon) {
    lon = std::fmod(lon, 360.0);
    if (lon >= 180.0) {
        lon -= 360.0;
    }
    if (lon < -180.0) {
        lon += 360.0;
    }
    return lon;
}

static double degreesToRadians(double degrees) {
    return degrees * M_PI / 180;
}

static double metersToDegreesLatitude(double distance) {
    return distance / APPROXIMATE_METERS_PER_DEGREE_AT_EQUATOR;
}

static double metersToDegreesLongitude(double distance, double lat) {
    return distance / APPROXIMATE_METERS_PER_DEGREE_AT_EQUATOR / std::cos(degreesToRadians(lat));
}

void locUtilCreateCoarseLocation(const Location& fine, Location& coarse) {
    memset(&coarse, 0, sizeof(coarse));
    coarse.size = sizeof(Location);

    coarse.timestamp = fine.timestamp;

    updateOffsets();

    double latitude = fine.latitude;
    double longitude = fine.longitude;
    longitude += wrapLongitude(metersToDegreesLongitude(sLongitudeOffsetM, latitude));
    latitude += wrapLatitude(metersToDegreesLatitude(sLatitudeOffsetM));

    double latGranularity = metersToDegreesLatitude(sAccuracyM);
    latitude = wrapLatitude(std::round(latitude / latGranularity) * latGranularity);
    double lonGranularity = metersToDegreesLongitude(sAccuracyM, latitude);
    longitude = wrapLongitude(std::round(longitude / lonGranularity) * lonGranularity);

    coarse.latitude = latitude;
    coarse.longitude = longitude;
    coarse.flags |= LOCATION_HAS_LAT_LONG_BIT;

    coarse.accuracy = std::max(sAccuracyM, fine.accuracy);
    coarse.flags |= LOCATION_HAS_ACCURACY_BIT;

    if (fine.flags & LOCATION_HAS_TECH_MASK_BIT) {
        coarse.flags |= LOCATION_HAS_TECH_MASK_BIT;
        coarse.techMask = fine.techMask;
    }
}
