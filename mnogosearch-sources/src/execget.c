/* Copyright (C) 2000-2013 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "udm_config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#ifdef WIN32
#include <io.h>
#endif

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_execget.h"
#include "udm_log.h"


/*
#define DEBUG_EXECGET
*/

int UdmExecGet(UDM_AGENT * Indexer,UDM_DOCUMENT * Doc){
	char * args;
	char cmdline[1024];
	FILE * f;
	UDM_URL *Url=&Doc->CurURL;

	Doc->Buf.buf[Doc->Buf.size = 0] = '\0';

	if((args = strchr(UDM_NULL2EMPTY(Url->filename), '?'))) {
		*args='\0';
		args++;
	}
	sprintf(cmdline, "%s%s", UDM_NULL2EMPTY(Url->path), UDM_NULL2EMPTY(Url->filename));
	
	if(!strcmp(UDM_NULL2EMPTY(Url->schema), "exec")) {
		if(args){
			sprintf(UDM_STREND(cmdline)," \"%s\"",args);
		}
	}else
	if(!strcmp(UDM_NULL2EMPTY(Url->schema), "cgi")) {
		/* Non-parsed-headers CGI return HTTP status itself */
		if(strncasecmp(UDM_NULL2EMPTY(Url->filename), "nph-", 4)) {
			sprintf(Doc->Buf.buf,"HTTP/1.0 200 OK\r\n");
			Doc->Buf.size = strlen(Doc->Buf.buf);
		}
		UdmSetEnv("QUERY_STRING",args?args:"");
		UdmSetEnv("REQUEST_METHOD","GET");
	}

#ifdef DEBUG_EXECGET
	fprintf(stderr,"cmd='%s'\n",cmdline);
#endif

	UdmLog(Indexer,UDM_LOG_DEBUG,"Starting program '%s'",cmdline);

	f=popen(cmdline,"r");

	/* Remove set variables */
	if(!strcmp(UDM_NULL2EMPTY(Url->schema), "cgi")) {
		UdmUnsetEnv("REQUEST_METHOD");
		UdmUnsetEnv("QUERY_STRING");
	}

	if(f){
		int fd,bytes;
		
		fd=fileno(f);
		while((bytes = read(fd, Doc->Buf.buf + Doc->Buf.size, Doc->Buf.maxsize - Doc->Buf.size))){
			Doc->Buf.size += bytes;
			Doc->Buf.buf[Doc->Buf.size] = '\0';
		}
		pclose(f);
	}else{
		int status;
		printf("error=%s\n",strerror(errno));
		switch(errno){
			case ENOENT: status=404;break; /* Not found */
			case EACCES: status=403;break; /* Forbidden*/
			default: status=500;
		}
		sprintf(Doc->Buf.buf,"HTTP/1.0 %d %s\r\n\r\n",status,strerror(errno));
		Doc->Buf.size = strlen(Doc->Buf.buf);
	}
#ifdef DEBUG_EXECGET
	fprintf(stderr,"buf='%s'\n",Indexer->buf);
#endif
	return(Doc->Buf.size);
}

