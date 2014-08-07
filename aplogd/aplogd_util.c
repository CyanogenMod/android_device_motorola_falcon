/********************************************************************
 * Copyright (C) 2009 Motorola, All Rights Reserved
 *
 * File Name: aplogd_util.c
 *
 * General Description: This file provides the utilily function of the aplogd daemon.
 *
 * This program is licensed under a BSD license with the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    o Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    o Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *    o Neither the name of Motorola nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *********************************************************************/

/***********************************************************
 * Local Includes
 ************************************************************/
/* C Standard Includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <log/logger.h>
#include <log/logd.h>
#include <log/logprint.h>
#include <log/event_tag_map.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>

#include <dirent.h>
#include <zlib.h>
#include <sys/time.h>
#include <sys/mman.h>

/* Motorola Includes */
/* Removed in LJ-OMAP bring-up phase */
/* #include <mot_security.h> */

/* Aplogd Headers */
#include "aplogd.h"
#include "log_io.h"
#include "rambuf.h"
#include "aplogd_util.h"
/***********************************************************
 * Local Constants
 ***********************************************************/
int aplogd_util_gzip(char *src)
{
	int ret;
	char cmd[MAX_PATH_LEN+9] = "gzip -5 ";
	strcpy(cmd+8, src);
	ret = system(cmd);
	/* gzip (specifically, minigzip, part of zlib), blindly creates all gzip
	 * output files with 0666 permissions.  So, we need to undo that damage. */
	char gzip_file[MAX_PATH_LEN];
	snprintf(gzip_file, MAX_PATH_LEN, "%s.gz", src);
	/* See comment on permissions in aplogd_output_setup(). */
	chmod(gzip_file, S_IRUSR | S_IWUSR | S_IRGRP);
	return ret;
}

int aplogd_util_move(char *src, char *dst, char *pstr)
{
	struct dirent *dirp;
	DIR *dp;
	char file_name[MAX_PATH_LEN];
	char bak_name[MAX_PATH_LEN];
	int ret=0;
	DPRINT("In aplogd_io_move:src=%s, dst=%s,str=%s\n",src,dst,pstr);
	if((dp=opendir(src))==NULL){
		EPRINT("Error in opendir.\n");
		return -1;
		}
	while ((dirp=readdir(dp))!=NULL){
		if(strcmp(dirp->d_name, ".") ==0 ||
		strcmp(dirp->d_name, "..") == 0)
			continue;
		if(strstr(dirp->d_name, pstr)==NULL)
			continue;
		snprintf(file_name,MAX_PATH_LEN,"%s/%s",src,dirp->d_name);
		snprintf(bak_name,MAX_PATH_LEN,"%s/%s", dst, dirp->d_name);
		if(rename(file_name, bak_name)<0){
			DPRINT("Cannot move file %s.\n",file_name);
			unlink(file_name);
			ret = -1;
		}
	}
	closedir(dp);
	return ret;
}

static int aplogd_remove_dir(const char *dir)
{
	struct dirent *dirp;
	char file_name[MAX_PATH_LEN];
	struct stat buf;
	DIR *dp;
	if (!dir)
		return -1;
	if ((dp=opendir(dir)) == NULL) {
		EPRINT("Error in opendir.\n");
		return -1;
	}
	while ((dirp=readdir(dp)) != NULL) {
		if(strcmp(dirp->d_name, ".") ==0 ||
		strcmp(dirp->d_name, "..") == 0)
			continue;
		snprintf(file_name,MAX_PATH_LEN,"%s/%s",dir,dirp->d_name);
		if (lstat(file_name, &buf) == -1) {
			EPRINT("Error in lstat. Errno = %d(%s)\n", errno, strerror(errno));
			closedir(dp);
			return -1;
		}

		if (S_ISDIR(buf.st_mode))
			aplogd_remove_dir(file_name);
		else
			unlink(file_name);
	}
	closedir(dp);
	rmdir(dir);
	return 0;
}

void filter_numstr(const char *str, char *numstr)
{
	const char *t_str = str;
	char *n_str = numstr;
	int c = 0;
	int f_head = 0;

	while (*t_str) {
		c = (unsigned char) *t_str++;
		if (!(isdigit(c)) && !f_head)
			continue;

		if (isdigit(c)) {
			f_head = 1;
			*n_str++ = (unsigned char)c;
		} else
			break;
	}
	*n_str = '\0';
}

long aplogd_util_cleardir(char *dir, int max_count)
{
	struct dirent *dirp;
	DIR *dp;
	long ret = 0;
	struct stat buf;
	int count;
	char path[MAX_PATH_LEN];
	char oldest[MAX_PATH_LEN];
	char d_name[MAX_PATH_LEN];
	long t_num;
	long dir_num;

	do {
		count = 0;
		memset(path, 0, MAX_PATH_LEN);
		memset(oldest, 0, MAX_PATH_LEN);
		t_num = LONG_MAX;
		dp = NULL;
		dirp = NULL;

		if (!(dp = opendir(dir))) {
			EPRINT("Error in opendir.\n");
			return -1;
		}
		while ((dirp=readdir(dp)) != NULL) {
			if(strcmp(dirp->d_name, ".") ==0 ||
			strcmp(dirp->d_name, "..") == 0 ||
			!strstr(dirp->d_name, "No."))
				continue;
			snprintf(path, MAX_PATH_LEN,"%s/%s", dir, dirp->d_name);
			if(stat(path, &buf) == -1) {
				EPRINT("Error in stat. Errno = %d(%s)\n", errno, strerror(errno));
				closedir(dp);
				return -1;
			}
			if(S_ISDIR(buf.st_mode)){
				count++;
				memset(d_name, 0, MAX_PATH_LEN);
				filter_numstr(dirp->d_name, d_name);
				dir_num = strtol(d_name, NULL, 10);
				if (dir_num < t_num) {
					strcpy(oldest, path);
					t_num = dir_num;
				}
				if (dir_num > ret)
					ret = dir_num;
			}
		}
		closedir(dp);
		DPRINT("count=%d max_count=%d oldest=%s ret=%ld\n", count, max_count, oldest, ret);
		if (count > (max_count -1) && oldest[0]) {
			count--;
			if (aplogd_remove_dir(oldest) == -1)
				return -1;
		}
	} while (count > (max_count - 1));

	return ret;
}

/*aplogd_move_logs()
*
*Description: This function moves the log file storage locations
*
*/
void aplogd_move_logs(STORAGE_T old_storage,STORAGE_T new_storage)
{
        if (!g_output_path[old_storage] || !g_output_path[new_storage])
            return;
        int i;
        char src[MAX_PATH_LEN];
        char dst[MAX_PATH_LEN];
        char buf[4096];
        int sfd,dfd;
        int ret;
        for (i=0;i<=APLOGD_INPUT_LAST;i++){
                snprintf(src, MAX_PATH_LEN, "%s/%s", g_output_path[old_storage], output_filename[i]);
                snprintf(dst, MAX_PATH_LEN, "%s/%s", g_output_path[new_storage], output_filename[i]);
                if((sfd=open(src, O_RDONLY |O_NONBLOCK ))<0)
                        continue;
                if((dfd=open(dst, O_WRONLY |O_NONBLOCK |O_APPEND | O_CREAT, 0755))<0)
                        continue;
                while((ret = read(sfd,buf,4095))>0){
			if(ret>4095)ret=4095;
                        write(dfd,buf,ret);
		}
                close(sfd);
                close(dfd);
                unlink(src);
        }
}

unsigned int aplogd_get_filesize(STORAGE_T storage)
{
        DPRINT("Enter aplogd_get_filesize.\n");
	int size;
        switch (storage){
                case STORAGE_SECONDARY:
                case STORAGE_EXTERNAL:
			size=usr_cfg_size;
			if(size <= 0 || size > 1000)/*The default size is 50M,while we allow users to set it to 1G*/
				size=CONFIG_LOGFILE_MAX_VALUE_EXTERNAL;
			else
				size=size*1024*1024;
                        return size;
                case STORAGE_USERDATA:
                        return CONFIG_LOGFILE_MAX_VALUE_USERDATA;
                default:
                        return CONFIG_LOGFILE_MAX_VALUE_EXTERNAL;
        }
}

STORAGE_T aplogd_calc_storage_pref()
{
        if (0 == usr_cfg_ext)
            return STORAGE_USERDATA;
        else
            return STORAGE_SECONDARY;
}

int aplogd_is_collected(char stream)
{
    return (NULL == strchr(usr_cfg_collect, stream)) ? 0 : 1;
}
