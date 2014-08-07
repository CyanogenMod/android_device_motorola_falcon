/********************************************************************
 * Copyright (C) 2009 Motorola, All Rights Reserved
 *
 * File Name: rambuf.c
 *
 * General Description: This file provides RAM buffer functionality
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

/* Includes */
/* Lib C includes */
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <log/logger.h>
#include <log/logd.h>
#include <log/logprint.h>

/* Aplogd includes */
#include "rambuf.h"
#include "aplogd.h"
//#include "aplogd_util.h"
#include "log_io.h"
#include "rambuf.h"
/* Macros */

int aplogd_ram_buff_size = 8*1024;
char *aplogd_ram_buffer = NULL;

/* Functions */

/* aplogd_rambuf_init()
 *
 * Description: This function performs the necessary setup for the RAM buffer
 *
 * Return: (int) 0 on success
 *
 * Notes: None
 */
int aplogd_rambuf_init(void)
{
	DPRINT("Initializing ram buffer\n");
	int i;
	aplogd_ram_buffer = (char *)malloc(aplogd_ram_buff_size * (APLOGD_INPUT_LAST +1));
	if(!aplogd_ram_buffer)
		exit(EXIT_FAILURE);
	for (i = 0; i <=APLOGD_INPUT_LAST ; i++){

		aplogd_io_array[i].input_buf_base = aplogd_ram_buffer + i * aplogd_ram_buff_size;
		aplogd_io_array[i].input_buf = aplogd_io_array[i].input_buf_base;

	}
	return 0;
}

/* aplogd_rambuf_destroy()
 *
 * Description: Cleans up RAM buf specific items
 *
 * Return: None
 *
 * Notes: None
 */
void aplogd_rambuf_destroy(void)
{
	int i;
	for (i = 0; i < APLOGD_MAX_IO_FDS; i++){
		aplogd_io_array[i].input_buf_base = NULL;
	}
	free(aplogd_ram_buffer);
	return;
}

/* aplogd_rambuf_space()
 *
 * Description: This function returns the amount of free space in the buffer
 *
 * @buffer_type: (int) whether to get the size of the input or output buffer
 *
 * Return: (int) amount of free space in the buffer; -1 on failure
 *
 * Notes: The rambuf lock must be held before calling this function.
 */

int aplogd_rambuf_space(int index)
{
	int temp_filled = aplogd_rambuf_bytes_filled(index);
	if (temp_filled < 0 || temp_filled > aplogd_ram_buff_size)
	{
		WPRINT("Could not get bytes filled\n");
		return -1;
	}
	return (aplogd_ram_buff_size - temp_filled);
}

/* aplogd_rambuf_bytes_filled()
 *
 * Description: This function returns the number of bytes that are free in a
 *              buffer
 *
 * @buffer_type: (int) input or output buffer choice
 *
 * Return: (int) number of bytes filled in the buffer
 *
 * Notes: Rambuf lock must be held before calling this function
 */

int aplogd_rambuf_bytes_filled(int index)
{
	int ret_val = -1;

	ret_val = (aplogd_io_array[index].input_buf - aplogd_io_array[index].input_buf_base);
	return ret_val;
}


/* aplogd_rambuf_output()
 *
 * Description: This function calls the associated writev to the passed output
 * struct
 *
 * @index: pointer to the input/output file
 *
 * Output: (int) 0 on full write; -1 on partial or no write
 *
 * Notes: This function will cleanup this list after outputting its contents;
 *        it should only be called when you are ready to output data.
 */

int aplogd_rambuf_output(int index)
{
	int ret_val = 0;
	int need_written = 0;
	int size_written = 0;

	/* From here on out we can only manipulate the output list */

	VPRINT("output index:%d\n", index );
	need_written=aplogd_rambuf_bytes_filled(index);
	VPRINT("need_written=%d.\n",need_written);
	if(!aplogd_io_array[index].input_buf_base || !(need_written >0))
	{
		return -1;
	}
	if (aplogd_io_array[index].output_fd < 0)
	{
		WPRINT("output fd hasn't been initialized.\n");
		aplogd_io_array[index].input_buf = aplogd_io_array[index].input_buf_base;
		ret_val = -1;
		return ret_val;
	}

	while(need_written >0) {
		size_written = write(aplogd_io_array[index].output_fd, (void*)aplogd_io_array[index].input_buf_base, need_written);
		if (size_written < 0 )
		{
			EPRINT("output write: size_written=%d, errno=%d\n", size_written, errno);
			if (ENOSPC == errno)
				aplogd_close_output();
			if (EBADF == errno)
				aplogd_io_array[index].output_fd=-1;
			ret_val=-1;
			return ret_val;
		}
		need_written -= size_written;
		total_write_size+=size_written;
	}
	aplogd_io_array[index].input_buf = aplogd_io_array[index].input_buf_base;
        if (total_write_size >=aplogd_logfile_max)
        {
		ALOGI("aplogd_logfile_max=%d, total_write_size=%d\n",aplogd_logfile_max, total_write_size);
		aplogd_close_output();
		aplogd_io_backupall(g_current_storage);
		aplogd_output_setup(g_current_storage);
		total_write_size=0;
        }
	return ret_val;
}

void aplogd_rambuf_outputall()
{
	int i;
	for (i=0;i<=APLOGD_INPUT_LAST;i++)
		aplogd_rambuf_output(i);
}
