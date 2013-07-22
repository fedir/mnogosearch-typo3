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

/*
 * search_tl.c - search using time limits
 * (C) 2000 Kir <kir@sever.net>, UdmSearch Developers Team.
 */
 
#include "udm_config.h"

#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>

#include "udm_common.h"
#include "udm_search_tl.h"
#include "udm_utils.h"

extern long tz_offset;

/******************
 * Data definitions 
 */

/* search with time limits */
struct time_search_type{
	int type;
	const char *name;
};

struct time_search_type dt_types[]=
	{{ 1, "back"},
	 { 2, "er"},
	 { 3, "range"},
	 /* add more types just before this line */
	 { 0, ""}};

int getSTLType(char *type_str){
	int i=0;
	size_t len=0;

	while (dt_types[i].type){
		len=strlen(dt_types[i].name);
		if (type_str[len]=='\0') /* strings are equal in size */
			if ( !strncmp(type_str, dt_types[i].name, len) )
				return dt_types[i].type;
		i++;
	}
	return 0; /* not found */
}

time_t d_m_y2time_t(int d, int m, int y){
	struct tm t;
	time_t ts;
	
	bzero((void*)&t, sizeof(t));
	t.tm_mday=d;
	t.tm_mon=m-1;
	t.tm_year=y-1900;
	ts=mktime(&t);
	if (ts>0)
		return ts-tz_offset;
	else
		return -1; /* error */
}

time_t dmy2time_t(char *str){
	int d,m,y;
	char *beg,*end;
		
	/* Convert string  "%d/%m/%Y" to time_t */
	
	beg=str;
	if(!(end=strchr(beg,'/')))return(-1);
	d=atoi(beg);
	beg=end+1;
	if(!(end=strchr(beg,'/')))return(-1);
	m=atoi(beg);
	beg=end+1;
	y=atoi(beg);
	return(d_m_y2time_t(d,m,y));
}


/* * * * * * * * * * * * *
 * BELOW IS DEBUG CRAP...
 */

/*
#define TL_DEBUG_GSTLT
*/

#ifdef TL_DEBUG_GSTLT
int main(int argc, char **argv){

	char *str;
	
	if (argc>1)
		str=argv[1];
	else{
		printf("Usage: %s search_type_string\n", argv[0]);
		return 1;
	}
	
	printf("str='%s'\ttype=%i\n", str, getSTLType(str));
	return 0;
}
#endif /* TL_DEBUG_GSTLT */
