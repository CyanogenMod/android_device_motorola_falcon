/********************************************************************
 * Copyright (C) 2009 Motorola, All Rights Reserved
 *
 * File Name: aplogd_util.h
 *
 * General Description: This file provides the utility function for aplogd
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
 * 10/11/2010    Motorola        Initial creation.
 *
 *********************************************************************/

/***********************************************************
 * Local Includes
 ************************************************************/
/* C Standard Includes */
/* Motorola Includes */
/***********************************************************
 * Local Constants
 ***********************************************************/

int aplogd_util_gzip(char *src);
int aplogd_util_move(char *src, char *dst, char *pstr);
long aplogd_util_cleardir(char *dir, int max_count);
void aplogd_move_logs(STORAGE_T,STORAGE_T);
unsigned int aplogd_get_filesize(STORAGE_T);
STORAGE_T aplogd_calc_storage_pref();
int aplogd_is_collected(char stream);
