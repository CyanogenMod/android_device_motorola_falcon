/*
 * Copyright (C) 2015 The CyanogenMod Project
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ANDROID_LIGHTPROX_SENSOR_H
#define ANDROID_LIGHTPROX_SENSOR_H

#include "sensors.h"
#include "SensorBase.h"
#include "InputEventReader.h"


class LightProxSensor : public SensorBase {

private:
    enum {
        LIGHT = 0,
        PROX,
        NUM_SENSORS
    };
    bool mEnabled[NUM_SENSORS];
    InputEventCircularReader mInputReader;
    sensors_event_t mPendingEvents[NUM_SENSORS];
    sensors_event_t mPendingEventsFlush;
    uint32_t mPendingEventsMask;
    uint32_t mPendingEventsFlushMask;
    bool mProxHasPendingEvent;
    void setProxInitialState();

public:
            LightProxSensor();
    virtual ~LightProxSensor();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual bool hasPendingEvents() const;
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);
    virtual int flush(int handle);
};

#endif  // ANDROID_LIGHTPROX_SENSOR_H
