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

#ifndef ANDROID_SENSORS_H
#define ANDROID_SENSORS_H

#include <hardware/sensors.h>
#include <linux/input.h>

__BEGIN_DECLS

/*****************************************************************************/

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*
 * The screen orientation sensor is not supported by Android.
 * The following value was taken from a prebuilt sensor HAL by Motorola.
 */
#define SENSOR_TYPE_SCREEN_ORIENTATION    (65536)

#define ID_A  (0)
#define ID_M  (1)
#define ID_O  (2)
#define ID_L  (3)
#define ID_P  (4)
#define ID_SO (5)
#define ID_SM (6)

/*****************************************************************************/

/*
 * The SENSORS Module
 */

/*****************************************************************************/

#define ACC_MAX_DELAY               200000

/* Yes, X and Y are swapped */
#define EVENT_TYPE_ACCEL_X          ABS_Y
#define EVENT_TYPE_ACCEL_Y          ABS_X
#define EVENT_TYPE_ACCEL_Z          ABS_Z

#define EVENT_TYPE_SO               MSC_RAW

#define EVENT_TYPE_SM               MSC_GESTURE

#define EVENT_TYPE_MAGV_X           ABS_RX
#define EVENT_TYPE_MAGV_Y           ABS_RY
#define EVENT_TYPE_MAGV_Z           ABS_RZ
#define EVENT_TYPE_MAGV_STATUS      ABS_RUDDER

#define EVENT_TYPE_YAW              ABS_HAT0X
#define EVENT_TYPE_PITCH            ABS_HAT0Y
#define EVENT_TYPE_ROLL             ABS_HAT1X
#define EVENT_TYPE_ORIENT_STATUS    ABS_HAT1Y

#define EVENT_TYPE_PROXIMITY        MSC_RAW
#define EVENT_TYPE_LIGHT            LED_MISC

#define ADJUST_PROX                 0.1f

/* conversion of acceleration data to SI units (m/s^2) */
#define CONVERT_A                   (GRAVITY_EARTH / -1000)
#define CONVERT_A_X                 (CONVERT_A)
#define CONVERT_A_Y                 (CONVERT_A)
#define CONVERT_A_Z                 (CONVERT_A)

/* conversion of acceleration data for AKM accel interface */
#define CONVERT_ACC                 (7200.0f)
#define CONVERT_ACC_X               (CONVERT_ACC)
#define CONVERT_ACC_Y               (CONVERT_ACC)
#define CONVERT_ACC_Z               (CONVERT_ACC)

/* conversion of orientation data to degree units */
#define CONVERT_O                   (1.0f/64.0f)
#define CONVERT_O_A                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (CONVERT_O)

/* conversion of magnetic data to uT units */
#define CONVERT_M                   (0.06f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (CONVERT_M)
#define CONVERT_M_Z                 (CONVERT_M)

/*****************************************************************************/

__END_DECLS

#endif  // ANDROID_SENSORS_H
