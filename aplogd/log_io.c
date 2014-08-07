/********************************************************************
 * Copyright (C) 2009 Motorola, All Rights Reserved
 *
 * File Name: log_io.c
 *
 * General Description: This file provides the core of the aplogd daemon.
 *
 * Revision History
 *   Date          Author         Description
 * ----------    --------------  ------------------------------------------
 * 02/19/2009    Motorola        Initial creation
 *
 *********************************************************************/

/************************
 * Includes
 ************************/
/* "Standard" Includes */
#include <log/logger.h>
#include <log/logd.h>
#include <log/logprint.h>
#include <log/event_tag_map.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/statfs.h>
#include <cutils/sockets.h>
#include <cutils/properties.h>
/* Aplogd local includes */
#include "log_io.h"
#include "rambuf.h"
#include "aplogd.h"
#include "aplogd_util.h"
/************************
 * Local defines and macros
 ************************/

/* #define values */
#define APLOGD_OUTPUT_DELAY         5   /* Seconds between writes */

#define APLOGD_POLL_STATUS_RM_FD    0x00000001

/* Macros */

#ifdef APLOGD_TEST
#define LOG_MAIN_PATH "/dev/log/main_test"
#define LOG_RADIO_PATH "/dev/log/radio_test"
#define LOG_EVENTS_PATH "/dev/log/events_test"
#define LOG_SYSTEM_PATH "/dev/log/system_test"
#define LOG_KERNEL_PATH "/proc/kmsg"
#else
#define LOG_MAIN_PATH "/dev/log/main"
#define LOG_RADIO_PATH "/dev/log/radio"
#define LOG_EVENTS_PATH "/dev/log/events"
#define LOG_SYSTEM_PATH "/dev/log/system"
#define LOG_KERNEL_PATH "/proc/kmsg"
#endif /* APLOGD_TEST */
/************************
 * Local Globals
 ************************/
void aplogd_log_save(STORAGE_T storage);
/* Input fd's */
char input_filename[APLOGD_INPUT_LAST+1][64]={LOG_MAIN_PATH, LOG_RADIO_PATH, LOG_EVENTS_PATH, LOG_SYSTEM_PATH, LOG_KERNEL_PATH};
char output_filename[APLOGD_INPUT_LAST+1][64]={"log.main.txt", "log.radio.txt", "log.events.txt", "log.system.txt", "log.kernel.txt"};
const unsigned int aplogd_output_buffering = 4*1024;
int g_current_storage=STORAGE_USERDATA;
extern AndroidLogFormat * g_logformat;

struct log_io_struct aplogd_io_array[APLOGD_MAX_IO_FDS];
/* Poll variables */
static struct pollfd aplogd_io_fd[APLOGD_MAX_IO_FDS];
static int aplogd_poll_timeout=-1;
/* Statistic variables */
unsigned int aplogd_bytes_logged = 0;
unsigned int aplogd_bytes_lost = 0;
static unsigned int unwritten_bytes=0;

extern EventTagMap* g_eventTagMap ;
static char defaultBuffer[1024];

int LogfilterAndBuffering(
		AndroidLogFormat *p_format,
		const AndroidLogEntry *entry, int index)
{
	char *outBuffer = NULL;
	size_t totalLen=0;
	int ret_val=0;
	size_t len=0;
	if (0 == android_log_shouldPrintLine(p_format, entry->tag,
				entry->priority)) {
		VPRINT("No need to output.\n");
		ret_val=0;
		return ret_val;
	}

	outBuffer = android_log_formatLogLine(p_format, defaultBuffer,
			sizeof(defaultBuffer), entry, &totalLen);

	if (!outBuffer){
		EPRINT("Failed in android_log_formatLogLine.\n");
		ret_val=-1;
		return ret_val;
	}
        len= aplogd_rambuf_space(index);
	if(len >  totalLen ){
		memcpy(aplogd_io_array[index].input_buf, outBuffer, totalLen);
		aplogd_io_array[index].input_buf += totalLen;
		ret_val=totalLen;
		if(outBuffer !=defaultBuffer)
			free(outBuffer);
	}else{
		WPRINT("no enough buffer to contain log messages.len=%d totallen=%d.entry->messageLen=%d\n",len, totalLen,entry->messageLen);
		if(outBuffer !=defaultBuffer)
			free(outBuffer);
		ret_val=-1;
		aplogd_bytes_lost += totalLen;
	}
	return ret_val;
}

int switch_storage(STORAGE_T old_storage, STORAGE_T new_storage)
{
	aplogd_logfile_max = aplogd_get_filesize(new_storage);
	aplogd_close_output();
	if (aplogd_output_setup(new_storage)<0){
		ALOGE("aplogd_output_setup in %s failed.",g_output_path[new_storage]);
		aplogd_output_setup(old_storage);
		return old_storage;
	}
	return new_storage;
}


/************************
 * Functions
 ************************/

/* aplogd_io_fd_setup()
 *
 * Description: This function sets up a unix datagram socket at the path
 * provided.
 *
 * @pathname: (const char *) Desired unix socket path
 *
 * Return: (int) Sockets fd on success; -1 on failure
 *
 * Notes: None
 */
int aplogd_io_fd_setup(const char * pathname)
{
	int fd = -1;

	DPRINT("Entered fd setup.pathname=%s\n",pathname);
	/* Setup sockaddr */
	fd = open(pathname,  O_RDONLY |O_NONBLOCK);
	if (fd < 0)
	{
		WPRINT("Couldn't open the file: %s\n",pathname);
		return -1;
	}
	DPRINT("Successfully returning fd: %d\n", fd);
	return fd;
}


/* aplogd_io_add_postatic EventTagMap* g_eventTagMap = NULL;
 * ll_fd()
 *
 * Description: This function adds a file descriptor to our pollfd list
 *
 * @fd: (int) File descriptor to add
 * @events: (short) events to poll for on @fd
 *
 * Return: None
 *
 * Notes: None
 */
void aplogd_io_add_poll_fd(int fd, short events, int index)
{
	DPRINT("Adding fd %d; poll for %d; index %d\n", fd, events, index);
	aplogd_io_fd[index].fd = fd;
	aplogd_io_fd[index].events = events;
	aplogd_io_fd[index].revents = 0;
	return;
}

/* aplogd_io_remove_poll_fd()
 *
 * Description: This function removes an fd from the poll list
 *
 * @num: (int) Poll struct array offset to remove
 *
 * Return: None
 *
 * Notes: None
 */
void aplogd_io_remove_poll_fd(int num)
{
	DPRINT("Removing fd %d (fd = %d) from list\n", num,
			aplogd_io_fd[num].fd);
	if (num < 0 || num >= APLOGD_MAX_IO_FDS )
	{
		WPRINT("Poll index out of bounds\n");
		return;
	}
	aplogd_io_fd[num].fd = -1;
	aplogd_io_fd[num].events = 0;
	aplogd_io_fd[num].revents = 0;
}

/* aplogd_io_add_poll_flag()
 *
 * Description: This function allows code to add poll flags in the
 *              poll structure
 *
 * @index: (int) index of value ot modify in poll list
 * @flags: (int) flags to add to what is being polled for
 *
 * Return: None
 *
 * Notes: None
 */

void aplogd_io_add_poll_flag(int index, int flags)
{
	DPRINT("Adding poll flag 0x%08X from index %d\n", (unsigned int)
			flags, index);
	aplogd_io_fd[index].events |= flags;
}

/* aplogd_io_remove_poll_flag()
 *
 * Description: This function allows code to remove poll flags in the
 *              poll structure
 *
 * @index: (int) index of value ot modify in poll list
 * @flags: (int) flags to remove from what is being polled for
 *
 * Return: None
 *
 * Notes: None
 */

void aplogd_io_remove_poll_flag(int index, int flags)
{
	DPRINT("Removing poll flag 0x%08X from index %d\n", (unsigned int)
			flags, index);
	aplogd_io_fd[index].events &= ~flags;
}

/* aplogd_output_setup()
 *
 * Description: This function sets up the output mechanisms for the aplogger
 *
 * @pathname:(char *) full path of output file
 *
 * Return: 0 on success; -1 on failure
 *
 * Notes: None
 */
int aplogd_output_setup(STORAGE_T storage)
{
	int ret_val = 0;
	int i;
	char fullpath[MAX_PATH_LEN];
	DPRINT("Setting up output fd\n");
	char pathname[MAX_PATH_LEN];
	sprintf(pathname,"%s",g_output_path[storage]);
	ret_val=access(pathname,F_OK );
        if(ret_val!=0 ) {
		DPRINT("The path %s doesn't exist.Create it.\n", pathname);
		/* Our group is 'log', so anybody who wants to view our files should
		 * be in the 'log' group.  The 'adb shell' user without root has this
		 * permission, so this should not be a problem.  Since any Java apps
		 * with READ_LOGS permission are also in the 'log' group, we don't want
		 * to give write access to the group.  We should not give "other" any
		 * permissions.  If the storage path is to external storage, then our
		 * choice of permissions is overridden by the system anyway.  If this
		 * is a concern (ex: privacy issues on user builds), then aplogd should
		 * be configured to not store to external storage (no -x option). */
		ret_val=mkdir(pathname, 0750);
        }
	ret_val=access(pathname,W_OK);
        if(ret_val!=0 ) {
		EPRINT("The path %s isn't accessed.\n", pathname);
		return ret_val;
	}
	/* Setup the fd output */
	for (i=0;i<= APLOGD_INPUT_LAST;i++)
	{
		snprintf(fullpath, MAX_PATH_LEN, "%s/%s",pathname,output_filename[i]);
		/* See comments above about permissions. */
		if ((aplogd_io_array[i].output_fd = open(fullpath,  O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP )) < 0)
		{
			EPRINT("Could not setup output fd on %s\n", fullpath);
			ret_val = -1;
		}
	}
	DPRINT("Done setting up output fd: %d\n", ret_val);
	return ret_val;
}

/* aplogd_io_array_init()
 *
 * Description: This function initializes the array aplogd_io_fd.
 *
 * Return: None
 *
 * Notes: None
 */
void aplogd_io_array_init(void)
{
	int i;

	for (i = 0; i < APLOGD_MAX_IO_FDS; i++)
	{
		aplogd_io_fd[i].fd = -1;
		aplogd_io_fd[i].events = 0;
		aplogd_io_fd[i].revents = 0;
		aplogd_io_array[i].input_fd = -1;
		aplogd_io_array[i].output_fd = -1;
		aplogd_io_array[i].input_buf=NULL;
		aplogd_io_array[i].input_buf_base=NULL;
		aplogd_io_array[i].output_buf=NULL;
		aplogd_io_array[i].output_buf_base=NULL;

	}
}

/* aplogd_input_setup()
 *
 * Description: This function sets up the input mechanisms used by aplogd.
 *
 * Return: (int) 0 on success; -1 on failure
 *
 * Notes: None
 */
int aplogd_input_setup(void)
{
	int ret_val = 0;
	DPRINT("Setting up input fds\n");
	int i;

	for(i=0;i<= APLOGD_INPUT_LAST ;i++){
		if (aplogd_io_array[i].collect_flag && aplogd_io_array[i].input_fd < 0){
			aplogd_io_array[i].input_fd = aplogd_io_fd_setup(input_filename[i]);
			if (aplogd_io_array[i].input_fd >= 0){
				aplogd_io_add_poll_fd(aplogd_io_array[i].input_fd, POLLIN,i);
				DPRINT("Setup input[%d] fd on %d, filename is %s\n", i, aplogd_io_array[i].input_fd, input_filename[i]);
			}else{
				ret_val = -1;
			}
		}
	}
	aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd = socket_local_client("vold", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
	if(aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd >= 0){
		DPRINT("Vold socket opened.\n");
		aplogd_io_add_poll_fd(aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd, POLLIN,APLOGD_VOLD_STATUS_POLL_INDEX);
	}else{
		EPRINT("VOLD socket open failed.\n");
	}
	DPRINT("Done setting up: %d\n", ret_val);
	return ret_val;
}

/* aplogd_io_cleanup()
 *
 * Description: This function cleans up open fds, and the poll struct
 *
 * Return: None
 *
 * Notes: None
 */
static void aplogd_io_cleanup(void)
{
	int i = APLOGD_MAX_IO_FDS - 1;

	DPRINT("Cleaning up input fd\n");
	for ( ; i >= 0; i--)
	{
		DPRINT("Cleaning up fd %d\n", aplogd_io_fd[i].fd);
		aplogd_io_fd[i].fd = -1;
		aplogd_io_fd[i].events = 0;
		aplogd_io_fd[i].revents = 0;
		if(aplogd_io_array[i].input_fd >= 0 )
			close(aplogd_io_array[i].input_fd);
		aplogd_io_array[i].input_fd = -1;
		if(aplogd_io_array[i].output_fd)
			close(aplogd_io_array[i].output_fd);
		aplogd_io_array[i].output_fd = -1;
	}

	return;
}

// Below are the original messages that this module used to look for from vold.
//#define SDCARD_UNMOUNT "605 Volume sdcard /mnt/sdcard state changed from 4 (Mounted) to 5 (Unmounting)"
//#define SDCARD_MOUNT "605 Volume sdcard /mnt/sdcard state changed from 3 (Checking) to 4 (Mounted)"
//#define SDCARD_EXT_UNMOUNT "605 Volume sdcard-ext /mnt/sdcard-ext state changed from 4 (Mounted) to 5 (Unmounting)"
//#define SDCARD_EXT_MOUNT "605 Volume sdcard-ext /mnt/sdcard-ext state changed from 3 (Checking) to 4 (Mounted)"

int is_vold_msg_for(STORAGE_T storage, const char* vold_msg)
{
    if (NULL == g_storage_path[storage])
        return 0;
    const char* msg_path = strstr(vold_msg, g_storage_path[storage]);
    if (msg_path) {
        const char* space_loc = strchr(msg_path, ' ');
        if (space_loc) {
            size_t msg_path_len = space_loc - msg_path;
            if (strlen(g_storage_path[storage]) == msg_path_len) {
                return 1;
            }
        }
    }
    return 0;
}

int is_trans_to_mounted(STORAGE_T storage, const char* vold_msg)
{
    if (is_vold_msg_for(storage, vold_msg) &&
            (NULL != strstr(vold_msg, "state changed from 3 (Checking) to 4 (Mounted)"))) {
        DPRINT("%s trans to mounted\n", g_storage_path[storage]);
        return 1;
    }
    return 0;
}

int is_trans_to_unmounted(STORAGE_T storage, const char* vold_msg)
{
    if (is_vold_msg_for(storage, vold_msg) &&
            (NULL != strstr(vold_msg, "state changed from 4 (Mounted) to 5 (Unmounting)"))) {
        DPRINT("%s trans to unmounted\n", g_storage_path[storage]);
        return 1;
    }
    return 0;
}

int is_bad_removal(STORAGE_T storage, const char* vold_msg)
{
    if (is_vold_msg_for(storage, vold_msg) &&
            (NULL != strstr(vold_msg, "bad removal"))) {
        DPRINT("%s bad removal\n", g_storage_path[storage]);
        return 1;
    }
    return 0;
}

int is_storage_writable(STORAGE_T storage)
{
    if (g_storage_path[storage]) {
        if (0 == access(g_storage_path[storage], W_OK)) {
            DPRINT("%s is writable\n", g_storage_path[storage]);
            return 1;
        } else {
            DPRINT("%s isn't writable\n", g_storage_path[storage]);
        }
    }
    return 0;
}

/* aplogd_io_read_data()
 *
 * Description: This function reads data from an input file descriptor and adds
 * it to the circular RAM buffer.
 *
 * @poll_num: (int) Element of the poll struct array to read from
 *
 * Return: None
 *
 * Notes: None
 */
static void aplogd_io_read_data(int poll_num)
{
	int my_fd = 0;
	int len=0;
	int ret=0;
	int ReadableLogSize;
	my_fd   = aplogd_io_fd[poll_num].fd;
	VPRINT("aplogd_io_read_data poll_num=%d.\n",poll_num);
	if(poll_num < APLOGD_INPUT_KERNEL_POLL_INDEX){
		int ret=0;
		unsigned char buf[LOGGER_ENTRY_MAX_LEN + 1] __attribute__((aligned(4)));
		struct logger_entry *entry = (struct logger_entry *) buf;
		AndroidLogEntry log_entry;
		char binaryMsgBuf[1024];
		ReadableLogSize=ioctl(my_fd, LOGGER_GET_LOG_LEN);
		while(ReadableLogSize>0){
			ret = read (my_fd, entry, LOGGER_ENTRY_MAX_LEN);
			if (ret < 0) {
				if (errno == EINTR)
					continue;
				if (errno == EAGAIN)
					break;
				perror("logcat read");
				exit(EXIT_FAILURE);
			}
			else if (!ret) {
				EPRINT("read: Unexpected EOF!\n");
				exit(EXIT_FAILURE);
			}
			/* NOTE: driver guarantees we read exactly one full entry */
			if(entry->len >=LOGGER_ENTRY_MAX_LEN)
				entry->len=LOGGER_ENTRY_MAX_LEN-1;
			entry->msg[entry->len] = '\0';
			if(APLOGD_INPUT_EVENTS_POLL_INDEX == poll_num )
				errno = android_log_processBinaryLogBuffer(entry, &log_entry, g_eventTagMap,binaryMsgBuf, sizeof(binaryMsgBuf));
			else
				errno = android_log_processLogBuffer(entry, &log_entry);
			if(errno <0) {
				EPRINT("Error in android_log_processLogBuffer.\n");
				break;
				}
			ret=LogfilterAndBuffering(g_logformat, &log_entry, poll_num);
			if(ret<0){
				EPRINT("Error in LogfilterAndBuffering.\n");
				break;
			}else{
				unwritten_bytes+=ret;
				if(unwritten_bytes>= aplogd_output_buffering){
					aplogd_rambuf_outputall();
					unwritten_bytes=0;
					aplogd_poll_timeout=-1;
				}else{
					aplogd_poll_timeout=60000;
				}
			}
			ReadableLogSize-=ret;
		}//while loop
	}else if (APLOGD_INPUT_KERNEL_POLL_INDEX == poll_num){
		char buf[256];
		ret = read(my_fd, buf, 255);
		while(ret>0){
		        len = aplogd_rambuf_space(poll_num);
			if(len > 0 && len >= ret && ret <= 255 ){
				buf[ret]='\0';
                       memcpy(aplogd_io_array[poll_num].input_buf, buf, ret );
                       aplogd_io_array[poll_num].input_buf += ret;
               } else {
                       WPRINT("no enough buffer to contain log messages.\n");
                       break;
               }
			unwritten_bytes+=ret;
			if(unwritten_bytes >= aplogd_output_buffering){
				aplogd_rambuf_outputall();
				aplogd_poll_timeout=-1;
				unwritten_bytes=0;
			}else{
				aplogd_poll_timeout=60000;
			}
			ret=read(my_fd, buf, 255);
		}
	}else {
		int sock_fd= aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd;
		char buf[4096];
		read(sock_fd, buf, sizeof(buf));
		buf[4095]='\0';
		DPRINT("VOLD MSG: %s\n",buf);
		if (is_trans_to_mounted(STORAGE_SECONDARY, buf)){
			aplogd_log_save(STORAGE_SECONDARY);
			if(aplogd_calc_storage_pref() == STORAGE_SECONDARY){
				if(1==usr_cfg_seq)
					aplogd_move_logs(g_current_storage, STORAGE_SECONDARY);
				g_current_storage=switch_storage(g_current_storage, STORAGE_SECONDARY);
			}
		}
		if (is_trans_to_unmounted(STORAGE_SECONDARY, buf) || is_bad_removal(STORAGE_SECONDARY, buf)){
			if(STORAGE_SECONDARY == g_current_storage){
				g_current_storage=switch_storage(STORAGE_SECONDARY,STORAGE_EXTERNAL);
				if(g_current_storage==STORAGE_SECONDARY)
					g_current_storage=switch_storage(STORAGE_SECONDARY,STORAGE_USERDATA);
			}
		}
		if (is_trans_to_mounted(STORAGE_EXTERNAL, buf)){
			aplogd_log_save(STORAGE_EXTERNAL);
			if((aplogd_calc_storage_pref() !=STORAGE_USERDATA)&&(g_current_storage== STORAGE_USERDATA)){
				if(1==usr_cfg_seq)
					aplogd_move_logs(STORAGE_USERDATA,STORAGE_EXTERNAL);
				g_current_storage=switch_storage(g_current_storage, STORAGE_EXTERNAL);
			}
		}
		if (is_trans_to_unmounted(STORAGE_EXTERNAL, buf) || is_bad_removal(STORAGE_EXTERNAL, buf)){
			if(STORAGE_EXTERNAL == g_current_storage){
				g_current_storage=switch_storage(STORAGE_EXTERNAL,STORAGE_USERDATA);
			}
		}
	}
	VPRINT("Leaving aplogd_io_read_data poll_num=%d.\n",poll_num);
}


/* aplogd_io_boot_finish()
 *
 * Description: This function attempts to finish operations that may have been
 * affected by our early position in the boot order. This includes log file FS
 * setup.
 *
 * Return: (int) TRUE when all setup is complete; FALSE when some setup was not
 * completed.
 *
 * Notes: None
 */
static int aplogd_io_boot_finish(void)
{
	int ret_val = 0;
	static int aplogd_config_finish = FALSE;
	if (aplogd_config_finish == FALSE){
		aplogd_config_load();
		DPRINT("Finish config\n");
		aplogd_config_finish = TRUE;
	}
	if (aplogd_config_finish){
		DPRINT("Setting up output and input\n");
		if (STORAGE_SECONDARY == g_current_storage){
			if(!is_storage_writable(STORAGE_SECONDARY)){
					g_current_storage= STORAGE_EXTERNAL;
			}else{/* STORAGE_SECONDARY is writable */
				aplogd_log_save(STORAGE_SECONDARY);
				if (aplogd_output_setup(g_current_storage) != 0){
					ALOGE("Output setup in aplogd_io_boot_finish failed.\n");
					g_current_storage= STORAGE_EXTERNAL;
					}
			}
		}
		if (STORAGE_EXTERNAL == g_current_storage){
			aplogd_log_save(STORAGE_EXTERNAL);
			if(aplogd_output_setup(g_current_storage) !=0){
				WPRINT("Output setup failed.\n");
				g_current_storage= STORAGE_USERDATA;
			}
		}
		if (STORAGE_USERDATA == g_current_storage){
			if(aplogd_output_setup(g_current_storage) !=0){
				WPRINT("Output setup failed.\n");
				ret_val = -1;
			}
		}
		if (aplogd_input_setup() != 0){
			DPRINT("Input setup failed\n");
			ret_val = -1;
		}
	}else{
		DPRINT("NOT setting up output and input\n");
		ret_val = -1;
	}
	DPRINT("Returning %d\n", ret_val);
	return ret_val;
}

/* aplogd_io_handle_pollerr()
 *
 * Description: This function attempts to handle poll error in a sane way.
 *
 * @offset: (int) Offset in the poll list that received pollerr.
 *
 * Return: None
 *
 * Notes: - Poll return event must be cleared before calling this function;
 *          This is because the poll event may be removed from the list
 */

static void aplogd_io_handle_pollerr(int offset)
{
	int temp_fd = aplogd_io_fd[offset].fd;
	close(temp_fd);
	aplogd_io_remove_poll_fd(offset);
	return;
}


/* aplogd_io_handle_poll()
 *
 * Description: This function handles POLLIN, POLLERR, and POLLHUP for a
 *              variety of interfaces.
 *
 * @poll_offset: (int) index in poll fd list to handle
 *
 * Return: (int) Status flags;
 *         ----- 0x00000001 if removed fd
 *
 * Notes: None
 */
static int aplogd_io_handle_poll(int poll_offset)
{
	int ret_val = 0;

	/* Check if this fd returned an event */
	if (aplogd_io_fd[poll_offset].revents & POLLIN)
	{
		VPRINT("fd %d poll returned 0x%0X\n",
				aplogd_io_fd[poll_offset].fd,
				aplogd_io_fd[poll_offset].revents);
		/* Read the data from the logger device */
		aplogd_io_read_data(poll_offset);
		/* We found one returned event; clean up */
		aplogd_io_fd[poll_offset].revents &= ~POLLIN;
	}
	if (aplogd_io_fd[poll_offset].revents & POLLERR ||
			aplogd_io_fd[poll_offset].revents & POLLHUP)
	{
		EPRINT("Handling POLLERR on %d\n",
				aplogd_io_fd[poll_offset].fd);
		aplogd_io_handle_pollerr(poll_offset);
		aplogd_io_fd[poll_offset].revents &= ~POLLERR;
		aplogd_io_fd[poll_offset].revents &= ~POLLHUP;
		ret_val |= APLOGD_POLL_STATUS_RM_FD;
	}

	return ret_val;
}

void aplogd_log_save(STORAGE_T storage)
{
	char file_name[MAX_PATH_LEN];
	char bak_dir[MAX_PATH_LEN];
	int index=0;
	int count=0;
	static int backup_flag=0;
	time_t ltime;
	struct tm *Tm;
	long num = 0;
	struct statfs fs_stat;

	DPRINT("Enter aplogd_log_save.\n");
	if (((backup_flag&1)==1) && (STORAGE_SECONDARY==storage)) return;
	if (((backup_flag&2)==2) && (STORAGE_EXTERNAL==storage)) return;
	if(STORAGE_SECONDARY == storage || STORAGE_EXTERNAL== storage ){
		if(access(g_output_path[storage],0)!=0)
			return;
		if (statfs(g_output_path[storage], &fs_stat) == -1)
			ALOGE("Error on aplogd statfs: %s errno=%d(%s)\n",
				g_output_path[storage], errno, strerror(errno));
		else if (fs_stat.f_bfree * fs_stat.f_bsize < SZ_1G) {
			usr_cfg_backup = DEFAULT_USR_CFG_MAX_DIR_COUNT;
			ALOGW("%s free space size < 1G, only save the lastest %d log folders",
				g_output_path[storage], DEFAULT_USR_CFG_MAX_DIR_COUNT);
		}
		num = aplogd_util_cleardir(g_output_path[storage], usr_cfg_backup);
		ltime=time(NULL);
		Tm=localtime(&ltime);
		if ((num != -1) && (Tm!=NULL)) {
			num = (num + 1) % LONG_MAX;
			snprintf(bak_dir, MAX_PATH_LEN,  "%s/No.%ld_%d%02d%02d%02d%02d%02d",
				g_output_path[storage], num, Tm->tm_year+1900,Tm->tm_mon+1,
				Tm->tm_mday,Tm->tm_hour,Tm->tm_min, Tm->tm_sec);
		} else
			snprintf(bak_dir,MAX_PATH_LEN,  "%s/last",g_output_path[storage]);
		for (index=0;index <= APLOGD_INPUT_LAST;index++){
			snprintf(file_name, MAX_PATH_LEN, "%s/%s", g_output_path[storage], output_filename[index]);
			if(access(file_name, 0)!=0){
				count++;
				continue;
			}
			DPRINT("gzip %s.\n",file_name);
			aplogd_util_gzip(file_name);
		}
		if(STORAGE_SECONDARY==storage)
			backup_flag|=1;
		if(STORAGE_EXTERNAL==storage)
			backup_flag|=2;
		if (count== APLOGD_INPUT_LAST+1)
			return;
		/* See comments on permissions in aplogd_output_setup(). */
		mkdir(bak_dir, 0750);
		DPRINT("move %s to %s.\n",g_output_path[storage], bak_dir);
		aplogd_util_move(g_output_path[storage], bak_dir, ".gz");
	}else if(STORAGE_USERDATA == storage) {
		snprintf(bak_dir, MAX_PATH_LEN, "%s/last",g_output_path[storage]);
		/* See comments on permissions in aplogd_output_setup(). */
		mkdir(bak_dir, 0750);
		for (index=0;index <= APLOGD_INPUT_LAST; index++){
			snprintf(file_name, MAX_PATH_LEN, "%s/%s", g_output_path[storage], output_filename[index]);
			aplogd_util_gzip(file_name);
		}
		DPRINT("move %s to %s.\n",g_output_path[storage], bak_dir);
		aplogd_util_cleardir(bak_dir, 1);
		aplogd_util_move(g_output_path[storage], bak_dir, ".gz");
	}
	DPRINT("Leaving aplogd_log_save.\n");
}

/* aplogd_log_io()
 *
 * Description: This function provides the main log input / output
 * functionality.
 *
 * Return: None
 *
 * Notes: None
 */
void aplogd_log_io(void)
{
	/* Input setup */
	DPRINT("Setting up inputs & outputs\n");
	aplogd_log_save(STORAGE_SECONDARY);
	aplogd_log_save(STORAGE_USERDATA);
	if(aplogd_io_boot_finish()<0){
		ALOGE("Error in aplogd setup.\n");
		goto fail;
	}
	/* Do the actual input retrieval work */
	DPRINT("Starting main io loop\n");
	while (aplogd_continue_running)
	{
		int i = 0;
		int ret = 0;
		int poll_ret = 0;
		if(1== __atomic_swap(0, &config_changed)){
			ret = aplogd_calc_storage_pref();
			if(g_current_storage != ret){
				g_current_storage=switch_storage(g_current_storage,ret);
			}
			aplogd_config_load();
			for(i=0;i<=  APLOGD_INPUT_LAST ;i++){
				if(aplogd_io_array[i].collect_flag == 0)
					aplogd_io_remove_poll_fd(i);
				else
					if(aplogd_io_array[i].input_fd >= 0)
						aplogd_io_add_poll_fd(aplogd_io_array[i].input_fd, POLLIN,i);
			}
		}
		i=0;
		poll_ret = poll(aplogd_io_fd, APLOGD_MAX_IO_FDS,aplogd_poll_timeout);
		if(poll_ret ==0) {
			aplogd_rambuf_outputall();
			aplogd_poll_timeout=-1;
			unwritten_bytes=0;
		}else {
			while (poll_ret > 0 && i < APLOGD_MAX_IO_FDS){
				VPRINT("poll_ret =%d.\n",poll_ret);
				if (aplogd_io_fd[i].revents){
					VPRINT("fd %d had revent %d\n",
						aplogd_io_fd[i].fd,
						aplogd_io_fd[i].revents);
					if (aplogd_io_handle_poll(i) & APLOGD_POLL_STATUS_RM_FD)
						i--;
					poll_ret--;
				}
				i++;
			}           /* While we have poll events returned */
		}
		if(aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd <0){
			aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd = socket_local_client("vold", ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
			if(aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd >= 0){
				DPRINT("Vold socket opened.\n");
				aplogd_io_add_poll_fd(aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].input_fd, POLLIN,APLOGD_VOLD_STATUS_POLL_INDEX);
			}else{
				EPRINT("VOLD socket open failed.\n");
			}
		}
	}/* While aplogd_continue_running */
fail:
	/* I/O cleanup */
	DPRINT("Cleaning up I/O\n");
	aplogd_io_cleanup();
	return;
}


/* aplogd_io_backup()
 *
 * Description: This function will rename the output file to .old and create a new fd.
 *
 * @index:(int) index of output_filename
 *
 * Return: (int)New fd of output file description
 *
 * Notes: None
 */

int aplogd_io_file_backup(int index , STORAGE_T storage)
{
        char file_name[MAX_PATH_LEN];
        char bak_name[MAX_PATH_LEN];
	char cur_path[MAX_PATH_LEN];
        int ret_val=0;
	int i;
	static int n=0;
	snprintf(cur_path, MAX_PATH_LEN, "%s", g_output_path[storage]);
	i=n;
	while(i>0){
		snprintf(file_name, MAX_PATH_LEN, "%s/%s.%d.%s.gz", cur_path,APLOGD_OLD_FILE_EXT,i-1,  output_filename[index]);
		snprintf(bak_name, MAX_PATH_LEN, "%s/%s.%d.%s.gz", cur_path, APLOGD_OLD_FILE_EXT, i, output_filename[index]);
		file_name[MAX_PATH_LEN - 1] = '\0';
		bak_name[MAX_PATH_LEN - 1] = '\0';
		if (rename(file_name, bak_name) < 0){
			EPRINT("Couldn't rename %s to %s; errno=%s. We'll delete it\n",
					file_name, bak_name, strerror(errno));
			unlink(file_name);
			ret_val=-1;
		}
		i--;
	}
	n++;
	snprintf(file_name, MAX_PATH_LEN, "%s/%s", cur_path, output_filename[index]);
	snprintf(bak_name, MAX_PATH_LEN, "%s/%s.0.%s", cur_path, APLOGD_OLD_FILE_EXT,  output_filename[index]);
        file_name[MAX_PATH_LEN - 1] = '\0';
        bak_name[MAX_PATH_LEN - 1] = '\0';
        if(access(bak_name,0)==0)
                unlink(bak_name);
        if (rename(file_name, bak_name) < 0)
        {
		EPRINT("Couldn't rename %s to %s; errno=%s. We'll delete it\n",
				file_name, bak_name, strerror(errno));

                unlink(file_name);
                ret_val=-1;
        }
	struct stat statf;
        if (stat(bak_name, &statf) ){
               total_write_size -= statf.st_size;
        }
	aplogd_util_gzip(bak_name);
        DPRINT("Leaving file backup.\n");
        return ret_val;
}

int aplogd_io_backupall(STORAGE_T storage)
{
        int i;
        int ret_val=0;
        for (i=0;i<= APLOGD_INPUT_LAST;i++){
                ret_val+=aplogd_io_file_backup(i,storage);
	}
	return ret_val;
}

void aplogd_close_output()
{
	int i;
	for (i=0;i<=APLOGD_INPUT_LAST;i++){
		if (aplogd_io_array[i].output_fd > 0)
                        close(aplogd_io_array[i].output_fd);
		aplogd_io_array[i].output_fd =-1;
	}
}
