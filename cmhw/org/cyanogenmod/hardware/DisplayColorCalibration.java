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

package org.cyanogenmod.hardware;

import org.cyanogenmod.hardware.util.FileUtils;

import java.io.File;

public class DisplayColorCalibration {

    private static final String KCAL_TUNING_FILE = "/sys/devices/platform/kcal_ctrl.0/kcal";
    private static final String KCAL_CTRL_FILE = "/sys/devices/platform/kcal_ctrl.0/kcal_ctrl";

    public static boolean isSupported() {
        File file = new File(KCAL_TUNING_FILE);
        return file.exists();
    }

    public static int getMaxValue() {
        return 255;
    }

    public static int getMinValue() {
        return 0;
    }

    public static int getDefValue() {
        return 255;
    }

    public static String getCurColors() {
        return FileUtils.readOneLine(KCAL_TUNING_FILE);
    }

    public static boolean setColors(String colors) {
        if (!FileUtils.writeLine(KCAL_TUNING_FILE, colors)) {
            return false;
        }
        return FileUtils.writeLine(KCAL_CTRL_FILE, "1");
    }
}
