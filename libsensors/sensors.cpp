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

#include <hardware/sensors.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <utils/Log.h>

#include "sensors.h"
#include "LightProxSensor.h"
#include "AccelerometerSensor.h"
#include "CompOriSensor.h"

/*****************************************************************************/

#define SENSORS_ACCELERATION_HANDLE       (ID_A)
#define SENSORS_MAGNETIC_FIELD_HANDLE     (ID_M)
#define SENSORS_ORIENTATION_HANDLE        (ID_O)
#define SENSORS_LIGHT_HANDLE              (ID_L)
#define SENSORS_PROXIMITY_HANDLE          (ID_P)
#define SENSORS_SCREEN_ORIENTATION_HANDLE (ID_SO)

/*****************************************************************************/

static struct sensor_t sSensorList[] = {
    {
        .name = "LIS3DH 3-axis Accelerometer",
        .vendor = "ST Micro",
        .version = 1,
        .handle = SENSORS_ACCELERATION_HANDLE,
        .type = SENSOR_TYPE_ACCELEROMETER,
        .maxRange = 39.2400016784668f,
        .resolution = 0.009810000658035278f,
        .power = 0.25f,
        .minDelay = 10000,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .stringType = SENSOR_STRING_TYPE_ACCELEROMETER,
        .requiredPermission = 0,
        .maxDelay = (ACC_MAX_DELAY_MS * 1000),
        .flags = SENSOR_FLAG_CONTINUOUS_MODE,
        .reserved = {}
    },
    {
        .name = "AK8963 3-axis Magnetic field sensor",
        .vendor = "Asahi Kasei",
        .version = 1,
        .handle = SENSORS_MAGNETIC_FIELD_HANDLE,
        .type = SENSOR_TYPE_MAGNETIC_FIELD,
        .maxRange = 20000.0f,
        .resolution = 0.0625f,
        .power = 6.800000190734863f,
        .minDelay = 20000,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .stringType = SENSOR_STRING_TYPE_MAGNETIC_FIELD,
        .requiredPermission = 0,
        .maxDelay = 0,
        .flags = SENSOR_FLAG_CONTINUOUS_MODE,
        .reserved = {}
    },
    {
        .name = "AK8963 Orientation sensor",
        .vendor = "Asahi Kasei",
        .version = 1,
        .handle = SENSORS_ORIENTATION_HANDLE,
        .type = SENSOR_TYPE_ORIENTATION,
        .maxRange = 360.0f,
        .resolution = 0.015625f,
        .power = (6.800000190734863f + 0.25f),
        .minDelay = 10000,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .stringType = SENSOR_STRING_TYPE_ORIENTATION,
        .requiredPermission = 0,
        .maxDelay = 0,
        .flags = SENSOR_FLAG_CONTINUOUS_MODE,
        .reserved = {}
    },
    {
        .name = "CT406 Light sensor",
        .vendor = "TAOS",
        .version = 1,
        .handle = SENSORS_LIGHT_HANDLE,
        .type = SENSOR_TYPE_LIGHT,
        .maxRange = 27000.0f,
        .resolution = 1.0f,
        .power = 0.17499999701976776f,
        .minDelay = 20000,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .stringType = SENSOR_STRING_TYPE_LIGHT,
        .requiredPermission = 0,
        .maxDelay = 0,
        .flags = SENSOR_FLAG_ON_CHANGE_MODE,
        .reserved = {}
    },
    {
        .name = "CT406 Proximity sensor",
        .vendor = "TAOS",
        .version = 1,
        .handle = SENSORS_PROXIMITY_HANDLE,
        .type = SENSOR_TYPE_PROXIMITY,
        .maxRange = 100.0f,
        .resolution = 100.0f,
        .power = 3.0f,
        .minDelay = 0,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .stringType = SENSOR_STRING_TYPE_PROXIMITY,
        .requiredPermission = 0,
        .maxDelay = 0,
        .flags = SENSOR_FLAG_WAKE_UP | SENSOR_FLAG_ON_CHANGE_MODE,
        .reserved = {}
    },
    {
        .name = "Display Rotation sensor",
        .vendor = "Motorola",
        .handle = SENSORS_SCREEN_ORIENTATION_HANDLE,
        .type = SENSOR_TYPE_SCREEN_ORIENTATION,
        .version = 1,
        .maxRange = 4,
        .resolution = 1.0f,
        .power = 0,
        .minDelay = 0,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .stringType = 0,
        .requiredPermission = 0,
        .maxDelay = 0,
        .flags = SENSOR_FLAG_ON_CHANGE_MODE,
        .reserved = {}
    },
};

static int open_sensors(const struct hw_module_t* module, const char* id,
                        struct hw_device_t** device);

static int sensors__get_sensors_list(struct sensors_module_t*,
                                     struct sensor_t const** list) 
{
        *list = sSensorList;
        return ARRAY_SIZE(sSensorList);
}

static struct hw_module_methods_t sensors_module_methods = {
        .open = open_sensors,
};

struct sensors_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = SENSORS_HARDWARE_MODULE_ID,
        .name = "msm8226 Sensor module",
        .author = "Gabriele M",
        .methods = &sensors_module_methods,
        .dso = NULL,
        .reserved = {0}
    },
    .get_sensors_list = sensors__get_sensors_list,
    .set_operation_mode = NULL,
};

struct sensors_poll_context_t {
    sensors_poll_device_1_t device; // must be first

    sensors_poll_context_t();
    ~sensors_poll_context_t();
    int activate(int handle, int enabled);
    int setDelay(int handle, int64_t ns);
    int pollEvents(sensors_event_t* data, int count);
    int batch(int handle, int flags, int64_t period_ns, int64_t timeout);
    int flush(int handle);

private:
    enum {
        lightProx = 0,
        accel,
        compOri,
        numSensorDrivers,
        numFds,
    };

    static const size_t wake = numFds - 1;
    static const char WAKE_MESSAGE = 'W';
    struct pollfd mPollFds[numFds];
    int mWritePipeFd;
    SensorBase* mSensors[numSensorDrivers];

    bool mAccelActive;
    bool mOriActive;
    int mAccelDelay;
    int mOriDelay;

    int realActivate(int handle, int enabled);

    int handleToDriver(int handle) const {
        switch (handle) {
        case ID_A:
        case ID_SO:
            return accel;
        case ID_M:
        case ID_O:
            return compOri;
        case ID_P:
        case ID_L:
            return lightProx;
        default:
            return -EINVAL;
        }
    }
};

/*****************************************************************************/

sensors_poll_context_t::sensors_poll_context_t()
{
    mSensors[lightProx] = new LightProxSensor();
    mPollFds[lightProx].fd = mSensors[lightProx]->getFd();
    mPollFds[lightProx].events = POLLIN;
    mPollFds[lightProx].revents = 0;

    mSensors[accel] = new AccelerometerSensor();
    mPollFds[accel].fd = mSensors[accel]->getFd();
    mPollFds[accel].events = POLLIN;
    mPollFds[accel].revents = 0;

    mSensors[compOri] = new CompOriSensor();
    mPollFds[compOri].fd = mSensors[compOri]->getFd();
    mPollFds[compOri].events = POLLIN;
    mPollFds[compOri].revents = 0;

    int wakeFds[2];
    int result = pipe(wakeFds);
    ALOGE_IF(result < 0, "error creating wake pipe (%s)", strerror(errno));
    fcntl(wakeFds[0], F_SETFL, O_NONBLOCK);
    fcntl(wakeFds[1], F_SETFL, O_NONBLOCK);
    mWritePipeFd = wakeFds[1];

    mPollFds[wake].fd = wakeFds[0];
    mPollFds[wake].events = POLLIN;
    mPollFds[wake].revents = 0;
}

sensors_poll_context_t::~sensors_poll_context_t()
{
    for (int i = 0 ; i < numSensorDrivers ; i++) {
        delete mSensors[i];
    }
    close(mPollFds[wake].fd);
    close(mWritePipeFd);
}

int sensors_poll_context_t::activate(int handle, int enabled)
{
    int err;

    // Orientation requires accelerometer sensor
    if (handle == ID_O) {
        mOriActive = !!enabled;
        if (!mAccelActive) {
            err = realActivate(ID_A, enabled);
            if (err)
                return err;
        }
        // To conditionally write AKM accel interface
        mSensors[accel]->enable(ID_O, enabled);
        // Restore accelerometer delay if orientation is disabled
        if (!mOriActive && mAccelActive)
            mSensors[accel]->setDelay(ID_A, mAccelDelay);
    } else if (handle == ID_A) {
        mAccelActive = !!enabled;
        // No need to enable or disable if orientation sensor is active
        if (mOriActive)
            return 0;
    }

    return realActivate(handle, enabled);
}

int sensors_poll_context_t::realActivate(int handle, int enabled)
{
    int index = handleToDriver(handle);

    if (index < 0)
        return index;

    int err = mSensors[index]->enable(handle, enabled);
    if (enabled && !err) {
        const char wakeMessage(WAKE_MESSAGE);
        int result = write(mWritePipeFd, &wakeMessage, 1);
        ALOGE_IF(result < 0, "error sending wake message (%s)", strerror(errno));
    }

    return err;
}

int sensors_poll_context_t::setDelay(int handle, int64_t ns)
{
    int index = handleToDriver(handle);
    int ret = 0;

    if (index < 0)
        return index;

    if (handle == ID_A)
        mAccelDelay = ns;
    else if (handle == ID_O)
        mOriDelay = ns;

    // Choose the fastest between accelerometer and orientation
    if (handle == ID_O) {
        if (!mAccelActive) {
            // user enabled only orientation
            // use same delay for both
            ret = mSensors[accel]->setDelay(ID_A, ns);
            ret = mSensors[compOri]->setDelay(ID_O, ns);
        } else {
            // user enabled orientation while accelerometer enabled
            // use lowest delay for both
            int min = (mAccelDelay < ns) ? mAccelDelay : ns;
            ret = mSensors[accel]->setDelay(ID_A, min);
            ret = mSensors[compOri]->setDelay(ID_O, min);
        }
    } else if (handle == ID_A) {
        if (!mOriActive) {
            // user enabled only accelerometer
            ret = mSensors[accel]->setDelay(ID_A, ns);
        } else {
            // user enabled accelerometer while orientation enabled
            // set accelerometer delay if lower than orientation
            if (ns < mOriDelay)
                ret = mSensors[accel]->setDelay(ID_A, ns);
        }
    } else {
        ret = mSensors[index]->setDelay(handle, ns);
    }

    return ret;
}

int sensors_poll_context_t::pollEvents(sensors_event_t* data, int count)
{
    int nbEvents = 0;
    int n = 0;

    do {
        // see if we have some leftover from the last poll()
        for (int i = 0; count && i < numSensorDrivers; i++) {
            SensorBase* const sensor(mSensors[i]);
            if ((mPollFds[i].revents & POLLIN) || (sensor->hasPendingEvents())) {
                int nb = sensor->readEvents(data, count);
                if (nb < count) {
                    // no more data for this sensor
                    mPollFds[i].revents = 0;
                }
                count -= nb;
                nbEvents += nb;
                data += nb;
            }
        }

        if (count) {
            // we still have some room, so try to see if we can get
            // some events immediately or just wait if we don't have
            // anything to return
            do {
                n = poll(mPollFds, numFds, nbEvents ? 0 : -1);
            } while (n < 0 && errno == EINTR);
            if (n < 0) {
                ALOGE("poll() failed (%s)", strerror(errno));
                return -errno;
            }
            if (mPollFds[wake].revents & POLLIN) {
                char msg;
                int result = read(mPollFds[wake].fd, &msg, 1);
                ALOGE_IF(result < 0, "error reading from wake pipe (%s)", strerror(errno));
                ALOGE_IF(msg != WAKE_MESSAGE, "unknown message on wake queue (0x%02x)", int(msg));

                mPollFds[wake].revents = 0;
            }
        }
        // if we have events and space, go read them
    } while (n && count);

    return nbEvents;
}

int sensors_poll_context_t::batch(int handle, int flags, int64_t period_ns,
                                  int64_t timeout)
{
    int index = handleToDriver(handle);

    if (index < 0)
        return index;

    return mSensors[index]->batch(handle, flags, period_ns, timeout);
}

int sensors_poll_context_t::flush(int handle)
{
    int index = handleToDriver(handle);

    if (index < 0)
        return index;

    return mSensors[index]->flush(handle);
}

/*****************************************************************************/

static int poll__close(struct hw_device_t *dev)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    if (ctx) {
        delete ctx;
    }
    return 0;
}

static int poll__activate(struct sensors_poll_device_t *dev,
        int handle, int enabled)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->activate(handle, enabled);
}

static int poll__setDelay(struct sensors_poll_device_t *dev,
        int handle, int64_t ns)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->setDelay(handle, ns);
}

static int poll__poll(struct sensors_poll_device_t *dev,
        sensors_event_t* data, int count)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->pollEvents(data, count);
}

static int poll__batch(struct sensors_poll_device_1 *dev,
                      int handle, int, int64_t period_ns, int64_t)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    /*
     * NOTE: the kernel drivers currently don't support batching,
     * so using setDelay() (now deprecated) is enough.
     */
    return ctx->setDelay(handle, period_ns);
}

static int poll__flush(struct sensors_poll_device_1 *dev,
                      int handle)
{
    sensors_poll_context_t *ctx = (sensors_poll_context_t *)dev;
    return ctx->flush(handle);
}

/*****************************************************************************/

/** Open a new instance of a sensor device using name */
static int open_sensors(const struct hw_module_t* module, const char*,
                        struct hw_device_t** device)
{
    int status = -EINVAL;
    sensors_poll_context_t *dev = new sensors_poll_context_t();

    memset(&dev->device, 0, sizeof(sensors_poll_device_1));

    dev->device.common.tag = HARDWARE_DEVICE_TAG;
    dev->device.common.version  = SENSORS_DEVICE_API_VERSION_1_3;
    dev->device.common.module   = const_cast<hw_module_t*>(module);
    dev->device.common.close    = poll__close;
    dev->device.activate        = poll__activate;
    dev->device.setDelay        = poll__setDelay;
    dev->device.poll            = poll__poll;
    dev->device.batch           = poll__batch;
    dev->device.flush           = poll__flush;

    *device = &dev->device.common;
    status = 0;

    return status;
}
