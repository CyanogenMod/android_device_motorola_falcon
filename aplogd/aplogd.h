/********************************************************************
 * Copyright (C) 2009 Motorola, All Rights Reserved
 *
 * File Name: aplogd.h
 *
 * General Description: This file provides portions of the aplogging
 * daemon to other C files.
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
 * Revision History
 *   Date          Author         Description
 * ----------    --------------  ------------------------------------------
 * 02/19/2009    Motorola        Initial creation
 *
 *********************************************************************/

#ifndef _APLOGD_H_
#define _APLOGD_H_
#define LOG_TAG				"aplogd"
#define MAX_PATH_LEN                    255
#define APLOGD_LOGFILE_PATH_LEN     	32
#define APLOGD_OLD_FILE_EXT         "backup"
#define CONFIG_LOGFILE_MAX_VALUE_USERDATA  10*1024*1024     /* 10M */
#define CONFIG_LOGFILE_MAX_VALUE_EXTERNAL 50*1024*1024      /* 50M */
#define DEFAULT_USR_CFG_MAX_DIR_COUNT    10   /* the default max backup log folder numbers */
#define SZ_1G	1024*1024*1024 /* 1G */

/* 4 byte field for the message id of binary messages */

struct log_io_struct{
	int input_fd;
	int output_fd;
	unsigned int collect_flag;
	char *input_buf;
	char *output_buf;
	char *input_buf_base;
	char *output_buf_base;
};

typedef enum{
	STORAGE_USERDATA = 0,
	STORAGE_EXTERNAL,
	STORAGE_SECONDARY,
    STORAGE_MAX
}STORAGE_T;

/* Externs exported by aplogd.c*/
extern int aplogd_continue_running;
extern unsigned int aplogd_logfile_max;
extern unsigned int total_write_size;
extern int config_changed;
extern int g_current_storage;
extern struct log_io_struct aplogd_io_array[];
extern char* usr_cfg_collect;
extern char* usr_cfg_format;
extern unsigned int usr_cfg_size;
extern unsigned int usr_cfg_backup;
extern unsigned int usr_cfg_seq;
extern unsigned int usr_cfg_ext;
extern char* g_storage_path[STORAGE_MAX];
extern char* g_output_path[STORAGE_MAX];

void aplogd_config_load(void);

//#define DEBUG_APLOGD
#ifdef DEBUG_APLOGD
#define _APLOGD_DEBUG_PRINT(format, args...)				\
	printf("APLOGD DEBUG (%s:%d): " format, __FUNCTION__, __LINE__, ##args)
#define EPRINT(format, args...)						\
	printf("APLOGD ERROR (%s:%d): " format, __FUNCTION__, __LINE__, ##args)
#define WPRINT(format, args...)						\
	printf("APLOGD WARNING (%s:%d): " format, __FUNCTION__, __LINE__, ##args)
#define DPRINT(format, args...)						\
	printf("APLOGD DEBUG (%s:%d): " format, __FUNCTION__, __LINE__, ##args)
#else
#define  _APLOGD_DEBUG_PRINT(format, args...) do{}while(0)
#define  EPRINT(format, args...) do{}while(0)
#define  WPRINT(format, args...) do{}while(0)
#define  DPRINT(format, args...) do{}while(0)
#endif

//#define VERBOSE_APLOGD
#ifdef VERBOSE_APLOGD
#define VPRINT(format, args...)						\
	printf("APLOGD VERBOSE (%s:%d): " format, __FUNCTION__, __LINE__, ##args)
#else
#define  VPRINT(format, args...) do{}while(0)
#endif

#define TRUE 1
#define FALSE 0

#endif /* Not defined _APLOGD_H_ */
