# Copyright (C) 2016 The CyanogenMod Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import re
import os

TARGET_DIR = os.getenv('OUT')


def FullOTA_Assertions(info):
  AddBootloaderAssertion(info, info.input_zip)
  AddFirmwareAssertion(info, info.output_zip)


def FullOTA_PostValidate(info):
  ReplaceApnList(info)


def IncrementalOTA_Assertions(info):
  AddBootloaderAssertion(info, info.target_zip)
  AddFirmwareAssertion(info, info.output_zip)


def IncrementalOTA_PostValidate(info):
  ReplaceApnList(info)


def AddBootloaderAssertion(info, input_zip):
  android_info = input_zip.read("OTA/android-info.txt")
  m = re.search(r"require\s+version-bootloader\s*=\s*(\S+)", android_info)
  if m:
    bootloaders = m.group(1).split("|")
    if "*" not in bootloaders:
      info.script.AssertSomeBootloader(*bootloaders)
    info.metadata["pre-bootloader"] = m.group(1)

def AddFirmwareAssertion(info, output_zip):
  output_zip.write(os.path.join(TARGET_DIR, "check-firmware-files.sh"), "check-firmware-files.sh")
  info.script.AppendExtra('package_extract_file("check-firmware-files.sh", "/tmp/check-firmware-files.sh");')
  info.script.SetPermissions("/tmp/check-firmware-files.sh", 0, 0, 0755, None, None);
  info.script.AppendExtra('run_program("/tmp/check-firmware-files.sh") == 0' +
                          ' || abort("Outdated radio/baseband, please update it to continue.");');

def ReplaceApnList(info):
  info.script.AppendExtra('if getprop("ro.boot.radio") == "0x3" then')
  info.script.Mount("/system")
  info.script.AppendExtra('delete("/system/etc/apns-conf.xml");')
  info.script.AppendExtra('symlink("/system/etc/apns-conf-cdma.xml", "/system/etc/apns-conf.xml");')
  info.script.Unmount("/system")
  info.script.AppendExtra('endif;')
