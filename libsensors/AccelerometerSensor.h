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

#ifndef ANDROID_ACCELEROMETER_SENSOR_H
#define ANDROID_ACCELEROMETER_SENSOR_H

#include "sensors.h"
#include "SensorBase.h"
#include "InputEventReader.h"


#define LIS3DH_IOCTL_BASE		71
#define LIS3DH_IOCTL_SET_DELAY		_IOW(LIS3DH_IOCTL_BASE, 0, int)
#define LIS3DH_IOCTL_GET_DELAY		_IOR(LIS3DH_IOCTL_BASE, 1, int)
#define LIS3DH_IOCTL_SET_ENABLE		_IOW(LIS3DH_IOCTL_BASE, 2, int)
#define LIS3DH_IOCTL_GET_ENABLE		_IOR(LIS3DH_IOCTL_BASE, 3, int)

#define MODE_OFF		0
#define MODE_ACCEL		(1 << 0)
#define MODE_ROTATE		(1 << 1)
#define MODE_MOVEMENT		(1 << 2)
#define MODE_ACCEL_ROTATE	(MODE_ACCEL | MODE_ROTATE)

class AccelerometerSensor : public SensorBase {

private:
    enum {
        ACC = 0,
        SO,
        SM,
        NUM_SENSORS,
    };

    int indexToMask(int index)
    {
        switch (index) {
        case ACC:
            return MODE_ACCEL;
        case SO:
            return MODE_ROTATE;
        case SM:
            return MODE_MOVEMENT;
        default:
            ALOGE("AccelerometerSensor: unknown index %d", index);
            return -EINVAL;
        }
    }

    uint32_t mEnabled;
    bool mOriEnabled;
    InputEventCircularReader mInputReader;
    sensors_event_t mPendingEvents[NUM_SENSORS];
    sensors_event_t mPendingEventsFlush;
    uint32_t mPendingEventsMask;
    uint32_t mPendingEventsFlushMask;
    void writeAkmAccel(int16_t, int16_t, int16_t);

public:
            AccelerometerSensor();
    virtual ~AccelerometerSensor();
    virtual int readEvents(sensors_event_t* data, int count);
    virtual int setDelay(int32_t handle, int64_t ns);
    virtual int enable(int32_t handle, int enabled);
    virtual int flush(int32_t handle);
};

#endif  // ANDROID_ACCELEROMETER_SENSOR_H
