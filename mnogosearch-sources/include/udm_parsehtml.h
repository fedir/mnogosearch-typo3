/* Copyright (C) 2000-2011 Lavtech.com corp. All rights reserved.

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

#ifndef _UDM_PARSE_HTML_H
#define _UDM_PARSE_HTML_H

/* HTML parser states */
#define UDM_HTML_TAG	1
#define UDM_HTML_TXT	2
#define UDM_HTML_COM	3

typedef struct udm_langstack_struct {
	char *tag;
	char lang[4];
	struct udm_langstack_struct *prev;
} UDM_LANGSTACK;

#if (WIN32|WINNT)
#undef ISTAG
#endif
#define ISTAG(n,s)	(!strncasecmp(tag->toks[n].name,s,tag->toks[n].nlen)&&(tag->toks[n].nlen==strlen(s)))

extern int    UdmHTMLParse(UDM_AGENT*, UDM_DOCUMENT*);
extern int UdmParseURLText(UDM_AGENT*, UDM_DOCUMENT*);
extern int    UdmParseText(UDM_AGENT*, UDM_DOCUMENT*);
extern int UdmParseHeaders(UDM_AGENT*, UDM_DOCUMENT*);
extern int UdmPrepareWords(UDM_AGENT*, UDM_DOCUMENT*);

extern const char * UdmHTMLToken(const char * s, const char ** lt,UDM_HTMLTOK *t);
extern int UdmHTMLParseTag(UDM_HTMLTOK * tag,UDM_DOCUMENT * Doc);
extern int UdmHTMLTOKInit(UDM_HTMLTOK *t);

#endif
