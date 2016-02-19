/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <cutils/klog.h>

#define TAG "detect-gpe"

/* system/vold/cryptfs.h */
#define CRYPT_MNT_MAGIC 0xD0B5B1C4
#define CRYPT_FOOTER_OFFSET 0x4000

#define METADATA_BLOCK "/dev/block/platform/msm_sdcc.1/by-name/metadata"
#define USERDATA_BLOCK "/dev/block/platform/msm_sdcc.1/by-name/userdata"

#define GPE_FSTAB "/gpe-fstab.qcom"
#define FSTAB "/fstab.qcom"

int main()
{
	uint32_t magic;
	int ret;
	int fd;

	/* Not a GPE device, nothing to do here */
	if (access(METADATA_BLOCK, F_OK) == -1)
		return 0;

	fd = open(USERDATA_BLOCK, O_RDONLY);
	if (fd == -1) {
		KLOG_ERROR(TAG, "Could not open %s: %d\n",
			USERDATA_BLOCK, errno);
		return -errno;
	}

	/* The magic is stored in the first 32bit of the footer */
	if (lseek64(fd, -CRYPT_FOOTER_OFFSET, SEEK_END) < 0) {
		KLOG_ERROR(TAG, "Could not seek %s: %d\n",
			USERDATA_BLOCK, errno);
		ret = -errno;
		goto out;
	}

	ret = read(fd, &magic, sizeof(magic));
	if (ret < 0) {
		KLOG_ERROR(TAG, "Could not read magic: %d\n", errno);
		ret = -errno;
		goto out;
	}

	if (magic == CRYPT_MNT_MAGIC) {
		/*
		 * This is a GPE device, but it's encrypted and the metadata
		 * are in the footer. Don't swap the fstab to let it boot.
		 */
		KLOG_INFO(TAG, "Encrypted GPE with metadata in the footer\n");
		ret = 0;
	} else {
		ret = rename(GPE_FSTAB, FSTAB);
		KLOG_INFO(TAG, "GPE device detected\n");
		if (ret) {
			KLOG_ERROR(TAG, "Could not replace fstab: %d\n", errno);
			ret = -errno;
		}
	}

out:
	close(fd);
	return ret;
}
