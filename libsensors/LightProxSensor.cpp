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

#include <fcntl.h>
#include <string.h>
#include <cutils/log.h>

#include "LightProxSensor.h"


LightProxSensor::LightProxSensor()
    : SensorBase(NULL, "light-prox"),
      mInputReader(4),
      mPendingEventsMask(0),
      mPendingEventsFlushMask(0)
{
    mEnabled[LIGHT] = false;
    mPendingEvents[LIGHT].version = sizeof(sensors_event_t);
    mPendingEvents[LIGHT].sensor = ID_L;
    mPendingEvents[LIGHT].type = SENSOR_TYPE_LIGHT;
    memset(mPendingEvents[LIGHT].data, 0, sizeof(mPendingEvents[LIGHT].data));

    mEnabled[PROX] = false;
    mPendingEvents[PROX].version = sizeof(sensors_event_t);
    mPendingEvents[PROX].sensor = ID_P;
    mPendingEvents[PROX].type = SENSOR_TYPE_PROXIMITY;
    memset(mPendingEvents[PROX].data, 0, sizeof(mPendingEvents[PROX].data));

    mPendingEventsFlush.version = META_DATA_VERSION;
    mPendingEventsFlush.sensor = 0;
    mPendingEventsFlush.type = SENSOR_TYPE_META_DATA;
    mPendingEventsFlush.reserved0 = 0;
    mPendingEventsFlush.timestamp = 0;
}

LightProxSensor::~LightProxSensor()
{
    if (mEnabled[LIGHT])
        enable(ID_L, 0);

    if (mEnabled[PROX])
        enable(ID_P, 0);
}

int LightProxSensor::setDelay(int32_t handle, int64_t ns)
{
    switch (handle) {
    case ID_L:
        ALOGV("Light: delay=%lld ns", ns);
        break;
    case ID_P:
        ALOGV("Proximity: delay=%lld ns", ns);
        /*
         * NOTE: this function is used by batch() to set the delay
         * of the sensor. If we return an error the sensor will be
         * disabled, so let's pretend everything is fine.
         */
        return 0;
    default:
        ALOGE("LightProxSensor: unknown handle %d", handle);
        return -EINVAL;
    }

    int fd = open("/sys/module/ct406/parameters/als_delay", O_RDWR);
    if (fd < 0) {
        ALOGE("Light: could not open sysfs file: %d", fd);
        return fd;
    }
    char buf[80];
    sprintf(buf, "%lld", ns / 1000000);
    write(fd, buf, strlen(buf)+1);
    close(fd);

    return 0;
}

void LightProxSensor::setProxInitialState()
{
    struct input_absinfo absinfo;
    if (!ioctl(data_fd, EVIOCGABS(EVENT_TYPE_PROXIMITY), &absinfo)) {
        // make sure to report an event immediately
        mProxHasPendingEvent = true;
        mPendingEvents[PROX].distance = absinfo.value * ADJUST_PROX;
    }
}

int LightProxSensor::enable(int32_t handle, int en)
{
    char enable_path[PATH_MAX];
    bool enable = !!en;
    int sensor;
    int fd;

    switch (handle) {
    case ID_L:
        ALOGV("Light: enable=%d", en);
        sensor = LIGHT;
        strcpy(enable_path, "/sys/module/ct406/parameters/als_enable");
        break;
    case ID_P:
        ALOGV("Proximity: enable=%d", en);
        sensor = PROX;
        strcpy(enable_path, "/sys/module/ct406/parameters/prox_enable");
        break;
    default:
        ALOGE("LightProxSensor: unknown handle %d", handle);
        return -EINVAL;
    }

    if (enable == mEnabled[sensor])
        return 0;

    fd = open(enable_path, O_RDWR);
    if (fd >= 0) {
        int ret = write(fd, enable ? "1" : "0", 1);
        if (ret < 0) {
            ALOGE("LightProx: could not set state of handle %d to %d",
                  handle, en);
            return ret;
        }
        close(fd);
        mEnabled[sensor] = enable;
        if (sensor == PROX)
            setProxInitialState();
        return 0;
    }

    return fd;
}

bool LightProxSensor::hasPendingEvents() const
{
    return mProxHasPendingEvent;
}

int LightProxSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    if (mProxHasPendingEvent) {
        mProxHasPendingEvent = false;
        mPendingEvents[PROX].timestamp = getTimestamp();
        *data = mPendingEvents[PROX];
        return mEnabled[PROX] ? 1 : 0;
    }

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_LED) {
            if (event->code == EVENT_TYPE_LIGHT) {
                mPendingEventsMask |= 1 << LIGHT;
                mPendingEvents[LIGHT].light = event->value;
            } else {
                ALOGE("Light: unknown event (type=%d, code=%d)",
                        type, event->code);
            }
        } else if (type == EV_MSC) {
            if (event->code == EVENT_TYPE_PROXIMITY) {
                mPendingEventsMask |= 1 << PROX;
                mPendingEvents[PROX].distance = event->value * ADJUST_PROX;
            } else {
                ALOGE("Proximity: unknown event (type=%d, code=%d)",
                        type, event->code);
            }
        } else if (type == EV_SYN) {
            for (int i = 0; count && mPendingEventsMask && i < NUM_SENSORS; i++) {
                if (mPendingEventsMask & (1 << i)) {
                    mPendingEventsMask &= ~(1 << i);
                    mPendingEvents[i].timestamp = timevalToNano(event->time);
                    if (mEnabled[i]) {
                        *data++ = mPendingEvents[i];
                        count--;
                        numEventReceived++;
                    }

                    if (mPendingEventsFlushMask & (1 << i)) {
                        mPendingEventsFlushMask &= ~(1 << i);
                        mPendingEventsFlush.meta_data.what = META_DATA_FLUSH_COMPLETE;
                        mPendingEventsFlush.meta_data.sensor = i;
                        *data++ = mPendingEventsFlush;
                        count--;
                        numEventReceived++;
                     }
                }
            }
        } else {
            ALOGE("LightProxSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}

int LightProxSensor::flush(int handle)
{
    int id;

    switch (handle) {
    case ID_L:
        id = LIGHT;
        break;
    case ID_P:
        id = PROX;
        break;
    default:
        ALOGE("LightProxSensor: unknown handle %d", handle);
        return -EINVAL;
    }

    if (!mEnabled[id])
        return -EINVAL;

    mPendingEventsFlushMask |= 1 << id;

    return 0;
}
