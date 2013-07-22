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
#include <string.h>
#include <sys/types.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_id3.h"
#include "udm_vars.h"
#include "udm_textlist.h"

static int add_var(UDM_DOCUMENT *Doc, char *name,char *val){
	UDM_VAR		*Sec;
	
	if((Sec=UdmVarListFind(&Doc->Sections,name))){
		UDM_TEXTITEM	Item;
		bzero((void*)&Item, sizeof(Item));
		Item.section=Sec->section;
		Item.str=val;
		Item.section_name=name;
		Item.flags= 0;
		UdmTextListAdd(&Doc->TextList,&Item);
	}
	return UDM_OK;
}

static int get_tag(UDM_DOCUMENT *Doc){
	char	year[5]="";
	char	album[31];
	char	artist[31];
	char	songname[31];
	char	name[31];
	char	*tag=Doc->Buf.buf+Doc->Buf.size-128;
	
	memcpy(songname,tag+3,30);
	songname[30]='\0';
	
	memcpy(artist,tag+33,30);
	artist[30]='\0';
	
	memcpy(album,tag+63,30);
	album[30]='\0';
	
	memcpy(year,tag+93,5);
	year[4]='\0';
	
	UdmRTrim(songname, " ");
	UdmRTrim(artist, " ");
	UdmRTrim(album, " ");
	
	strcpy(name,"MP3.Song");
	add_var(Doc,name,songname);
	
	strcpy(name,"MP3.Album");
	add_var(Doc,name,album);
	
	strcpy(name,"MP3.Artist");
	add_var(Doc,name,artist);
	
	strcpy(name,"MP3.Year");
	add_var(Doc,name,year);
	
	return UDM_OK;
}

/* 
	id3 header v2.3.0 http://www.id3.org/
	ID3	3
	version	1+1
    	flag	1
	size	4
*/

static char* get_id3(UDM_DOCUMENT *Doc){
	char	*ch;
	char	*buf_in=Doc->Buf.content;
	size_t	hdr_len=Doc->Buf.content-Doc->Buf.buf;
	size_t	cont_len=Doc->Buf.size-hdr_len;
	char	*artist=NULL;
	char	*album=NULL;
	char	*songname=NULL;
	char	name[64];
	int	tagcount=0;
	
	if(hdr_len>Doc->Buf.size)return NULL;
	
	ch = buf_in;
	
	if ( *(ch+6) == 'b'){
		/*
			extened header:
			size	4
			flag	2
			size of pagging 4
		*/
		ch +=20;
	}else{
		ch +=10;
	}
	
	while(1){
		/*
			frame header:
			frame id 4
			size	 4
			flags	 2 
		*/
		
		unsigned char	frame_size = (unsigned char)*(ch+7);
		size_t		len = frame_size>cont_len?cont_len:frame_size;
		
		if (!strncmp(ch , "TPE1", 4)){
			ch +=10;
			artist = UdmMalloc(len+1);
			udm_snprintf(artist, len, "%s", ch+1);
			artist[len]='\0';
			UdmRTrim(artist," ");
			if (++tagcount == 3)
					break;
		}else if(!strncmp(ch , "TALB", 4)){
			ch +=10;
			album = UdmMalloc(len+1);
			udm_snprintf(album, len, "%s", ch+1);
			album[len]='\0';
			UdmRTrim(album," ");
			if (++tagcount == 3)
				break;
		}else if(!strncmp(ch , "TIT2", 4) ){
			ch +=10;
			songname = UdmMalloc(len+1);
			udm_snprintf(songname, len, "%s", ch+1);
			songname[len]='\0';
			UdmRTrim(songname," ");
			if (++tagcount == 3)
				break;
		}else if (((size_t)(ch - buf_in)+frame_size) <cont_len){
			ch +=10;
		}else{
			break;
		}
		ch +=frame_size;
	}
	
	if (!artist) artist = (char*)UdmStrdup("");
	if (!album) album = (char*)UdmStrdup("");
	if (!songname) songname = (char*)UdmStrdup("");
	
	strcpy(name,"MP3.Song");
	add_var(Doc,name,songname);
	
	strcpy(name,"MP3.Album");
	add_var(Doc,name,album);
	
	strcpy(name,"MP3.Artist");
	add_var(Doc,name,artist);
	
	UDM_FREE(artist);
	UDM_FREE(album);
	UDM_FREE(songname);
	
	return UDM_OK;
}

int UdmMP3Type(UDM_DOCUMENT *Doc){
	int	hd=((unsigned char)(Doc->Buf.content[0])+256*(unsigned char)(Doc->Buf.content[1])) & 0xf0ff;
	
	if(hd == 0xf0ff)
		return UDM_MP3_TAG;
        
        if (!strncmp(Doc->Buf.content,"RIFF",4))
		return UDM_MP3_RIFF;
	
        if (!strncmp(Doc->Buf.content, "ID3", 3))
		return UDM_MP3_ID3;
	
	return UDM_MP3_UNKNOWN;
}

int UdmMP3Parse(UDM_AGENT *A,UDM_DOCUMENT *Doc){
	if (!strncmp(Doc->Buf.content, "ID3", 3))
		get_id3(Doc);
	
	if ((Doc->Buf.size>=128) && (!strncmp(Doc->Buf.buf+Doc->Buf.size-128, "TAG", 3)))
		get_tag(Doc);
	
	return UDM_OK;
}
