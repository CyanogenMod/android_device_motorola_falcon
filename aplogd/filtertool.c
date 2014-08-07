/*
 * Copyright (c) 2009, The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Google, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Revision History
 *   Date          Author         Description
 * ----------    --------------  ------------------------------------------
 * 04/09/2009    Motorola        Initial creation
 *
 *
 */


#include <stdio.h>
#include <string.h>
#include <cutils/properties.h>

#include <sys/system_properties.h>

char filter_string[30]="ap.log.filter.MOT_";
char *filter_value;

static void propget(const char *key, const char *name,
                     void *user __attribute__((unused)))
{
    if(0==strncmp(key ,filter_string, strlen(filter_string)))
	    printf("%s:%s\n", key+strlen("ap.log.filter.MOT_"), name);
}

static void propset(const char *key, const char *name,
                     void *user __attribute__((unused)))
{
        property_set(filter_string, filter_value);
}

int __system_property_wait(prop_info *pi);

int main(int argc, char *argv[])
{
    int i;
    if (strcmp(argv[0],"logfilter-get")==0){
        if (argc == 1) {
          sprintf(filter_string,"ap.log.filter.MOT_");
          (void)property_list(propget, NULL);
        } else {
	  for(i=1;i<argc;i++){
          sprintf(filter_string,"ap.log.filter.MOT_%s",argv[i]);
	  (void)property_list(propget, NULL);
	  }
	}
    }else if  (strcmp(argv[0],"logfilter-set")==0){
        if (argc <3){
		printf("logfilter-set <filter-tag> <filter-prio>\n");
		return 0;
	}
        sprintf(filter_string,"ap.log.filter.MOT_%s",argv[1]);
        filter_value  = argv[2];
        (void)property_list(propset, NULL);
    }
    return 0;
}
