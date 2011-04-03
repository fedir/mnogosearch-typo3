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

#ifndef _UDM_TEMPLATE_H
#define _UDM_TEMPLATE_H

extern int  UdmTemplateLoad(UDM_AGENT *Agent, const char *tname, UDM_VARLIST *tmpl);
extern __C_LINK void __UDMCALL UdmTemplatePrint(UDM_AGENT * Agent, FILE *stream, char *dst, size_t dst_len, 
			     UDM_VARLIST *vars, UDM_VARLIST *t, const char *where);
extern __C_LINK size_t __UDMCALL UdmTemplatePrintVar(UDM_ENV *Conf,
                                                     FILE *stream,
                                                     char *dst, size_t dlen,
                                                     const char *val,
                                                     int type,
                                                     const char *hlbeg,
                                                     const char *hlend);
#endif
