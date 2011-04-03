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

#include "udm_common.h"
#include "udm_utils.h"
#include "udm_crossword.h"

#define RESORT_WORDS	256
#define WSIZE		1024


static int AddOneCrossWord(UDM_DOCUMENT * Doc,UDM_CROSSWORD * CrossWord){
	CrossWord->pos=Doc->CrossWords.wordpos[CrossWord->weight];

	/* Realloc memory when required  */
	if(Doc->CrossWords.ncrosswords>=Doc->CrossWords.mcrosswords){
		Doc->CrossWords.mcrosswords+=WSIZE;
		Doc->CrossWords.CrossWord=(UDM_CROSSWORD *)UdmRealloc(Doc->CrossWords.CrossWord,Doc->CrossWords.mcrosswords*sizeof(UDM_CROSSWORD));
	}

	/* Add new word */
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].word = (char*)UdmStrdup(CrossWord->word);
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].url = (char*)UdmStrdup(CrossWord->url);
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].weight=CrossWord->weight;
	Doc->CrossWords.CrossWord[Doc->CrossWords.ncrosswords].pos=CrossWord->pos;
	Doc->CrossWords.ncrosswords++;
	return(0);
}

/* This function adds a normalized word form(s) into list using Ispell */
int UdmCrossListAdd(UDM_DOCUMENT * Doc,UDM_CROSSWORD * CrossWord){

	Doc->CrossWords.wordpos[CrossWord->weight]++;
	AddOneCrossWord(Doc,CrossWord);
	return(0);
}

void UdmCrossListFree(UDM_CROSSLIST * CrossList) {
	size_t i;
	for(i=0;i<CrossList->ncrosswords;i++){
		UDM_FREE(CrossList->CrossWord[i].word);
		UDM_FREE(CrossList->CrossWord[i].url);
	}
	CrossList->ncrosswords=0;
	CrossList->mcrosswords=0;
	UDM_FREE(CrossList->CrossWord);
	return;
}

UDM_CROSSLIST * UdmCrossListInit(UDM_CROSSLIST * List){
	bzero((void*)List, sizeof(*List));
	return(List);
}
