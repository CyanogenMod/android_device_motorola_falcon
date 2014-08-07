/********************************************************************
 * Copyright (C) 2009 Motorola, All Rights Reserved
 *
 * File Name: log_io.h
 *
 * General Description: Header file for the log data input/output source
 *
 * Revision History
 *   Date          Author         Description
 * ----------    --------------  ------------------------------------------
 * 02/19/2009    Motorola        Initial creation
 *********************************************************************/

#ifndef _APLOGD_LOG_INPUT_H_
#define _APLOGD_LOG_INPUT_H_

#include "aplogd.h"

enum aplogd_poll_indexes {
	APLOGD_INPUT_MAIN_POLL_INDEX = 0,
	APLOGD_INPUT_RADIO_POLL_INDEX,
	APLOGD_INPUT_EVENTS_POLL_INDEX,
	APLOGD_INPUT_SYSTEM_POLL_INDEX,
	APLOGD_INPUT_KERNEL_POLL_INDEX,
    APLOGD_INPUT_LAST = APLOGD_INPUT_KERNEL_POLL_INDEX,
	APLOGD_VOLD_STATUS_POLL_INDEX,
	/* Add new entries above this line */
	APLOGD_MAX_IO_FDS
};

extern char output_folder[STORAGE_MAX][64];
extern char output_filename[APLOGD_INPUT_LAST+1][64];

struct aplogd_output_struct {
	int fd;
	ssize_t (*this_write) (int, const char*, int);
};

/* Function prototypes */
void aplogd_log_io(void);
int aplogd_io_fd_setup(const char *);
void aplogd_io_add_poll_fd(int, short, int);
void aplogd_io_add_poll_flag(int, int);
void aplogd_io_remove_poll_fd(int);
void aplogd_io_remove_poll_flag(int, int);
void aplogd_io_array_init(void);
int aplogd_input_setup(void);
int aplogd_output_setup(STORAGE_T);
int aplogd_io_file_backup(int index,STORAGE_T);
int aplogd_io_backupall(STORAGE_T);
void aplogd_close_output();
#endif /* Not defined _APLOGD_LOG_INPUT_H_ */
