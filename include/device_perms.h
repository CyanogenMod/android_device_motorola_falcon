/*
 * Copyright (C) 2014 The CyanogenMod Project
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

#ifndef DEVICE_PERMS_H
#define DEVICE_PERMS_H

#define PROPERTY_PERMS_APPEND \
    { "persist.audio.",   AID_SYSTEM,   0 }, \
    { "tcmd.",            AID_MOT_TCMD, AID_MOT_WHISPER }, \
    { "hw.whisper",       AID_MOT_WHISPER, 0 }, \
    { "sys.media.",       AID_RADIO, 0 }, \
    { "persist.atvc.",    AID_MOT_ATVC,  0 }, \
    { "persist.atvc.",    AID_SYSTEM, 0 }, \
    { "hw.aov.",          AID_AUDIO,    0 }, \
    { "persist.log.",     AID_SHELL,    0 }, \
    { "persist.log.",     AID_LOG,      0 }, \
    { "persist.mot.proximity.", AID_RADIO, 0 }, \
    { "persist.sprint.dm", AID_SPRINT_EXTENSION, 0}, \
    { "persist.tcmd.", AID_MOT_TCMD, AID_SYSTEM }, \
    { "persist.camera.", AID_MEDIA, 0 },

#define CONTROL_PERMS_APPEND \
    { "mmi-ril-daemon",AID_RADIO, AID_RADIO }, \
    { "ril-daemon1",AID_RADIO, AID_RADIO }, \
    { "ril-daemon2",AID_RADIO, AID_RADIO },

#endif /* DEVICE_PERMS_H */
