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

#include "CompOriSensor.h"


CompOriSensor::CompOriSensor()
    : SensorBase(NULL, "compass"),
      mInputReader(10),
      mPendingEventsMask(0),
      mPendingEventsFlushMask(0)
{
    mEnabled[MAG] = false;
    mPendingEvents[MAG].version = sizeof(sensors_event_t);
    mPendingEvents[MAG].sensor = ID_M;
    mPendingEvents[MAG].type = SENSOR_TYPE_MAGNETIC_FIELD;
    mPendingEvents[MAG].magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;
    memset(mPendingEvents[MAG].data, 0, sizeof(mPendingEvents[MAG].data));

    mEnabled[ORI] = false;
    mPendingEvents[ORI].version = sizeof(sensors_event_t);
    mPendingEvents[ORI].sensor = ID_O;
    mPendingEvents[ORI].type = SENSOR_TYPE_ORIENTATION;
    mPendingEvents[ORI].magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;
    memset(mPendingEvents[ORI].data, 0, sizeof(mPendingEvents[ORI].data));

    mPendingEventsFlush.version = META_DATA_VERSION;
    mPendingEventsFlush.sensor = 0;
    mPendingEventsFlush.type = SENSOR_TYPE_META_DATA;
    mPendingEventsFlush.reserved0 = 0;
    mPendingEventsFlush.timestamp = 0;
}

CompOriSensor::~CompOriSensor()
{
    if (mEnabled[MAG])
        enable(ID_M, 0);

    if (mEnabled[ORI])
        enable(ID_O, 0);
}

int CompOriSensor::enable(int32_t handle, int en)
{
    char enable_path[PATH_MAX];
    bool enable = !!en;
    int sensor;
    int fd;

    switch (handle) {
    case ID_M:
        ALOGV("Compass: enable=%d", en);
        sensor = MAG;
        strcpy(enable_path, "/sys/class/compass/akm8963/enable_mag");
        break;
    case ID_O:
        ALOGV("Orientation: enable=%d", en);
        sensor = ORI;
        strcpy(enable_path, "/sys/class/compass/akm8963/enable_ori");
        break;
    default:
        ALOGE("CompOriSensor: unknown handle %d", handle);
        return -EINVAL;
    }

    if (enable == mEnabled[sensor])
        return 0;

    fd = open(enable_path, O_RDWR);
    if (fd < 0) {
        ALOGE("CompOriSensor: could not open %s: %d", enable_path, fd);
        return fd;
    }
    int ret = write(fd, enable ? "1" : "0", 1);
    if (ret < 0) {
        ALOGE("CompOriSensor: could not set state of handle %d to %d",
              handle, en);
        return ret;
    }
    close(fd);
    mEnabled[sensor] = enable;

    return 0;
}

int CompOriSensor::setDelay(int32_t handle, int64_t ns)
{
    char delay_path[PATH_MAX];
    int fd;

    switch (handle) {
    case ID_M:
        ALOGV("Compass: delay=%lld ns", ns);
        strcpy(delay_path, "/sys/class/compass/akm8963/delay_mag");
        break;
    case ID_O:
        ALOGV("Orientation: delay=%lld ns", ns);
        strcpy(delay_path, "/sys/class/compass/akm8963/delay_ori");
        break;
    default:
        ALOGE("CompOriSensor: unknown handle %d", handle);
        return -EINVAL;
    }

    fd = open(delay_path, O_RDWR);
    if (fd >= 0) {
        char buf[80];
        int ret;
        sprintf(buf, "%lld", ns);
        ret = write(fd, buf, strlen(buf)+1);
        if (ret < 0) {
            ALOGE("CompOriSensor: could not set delay of handle %d to %lld",
                  handle, ns);
            return ret;
        }
        close(fd);
        return 0;
    }
    return fd;
}

int CompOriSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            float value = event->value;
            switch (event->code) {

            /* Compass */
            case EVENT_TYPE_MAGV_X:
                mPendingEventsMask |= 1 << MAG;
                mPendingEvents[MAG].magnetic.x = value * CONVERT_M_X;
                break;
            case EVENT_TYPE_MAGV_Y:
                mPendingEventsMask |= 1 << MAG;
                mPendingEvents[MAG].magnetic.y = value * CONVERT_M_Y;
                break;
            case EVENT_TYPE_MAGV_Z:
                mPendingEventsMask |= 1 << MAG;
                mPendingEvents[MAG].magnetic.z = value * CONVERT_M_Z;
                break;
            case EVENT_TYPE_MAGV_STATUS:
                mPendingEventsMask |= 1 << MAG;
                mPendingEvents[MAG].magnetic.status = value;
                break;

            /* Orientation */
            case EVENT_TYPE_YAW:
                mPendingEventsMask |= 1 << ORI;
                mPendingEvents[ORI].orientation.azimuth = value * CONVERT_O_A;
                break;
            case EVENT_TYPE_PITCH:
                mPendingEventsMask |= 1 << ORI;
                mPendingEvents[ORI].orientation.pitch = value * CONVERT_O_P;
                break;
            case EVENT_TYPE_ROLL:
                mPendingEventsMask |= 1 << ORI;
                mPendingEvents[ORI].orientation.roll = value * CONVERT_O_R;
                break;
            case EVENT_TYPE_ORIENT_STATUS:
                mPendingEventsMask |= 1 << ORI;
                mPendingEvents[ORI].orientation.status = value;
                break;

            default:
                ALOGE("CompOriSensor: unknown event (type=%d, code=%d)",
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
            ALOGE("CompOriSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    return numEventReceived;
}

int CompOriSensor::flush(int handle)
{
    int id;

    switch (handle) {
    case ID_M:
        id = MAG;
        break;
    case ID_O:
        id = ORI;
        break;
    default:
        ALOGE("CompOriSensor: unknown handle %d", handle);
        return -EINVAL;
    }

    if (!mEnabled[id])
        return -EINVAL;

    mPendingEventsFlushMask |= 1 << id;

    return 0;
}
