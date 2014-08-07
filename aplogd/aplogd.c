/********************************************************************
 * Copyright (C) 2009 Motorola, All Rights Reserved
 *
 * File Name: aplogd.c
 *
 * General Description: This file provides the core of the aplogd daemon.
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
 * 05/14/2010    Motorola        Don't depend on secure properties.
 * 02/19/2009    Motorola        Initial creation.
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
#include <dirent.h>
#include <getopt.h>
#include <linux/capability.h>
#include <linux/prctl.h>

#include <log/logger.h>
#include <log/logd.h>
#include <log/logprint.h>
#include <log/event_tag_map.h>
#include <cutils/properties.h>
#include <private/android_filesystem_config.h>
/* Motorola Includes */
/* Removed in LJ-OMAP bring-up phase */
/* #include <mot_security.h> */

/* Aplogd Headers */
#include "aplogd.h"
#include "aplogd_util.h"
#include "log_io.h"
#include "rambuf.h"
/***********************************************************
 * Local Constants
 ***********************************************************/

/* Config Defines */

/***********************************************************
 * Local Macros
 ***********************************************************/
#define MAX_ARGC 12
#define USR_CFG_PROP "persist.log.aplogd.config"
#define MAX_USR_CFG_VALUE 25
#define DEFAULT_USR_CFG_COLLECT "mrsek"
#define DEFAULT_USR_CFG_FORMAT "threadtime"
#define DEFAULT_USR_CFG_SIZE 50

/***********************************************************
 * Local Function Prototypes
 ************************************************************/
static void aplogd_signal_handler(int);

/***********************************************************
 * Global Variables
 ***********************************************************/

unsigned int aplogd_logfile_max = CONFIG_LOGFILE_MAX_VALUE_USERDATA;
unsigned int total_write_size=0;
unsigned int total_lost_size=0;
int aplogd_continue_running = 1;
EventTagMap* g_eventTagMap = NULL;
AndroidLogFormat * g_logformat;
int config_changed =0;

char* usr_cfg_collect = DEFAULT_USR_CFG_COLLECT;
char* usr_cfg_format = DEFAULT_USR_CFG_FORMAT;
unsigned int usr_cfg_size = DEFAULT_USR_CFG_SIZE;
unsigned int usr_cfg_backup = DEFAULT_USR_CFG_MAX_DIR_COUNT;
unsigned int usr_cfg_seq = 1;
unsigned int usr_cfg_ext = 1;

char* g_storage_path[STORAGE_MAX] = {NULL};
char* g_output_path[STORAGE_MAX] = {NULL};

/***********************************************************
 * Local Functions
 ***********************************************************/

/* setLogFormat(const char)
 *
 * Description: This function set the log output format
 *
 * Return: (int) 0 on success; -1 on failure.
 *
 * Notes: None
 */
static int setLogFormat(const char * formatString)
{
	static AndroidLogPrintFormat format;
	g_logformat = android_log_format_new();

	format = android_log_formatFromString(formatString);

	if (format == FORMAT_OFF) {
		// FORMAT_OFF means invalid string
		return -1;
	}

	android_log_setPrintFormat(g_logformat, format);
	return 0;
}


/* aplogd_signal_handler()
 *
 * Description: This function handles signals which call for the aplog
 * daemon to close.
 *
 * @signal: (int) The signal that was received
 *
 * Return: None
 *
 * Notes: None
 */
static void aplogd_signal_handler(int signal)
{
	DPRINT("Received signal: %d\n", signal);
	if (signal == SIGTERM){
		fflush(NULL);
		exit(1);
	}
	if (signal == SIGUSR1){
		config_changed=1;
	}
	return;
}

/* aplogd_config_load()
 *
 * Description: This function loads the necessary values from the config
 * file
 *
 * Notes: None
 */

void aplogd_config_load(void)
{
   DPRINT("Enter aplogd_config_load.\n");
	aplogd_io_array[APLOGD_INPUT_MAIN_POLL_INDEX].collect_flag = aplogd_is_collected('m');
	aplogd_io_array[APLOGD_INPUT_RADIO_POLL_INDEX].collect_flag = aplogd_is_collected('r');
	aplogd_io_array[APLOGD_INPUT_EVENTS_POLL_INDEX].collect_flag = aplogd_is_collected('e');
	aplogd_io_array[APLOGD_INPUT_SYSTEM_POLL_INDEX].collect_flag = aplogd_is_collected('s');
	aplogd_io_array[APLOGD_INPUT_KERNEL_POLL_INDEX].collect_flag = aplogd_is_collected('k');
	setLogFormat(usr_cfg_format);
	aplogd_io_array[APLOGD_VOLD_STATUS_POLL_INDEX].collect_flag=1;
	g_current_storage = aplogd_calc_storage_pref();
	aplogd_logfile_max=aplogd_get_filesize(g_current_storage);
	DPRINT("aplogd_logfile_max=%d.\n",aplogd_logfile_max);
}

/* aplogd_erase_dir_contents()
 *
 * Description: Erases all files and directories within a directory.
 *
 * Notes: This is not recursive, so sub-directories must be empty before calling this.
 */
void aplogd_erase_dir_contents(const char* dir_path)
{
	DIR *dir;
	struct dirent *dirent;
	char entry_path[FILENAME_MAX];
	if (NULL == (dir = opendir(dir_path))) {
		EPRINT("open(%s) failed; errno=%s\n", dir_path, strerror(errno));
		return;
	}
	while (NULL != (dirent = readdir(dir))) {
		if(0 == strcmp(dirent->d_name, ".") || 0 == strcmp(dirent->d_name, ".."))
			continue;
		snprintf(entry_path, FILENAME_MAX, "%s/%s", dir_path, dirent->d_name);
		DPRINT("Erasing %s\n", entry_path);
		if (-1 == remove(entry_path)) {
			EPRINT("remove(%s) failed; errno=%s\n", entry_path, strerror(errno));
		}
	}
	closedir(dir);
}

/* aplogd_erase_legacy_files()
 *
 * Description: Erases all legacy root-owned files so that the new non-root
 * version of aplogd can function properly.
 *
 * Notes: This must be called before dropping root.
 */
void
aplogd_erase_legacy_files()
{
	const char* canary_file = "/data/logger/aplogd.db";
	const char* root_dir = "/data/logger";
	const char* last_dir = "/data/logger/last";
	struct stat canary_stat;
	if (0 == stat(canary_file, &canary_stat)) {
		if (0 == canary_stat.st_uid) {
			DPRINT("Erasing legacy root-owned files\n");
			aplogd_erase_dir_contents(last_dir);
			aplogd_erase_dir_contents(root_dir);
		}
	} else {
		EPRINT("stat(%s) failed; errno=%s\n", root_dir, strerror(errno));
	}
}

/* aplogd_drop_root()
 *
 * Description: Drops root user and keeps only what we need.
 *
 * Notes: None
 */
void aplogd_drop_root()
{
	if (0 != getuid())
		return;
	DPRINT("Dropping from root user to log user\n");
	prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
	/* We need AID_SYSTEM supplemental group to have read access to /proc/kmsg.
	 * We need AID_SDCARD_RW supplemental group to have access to external storage.
	 * We need AID_MOUNT supplemental group to have access to /dev/socket/vold.
	 */
	gid_t groups[] = {AID_SYSTEM, AID_SDCARD_RW, AID_MOUNT};
	if (-1 == setgroups(sizeof(groups)/sizeof(groups[0]), groups)) {
		EPRINT("setgroups() failed; errno=%d", errno);
		exit(-1);
	}
	/* We need to be the AID_LOG as our primary group so that files we
	 * create allow AID_LOG group access. */
	if (-1 == setgid(AID_LOG)) {
		EPRINT("setgid() failed; errno=%d", errno);
		exit(-1);
	}
	/* We need to be the AID_LOG user because we are a logger.  This must
	 * be after changing groups. */
	if (-1 == setuid(AID_LOG)) {
		EPRINT("setuid() failed; errno=%d", errno);
		exit(-1);
	}
	/* We need to keep CAP_SYS_ADMIN in order to read from /proc/kmsg. */
	struct __user_cap_header_struct header;
	struct __user_cap_data_struct cap;
	header.version = _LINUX_CAPABILITY_VERSION;
	header.pid = 0;
	cap.effective = cap.permitted = cap.inheritable = 1 << CAP_SYS_ADMIN;
	if (-1 == capset(&header, &cap)) {
		EPRINT("capset() failed; errno=%d", errno);
		exit(-1);
	}
}

void usage()
{
    printf("\nUsage for aplogd:\n");
    printf("  -c, --collect:\t Streams to collect\n");
    printf("  -f, --format:\t Logcat format option\n");
    printf("  -s, --size:\t Maximum set size in MB\n");
    printf("  -b, --backup:\t Max number for backup log folders\n");
    printf("  -q, --seq:\t Keep logs sequential when moving\n");
    printf("  -x, --ext:\t Store to external storage if available\n");
    printf("  -h, --help:\t usage help\n");
    printf("Example: aplogd -c mrsek -f threadtime -s 50 -b 15 -q -x\n");
    exit(0);
}

void parse_usr_cfg(int argc, char* argv[]) {
    int cmd;
    struct option longopts[] = {
        {"collect",     1,    NULL,    'c'},
        {"format",      1,    NULL,    'f'},
        {"size",        1,    NULL,    's'},
        {"backup",      1,    NULL,    'b'},
        {"seq",         0,    NULL,    'q'},
        {"sdcard",      0,    NULL,    'x'}, // Left for backwards compatibility
        {"ext",         0,    NULL,    'x'},
        {"help",        0,    NULL,    'h'},
    };
    /* Boolean values must be initialized to 0, because their omission
     * indicates they should be 0. */
    usr_cfg_seq = 0;
    usr_cfg_ext = 0;
    while ((cmd = getopt_long(argc, argv, "c:f:s:b:qxh", longopts, NULL)) != -1) {
        switch (cmd) {
            case 'c':
                usr_cfg_collect = optarg;
                break;
            case 'f':
                usr_cfg_format = optarg;
                break;
            case 's':
                usr_cfg_size = atoi(optarg);
                if (0 >= usr_cfg_size)
                    usr_cfg_size = DEFAULT_USR_CFG_SIZE;
                break;
            case 'b':
                usr_cfg_backup = atoi(optarg);
                if (0 >= usr_cfg_backup)
                    usr_cfg_backup = DEFAULT_USR_CFG_MAX_DIR_COUNT;
                break;
            case 'q':
                usr_cfg_seq = 1;
                break;
            case 'x':
                usr_cfg_ext = 1;
                break;
            case 'h':
            default:
                usage(argv[0]);
        };
    }
}

/* This function builds argv from a single string using spaces and commas as
 * delimiters.  It does not recognize quoting, so if that is required then you
 * will need to write a better function or get the shell to parse the command
 * line for you.  Each option+value in the config string is separated by a comma,
 * but getopt_long() wants a true argv as input, which requires separation of
 * option and value.  So, we use spaces and commas as delimiters.
 * Sample:
 *     "--collect mrsek,--format threadtime,--size 50,--backup 10,--ext,--seq"
 */
char**
build_argv(char* args, int* argc)
{
    /* Allocate argv with extra room for arg0 and NULL. */
    char** argv = (char**)malloc((MAX_ARGC + 1) * sizeof(char*));
    if (!argv) {
        EPRINT("malloc() failed\n");
        exit(1);
    }
    /* Populate argv */
    argv[0] = "aplogd";
    DPRINT("argv[0]=\"%s\"\n", argv[0]);
    const char delims[] = " ,";
    int i = 1;
    char* token = strtok(args, delims);
    while (token) {
        if (MAX_ARGC <= i) {
            EPRINT("Args exceed MAX_ARGC=%d; ignoring remaining\n", MAX_ARGC);
            break;
        }
        DPRINT("argv[%d]=\"%s\"\n", i, token);
        argv[i] = token;
        token = strtok(NULL, delims);
        i++;
    }
    /* NULL-terminate the list */
    argv[i] = (char*)NULL;
    *argc = i;
    return argv;
}

void aplogd_load_usr_cfg(int argc, char* argv[])
{
    if (1 < argc) {
        DPRINT("Using cmd line argv\n");
        parse_usr_cfg(argc, argv);
    } else {
        /* Make cfg_prop static so that we can point usr_cfg_* persistent vars
         * at it from build_argv() & parse_usr_cfg().  This way we can avoid
         * making more copies.  In this way it will mimic the original argv
         * content. */
        static char cfg_prop[PROPERTY_VALUE_MAX];
        property_get(USR_CFG_PROP, cfg_prop, "");
        DPRINT(USR_CFG_PROP "=%s\n", cfg_prop);
        if ('\0' == cfg_prop[0]) {
            /* Just use the defaults. */
            DPRINT("Using default user config\n");
        } else {
            DPRINT("Building argv from " USR_CFG_PROP "\n");
            int new_argc;
            char** new_argv = build_argv(cfg_prop, &new_argc);
            if (new_argv) {
                parse_usr_cfg(new_argc, new_argv);
                free(new_argv);
            } else {
                /* Just use the defaults. */
                EPRINT("Couldn't parse " USR_CFG_PROP "; using default user config\n");
            }
        }
    }
    DPRINT("usr_cfg_collect=%s\n", usr_cfg_collect);
    DPRINT("usr_cfg_format=%s\n", usr_cfg_format);
    DPRINT("usr_cfg_size=%u\n", usr_cfg_size);
    DPRINT("usr_cfg_seq=%u\n", usr_cfg_seq);
    DPRINT("usr_cfg_ext=%u\n", usr_cfg_ext);
}

void aplogd_init_storage_refs(void)
{
    #define SUB_FOLDER "logger"

    g_storage_path[STORAGE_USERDATA] = "/data";
    DPRINT("g_storage_path[STORAGE_USERDATA]=%s\n", g_storage_path[STORAGE_USERDATA]);

    char const * ext_path = getenv("EXTERNAL_STORAGE");
    if (ext_path) {
        g_storage_path[STORAGE_EXTERNAL] = strdup(ext_path);
        DPRINT("g_storage_path[STORAGE_EXTERNAL]=%s\n", g_storage_path[STORAGE_EXTERNAL]);
    }

    // SECONDARY_STORAGE is a colon-separated list of paths.  We only support
    // the first path in the list.
    char const * sec_paths = getenv("SECONDARY_STORAGE");
    if (sec_paths) {
        const char* colon = strchr(sec_paths, ':');
        size_t len = (colon ? (size_t)(colon - sec_paths) : strlen(sec_paths));
        g_storage_path[STORAGE_SECONDARY] = strndup(sec_paths, len);
        DPRINT("g_storage_path[STORAGE_SECONDARY]=%s\n", g_storage_path[STORAGE_SECONDARY]);
    }

    // Initialize the full paths for where we store our output.
    int i;
    for (i = 0; i < STORAGE_MAX; i++) {
        if (!g_storage_path[i])
            continue;
        size_t len = strlen(g_storage_path[i]) + strlen("/" SUB_FOLDER) + 1;
        g_output_path[i] = malloc(len);
        g_output_path[i][0] = '\0';
        snprintf(g_output_path[i], len, "%s" "/" SUB_FOLDER, g_storage_path[i]);
        DPRINT("g_output_path[%d]=%s\n", i, g_output_path[i]);
    }
}

/* main()
 *
 * Description: The main startup function of aplogd.
 *
 * @argv: Number of arguments in @argc
 * @argc: Array of argument strings
 *
 * Return: 0 on success, -1 on failure.
 *
 * Notes: None
 */
int main (int argc, char* argv[])
{
	aplogd_erase_legacy_files();
	// Dropping root MUST be the very first thing we do after the cleanup.
	aplogd_drop_root();
	DPRINT("Start initialize.\n");
	aplogd_init_storage_refs();
	aplogd_load_usr_cfg(argc, argv);
	DPRINT("aplogd io array initilize.\n");
	aplogd_io_array_init();
	DPRINT("open EventTagMap.\n");
	g_eventTagMap = android_openEventTagMap(EVENT_TAG_MAP_FILE);
	if(signal(SIGUSR1, aplogd_signal_handler) == SIG_ERR)
		return -1;
	if(signal(SIGTERM, aplogd_signal_handler) == SIG_ERR)
		return -1;
	aplogd_rambuf_init();
	DPRINT("Starting logging I/O loop\n");
	aplogd_log_io();    /* This function will run until aplogd is stopped */
	return 0;
}
