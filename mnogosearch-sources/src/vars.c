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

#include "udm_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_wild.h"
#include "udm_vars.h"
#include "udm_env.h"
#include "udm_agent.h"
#include "udm_result.h"
#include "udm_conf.h"
#include "udm_db.h"
#include "udm_sqldbms.h"
#include "udm_searchtool.h"
#include "udm_server.h"
#include "udm_log.h"

typedef struct udm_var_method_st
{
  const char *name;
  int (*func)(UDM_VARLIST *L, UDM_VAR *Self, UDM_VAR **args, size_t nargs);
} UDM_VARMETHOD;

typedef struct udm_var_handler_st
{
  int type;
  const char *type_name;
  int (*Create)(struct udm_var_handler_st *ha,
                UDM_VAR *dst, UDM_VAR **args, size_t nargs);
  void (*Free)(UDM_VAR *var);
  int (*CopyValue)(UDM_VAR *D, UDM_VAR *S);
  UDM_VARMETHOD *method;
} UDM_VARHANDLER;


int udm_l1tolower[256]=
{
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
  0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
  0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x5B,0x5C,0x5D,0x5E,0x5F,
  0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
  0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
  0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
  0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
  0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
  0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
  0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xD7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xDF,
  0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
  0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};


static int
varcmp(const UDM_VAR *uv1, const UDM_VAR *uv2)
{
  const char *us1= uv1->name;
  const char *us2= uv2->name;
  while (udm_l1tolower[(unsigned char)*us1] == udm_l1tolower[(unsigned char)*us2++])
    if (*us1++ == '\0')
      return 0;
  return udm_l1tolower[(unsigned char)*us1] -
         udm_l1tolower[(unsigned char)*--us2];
}


int UdmVarType(UDM_VAR *v)
{
  return v->handler->type;
}

/************************************************************/

extern UDM_VARHANDLER SimpleVar;
extern UDM_VARHANDLER ResultVar;
extern UDM_VARHANDLER SQLResultVar;

static void UdmVarFreeSimple(UDM_VAR *V)
{
  UDM_FREE(V->val);
}


static int UdmVarCopyValueSimple(UDM_VAR *D, UDM_VAR *S)
{
  D->handler= S->handler ? S->handler : &SimpleVar;
  D->section= S->section;
  D->maxlen= S->maxlen;
  D->curlen= S->curlen;
  D->flags= S->flags;
  if (!S->val)
  {
    D->val= NULL;
  }
  else if (S->maxlen == 0)
  {
    D->val= UdmStrdup(S->val);
  }
  else
  {
    size_t len= S->maxlen > S->curlen ? S->maxlen : S->curlen; 
    D->val = (char*)UdmMalloc(len + 4);
    memcpy(D->val, S->val, S->curlen);
    D->val[S->curlen]= '\0';
  }
  return UDM_OK;
}

UDM_VARHANDLER SimpleVar=
{
  UDM_VAR_STR,
  "Simple",
  NULL,
  UdmVarFreeSimple,
  UdmVarCopyValueSimple,
  NULL
};


static int UdmVarCopyValueEmptyStr(UDM_VAR *D)
{
  D->handler= &SimpleVar;
  D->section= 0;
  D->maxlen= 0;
  D->curlen= 0;
  D->flags= 0;
  D->val= NULL;
  return UDM_OK;
}



/********************************************************/

static int UdmVarCreateEnv(UDM_VARHANDLER *ha, UDM_VAR *dst,
                           UDM_VAR **args, size_t nargs)
{
  UDM_ENV *Env;
  bzero((void*)dst,sizeof(*dst));
  dst->handler= ha;
  if (nargs)
  {
    dst->val= args[0]->val;
    dst->flags= UDM_VARFLAG_KEEPVAL;
  }
  else
  {
    Env= UdmEnvInit(NULL);
    dst->val= (char*) Env;
    UdmOpenLog("search.cgi", Env,
               !strcasecmp(UdmVarListFindStr(&Env->Vars, "Log2stderr", "yes"),"yes"));
  }
  return UDM_OK;
}


static void UdmVarFreeEnv(UDM_VAR *V)
{
  if (!(V->flags & UDM_VARFLAG_KEEPVAL))
    UdmEnvFree((UDM_ENV*)V->val);
}


static int UdmVarCopyValueEnv(UDM_VAR *D, UDM_VAR *S)
{
  UdmVarCopyValueEmptyStr(D);
  return UDM_OK;
}


static int UdmVarListMethodEnvAddLine(UDM_VARLIST *Lst, UDM_VAR *Var,
                                      UDM_VAR **args, size_t nargs)
{
  if (Var->handler->type == UDM_VAR_ENV)
  {
    int rc;
    UDM_CFG Cfg;
    UDM_AGENT Agent;
    UDM_SERVER Srv;
    UDM_ENV *Env= (UDM_ENV*)Var->val;
    UdmServerInit(&Srv);
    Env->Cfg_Srv= &Srv;
    bzero((void*)&Cfg, sizeof(Cfg));
    Cfg.Indexer= &Agent;
    Cfg.Srv= &Srv;
    Cfg.flags= UDM_FLAG_SPELL;
    UdmAgentInit(&Agent, Env, 0);
    rc= UdmEnvAddLine(&Cfg, args[0]->val);
    UdmAgentFree(&Agent);
    UdmServerFree(&Srv);
    Env->Cfg_Srv= NULL;
  }
  return UDM_OK;
}


#if 0
static int UdmVarListPrint(UDM_VARLIST *Vars)
{
  size_t i;
  for (i= 0; i < Vars->nvars; i++)
  {
  UDM_VAR *v= &Vars->Var[i];
  /*fprintf(stderr, "[%d] '%s'='%s'\n", i, v->name, v->val);*/
  }
  return UDM_OK;
}
#endif


/*
  Check if a VarList is sorted
  Return
    0 - sorted
    1 - almost sorted, only the last cooord is on a wrong position
    2 - very unsorted
*/
/*
static int
UdmVarListSortStatus(UDM_VARLIST *Vars)
{
  size_t var, nwrong;
  int res;
  for (nwrong=0, var=1; var < Vars->nvars; var++)
  {
    UDM_VAR *Var= &Vars->Var[var];
    res= varcmp(&Var[-1], &Var[0]);
    if (res > 0)
    {
      nwrong++;
      if (nwrong > 1)
        return nwrong;
    }
  }
  return nwrong ? (res > 0 ? 1 : 2) : 0;
}
*/

/*
  This function is called after UdmVarListAdd().
  Move the last added variable to its sorted place.
  
  If there are variables with the same name, the
  new variable is inserted after them.
*/
static void
UdmVarListMoveLast(UDM_VARLIST *Vars)
{
  size_t movevars;
  UDM_VAR *Last= &Vars->Var[Vars->nvars-1];
  UDM_VAR *Left= &Vars->Var[0];
  UDM_VAR *Right= &Vars->Var[Vars->nvars-1];

  for ( ; Left < Right ; )
  {
    UDM_VAR *Middle= Left + (Right - Left)/2;
    if (varcmp(Middle, Last) <= 0) /* We are looking for the last, see above */
      Left= Middle + 1;
    else
     Right= Middle;
  }

  if ((movevars= Vars->nvars - (Right - Vars->Var) - 1))
  {
    UDM_VAR Tmp= *Last;
    memmove(Right + 1, Right, movevars * sizeof(UDM_VAR));
    *Right= Tmp;
  }
}


static void
UdmVarListSort(UDM_VARLIST *Vars)
{
/*
  int res;
*/  
#ifdef DEBUG_VARS
  fprintf(stderr, "VarListSort: %d\n", Vars->nvars);
#endif

  if (Vars->nvars < 2)
    return;

/*
  if (!(res= UdmVarListSortStatus(Vars)))
    return;

  if (res == 1)
  {
    UdmVarListMoveLast(Vars);
    return;
  }
*/
  if (Vars->nvars == Vars->svars + 1)
    UdmVarListMoveLast(Vars);
  else
    UdmSort(Vars->Var, Vars->nvars, sizeof(UDM_VAR), (udm_qsort_cmp)varcmp);
  Vars->svars= Vars->nvars;
}


static int UdmVarListMethodEnvFind(UDM_VARLIST *Vars, UDM_VAR *Var,
                                   UDM_VAR **args, size_t nargs)
{
  if (Var->handler->type == UDM_VAR_ENV && nargs==2)
  {
    UDM_ENV *Env= (UDM_ENV*) Var->val;
    UDM_RESULT *Res;
    UDM_AGENT Agent;
    char resname[32]; /* We have to copy args[1]->name here
    because args[1] is not valid after first VarList operation */
    udm_snprintf(resname, sizeof(resname), "%s", args[1]->val);

    UdmEnvPrepare(Env);
    UdmAgentInit(&Agent, Env, 0);
    UdmVarListDel(&Env->Vars, "np");
    UdmVarListDel(&Env->Vars, "m");
    UdmVarListDelBySection(&Env->Vars, UDM_VARSRC_QSTRING);
    UdmParseQueryString(&Agent, &Env->Vars, args[0]->val);
    Res= UdmFind(&Agent);
    if (Res)
    {
      UDM_VAR *New;
      char prmname[64];
      UdmVarListDel(Vars, resname);
      UdmVarListAdd(Vars, NULL);
      New= &Vars->Var[Vars->nvars-1];
      New->val= (char*) Res;
      New->handler= &ResultVar;
      New->name= UdmStrdup(resname);
      UdmVarListSort(Vars);
      
      udm_snprintf(prmname, sizeof(prmname), "%s.first", resname);
      UdmVarListReplaceInt(Vars, prmname, (int)Res->first);
      udm_snprintf(prmname, sizeof(prmname), "%s.last", resname);
      UdmVarListReplaceInt(Vars, prmname, (int)Res->last);
      udm_snprintf(prmname, sizeof(prmname), "%s.total", resname);
      UdmVarListReplaceInt(Vars, prmname, (int)Res->total_found);
      udm_snprintf(prmname, sizeof(prmname), "%s.rows", resname);
      UdmVarListReplaceInt(Vars, prmname, (int)Res->num_rows);
    }
    UdmAgentFree(&Agent);
  }
  return UDM_OK;
}

static int UdmVarListMethodEnvSQLQuery(UDM_VARLIST *Vars, UDM_VAR *Var,
                                   UDM_VAR **args, size_t nargs)
{
#ifdef HAVE_SQL
  if (Var->handler->type == UDM_VAR_ENV && nargs==2)
  {
    UDM_ENV *Env= (UDM_ENV*) Var->val;
    UDM_SQLRES *Res;
    UDM_AGENT Agent;
    char resname[32]; /* We have to copy args[1]->name here
    because args[1] is not valid after first VarList operation */
    udm_snprintf(resname, sizeof(resname), "%s", args[1]->val);

    UdmEnvPrepare(Env);
    UdmAgentInit(&Agent, Env, 0);
    if (Agent.Conf->dbl.nitems > 0)
    {
      UDM_VAR *New;
      UDM_DB *db= &Agent.Conf->dbl.db[0];
      char prmname[64];
      Res= UdmMalloc(sizeof(UDM_SQLRES));
      UdmSQLQuery(db, Res, args[0]->val);
      UdmVarListDel(Vars, resname);
      UdmVarListAdd(Vars, NULL);
      New= &Vars->Var[Vars->nvars-1];
      New->val= (char*) Res;
      New->handler= &SQLResultVar;
      New->name= UdmStrdup(resname);
      UdmVarListSort(Vars);
      udm_snprintf(prmname, sizeof(prmname), "%s.num_rows", resname);
      UdmVarListReplaceUnsigned(Vars, prmname, UdmSQLNumRows(Res));
      udm_snprintf(prmname, sizeof(prmname), "%s.errstr", resname);
      UdmVarListReplaceStr(Vars, prmname, db->errstr);
    }
    UdmAgentFree(&Agent);
  }
#endif
  return UDM_OK;
}

static UDM_VARMETHOD EnvMethods[]=
{
  {"AddLine", UdmVarListMethodEnvAddLine},
  {"Find", UdmVarListMethodEnvFind},
  {"SQLQuery", UdmVarListMethodEnvSQLQuery},
  {NULL,NULL}
};

static UDM_VARHANDLER EnvVar=
{
  UDM_VAR_ENV,
  "UdmEnv",
  UdmVarCreateEnv,
  UdmVarFreeEnv,
  UdmVarCopyValueEnv,
  EnvMethods
};

/********************************************************/

static int UdmVarCreateResult(UDM_VARHANDLER *ha, UDM_VAR *dst,
                              UDM_VAR **args, size_t nargs)
{
  if (args && args[0] && args[0]->handler->type == UDM_VAR_ENV)
  {
    bzero((void*)dst,sizeof(*dst));
    dst->val= (char*) UdmResultInit(NULL /*, (UDM_ENV*)args[0]->val, 0*/);
    dst->handler= ha;
  }
  return UDM_OK;
}


static void UdmVarFreeResult(UDM_VAR *V)
{
  UdmResultFree((UDM_RESULT*)V->val);
}


static int UdmVarCopyValueResult(UDM_VAR *D, UDM_VAR *S)
{
  UdmVarCopyValueEmptyStr(D);
  return UDM_OK;
}


static int UdmVarListMethodResultFetch(UDM_VARLIST *Vars, UDM_VAR *Var,
                                       UDM_VAR **args, size_t nargs)
{
  if (Var->handler->type == UDM_VAR_RESULT && nargs == 2 && args[1]->val)
  {
    UDM_RESULT *Res= (UDM_RESULT*) Var->val;
    char rowname[32];
    char prmname[64];
    udm_snprintf(rowname, sizeof(rowname), args[1]->val);
    udm_snprintf(prmname, sizeof(prmname), "%s.*", rowname);
    UdmVarListDelByName(Vars, prmname);
    if (Res->cur_row < Res->num_rows)
    {
      UdmVarListReplaceLst(Vars, &Res->Doc[Res->cur_row].Sections, rowname, "*");
      Res->cur_row++;
    }
  }
  return UDM_OK;
}

/******************************** SQLResult ***********************/
static int UdmVarCreateSQLResult(UDM_VARHANDLER *ha, UDM_VAR *dst,
                              UDM_VAR **args, size_t nargs)
{
  if (args && args[0] && args[0]->handler->type == UDM_VAR_ENV)
  {
    bzero((void*)dst,sizeof(*dst));
    dst->val= (char*) NULL;
    dst->handler= ha;
  }
  return UDM_OK;
}


static void UdmVarFreeSQLResult(UDM_VAR *V)
{
#ifdef HAVE_SQL
  UdmSQLFree((UDM_SQLRES*)V->val);
  UdmFree(V->val);
#endif
}


static int UdmVarCopyValueSQLResult(UDM_VAR *D, UDM_VAR *S)
{
  UdmVarCopyValueEmptyStr(D);
  return UDM_OK;
}


static int UdmVarListMethodResultSQLFetch(UDM_VARLIST *Vars, UDM_VAR *Var,
                                       UDM_VAR **args, size_t nargs)
{
#ifdef HAVE_SQL
  if (Var->handler->type == UDM_VAR_SQLRESULT && nargs == 2 && args[1]->val)
  {
    UDM_SQLRES *Res= (UDM_SQLRES*) Var->val;
    char rowname[32];
    char prmname[64];
    size_t i;
    udm_snprintf(rowname, sizeof(rowname), "%s", args[1]->val);
    udm_snprintf(prmname, sizeof(prmname), "%s.*", rowname);
    UdmVarListDelByName(Vars, prmname);
    if (Res->curRow < Res->nRows)
    {
      for (i= 0; i < Res->nCols; i++)
      {
        udm_snprintf(prmname, sizeof(prmname), "%s.%u", rowname, i);
        UdmVarListReplaceStr(Vars, prmname, UdmSQLValue(Res, Res->curRow, i));
      }
      Res->curRow++;
    }
  }
#endif
  return UDM_OK;
}
/******************************** /SQLResult ***********************/

static UDM_VARMETHOD ResultMethods[]=
{
  {"Fetch", UdmVarListMethodResultFetch},
  {NULL,NULL}
};

UDM_VARHANDLER ResultVar=
{
  UDM_VAR_RESULT,
  "UdmResult",
  UdmVarCreateResult,
  UdmVarFreeResult,
  UdmVarCopyValueResult,
  ResultMethods
};

static UDM_VARMETHOD SQLResultMethods[]=
{
  {"SQLFetch", UdmVarListMethodResultSQLFetch},
  {NULL,NULL}
};

UDM_VARHANDLER SQLResultVar=
{
  UDM_VAR_SQLRESULT,
  "UdmSQLResult",
  UdmVarCreateSQLResult,
  UdmVarFreeSQLResult,
  UdmVarCopyValueSQLResult,
  SQLResultMethods
};

/********************************************************/

static UDM_VARHANDLER *VarHandlers[]=
{
  &SimpleVar,
  &EnvVar,
  &ResultVar,
  &SQLResultVar,
  NULL
};


static UDM_VARHANDLER *UdmVarHandlerByType(int type)
{
  UDM_VARHANDLER **h;
  for (h= VarHandlers; *h; h++)
  {
    if ((*h)->type == type)
      return *h;
  }
  return NULL;
}


int UdmVarListCreateObject(UDM_VARLIST *Vars, const char *name, int type,
                          UDM_VAR **args, size_t nargs)
{
  UDM_VARHANDLER *h= UdmVarHandlerByType(type);
  if (h && h->Create)
  {
    UDM_VAR *New;
    UdmVarListDel(Vars, name);
    UdmVarListAdd(Vars, NULL);
    New= &Vars->Var[Vars->nvars-1];
    h->Create(h, New, args, nargs);
    New->name= UdmStrdup(name);
    UdmVarListSort(Vars);
  }
  return UDM_OK;
}


int UdmVarListInvokeMethod(UDM_VARLIST *Vars,
                          UDM_VAR *Var, const char *methodname,
                          UDM_VAR **args, size_t nargs)
{
  UDM_VARMETHOD *method;
  if (!Var->handler->method)
    return UDM_OK;
  for (method= Var->handler->method; method->name; method++)
  {
    if (!strcasecmp(method->name, methodname))
      return method->func(Vars, Var, args, nargs);
  }
  return UDM_OK;
}


void UdmVarFree(UDM_VAR *S)
{
  S->handler->Free(S);
  UDM_FREE(S->name);
}


static int UdmVarCopyName(UDM_VAR *D, UDM_VAR *S, const char *name)
{
  if(name)
  {
    size_t len = strlen(name) + strlen(S->name) + 3;
    D->name = (char*)UdmMalloc(len);
    udm_snprintf(D->name, len, "%s.%s", name, S->name);
  }
  else
  {
    D->name = (char*)UdmStrdup(S->name);
  }
  return UDM_OK;
}


static int UdmVarCopy(UDM_VAR *D, UDM_VAR *S, const char *name)
{
  UDM_VARHANDLER *handler= S->handler ? S->handler : &SimpleVar;
  UdmVarCopyName(D, S, name);
#ifdef DEBUG_VARS
  fprintf(stderr, "Copy: '%s:%s'\n", name, S->name);
#endif
  UDM_ASSERT(name || S->name);
  handler->CopyValue(D, S);
  return UDM_OK;
}

int UdmVarListAdd(UDM_VARLIST * Lst,UDM_VAR * S)
{
  UDM_VAR *New;
  if (Lst->nvars >= Lst->mvars)
  {
    Lst->mvars+= 256;
    Lst->Var=(UDM_VAR*)UdmRealloc(Lst->Var, Lst->mvars*sizeof(*Lst->Var));
  }
  New= &Lst->Var[Lst->nvars];
  if(S) UdmVarCopy(New, S, NULL);
  else bzero((void*) New, sizeof(UDM_VAR));
  if (!New->handler)
    New->handler= &SimpleVar;
  Lst->nvars++;
  if(S) UdmVarListSort(Lst);
  return UDM_OK;
}

static int UdmVarListAddNamed(UDM_VARLIST *Lst,UDM_VAR *S, const char *name)
{
  UDM_VAR  *v;
  UdmVarListAdd(Lst,NULL);
  v=&Lst->Var[Lst->nvars-1];
  UdmVarCopy(v,S,name);
  UdmVarListSort(Lst);
  return UDM_OK;
}

UDM_VAR * UdmVarListFind(UDM_VARLIST * vars,const char * name)
{
  UDM_VAR key;
#ifdef DEBUG_VARS
  fprintf(stderr, "Find: '%s'\n", name);
#endif
  if (!vars->nvars) return NULL;
  key.name= (char*)name;
  return (UDM_VAR*)UdmBSearch(&key, vars->Var, vars->nvars, sizeof(UDM_VAR), (udm_qsort_cmp)varcmp);
}

int UdmVarListDelBySection(UDM_VARLIST *vars, int sec)
{
  UDM_VAR *v;
  for (v= vars->Var; v < vars->Var + vars->nvars; )
  {
    if (v->section == sec)
    {
      size_t nvars= vars->nvars - (v - vars->Var) - 1;
      UdmVarFree(v);
      if (nvars > 0) memmove(v, v+1, nvars*sizeof(*v));
      vars->nvars--;
    }
    else
      v++;
  }
  return UDM_OK;
}

int UdmVarListDelByName(UDM_VARLIST *vars, const char *name)
{
  UDM_VAR *v;
  for (v= vars->Var; v < vars->Var + vars->nvars; )
  {
    if (!UdmWildCaseCmp(v->name, name))
    {
      size_t nvars= vars->nvars - (v - vars->Var) - 1;
      UdmVarFree(v);
      if (nvars > 0) memmove(v, v+1, nvars*sizeof(*v));
      vars->nvars--;
    }
    else
      v++;
  }
  return UDM_OK;
}

int UdmVarListDel(UDM_VARLIST *vars, const char *name)
{
  UDM_VAR  *v= UdmVarListFind(vars, name);
  if (v)
  {
    size_t nvars= vars->nvars - (v - vars->Var) - 1;
    UdmVarFree(v);
    if (nvars > 0) memmove(v, v + 1, nvars * sizeof(*v));
    vars->nvars--;
  }
  return UDM_OK;
}


int __UDMCALL UdmVarListReplace(UDM_VARLIST * Lst,UDM_VAR * S)
{
  UDM_VAR  *v = UdmVarListFind(Lst, S->name);
  if (v == NULL)
  {
    return UdmVarListAdd(Lst, S);
  }
  else
  {
    UdmVarFree(v);
    UdmVarCopy(v, S, NULL);
  }
  return UDM_OK;
}

static int UdmVarListReplaceNamed(UDM_VARLIST *Lst,UDM_VAR *S, const char *name)
{
  int  rc;
  UDM_VAR  *v;
  char fullname[64];
  if (name)
    udm_snprintf(fullname, sizeof(fullname), "%s.%s", name, S->name);
  else
    udm_snprintf(fullname, sizeof(fullname), "%s", S->name);
  
  if ((v= UdmVarListFind(Lst, fullname)))
  {
    UdmVarFree(v);
    rc=UdmVarCopy(v,S,name);
  }
  else
  {
    UdmVarListAdd(Lst,NULL);
    v=&Lst->Var[Lst->nvars-1];
    rc=UdmVarCopy(v,S,name);
    UdmVarListSort(Lst);
  }
  return rc;
}

static int UdmVarListInsNamed(UDM_VARLIST *Lst,UDM_VAR *S, const char *name)
{
  int  rc= UDM_OK;
  UDM_VAR  *v=UdmVarListFind(Lst, S->name);
  if (!v)
  {
    UdmVarListAdd(Lst,NULL);
    v=&Lst->Var[Lst->nvars-1];
    rc=UdmVarCopy(v,S,name);
    UdmVarListSort(Lst);
  }
  return rc;
}


__C_LINK const char * __UDMCALL UdmVarListFindStr(UDM_VARLIST *vars,
                                                  const char *name,
                                                  const char *defval)
{
  UDM_VAR * var;
  if((var=UdmVarListFind(vars,name)) != NULL)
    return((var->val != NULL)?var->val:defval);
  else
    return(defval);
}

UDM_VARLIST * UdmVarListInit(UDM_VARLIST *l)
{
  if(!l)
  {
    l=(UDM_VARLIST*)UdmMalloc(sizeof(*l));
    bzero((void*)l,sizeof(*l));
    l->freeme=1;
  }
  else
  {
    bzero((void*)l,sizeof(*l));
  }
  return l;
}


void UdmVarListFree(UDM_VARLIST * vars)
{
  size_t i;
  for(i=0;i<vars->nvars;i++)
  {
    UdmVarFree(&vars->Var[i]);
  }
  UDM_FREE(vars->Var);
  vars->nvars= 0;
  vars->mvars= 0;
  if(vars->freeme)
    UDM_FREE(vars);
}

static int
UdmVarListAddStrWithSection(UDM_VARLIST *vars,
                            const char *name, const char *val, int sec,
                            int sort_flag)
{
  UDM_VAR *New;
  UdmVarListAdd(vars, NULL);
  New= &vars->Var[vars->nvars-1];
  New->handler= &SimpleVar;
  New->flags= 0;
  New->section= sec;
  New->maxlen=0;
  New->curlen= (val != NULL) ? strlen(val) : 0;  
  New->name= name ? (char*)UdmStrdup(name) : NULL;
#ifdef DEBUG_VARS
  fprintf(stderr, "AddSec: '%s'\n", name);
#endif
  New->val= val ? (char*)UdmStrdup(val) : NULL;
  if (sort_flag)
    UdmVarListSort(vars);
  return vars->nvars;
}

int UdmVarListAddStr(UDM_VARLIST *vars, const char *name, const char *val)
{
  return UdmVarListAddStrWithSection(vars, name, val, 0, 1);
}

int UdmVarListAddQueryStr(UDM_VARLIST *vars, const char *name, const char *val)
{
  return UdmVarListAddStrWithSection(vars, name, val, UDM_VARSRC_QSTRING, 1);
}


int UdmVarListAddInt(UDM_VARLIST * vars,const char * name, int val)
{
  char num[64];
  udm_snprintf(num, 64, "%d", val);
  return UdmVarListAddStr(vars,name,num);
}

int UdmVarListAddUnsigned(UDM_VARLIST * vars,const char * name, uint4 val)
{
  char num[64];
  udm_snprintf(num, 64, "%u", val);
  return UdmVarListAddStr(vars, name, num);
}

int UdmVarListAddDouble(UDM_VARLIST * vars,const char * name, double val)
{
  char num[128];
  udm_snprintf(num, 128, "%f", val);
  return UdmVarListAddStr(vars, name, num);
}

int UdmVarListInsStr(UDM_VARLIST * vars,const char * name,const char * val)
{
  return UdmVarListFind(vars,name) ? UDM_OK : UdmVarListAddStr(vars,name,val);
}

int UdmVarListInsInt(UDM_VARLIST * vars,const char * name,int val)
{
  return UdmVarListFind(vars,name) ? UDM_OK : UdmVarListAddInt(vars,name,val);
}



int UdmVarAppendStrn(UDM_VAR *var, const char *val, size_t len)
{
  size_t newlen= var->curlen + len;
  var->val= (char*) UdmRealloc(var->val, newlen + 1);
  memcpy(var->val + var->curlen, val, len);
  var->val[newlen]= '\0';
  var->curlen= newlen;
  return UDM_OK;
}


int
UdmVarListReplaceStrn(UDM_VARLIST *vars,
                      const char *name,
                      const char *val, size_t len)
{
  int rc;
  char *v= UdmStrndup(val, len);
  rc= UdmVarListReplaceStr(vars, name, v);
  UdmFree(v);
  return rc;
}



__C_LINK int __UDMCALL UdmVarListReplaceStr(UDM_VARLIST *vars,
                                            const char *name, const char *val)
{
  UDM_VAR * var;

  if((var= UdmVarListFind(vars, name)) != NULL)
  {
    UDM_FREE(var->val);
    if (val == NULL)
    {
      var->val= NULL;
      var->curlen= 0;
    }
    else if (var->maxlen == 0)
    {
      var->curlen= strlen(val);
      var->val= (char*)UdmMalloc(var->curlen + 1);
      memcpy(var->val, val, var->curlen + 1);
    }
    else
    {
      size_t len;
      var->curlen= strlen(val);
      len= var->maxlen > var->curlen ? var->maxlen : var->curlen;
      var->val= (char*)UdmMalloc(len + 4);
      memcpy(var->val, val, var->curlen);
      var->val[var->curlen]= '\0';
    }
  }
  else
  {
    UdmVarListAddStr(vars,name,val);
  }
  return vars->nvars;
}

__C_LINK int __UDMCALL UdmVarListReplaceInt(UDM_VARLIST * vars,
                                            const char * name,int val)
{
  UDM_VAR * var;
  char num[64];
  if((var=UdmVarListFind(vars,name)) != NULL)
  {
    udm_snprintf(num, 64, "%d", val);
    UdmVarListReplaceStr(vars, name, num);
  }
  else
    UdmVarListAddInt(vars,name,val);
  return vars->nvars;
}

int UdmVarListReplaceUnsigned(UDM_VARLIST * vars, const char * name, uint4 val)
{
  UDM_VAR * var;
  char num[64];
  if((var = UdmVarListFind(vars, name)) != NULL)
  {
    udm_snprintf(num, 64, "%u", val);
    UdmVarListReplaceStr(vars, name, num);
  }
  else
    UdmVarListAddUnsigned(vars, name, val);
  return vars->nvars;
}

int UdmVarListReplaceDouble(UDM_VARLIST * vars, const char * name, double val)
{
  UDM_VAR * var;
  char num[128];
  if((var = UdmVarListFind(vars, name)) != NULL)
  {
    udm_snprintf(num, 128, "%lf", val);
    UdmVarListReplaceStr(vars, name, num);
  }
  else
    UdmVarListAddDouble(vars, name, val);
  return vars->nvars;
}


UDM_VAR * UdmVarListFindWithValue(UDM_VARLIST * vars,
                                  const char *name,const char *val)
{
  size_t i;
  
  for(i=0;i<vars->nvars;i++)
    if(!strcasecmp(name,vars->Var[i].name)&&!strcasecmp(val,vars->Var[i].val))
      return(&vars->Var[i]);
  return(NULL);
}

int UdmVarIntVal(UDM_VAR *var)
{
  return var->val ? atoi(var->val) : 0;
}


int UdmVarListFindInt(UDM_VARLIST * vars,const char * name,int defval)
{
  UDM_VAR * var;
  if((var=UdmVarListFind(vars,name)) != NULL)
    return((var->val != NULL)?atoi(var->val):defval);
  else
    return(defval);
}

int UdmVarListFindBool(UDM_VARLIST * vars,const char * name,int defval)
{
  UDM_VAR * var;
  if((var= UdmVarListFind(vars,name)) && var->val)
  {
    return !strcasecmp(var->val,"yes") || atoi(var->val) == 1;
  }
  else
    return defval;
}

unsigned UdmVarListFindUnsigned(UDM_VARLIST * vars,
                                const char * name, unsigned defval)
{
  UDM_VAR * var;
  if((var=UdmVarListFind(vars,name)) != NULL)
    return (var->val != NULL) ? 
           (unsigned)strtoul(var->val, (char**)NULL, 10) : defval;
  else
    return(defval);
}

double UdmVarListFindDouble(UDM_VARLIST * vars, const char *name, double defval)
{
  UDM_VAR * var;
  if((var=UdmVarListFind(vars,name)) != NULL)
    return((var->val != NULL) ? strtod(var->val, (char**)NULL) : defval);
  else
    return(defval);
}

int UdmVarListReplaceLst(UDM_VARLIST *D, UDM_VARLIST *S,
                         const char *name, const char *mask)
{
  size_t i;
  
  for(i=0;i<S->nvars;i++)
  {
    UDM_VAR  *v=&S->Var[i];
    if(!UdmWildCaseCmp(v->name,mask))
      UdmVarListReplaceNamed(D,v,name);
  }
  return UDM_OK;
}

int UdmVarListAddLst(UDM_VARLIST *D, UDM_VARLIST *S,
                     const char *name,const char *mask)
{
  size_t i;
  
  for(i=0;i<S->nvars;i++){
    UDM_VAR  *v=&S->Var[i];
    if(!UdmWildCaseCmp(v->name,mask))
      UdmVarListAddNamed(D,v,name);
  }
  return UDM_OK;
}

int UdmVarListInsLst(UDM_VARLIST *D, UDM_VARLIST *S,
                     const char *name,const char *mask)
{
  size_t i;
  
  for(i=0;i<S->nvars;i++)
  {
    UDM_VAR  *v=&S->Var[i];
    if(!UdmWildCaseCmp(v->name,mask))
      UdmVarListInsNamed(D,v,name);
  }
  return UDM_OK;
}


/*
  Merge two varlists with single sorting
*/
int UdmVarListMerge(UDM_VARLIST * Lst,UDM_VARLIST *Src1, UDM_VARLIST *Src2)
{
  size_t i;
  Lst->nvars= Lst->mvars= Src1->nvars + Src2->nvars;
  if (!(Lst->Var= (UDM_VAR*) UdmMalloc(Lst->nvars * sizeof(UDM_VAR))))
    return UDM_ERROR;

  for (i= 0; i < Src1->nvars; i++)
  {
    UdmVarCopy(&Lst->Var[i], &Src1->Var[i], NULL);
  }
  for (i= 0; i < Src2->nvars; i++)
  {
    UdmVarCopy(&Lst->Var[i+Src1->nvars], &Src2->Var[i], NULL);
  }
  if (Lst->nvars)
    UdmVarListSort(Lst);
  return UDM_OK;
}



/* 
  Add environment variables 
  into a varlist
*/

extern char **environ;

int UdmVarListAddEnviron(UDM_VARLIST *Vars, const char *name)
{
  char  **e, *val, *str;
  size_t  lenstr = 1024;
  
  if ((str = (char*)UdmMalloc(1024)) == NULL)
    return UDM_ERROR;

  for ( e=environ; e[0] ; e++)
  {
    size_t len = strlen(e[0]);
    if (len > lenstr)
    {
      if ((str = (char*)UdmRealloc(str, lenstr = len + 64)) == NULL)
        return UDM_ERROR;
    }
    len = udm_snprintf(str, lenstr - 1, "%s%s%s",
                       name ? name : "", name ? "." : "", e[0]);
    str[len]='\0';
    
    if((val=strchr(str,'=')))
    {
      *val++='\0';
      UdmVarListAddStrWithSection(Vars,str,val,UDM_VARSRC_ENV,0);
    }
  }
  UdmVarListSort(Vars);
  UDM_FREE(str);
  return UDM_OK;
}

int UdmVarListConvert(UDM_VARLIST *Vars, UDM_CONV *conv)
{
  size_t i;
  for (i= 0; i < Vars->nvars; i++)
  {
    UDM_VAR *Var= &Vars->Var[i];
    if (UdmVarType(Var) == UDM_VAR_STR)
    {
      size_t len= strlen(Var->val), newlen;
      char *newval= (char*)UdmMalloc(len * 12 + 1);
      newlen= UdmConv(conv, newval, len * 12 + 1, Var->val, len);
      newval[newlen]= '\0';
      UDM_FREE(Var->val);
      Var->val= newval;
      Var->curlen= newlen;
    }
  }
  return UDM_OK;
}
