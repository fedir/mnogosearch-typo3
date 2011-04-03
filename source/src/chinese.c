/*
###############################################################################
# This software is being provided to you, the LICENSEE, by the Linguistic     #
# Data Consortium (LDC) and the University of Pennsylvania (UPENN) under the  #
# following license.  By obtaining, using and/or copying this software, you   #
# agree that you have read, understood, and will comply with these terms and  #
# conditions:                                                                 #
#                                                                             #
# Permission to use, copy, modify and distribute, including the right to      #
# grant others the right to distribute at any tier, this software and its     #
# documentation for any purpose and without fee or royalty is hereby granted, #
# provided that you agree to comply with the following copyright notice and   #
# statements, including the disclaimer, and that the same appear on ALL       #
# copies of the software and documentation, including modifications that you  #
# make for internal use or for distribution:                                  #
#                                                                             #
# Copyright 1999 by the University of Pennsylvania.  All rights reserved.     #
#                                                                             #
# THIS SOFTWARE IS PROVIDED "AS IS"; LDC AND UPENN MAKE NO REPRESENTATIONS OR #
# WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not limitation,     #
# LDC AND UPENN MAKE NO REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR   #
# FITNESS FOR ANY PARTICULAR PURPOSE.                                         #
###############################################################################
# mansegment.perl Version 1.0
# Run as: mansegment.perl [dictfile] < infile > outfile
# Mandarin segmentation for both GB and BIG5 as long as the conresponding 
# word frequency dictionary is used.
#
# Written by Zhibiao Wu at LDC on April 12 1999
#
# Algorithm: Dynamic programming to find the path which has the highest 
# multiple of word probability, the next word is selected from the longest
# phrase.
#
# dictfile is a two column text file, first column is the frequency, 
# second column is the word. The program will change the file into a dbm 
# file in the first run. So be sure to remove the dbm file if you have a
# newer version of the text file.
##############################################################################
*/
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

This is C version of a LDC Chinese segmentor

*/
#include "udm_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "udm_common.h"
#include "udm_chinese.h"
#include "udm_utils.h"
#include "udm_xmalloc.h"
#include "udm_unicode.h"
#include "udm_unidata.h"

static int cmpchinese(const void *s1,const void *s2){
     return(UdmUniStrCmp(((const UDM_CHINAWORD*)s1)->word, ((const UDM_CHINAWORD*)s2)->word));
}

static void UdmChineseListSort(UDM_CHINALIST *List){
     UdmSort(List->ChiWord, List->nwords, sizeof(UDM_CHINAWORD), cmpchinese);
}

__C_LINK void __UDMCALL UdmChineseListFree(UDM_CHINALIST *List){
     size_t i;
     for(i = 0; i < List->nwords; i++){
          UDM_FREE(List->ChiWord[i].word);
     }
     UDM_FREE(List->ChiWord);
     UDM_FREE(List->hash);
     List->nwords = 0;
     List->mwords = 0;
}

static UDM_CHINAWORD * UdmChineseListFind(UDM_CHINALIST *List, const int *word) {
     int low  = 0;
     int high = List->nwords - 1;

     if(!List->ChiWord) return(0);
     while (low <= high) {
          int middle = (low + high) / 2;
          int match = UdmUniStrCmp(List->ChiWord[middle].word, word);
          if (match < 0)  { low = middle + 1;
          } else if (match > 0) { high = middle - 1;
          } else return(&List->ChiWord[middle]);
     }
     return(NULL);
}

static void UdmChineseListAdd(UDM_CHINALIST *List, UDM_CHINAWORD * chinaword){
     unsigned int h;
/*
     size_t j;
     for(j = 0; j < List->nwords; j++){
          if(!strcmp(List->ChiWord[j].word, chinaword->word)){
               return 0;
          }
     }
*/
     if (List->nwords + 1 > List->mwords) {
       List->mwords += 1024;
       List->ChiWord = (UDM_CHINAWORD *)UdmRealloc(List->ChiWord, (List->mwords)*sizeof(UDM_CHINAWORD));
     }
     if (List->hash == NULL) {
       List->hash = (size_t *)UdmXmalloc(65536 * sizeof(size_t));
     }
     List->ChiWord[List->nwords].word = (int*)UdmUniDup(chinaword->word);
     List->ChiWord[List->nwords].freq = chinaword->freq;
     List->total += chinaword->freq;
     h = (unsigned int)(chinaword->word[0] & 0xffff);
/*     h = ((((unsigned int)chinaword->word[1]) << 8) & 0xff00) + (((unsigned int)chinaword->word[0]) & 0xff);*/
     if (List->hash[h] < UdmUniLen(chinaword->word)) {
       List->hash[h] = UdmUniLen(chinaword->word);
     }
     List->nwords++;
}

int UdmChineseListLoad(UDM_AGENT *Agent, UDM_CHINALIST *List, const char *charset, const char *fname) {
     char str[1024];
     FILE *file;
     UDM_CHINAWORD chinaword;
     char word[64];
     int uword[256];
     UDM_CHARSET *sys_int, *fcs;
     UDM_CONV to_uni;
     UDM_ENV *Env= Agent->Conf;
     
     sys_int = &udm_charset_sys_int;
     if (!(fcs = UdmGetCharSet(charset))) {
       udm_snprintf(Env->errstr, sizeof(Env->errstr),
                    "Charset '%s' not found or not supported", charset);
       return UDM_ERROR;
     }
     UdmConvInit(&to_uni, fcs, sys_int, UDM_RECODE_HTML);

     if (!(file = fopen(fname, "r"))) {
          udm_snprintf(Env->errstr, sizeof(Env->errstr),
                       "Can't open frequency dictionary file '%s' (%s)",
                       fname, strerror(errno));
          return UDM_ERROR;
     }
     bzero((void*)&chinaword, sizeof(chinaword));
     chinaword.word = uword;
     while(fgets(str, sizeof(str), file)){
          if(!str[0])continue;
          if(str[0]=='#')continue;
          sscanf(str, "%d %63s ", &chinaword.freq, word );
	  UdmConv(&to_uni, (char*)uword, sizeof(uword), word, strlen(word)+1);
          UdmChineseListAdd(List, &chinaword);
     }
     fclose(file);
     UdmChineseListSort(List);
     return UDM_OK;
}

static int *UdmSegmentProcess(UDM_CHINALIST *List, int *line) {
  int top, nextid, *position, *next, len, maxid, i, current, father, needinsert, iindex;
  unsigned int h;
  double *value;
  int **result;
  int *otv, space[] = {32, 0};
  UDM_CHINAWORD *chinaword, chiw;
  
  if ((line[0] >= 0x80) && (List->hash != NULL)) {

    len = UdmUniLen(line);
    maxid = 2 * len;
    position = (int*)UdmMalloc(maxid * sizeof(int));
    next = (int*)UdmMalloc(maxid * sizeof(int));
    value = (double*)UdmMalloc(maxid * sizeof(double));
    result = (int **)UdmMalloc(maxid * sizeof(int *));
    
    top = 0;
/*    value[0] = 1;*/
    value[0] = 0;
    position[0] = 0;
    next[0] = -1;
    result[0] = (int*)UdmUniDup(&space[1]);
    nextid = 1;


    while ((top != -1) && (!((position[top] >= len) && (next[top] == -1)))) {

/*      fprintf(stderr, "top: %d  position: %d (len: %d)  next:%d\n", top, position[top], len, next[top]);*/


/*   # find the first open path */
      current = top;
      father = top;
      while ((current != -1) && (position[current] >= len)) {
	father = current;
	current = next[current];
      }
/*   # remove this path */
      if (current == top) {
	top = next[top];
      } else {
	next[father] = next[current];
      }

      if (current == -1) {
/*       # no open path, finished, take the first path */
	next[top] = -1;
      } else {
	otv = &line[position[current]];
	h = (unsigned int)(otv[0] & 0xffff);
/*	h = ((((unsigned int)otv[1]) << 8) & 0xff00) + (((unsigned int)otv[0]) & 0xff);*/

/*       # if the first character doesn't have word phrase in the dict.*/
	if (List->hash[h] == 0) {
	  List->hash[h] = 1 /*2*/;
	}

	i = List->hash[h];
	if (i + position[current] > len) {
	  i = len - position[current];
	}
	i = i + 1 /*2*/;
	otv = NULL;
	do {
	  i = i - 1 /*2*/;
	  UDM_FREE(otv);
	  otv = UdmUniNDup(&line[position[current]], (size_t)i);
	  chinaword = UdmChineseListFind(List, otv);
	} while ((i >= 1/*2*/) && ( chinaword == NULL));

	if (i < 1 /*2*/) {
	  UDM_FREE(otv);
	  otv = UdmUniNDup(&line[position[current]], 1/*2*/);
	  chiw.word = otv;
	  chiw.freq = 1;
	  UdmChineseListAdd(List, chinaword = &chiw);
	  i = 1/*2*/;
	}

	if (chinaword->freq) {
/*       # pronode()   */
/*	  value[nextid] = value[current] * chinaword->freq / List->total;*/
	  value[nextid] = value[current] + chinaword->freq / List->total;
	  position[nextid] = position[current] + i;
	  h = UdmUniLen(result[current]) + UdmUniLen(otv) + 2;
	  result[nextid] = (int*)UdmXmalloc((size_t)h * sizeof(int));
	  UdmUniStrCpy(result[nextid], result[current]);
	  UdmUniStrCat(result[nextid], space);
	  UdmUniStrCat(result[nextid], otv);
/*
    # check to see whether there is duplicated path
    # if there is a duplicate path, remove the small value path
*/
	  needinsert = 1;
	  iindex = top;
	  father = top;
	  while (iindex != -1) {
	    if (position[iindex] == position[nextid]) {
	      if (value[iindex] >= value[nextid]) {
		needinsert = 0;
	      } else {
		if (top == iindex) {
		  next[nextid] = next[iindex];
		  top = nextid;
		  needinsert = 0;
    /*          } else {
	          next[nextid] = next[father];*/ /*  next[father] = next[nextid];*/
		}
	      }
	      iindex = -1;
	    } else {
	      father = iindex;
	      iindex = next[iindex];
	    }
	  }
/*    # insert the new path into the list */
	  if (needinsert == 1) {
	    while ((iindex != -1) && (value[iindex] > value[nextid])) {
	      father = iindex;
	      iindex = next[iindex];
	    }
	    if (top == iindex) {
	      next[nextid] = top;
	      top = nextid;
	    } else {
	      next[father] = nextid;
	      next[nextid] = iindex;
	    }
	  }
	  nextid++;
	  if (nextid >= maxid) {
	    maxid +=128;
	    position = (int*)UdmRealloc(position, maxid * sizeof(int));
	    next = (int*)UdmRealloc(next, maxid * sizeof(int));
	    value = (double*)UdmRealloc(value, maxid * sizeof(double));
	    result = (int **)UdmRealloc(result, maxid * sizeof(int *));
	  }
	}
	UDM_FREE(otv);
      }
    }

    UDM_FREE(position); UDM_FREE(next); UDM_FREE(value);
    for (i = 0; i < nextid; i++) {
      if (i != top) UDM_FREE(result[i]);
    }
    otv = result[top];
    UDM_FREE(result);
    return otv;

  } else {
    return (int*)UdmUniDup(line);
  }
}

int *UdmSegmentByFreq(UDM_CHINALIST *List, int *line) {
  int *out, *mid, *midend, *last, *sentence, *segmented_sentence, part;
  size_t i, j, l, a;
  int reg = 1, ctype;
  int space[] = { 32, 0 };
  UDM_UNIDATA *unidata= udm_unidata_default /* FIXME */;
  
  l = 2 * (UdmUniLen(line) + 1);
  if (l < 2) return NULL;
  out = (int*)UdmMalloc(l * sizeof(int));
  if (out == NULL) return NULL;
  *out = '\0';
  mid = (int*)UdmMalloc(l * sizeof(int));
  if (mid == NULL) { UDM_FREE(out); return NULL; }
  *mid = '\0';
  
  for (i = j = 0; i < UdmUniLen(line); i++) {
    if (line[i] >= 0x80) {
      if (reg == 0) {
	mid[j++] = ' ';
	reg = 1;
      }
    } else {
      if (reg == 1) {
	mid[j++] = ' ';
	reg = 0;
      }
    }
    mid[j++] = line[i];
  }
  mid[j] = 0;
  midend= mid + UdmUniLen(mid);

  for (sentence = UdmUniGetSepToken(unidata, mid, midend, &last, &ctype);
       sentence;
       sentence = UdmUniGetSepToken(unidata, NULL, midend, &last, &ctype)) {
    part = *last;
    *last = 0;
    segmented_sentence = UdmSegmentProcess(List, sentence);
    *last = part;
    a = 2 * (UdmUniLen(segmented_sentence) + 1);
    j = UdmUniLen(out);
    if (j + a >= l) {
      l = j + a + 1;
      out = (int*)UdmRealloc(out, l * sizeof(int));
    }
    if (*out) UdmUniStrCat(out, space);
    UdmUniStrCat(out, segmented_sentence);
    UDM_FREE(segmented_sentence);
  }

  UDM_FREE(mid);
  return out;
}
