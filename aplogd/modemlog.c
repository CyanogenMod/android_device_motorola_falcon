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
 * Apr 30th, 2010   Xu Wenzhao-a23003   [KW] - Security Critical in Product Engine Team
 * Apr 22nd, 2010   Liu Peng-a22543     the kernel panic report shall include the
 *                                      dumping around the address pointed by R0
 * Mar 11th, 2010   Mao Weiyang-A22957  reformat the panic report file
 * Mar 09th, 2010   Liu Peng-a22543     enable/disable each panic reporting
 * Feb 25th, 2010   Liu Peng-a22543     report watchdog reset as a special kernel panic
 * Jan 25th, 2010   Liu Peng-a22543     Only keep useful kernel panic logs
 * Jan 22nd, 2010   Mao Weiyang-A22957  Add pointer cheking to avoid process crash
 * Jan 18rd, 2010   Mao Weiyang-A22957	Rename kernel panic logs
 *  						from ap_XXXX to ap_kernel_panic_XXXX
 *   					Rename watchdog reset logs
 *  						from bp_XXXX to wdog_ap_reset_boot_info_XXXX
 * Jan 13rd, 2010   Mao Weiyang-A22957  aplogd shall make the last 8KB of
 *                                         apanic_console uploaded by the BCS
 * Oct 30th, 2009	Liu Peng-A22543     Moving modemlog to MAP3
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <cutils/properties.h>
#include "modemlog.h"

/* The Bit Position to Enable/Disable Each Panic Report */
#define ENABLE_BOOTLOADER_WDT_REPORT	0x01
#define ENABLE_KERNEL_WDT_REPORT	0x02
#define ENABLE_KERNEL_PANIC_REPORT	0x04
#define ENABLE_POWERCUT_REPORT		0x08

static struct FileRec *first = NULL;

static int Report_Flag = 0;
static unsigned char panic_report_config=0;

static struct property_and_bit_position_t
{
	char * property_name;
	int bit_position;
}property_and_bit_position[] =
{
	{
		.property_name = "persist.dbg.btwdt.report",
		.bit_position = ENABLE_BOOTLOADER_WDT_REPORT,
	},
	{
		.property_name = "persist.dbg.kwdt.report",
		.bit_position = ENABLE_KERNEL_WDT_REPORT,
	},
	{
		.property_name = "persist.dbg.kpanic.report",
		.bit_position = ENABLE_KERNEL_PANIC_REPORT,
	},
	{
		.property_name = "persist.dbg.pwrcut.report",
		.bit_position = ENABLE_POWERCUT_REPORT,
	},
	{
		.property_name = NULL,
	}
};

#define LAST_KMSG_PATHNAME	"/proc/last_kmsg"
#define WDRST_REPORT_SIZE	(64*1024L)

void append_last_kmsg(FILE *outfile)
{
	struct stat statinfo;
	long size;

	if (lstat(LAST_KMSG_PATHNAME, &statinfo)) {
		if (errno == ENOENT)
			fprintf(outfile, "%s does not exist.\n", LAST_KMSG_PATHNAME);
		else
			fprintf(outfile, "%s not accessible %d.\n", LAST_KMSG_PATHNAME, errno);
	} else {
		size = WDRST_REPORT_SIZE - ftell(outfile);
		if (size > 0) {
			FILE *in = fopen(LAST_KMSG_PATHNAME, "r");
			char buf[MAX_LOG_LEN];
			if (in != NULL) {
				if (statinfo.st_size > size)
					fseek(in, statinfo.st_size - size, SEEK_SET);
				do {
					fgets(buf, MAX_LOG_LEN, in);
					if (feof(in))
						break;
					fprintf(outfile, "%s", buf);
				} while(1);
				fclose(in);
			}
		}
	}
}

void writePanicData(int pufile)
{
	char persist_powercuts[PROPERTY_VALUE_MAX];
	char countbuf[PROPERTY_VALUE_MAX];
	int powercuts_count;
	FILE *thefile = fdopen(pufile, "r");
	FILE *outfile = stdout;
	char buf[MAX_LOG_LEN];
	if (thefile == NULL) {
		return;
	}

	while (!feof(thefile)) {
		fgets(buf, MAX_LOG_LEN, thefile);
		/* TODO: watchdog reset - we don't have it with cold reset
		   mechanism used by MAP3 now. Once we have it, there shall
		   be a specific folder to store the data. */
		if (strstr(buf, "POWERUPREASON : 0x00008000")) {
			char modemfile[] = MODEMFILE;

			/* configuration says don't report this */
			if ((panic_report_config & ENABLE_BOOTLOADER_WDT_REPORT) == 0)
				break;

			mkstemp(modemfile);
			outfile = fopen(modemfile, "w");
			if (outfile != NULL) {
				fchown(fileno(outfile), AID_SYSTEM, AID_LOG);
				fchmod(fileno(outfile), 0640);
				fprintf(outfile, "Type: SYSTEM_WDT_RESET\n");
				fprintf(outfile, "%s", buf);
				while (!feof(thefile)){
					fgets(buf, MAX_LOG_LEN, thefile);
					fprintf(outfile, "%s", buf);
				}
				append_last_kmsg(outfile);
				fclose(outfile);
			}
			break;
		}

		if (strstr(buf, "POWERUPREASON : 0x00020000") && Report_Flag==0) {
			char modemfile[] = MODEMFILE;

			if (access(LAST_KMSG_PATHNAME, F_OK) == -1) {
				/* configuration says don't report this */
				if ((panic_report_config & ENABLE_KERNEL_WDT_REPORT) == 0)
					break;
			}

			mkstemp(modemfile);
			outfile = fopen(modemfile, "w");
			if (outfile != NULL) {
				fchown(fileno(outfile), AID_SYSTEM, AID_LOG);
				fchmod(fileno(outfile), 0640);
				fprintf(outfile, "Type: SYSTEM_WDT_RESET\n");
				fprintf(outfile, "%s", buf);
				while (!feof(thefile)){
					fgets(buf, MAX_LOG_LEN, thefile);
					fprintf(outfile, "%s", buf);
				}
				append_last_kmsg(outfile);
				fclose(outfile);
			}
			break;
		}

		/* Power Cut */
		if (strstr(buf, "POWERUPREASON : 0x00000200")) {
			/* configuration says don't report this */
			if ((panic_report_config & ENABLE_POWERCUT_REPORT) == 0)
				break;
			property_get(PERSIST_POWERCUTS, persist_powercuts, "0");
			powercuts_count = atoi(persist_powercuts) + 1;
			snprintf(countbuf, PROPERTY_VALUE_MAX, "%d", powercuts_count);
			property_set(PERSIST_POWERCUTS, countbuf);
			break;
		}
	}
}

void readFile(const char * modemfile, int filefd)
{
	FILE *thefile = fdopen(filefd, "r");
	FILE *outfile = stdout;
	char lastfew[BACKLINES][MAX_LOG_LEN];
	int oldline;
	int dumpall;

	/* We don't have to search for particular strings in order to
	 * determine whether or not a real Linux kenrel panic
	 * happened. Because once /data/dontpanic/apanic_console is
	 * modified, a Linux kernel panic must have happened.
	 * So no need of backward buffer.
	 */
	int count  = 0;


	do /* one time loop to prevent too many if's and goto's */
	{
		outfile = fopen(modemfile, "w");
		if (outfile == NULL) {
			break;
		}

		fchown(fileno(outfile), AID_SYSTEM, AID_LOG);
		fchmod(fileno(outfile), 0640);

		/* Peng:
		 * We don't have this in UMTS phone. While, the specific
		 * string can be generated by syspanic driver.  I comment it
		 * temporarily.
		 */
		/*
		  if ((crashtype == NO_CRASH)
		  && strstr(buf, "proc_comm: ARM9 has crashed")) {
		  char modemfile[] = MODEMFILE;
		  mkstemp(modemfile);
		  outfile = fopen(modemfile, "w");
		  if (outfile != NULL)
		  fchown(fileno(outfile), AID_SYSTEM, AID_SYSTEM);

		  crashtype = MODEM_CRASH;
		  }
		*/

		/* there's been indications of a crash, have we spilled our
		 * guts yet?  */
		fprintf(outfile, "Type: SYSTEM_LAST_KMSG\n");

		FILE *info_fp = fopen("/proc/version", "r");
		char info_buf[MAX_LOG_LEN];
		if (info_fp != NULL) {
			while (!feof(info_fp)) {
				fgets(info_buf, MAX_LOG_LEN, info_fp);
				fprintf(outfile, "%s", info_buf);
			}
			fclose(info_fp);
		}

		if ((info_fp = fopen("/proc/mot_version", "r")) != NULL) {
			while (!feof(info_fp)) {
				fgets(info_buf, MAX_LOG_LEN, info_fp);
				fprintf(outfile, "%s\n", info_buf);
			}
			fclose(info_fp);
		}
		if ((info_fp = fopen("/proc/bootinfo", "r")) != NULL) {
			fgets(info_buf, MAX_LOG_LEN, info_fp);
			fprintf(outfile, "%s", info_buf);
			fgets(info_buf, MAX_LOG_LEN, info_fp);
			fprintf(outfile, "%s", info_buf);
			fclose(info_fp);
		}

		/* Following code changed again: the BCS can't retain exactly
		   8KB so we have to select those lines helpful for
		   debugging. Old simple 8KB code is kept, for one day we have
		   the size limitation removed */

		oldline = 0;
		dumpall = 0;
		while (!feof(thefile)) {
			fgets(lastfew[oldline], MAX_LOG_LEN, thefile);
			if (strstr(lastfew[oldline], "PC is at")) {
				break;
			}
			else if (strstr(lastfew[oldline], "Kernel panic -")) {
				dumpall = 1;
				break;
			}
			oldline = (oldline + 1) % BACKLINES;
		}

		if (!dumpall) {

			for (count = 1; count <= BACKLINES; count++) {
				fputs(lastfew[(oldline + count)%BACKLINES] + 18, outfile);
			}

			for (count = 0; (count < 9) && (!feof(thefile)) ; count++) {
				fgets(lastfew[0], MAX_LOG_LEN, thefile);
				fputs(lastfew[0] + 18, outfile);
			}
		}
		int Discard = 0;
		while (!feof(thefile)) {
			fgets(lastfew[0], MAX_LOG_LEN, thefile);
			if (strstr(lastfew[0], "PC: ")
				|| strstr(lastfew[0], "LR: ")
				|| strstr(lastfew[0], "IP: ")
				|| strstr(lastfew[0], "FP: ")
				|| strstr(lastfew[0], "Stack: "))
				Discard = 1;
			else if (strstr(lastfew[0], "R0: ")
				|| strstr(lastfew[0], "R1: ")
				|| strstr(lastfew[0], "R2: ")
				|| strstr(lastfew[0], "R3: ")
				|| strstr(lastfew[0], "R4: ")
				|| strstr(lastfew[0], "R5: ")
				|| strstr(lastfew[0], "R6: ")
				|| strstr(lastfew[0], "R7: ")
				|| strstr(lastfew[0], "R8: ")
				|| strstr(lastfew[0], "R9: ")
				|| strstr(lastfew[0], "SP: ")
				|| strstr(lastfew[0], "Code: ")
				|| strstr(lastfew[0], "Backtrace:")
				|| strstr(lastfew[0], "Kernel panic - "))
				Discard = 0;
			if(!Discard){
				fputs(lastfew[0] + 18,outfile);
				if (strstr(lastfew[0], "Code:"))
					Discard = 1;
			}

		}
		fflush(outfile);

		/* Here we pick up the last 7KB
		   because the Blur Checkin Service * defined an 8KB limitation
		   for the uploaded data. 1KB spared for * the version data.

		   fseek(thefile, 0, SEEK_END);
		   offset = ftell(thefile);
		   if (offset == -1) {
		   break;
		   }
		   offset = offset > (10240) ? (offset - 10240) : 0;
		   if(fseek(thefile, offset, SEEK_SET) < 0) {
		   break;
		   }

		   while (!feof(thefile)) {
		   fgets(buf, 256, thefile);
		   fputs(buf + 18,outfile);
		   }
		   fflush(outfile);
		*/
	} while (0);

	if (outfile != NULL){
		fclose(outfile);
	}

}

struct FileRec * FileRecCopy( char *n, time_t t, int s){
	struct FileRec *filerec;
	filerec = (struct FileRec * )malloc (sizeof(struct FileRec));
	if (filerec == NULL)
		return first;
        filerec->name = (char *) malloc(strlen(n) + 1);
	filerec->prev = NULL;
	filerec->next = NULL;
        if (filerec->name == NULL)
	{
		free (filerec);
            	return first;
	}
	int idx = 0;
        for (idx=0; (filerec->name[idx] = n[idx]) ; idx++);
	filerec->size =s;
	filerec->timestamp = t;

        if (first == NULL)
		first = filerec;
        else {
		filerec->next = first;
		first->prev = filerec;
		first = filerec;
        }
	return first;
}

void FileRecDel(struct FileRec *filerec) {
	if (filerec->prev )
		filerec->prev->next=filerec->next;
	else
		first = filerec->next;
        if (filerec->next){
		filerec->next->prev= filerec->prev;
	}
	filerec->prev = NULL;
	filerec->next = NULL;
        free(filerec->name);
        free (filerec);
	filerec = NULL;
}



void checkdir(char *dirname)
{
	// Now loop through all entries in the /data/panicreports dir, summing
	// up their sizes.
	int totalsize = 0;

	DIR *dirptr = opendir(dirname);
	if (dirptr == NULL)
		return ;
	struct dirent *entry;
	struct FileRec *idx;
	struct FileRec *oldest = NULL;
	while ((entry = readdir(dirptr)) != NULL) {
		char fullpath[FILENAMELEN];
		if ((strcmp(".", (char *) entry->d_name) == 0)
			|| (strcmp("..", (char *) entry->d_name) == 0))
			continue;
		struct stat statinfo;

		snprintf(fullpath, sizeof(fullpath),"%s/%s", dirname, (char *) entry->d_name);

		if (lstat(fullpath, &statinfo))
			printf("lstat failed for some odd reason!\n");
		first = FileRecCopy(fullpath, statinfo.st_mtime, statinfo.st_size);
		totalsize += statinfo.st_size;
	}

	while (totalsize > MAXTOTALSIZE) {
		idx = first;
		while (idx) {
			if (oldest) {
				if (oldest->timestamp > idx->timestamp)
					oldest = idx;
			} else {
				oldest = idx;
			}
			idx = idx->next;
		}
		if(oldest){
			unlink(oldest->name);
			totalsize -= oldest->size;
			FileRecDel(oldest);
			oldest = NULL;
		}
	}
	while (first){FileRecDel(first);}
	closedir(dirptr);
}

int main()
{
	int f_apanic, file_powerup, file_bppanic, i, f_lastkmsg;
	struct stat statinfo;
	unsigned long long pb_statinfo_size = 0;
	unsigned long long pb_statinfo_time = 0;
	char value[PROPERTY_VALUE_MAX];
	FILE *fp_apanic_backup_stat = NULL;


	/* Create the panic reports folders if they're not existing */
	if (access(MODEMDIR, F_OK) == -1) {
		int res = mkdir(MODEMDIR, 0775);
		chown(MODEMDIR, AID_SYSTEM, AID_SYSTEM);
	}
	if (access(KERNELDIR, F_OK) == -1) {
		int res = mkdir(KERNELDIR, 0775);
		chown(KERNELDIR, AID_SYSTEM, AID_SYSTEM);
	}

	/* Read the properties' values which enable/disable the reporting */
	for (i = 0; property_and_bit_position[i].property_name != NULL; i++) {
		property_get(property_and_bit_position[i].property_name, value, "1");
		if (strncmp(value, "0", 1) != 0)
			panic_report_config |= property_and_bit_position[i].bit_position;
	}

	/* When there's a valid apanic_console file, report a normal kpanic */
	f_apanic = open("/data/dontpanic/apanic_console", O_RDONLY);
	if (f_apanic != -1) {
		/* we can't simply create a temporary file, keep the
		 * original time stamp to avoid redundant panic report */
		char modemfile[35+8+1];
		do {
			fp_apanic_backup_stat = fopen(APANIC_STATE_FILE, "r");
			if (fp_apanic_backup_stat) {
				fscanf(fp_apanic_backup_stat, "%llu\t%llu\n", &pb_statinfo_size, &pb_statinfo_time);
				fclose (fp_apanic_backup_stat);
			}
			/* If we have a report file with the same time stamp of current
			 * apanic_console, don't do repot again */
			if ((fstat(f_apanic, &statinfo)) != 0) {
				break;
			}

			if ((pb_statinfo_size == (unsigned long long)statinfo.st_size) &&
				(pb_statinfo_time == (unsigned long long)statinfo.st_mtime)){
				break;
			}
			fp_apanic_backup_stat = fopen(APANIC_STATE_FILE, "w+");
			if (fp_apanic_backup_stat) {
				fprintf(fp_apanic_backup_stat, "%llu\t%llu\n",
				(unsigned long long)statinfo.st_size, (unsigned long long)statinfo.st_mtime);
				fchown(fileno(fp_apanic_backup_stat), AID_SYSTEM, AID_LOG);
				fchmod(fileno(fp_apanic_backup_stat), 0640);
				fclose (fp_apanic_backup_stat);
			}
			snprintf(modemfile, 35+8+1, "/data/kernelpanics/ap_kernel_panic_%lX", statinfo.st_mtime);

			if (access(modemfile, F_OK) == -1) {
				Report_Flag=1;
				if (panic_report_config & ENABLE_KERNEL_PANIC_REPORT) {
					readFile(modemfile, f_apanic);
					/* Set kernel panic bug to highest priority */
					close(f_apanic);
					return 0;
				}
			}
		} while(0);
		close(f_apanic);
	}

	file_powerup = open("/proc/bootinfo", O_RDONLY);
	if (file_powerup != -1) {
		/* If there's no apanic_console, we'll see if a wdt report need
		 * be sent */
		writePanicData(file_powerup);
	}
	close(file_powerup);
	checkdir(KERNELDIR);
	return 0;
}
