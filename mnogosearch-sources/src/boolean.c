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
#include <sys/types.h>
#include <string.h>

#include "udm_common.h"
#include "udm_boolean.h"
#include "udm_utils.h"

/*#define DEBUG_BOOL*/


#ifdef DEBUG_BOOL
static char
item_type(int cmd, unsigned long arg)
{
  switch(cmd)
  {
    case UDM_STACK_LEFT:   return('(');
    case UDM_STACK_RIGHT:  return(')');
    case UDM_STACK_OR:     return('|');
    case UDM_STACK_AND:    return('&');
    case UDM_STACK_NOT:    return('~');
    case UDM_STACK_PHRASE: return('"');
    case UDM_STACK_WORD:   return(arg ? '1' : '0');
    default:               return('?');
  }
}
#endif

static int
TOPCMD(UDM_BOOLSTACK *s)
{
  if (s->ncstack)
    return s->cstack[s->ncstack-1];
  else
    return UDM_STACK_BOT;
}

static int
POPARG(UDM_BOOLSTACK *s)
{
  if (s->nastack>0)
  {
    s->nastack--;
    return s->astack[s->nastack];
  }
  else
  {
    return UDM_STACK_BOT;
  }
}


static int
POPCMD(UDM_BOOLSTACK *s)
{
  if(s->ncstack>0)
  {
    s->ncstack--;
    return s->cstack[s->ncstack];
  }
  else
  {
    return UDM_STACK_BOT;
  }
}


static void
PUSHARG(UDM_BOOLSTACK *s, unsigned long arg)
{
  s->astack[s->nastack]= arg;
  s->nastack++;
  if (s->nastack >= s->mastack)
  {
    s->mastack+= UDM_MAXSTACK;
    s->astack= (unsigned long*)UdmRealloc(s->astack, s->mastack * sizeof(unsigned long));
  }
}

static void
PUSHCMD(UDM_BOOLSTACK *s, int arg)
{
  s->cstack[s->ncstack]= arg;
  s->ncstack++;
  if (s->ncstack >= s->mcstack)
  {
    s->mcstack+= UDM_MAXSTACK;
    s->cstack= (int*)UdmRealloc(s->cstack, s->mcstack * sizeof(int));
  }
}

/* Compute one operation and store result */
static int
perform(UDM_BOOLSTACK *s, int com)
{
  unsigned long res,x1,x2;
  switch(com)
  {
    case UDM_STACK_OR:
      /* This is to avoid compiler      */
      /* optimization in || calculation */
      x1=POPARG(s);
      x2=POPARG(s);
      res=(x1||x2);
#ifdef DEBUG_BOOL
      fprintf(stderr,"Perform %lu | %lu\n",x1,x2);
#endif
      PUSHARG(s,res);
      break;
    case UDM_STACK_AND:
      x1=POPARG(s);
      x2=POPARG(s);
      res=(x1&&x2);
#ifdef DEBUG_BOOL
      fprintf(stderr,"Perform %lu & %lu\n",x1,x2);
#endif
      PUSHARG(s,res);
      break;
    case UDM_STACK_NOT:
      x1=POPARG(s);
      res=x1?0:1;
#ifdef DEBUG_BOOL
      fprintf(stderr,"Perform ~%lu\n",x1);
#endif
      PUSHARG(s,res);
      break;
  }
  return(0);
}


/* Main function to calculate items sequence */
int
UdmCalcBoolItems(UDM_STACK_ITEM *items, size_t nitems, char *count)
{
  UDM_BOOLSTACK s;
  int res;
  size_t i;

  /* Init stack */
  s.ncstack= 0;
  s.nastack= 0;
  s.mcstack= s.mastack = UDM_MAXSTACK;
  s.cstack= (int*)UdmMalloc(UDM_MAXSTACK * sizeof(int));
  s.astack= (unsigned long*)UdmMalloc(UDM_MAXSTACK * sizeof(unsigned long));

#ifdef DEBUG_BOOL
  for (i= 0; i < nitems; i++)
  {
    fprintf(stderr, "[%d]cmd=%d arg=%d\n", i, items[i].cmd, (int)items[i].arg);
  }
#endif

#ifdef DEBUG_BOOL
  fprintf(stderr,"--------\n");
#endif
  for(i= 0; i < nitems; i++)
  {
    int c;
#ifdef DEBUG_BOOL
    fprintf(stderr,".[%d] %c\n", i, item_type(items[i].cmd, items[i].arg));
#endif
    switch((c= items[i].cmd))
    {
      case UDM_STACK_RIGHT:
        /* Perform till LEFT bracket */
        while((TOPCMD(&s)!=UDM_STACK_LEFT)&&(TOPCMD(&s)!=UDM_STACK_BOT))
          perform(&s,POPCMD(&s));
        /* Pop LEFT bracket itself */
        if(TOPCMD(&s)==UDM_STACK_LEFT)
          POPCMD(&s);
        break;
      case UDM_STACK_OR:
      case UDM_STACK_AND:
        while(c<=TOPCMD(&s))
          perform(&s,POPCMD(&s));
        /* IMPORTANT! No break here! That's OK*/
        /* Так надо ! */
      case UDM_STACK_LEFT:
      case UDM_STACK_NOT:
        PUSHCMD(&s,c);
        break;
      case UDM_STACK_PHRASE:
        PUSHARG(&s, (count[items[i+1].arg]) ? 1UL : 0UL);
        for (i++; items[i].cmd != UDM_STACK_PHRASE; i++)
#ifdef DEBUG_BOOL
    fprintf(stderr,"[%d] %c\n", i, item_type(items[i].cmd, count[items[i].arg]));
    fprintf(stderr,"[%d] %c\n", i, item_type(items[i].cmd, count[items[i].arg]));
#endif
        ;
        break;
      case UDM_STACK_STOP:
        PUSHARG(&s, 1UL);
        break;
      case UDM_STACK_WORD:
        PUSHARG(&s, (count[items[i].arg]) ? 1UL : 0UL);
        break;
      default:
        UDM_ASSERT(0);
    }
  }
  while(TOPCMD(&s) != UDM_STACK_BOT)
    perform(&s, POPCMD(&s));

  res= POPARG(&s);
  UDM_FREE(s.cstack);
  UDM_FREE(s.astack);
#ifdef DEBUG_BOOL
  fprintf(stderr, "result: %d\n", res);
#endif
  return res;
}


void
UdmStackItemListFree(UDM_STACKITEMLIST *L)
{
  UDM_FREE(L->items);
  bzero((void*) L, sizeof(*L));
}


int
UdmStackItemListAdd(UDM_STACKITEMLIST *List, UDM_STACK_ITEM *item)
{
  if (List->nitems >= List->mitems)
  {
    List->mitems+= UDM_MAXSTACK;
    if (!(List->items= (UDM_STACK_ITEM*)UdmRealloc(List->items,
                                                   List->mitems *
                                                   sizeof(UDM_STACK_ITEM))))
      return UDM_ERROR;
  }
  List->items[List->nitems]= item[0];
  List->nitems++;
  return UDM_OK;
}


typedef struct udm_stack_stat_st
{
  size_t n_left;
  size_t n_right;
  size_t n_or;
  size_t n_and;
  size_t n_not;
  size_t n_phrase;
  size_t n_word;
  size_t n_stop;
} UDM_STACK_STAT;


static void
UdmStackStatAdd(UDM_STACK_STAT *S, UDM_STACK_ITEM *item)
{
  switch (item->cmd)
  {
    case UDM_STACK_LEFT:   S->n_left++;   break;
    case UDM_STACK_RIGHT:  S->n_right++;  break;
    case UDM_STACK_OR:     S->n_or++;     break;
    case UDM_STACK_AND:    S->n_and++;    break;
    case UDM_STACK_NOT:    S->n_not++;    break;
    case UDM_STACK_PHRASE: S->n_phrase++; break;
    case UDM_STACK_WORD:   S->n_word++;   break;
    case UDM_STACK_STOP:   S->n_stop++;   break;
    case UDM_STACK_BOT:    break;
    default:
      UDM_ASSERT(0);
      break;
  }
}

static int
UdmStackItemListAdd_with_Stat(UDM_STACKITEMLIST *List,
                              UDM_STACK_STAT *Stat,
                              UDM_STACK_ITEM *item)
{
  UdmStackStatAdd(Stat, item);
  return UdmStackItemListAdd(List, item);
}


int
UdmStackItemListCopy(UDM_STACKITEMLIST *Dst,
                     UDM_STACKITEMLIST *Src, int search_mode)
{
  size_t i;
  int rc, inphrase, add_cmd= UDM_STACK_AND;
  UDM_STACK_STAT Stat;
  
  bzero((void*) &Stat, sizeof(Stat));
  bzero((void*) Dst, sizeof(*Dst));
  
  switch(search_mode)
  {
    case UDM_MODE_ANY:
      add_cmd = UDM_STACK_OR;
      break;
    case UDM_MODE_BOOL:
    case UDM_MODE_ALL:
    case UDM_MODE_ALL_MINUS:
    case UDM_MODE_ALL_MINUS_HALF:
      add_cmd = UDM_STACK_AND;
      break;
  }

  if (UDM_OK != (rc= UdmStackItemListAdd_with_Stat(Dst, &Stat, &Src->items[0])))
    return rc;
  
  inphrase= (Src->items[0].cmd == UDM_STACK_PHRASE) ? 1 : 0;

  for (i= 1; i < Src->nitems; i++)
  {
    /*
      If previous item is WORD or PHRASE or RIGHT or STOPWORD
      and next item is WORD or PHRASE or LEFT or STOPWORD or NOT
      and we are not in phrase
      we have to insert search mode dependent operator.
     */
    if ((Src->items[i - 1].cmd == UDM_STACK_WORD ||
         Src->items[i - 1].cmd == UDM_STACK_STOP ||
         Src->items[i - 1].cmd == UDM_STACK_PHRASE ||
         Src->items[i - 1].cmd == UDM_STACK_RIGHT) &&
        (Src->items[i].cmd == UDM_STACK_WORD ||
         Src->items[i].cmd == UDM_STACK_STOP ||
         Src->items[i].cmd == UDM_STACK_PHRASE ||
         Src->items[i].cmd == UDM_STACK_LEFT ||
         Src->items[i].cmd == UDM_STACK_NOT) &&
         !inphrase)
    { 
      UDM_STACK_ITEM add;
      add.cmd= add_cmd;
      add.arg= 0;
      if (UDM_OK != (rc= UdmStackItemListAdd_with_Stat(Dst, &Stat, &add)))
        return rc;
    }
    if (Src->items[i].cmd == UDM_STACK_PHRASE)
      inphrase= !inphrase;
    if (UDM_OK != (rc= UdmStackItemListAdd_with_Stat(Dst, &Stat, &Src->items[i])))
      return rc;
  }
  /*
  fprintf(stderr, "paren=%d/%d or=%d and=%d not=%d phrase=%d word=%d stop=%d\n",
          Stat.n_left, Stat.n_right, Stat.n_or, Stat.n_and, Stat.n_not,
          Stat.n_phrase, Stat.n_word, Stat.n_stop);
  */       
  /* Ignore phrases in loose modes */
  if (search_mode == UDM_MODE_ALL_MINUS ||
      search_mode == UDM_MODE_ALL_MINUS_HALF)
  {
    Stat.n_phrase= 0;
  }
  
  /*
    If no commands other than UDM_STACK_PHRASE and UDM_STACK_AND and parenthesis,
    then no needs to calculate boolean expression.
  */
  if (!Stat.n_phrase && !Stat.n_or && !Stat.n_not)
  {
    UdmStackItemListFree(Dst);
  }
  return UDM_OK;
}
