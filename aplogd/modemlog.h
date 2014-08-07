/*
 * modemlog
 *
 * Usage: /system/bin/modemlog [filename]
 *
 * Grabs BP panic data from: /proc/last_kmsg and /proc/kmsg or a log file
 * given on the command line BP panic data is stuck into
 * /data/panicreports. AP panic data is stuck into /data/kernelpanics.
 *
 * Author: Rob Stoddard jwnd84@motorola.com License: Motorola Proprietary
 *
 * when                 who             what, where, why
 * ----------------------------------------------------------------------
 * Feb 25th, 2010   Liu Peng-a22543     report watchdog reset as a special kernel panic
 * Jan 25th, 2010   Liu Peng-a22543     Only keep useful kernel panic logs
 * Jan 18rd, 2010   Mao Weiyang-A22957  Rename kernel panic logs
 * 						from ap_XXXX to ap_kernel_panic_XXXX
 *                                     Rename watchdog reset logs
 *	                                     	from bp_XXXX to wdog_ap_reset_boot_info_XXXX
 * Oct 30th, 2009	Liu Peng-A22543	Moving modemlog to MAP3
 * 7/10/2009		qcf001 bug11379: Not logging Power-cuts to panicreports.
 *                            They are uninteresting.
 * 5/6/09		bfp468 Logging hardware watchdog and power cut as panics in
 * /data/panicreports/.
 * 5/1/09		jwnd84 Added linux kernel panics again.
 * Don't know what happened to my previous mod; git must have eaten it.
 * 2/12/09		jwnd84 Created this program for cutting out modem panic data
 * and managing panic dump size.
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <private/android_filesystem_config.h>

#include <cutils/properties.h>
#define PERSIST_POWERCUTS      "persist.motorola.powercuts"

#define BACKLINES 10

#define MAX_LOG_LEN 256

// mktemp takes six X characters at the end of the string and changes them
// to a temp name.
#define APANIC_STATE_FILE "/data/dontpanic/apanic_console_statinfo"
#define MODEMFILE "/data/kernelpanics/wdog_ap_reset_boot_info_XXXXXX"
#define MODEMDIR "/data/kernelpanics"
#define KERNELFILE "/data/kernelpanics/ap_kernel_panic_XXXXXX"
#define KERNELDIR "/data/kernelpanics"
#define FILENAMELEN 255


// 100k max for panic data.
#define MAXTOTALSIZE 102400
#define true 1
#define false 0
#define NO_CRASH 0
#define LINUX_CRASH 1
#define MODEM_CRASH 2

void writePanicData(int pufile);


void readFile(const char * modemfile, int filefd);

struct FileRec {
    struct FileRec *next;
    struct FileRec *prev;

    char *name;
    time_t timestamp;
    int size;
};


struct FileRec * FileRecCopy( char *n, time_t t, int s);

void FileRecDel(struct FileRec *filerec) ;

void checkdir(char *dirname);

