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

#include "AccelerometerSensor.h"

AccelerometerSensor::AccelerometerSensor()
    : SensorBase("/dev/lis3dh", "accelerometer"),
      mEnabled(0),
      mOriEnabled(false),
      mInputReader(6),
      mPendingEventsMask(0),
      mPendingEventsFlushMask(0)
{
    mPendingEvents[ACC].version = sizeof(sensors_event_t);
    mPendingEvents[ACC].sensor = ID_A;
    mPendingEvents[ACC].type = SENSOR_TYPE_ACCELEROMETER;
    memset(mPendingEvents[ACC].data, 0, sizeof(mPendingEvents[ACC].data));

    mPendingEvents[SO].version = sizeof(sensors_event_t);
    mPendingEvents[SO].sensor = ID_SO;
    mPendingEvents[SO].type = SENSOR_TYPE_SCREEN_ORIENTATION;
    memset(mPendingEvents[SO].data, 0, sizeof(mPendingEvents[SO].data));

    mPendingEventsFlush.version = META_DATA_VERSION;
    mPendingEventsFlush.sensor = 0;
    mPendingEventsFlush.type = SENSOR_TYPE_META_DATA;
    mPendingEventsFlush.reserved0 = 0;
    mPendingEventsFlush.timestamp = 0;
}

AccelerometerSensor::~AccelerometerSensor()
{
    if (mEnabled & MODE_ACCEL)
        enable(ID_A, 0);

    if (mEnabled & MODE_ROTATE)
        enable(ID_SO, 0);

    if (mOriEnabled)
        enable(ID_O, 0);
}

int AccelerometerSensor::enable(int32_t handle, int en)
{
    uint32_t enable = en ? 1 : 0;
    uint32_t flag = mEnabled;
    int mask;

    switch (handle) {
    case ID_A:
        ALOGV("Accelerometer (ACC): enable=%d", en);
        mask = MODE_ACCEL;
        break;
    case ID_SO:
        ALOGV("Accelerometer (SO): enable=%d", en);
        mask = MODE_ROTATE;
        break;
    case ID_O:
        ALOGV("Accelerometer (ORI): enable=%d", en);
        mOriEnabled = !!en;
        /* We are not enabling/disabling an actual sensor */
        return 0;
    default:
        ALOGE("Accelerometer: unknown handle %d", handle);
        return -1;
    }

    if ((mEnabled & mask) == enable)
        return 0;

    if (enable)
        flag |= mask;
    else
        flag &= ~mask;

    open_device();
    int ret = ioctl(dev_fd, LIS3DH_IOCTL_SET_ENABLE, &flag);
    close_device();
    if (ret < 0) {
        ALOGE("Accelerometer: could not change sensor state");
        return ret;
    }
    mEnabled = flag;

    return 0;
}

void AccelerometerSensor::writeAkmAccel(int16_t x, int16_t y, int16_t z)
{
    int16_t buf[3];
    int ret;
    int fd;

    buf[0] = x;
    buf[1] = y;
    buf[2] = z;

    fd = open("/sys/devices/virtual/compass/akm8963/accel", O_WRONLY);
    if (fd < 0)
        ALOGE("Accelerometer: could not open accel file");

    ret = write(fd, buf, sizeof(buf));
    if (ret < 0)
        ALOGE("Accelerometer: could not write accel data");

    close(fd);
}

int AccelerometerSensor::setDelay(int32_t handle, int64_t ns)
{
    int delay = ns / 1000000;
    int ret;

    switch (handle) {
    case ID_A:
        ALOGV("Accelerometer (ACC): delay=%lld", ns);
        break;
    case ID_SO:
        ALOGV("Accelerometer (SO): ignoring delay=%lld", ns);
        return 0;
    }

    if (delay > ACC_MAX_DELAY_MS)
        delay = ACC_MAX_DELAY_MS;

    open_device();
    ret = ioctl(dev_fd, LIS3DH_IOCTL_SET_DELAY, &delay);
    close_device();
    if (ret < 0)
        ALOGE("Accelerometer: could not set delay: %d", ret);

    return ret;
}

int AccelerometerSensor::readEvents(sensors_event_t* data, int count)
{
    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    float accData[3] = { 0 };
    bool isAcc = false;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            isAcc = true;
            float value = event->value;
            if (event->code == EVENT_TYPE_ACCEL_X) {
                mPendingEventsMask |= 1 << ACC;
                accData[0] = value;
                mPendingEvents[ACC].acceleration.x = value * CONVERT_A_X;
            } else if (event->code == EVENT_TYPE_ACCEL_Y) {
                mPendingEventsMask |= 1 << ACC;
                accData[1] = value;
                mPendingEvents[ACC].acceleration.y = value * CONVERT_A_Y;
            } else if (event->code == EVENT_TYPE_ACCEL_Z) {
                mPendingEventsMask |= 1 << ACC;
                accData[2] = value;
                mPendingEvents[ACC].acceleration.z = value * CONVERT_A_Z;
            } else {
                ALOGE("Accelerometer: unknown event (type=%d, code=%d)",
                        type, event->code);
            }
        } else if (type == EV_MSC) {
            if (event->code == EVENT_TYPE_SO) {
                mPendingEventsMask |= 1 << SO;
                mPendingEvents[SO].data[0] = event->value;
            } else {
                ALOGE("Accelerometer: unknown event (type=%d, code=%d)",
                        type, event->code);
            }
        } else if (type == EV_SYN) {
            for (int i = 0; count && mPendingEventsMask && i < NUM_SENSORS; i++) {
                if (mPendingEventsMask & (1 << i)) {
                    mPendingEventsMask &= ~(1 << i);
                    mPendingEvents[i].timestamp = timevalToNano(event->time);
                    if (mEnabled & indexToMask(i)) {
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
            ALOGE("Accelerometer: unknown event (type=%d, code=%d)",
                    type, event->code);
        }
        mInputReader.next();
    }

    if (mOriEnabled && isAcc)
        writeAkmAccel(accData[0] * CONVERT_ACC_X,
                      accData[1] * CONVERT_ACC_Y,
                      accData[2] * CONVERT_ACC_Z);

    return numEventReceived;
}

int AccelerometerSensor::flush(int handle)
{
    int id;

    switch (handle) {
    case ID_A:
        id = ACC;
        break;
    case ID_SO:
        id = SO;
        break;
    default:
        ALOGE("Accelerometer: unknown handle %d", handle);
        return -EINVAL;
    }

    if (!(mEnabled & indexToMask(id)))
        return -EINVAL;

    mPendingEventsFlushMask |= 1 << id;

    return 0;
}
