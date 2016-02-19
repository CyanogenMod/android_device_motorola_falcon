/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include <cutils/klog.h>

#define TAG "detect-gpe"

#define CRYPT_MNT_MAGIC 0xD0B5B1C4

#define METADATA_BLOCK "/dev/block/platform/msm_sdcc.1/by-name/metadata"
#define USERDATA_BLOCK "/dev/block/platform/msm_sdcc.1/by-name/userdata"
#define FOOTER_SIZE 16384

#define GPE_FSTAB "/gpe-fstab.qcom"
#define FSTAB "/fstab.qcom"

#define FSTAB_DONE "/.fstab_done"

void signal_done(void)
{
	int fd = open(FSTAB_DONE, O_CREAT);
	if (fd == -1) {
		KLOG_ERROR(TAG, "Could not create %s: %d\n",
			FSTAB_DONE, errno);
	} else {
		close(fd);
	}
}

int main()
{
	uint32_t magic;
	off64_t ret;
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
	ret = lseek64(fd, -FOOTER_SIZE, SEEK_END);
	if (ret < 0) {
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
			goto out;
		}
	}
	signal_done();

out:
	close(fd);
	return ret;
}
