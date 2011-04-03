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

#ifndef _UDM_VARS_H
#define _UDM_VARS_H

extern __C_LINK UDM_VARLIST * __UDMCALL UdmVarListInit(UDM_VARLIST * vars);
extern __C_LINK void __UDMCALL UdmVarListFree(UDM_VARLIST * vars);
extern void UdmVarFree(UDM_VAR *);
extern int UdmVarType(UDM_VAR *);
extern int UdmVarIntVal(UDM_VAR *);

extern int UdmVarListAddEnviron(UDM_VARLIST *vars, const char *name);

extern int UdmVarAppendStrn(UDM_VAR *var, const char *val, size_t len);
extern __C_LINK int __UDMCALL UdmVarListAddStr(UDM_VARLIST * vars,const char * name, const char * val);
extern int UdmVarListAddQueryStr(UDM_VARLIST * vars,const char * name, const char * val);
extern int UdmVarListAddInt(UDM_VARLIST * vars,const char * name, int val);
extern int UdmVarListAddUnsigned(UDM_VARLIST * vars, const char * name, uint4 val);
extern int UdmVarListAddDouble(UDM_VARLIST * vars, const char * name, double val);
extern int UdmVarListAdd(UDM_VARLIST * Lst,UDM_VAR * S);
extern int UdmVarListDel(UDM_VARLIST * Lst,const char *name);
extern int UdmVarListDelBySection(UDM_VARLIST * Lst, int secno);
extern int UdmVarListDelByName(UDM_VARLIST * Lst, const char *name);
extern int UdmVarListReplace(UDM_VARLIST * Lst,UDM_VAR * S);
extern int UdmVarListCreateObject(UDM_VARLIST *Lst,
                                  const char *name, int type,
                                  UDM_VAR **args, size_t nargs);
extern int UdmVarListInvokeMethod(UDM_VARLIST *Lst,
                                  UDM_VAR *Var, const char *method,
                                  UDM_VAR **args, size_t nargs);
extern int UdmVarListAddLst(UDM_VARLIST * Lst,UDM_VARLIST *Src, const char *name, const char *w);
extern int UdmVarListInsLst(UDM_VARLIST * Lst,UDM_VARLIST *Src, const char *name, const char *w);
extern int UdmVarListReplaceLst(UDM_VARLIST * Lst,UDM_VARLIST *Src, const char *name, const char *w);
extern int UdmVarListMerge(UDM_VARLIST * Lst,UDM_VARLIST *Src1, UDM_VARLIST *Src2);

extern __C_LINK int __UDMCALL UdmVarListReplaceStr(UDM_VARLIST * vars,const char * name,const char * val);
extern __C_LINK int __UDMCALL UdmVarListReplaceInt(UDM_VARLIST * vars,const char * name,int val);
extern __C_LINK int __UDMCALL UdmVarListReplaceUnsigned(UDM_VARLIST * vars, const char * name, uint4 val);
extern int UdmVarListReplaceDouble(UDM_VARLIST * vars, const char * name, double val);

extern int UdmVarListReplaceStrn(UDM_VARLIST *vars, const char *name,
                                 const char *val, size_t vallen);

size_t UdmDSTRParse(UDM_DSTR *d, const char *fmt, UDM_VARLIST *L);

extern int UdmVarListInsStr(UDM_VARLIST * vars,const char * name,const char * val);
extern int UdmVarListInsInt(UDM_VARLIST * vars,const char * name,int val);

extern UDM_VAR *UdmVarListFind(UDM_VARLIST * vars,const char * name);
extern UDM_VAR *UdmVarListFindWithValue(UDM_VARLIST * vars,const char * name,const char * val);

extern __C_LINK const char * __UDMCALL UdmVarListFindStr(UDM_VARLIST * vars,const char * name,const char * defval);
extern __C_LINK int          UdmVarListFindInt(UDM_VARLIST * vars,const char * name,int defval);
extern __C_LINK int          UdmVarListFindBool(UDM_VARLIST * vars,const char * name,int defval);
extern __C_LINK unsigned     UdmVarListFindUnsigned(UDM_VARLIST * vars, const char * name, unsigned defval);
extern __C_LINK double       UdmVarListFindDouble(UDM_VARLIST * vars, const char * name, double defval);

extern int UdmVarListConvert(UDM_VARLIST *L, UDM_CONV *conv);
extern void UdmFeatures(UDM_VARLIST *L);
#endif
