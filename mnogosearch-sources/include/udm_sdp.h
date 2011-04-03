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

#ifndef _UDM_SDP_H
#define _UDM_SDP_H

#include "udm_db_int.h"

extern __C_LINK int __UDMCALL UdmFindWordsSearchd(UDM_AGENT *query,UDM_RESULT *Res, UDM_DB *searchd);
extern __C_LINK int __UDMCALL UdmSearchdCatAction(UDM_AGENT *A, UDM_CATEGORY *C, int cmd, void *vdb);
extern __C_LINK int __UDMCALL UdmSearchdURLAction(UDM_AGENT *A, UDM_DOCUMENT *D, int cmd, void *vdb);
extern __C_LINK int __UDMCALL UdmResAddDocInfoSearchd(UDM_AGENT * query,
                                                      UDM_RESULT *TmpRes,
                                                      UDM_DB *db,
                                                      UDM_RESULT *Res,
                                                      size_t clnum);
extern int UdmCloneListSearchd(UDM_AGENT * Indexer, UDM_DOCUMENT *Doc, UDM_RESULT *Res, UDM_DB *db);

#endif
