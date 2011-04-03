/*
* parsedate.c
* author: Heiko Stoermer, innominate AG, stoermer@innominate.de
* 20000316
*
* 23 Aug 2000, modified by Kir <kir@sever.net>
*/

#include "udm_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "udm_common.h"
#include "udm_parsedate.h"
#include "udm_xmalloc.h"
#include "udm_utils.h"
#include "udm_sdp.h"

#define BUFF 20

static int get_month(char * month)
{
	if(strcmp(month,"Jan") == 0)
		return 1;
	else if(strcmp(month,"Feb") == 0)
		return 2;
	else if(strcmp(month,"Mar") == 0)
		return 3;
	else if(strcmp(month,"Apr") == 0)
		return 4;
	else if(strcmp(month,"May") == 0)
		return 5;
	else if(strcmp(month,"Jun") == 0)
		return 6;
	else if(strcmp(month,"Jul") == 0)
		return 7;
	else if(strcmp(month,"Aug") == 0)
		return 8;
	else if(strcmp(month,"Sep") == 0)
		return 9;
	else if(strcmp(month,"Oct") == 0)
		return 10;
	else if(strcmp(month,"Nov") == 0)
		return 11;
	else if(strcmp(month,"Dec") == 0)
		return 12;
	else
		return 0;
}

/* Returns allocated pointer to string 
 * containing the sql-conformant datetime type.
 *
 * Processes nntp datestrings of the following types:
 * 1: 17 Jun 1999 11:05:42 GMT
 * 2: Wed, 08 Mar 2000 13:52:43 +0100
 * 3: Tuesday, 28-Nov-95 9:15:26 GMT
 *
 * 23 Aug 2000, modified by Kir <kir@sever.net>
 * to support the (3) date format
 * Bug report was submitted by Nagy Erno <ned@elte.hu>
 */

char * UdmDateParse(const char * datestring)
{
	
	/* buffers */
	char year  [BUFF] = "";
	char month [BUFF] = "";
	char date  [BUFF] = "";
	char timebuf[BUFF] = "";
	char * tokens[4];
	/* result */
	char * res = NULL;
	/* some ints for substring indices */
	SDPALIGN pos  = 0;
	size_t allocsize = 0;

	/* pointers for parsing */
	char * next = NULL;
	char * prev = NULL;
	size_t len = 0;
	/* working copy and copy-copy */
	char * copy = NULL;
	char * copyref = NULL;
	/* we need this for the different types */
	int offset = 0;
	/* initialize everythign nicely */
	tokens[0] = date;
	tokens[1] = month;
	tokens[2] = year;
	tokens[3] = timebuf;

	if(strlen(datestring))
	{
		/* select string type */
		if((pos = (SDPALIGN)strchr(datestring,',')))
		{
			/* simply skip the first characters containing the weekday string */
			offset = (int) ( (SDPALIGN)pos - (SDPALIGN)datestring);	
			offset += 2; /* skip comma and whitespace */
		}
		copy = UdmMalloc(strlen(datestring+offset)+1);
		strcpy(copy,datestring+offset);
		copyref = copy; /* save the pointer for deallocation later */
	
		/* now we can start.*/
		/* split up the string*/
		pos = 0;
		prev = copy;
		next = strtok(copy," -");
		while(pos < 4)
		{
			if((next = strtok(NULL," -")) == NULL) /* last element */
				len = strlen(prev);
			else
				len = (size_t) (next-prev);
			if(len > BUFF) /* unexpected token length*/
				return NULL;
			strncpy(tokens[pos],prev,len);
			/* preparations for the next run*/
			prev = next;
			++pos;
			 /*next = strtok(NULL," ");*/
		}


		/* o.k., this type of parsing is no fexible enough... :(*/
		/*
		strncpy(date,datestring+offset,2);
		date[2] = 0;
		strncpy(month,datestring+offset+3,3);
		month[3] = 0;
		strncpy(year,datestring+offset+7,4); 
		year[4] = 0;
		strncpy(timebuf,datestring+offset+12,8);	
		timebuf[8] = 0;
		*/
		
		if (strlen(year)==2){
			/* We have two-digit year, need to convert to four-digit */
			year[2]=year[0];
			year[3]=year[1];

			/* Here we assume that 70 and up are 19xx,
			 * and 00-69 are 20xx years. --kir.
			 */
			if (year[0]>'6'){
				year[0]='1';
				year[1]='9';    
			}
			else{
				year[0]='2';
				year[1]='0';    
			}
		}

		/* we allocate 4 extra characters for separators and \0  */
		allocsize = strlen(date)+strlen(month)+strlen(year)+strlen(timebuf)+ 4;
		res = UdmMalloc(allocsize);
		udm_snprintf(res,allocsize,"%s-%02i-%02i %s",year,get_month(month),atoi(date),timebuf);
		res[allocsize-1] = '\0';
		/* clean up nicely. we dont want memory leaks within 100 lines...*/
		UDM_FREE(copyref);
	} else {
		if ((res = UdmMalloc(20)))
			sprintf(res, "1970-01-01 00:01");
	}
	return res;
}
