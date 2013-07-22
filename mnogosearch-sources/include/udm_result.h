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

#ifndef _UDM_RESULT_H
#define _UDM_RESULT_H

extern UDM_RESULT   *UdmResultInit(UDM_RESULT*);
extern __C_LINK void __UDMCALL UdmResultFree(UDM_RESULT*);
extern int           UdmResultToTextBuf(UDM_RESULT *R,char *buf,size_t len);
extern int           UdmResultFromTextBuf(UDM_RESULT *R,char *buf);
extern int           UdmResultFromXML(UDM_AGENT *A, UDM_RESULT *Res, const char *s, size_t l, UDM_CHARSET *cs);

#endif
