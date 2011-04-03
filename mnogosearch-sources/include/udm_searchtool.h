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

#ifndef _UDM_SEARCH_TOOL_H
#define _UDM_SEARCH_TOOL_H

#define UDM_FAST_PRESORT_DOCS	300
#define UDM_DEFAULT_USER_WORD_WEIGHT 256


/* Functions form urldata.c */
extern void UdmURLDataSortBySite(UDM_URLDATALIST *L);
extern void UdmURLDataSortByPattern(UDM_URLDATALIST *D, const char *pattern);
extern int UdmURLDataListSearch(UDM_URLDATALIST *List, urlid_t id);
extern int UdmURLDataListClearParams(UDM_URLDATALIST *List, size_t num_best_rows);
extern size_t UdmURLDataCompact(UDM_URLDATA *dst, UDM_URLDATA *src, size_t n);

/* Functions from groupby.c */
extern void UdmURLDataGroupBySite(UDM_URLDATALIST *L);
extern int UdmURLDataListGroupBySiteUsingHash(UDM_AGENT *A,
                                              UDM_URLDATALIST *DataList,
                                              const char *rec_id_str, size_t rec_id_len,
                                              const char *site_id_str, size_t site_id_len);
extern int UdmURLDataListGroupBySiteUsingSort(UDM_AGENT *A,
                                              UDM_URLDATALIST *R,
                                              UDM_DB *db);
extern void UdmURLDataApplySiteRank(UDM_AGENT *A, UDM_URLDATALIST *DataList,
                                    int is_aggregation_point);


/* Functions from distance.c */
typedef struct udm_distance_stat_st
{
  size_t sum;
  size_t num;
} UDM_WORD_DISTANCE_STAT;


extern void CalcAverageWordDistance(size_t wf2_secno,
                                    UDM_COORD2 *phr, size_t num, size_t nuniq,
                                    UDM_WORD_DISTANCE_STAT *dist);
                                                 
/* Functions from score.c */
extern int UdmUserScoreListApplyToURLScoreList(UDM_AGENT *A,
                                               UDM_URLSCORELIST *URLScoreList,
                                               UDM_URL_INT4_LIST *UserScoreList,
                                               int UserScoreFactor);
extern int UdmUserScoreListApplyToURLDataList(UDM_AGENT *A,
                                              UDM_URLDATALIST *URLDataList,
                                              UDM_URL_INT4_LIST *UserScoreList,
                                              int UserScoreFactor);

extern int
UdmURLDataListApplyRelevancyFactors(UDM_AGENT *Agent,
                                    UDM_URLDATALIST *DataList,
                                    int RelevancyFactor,
                                    int DateFactor);

extern void UdmURLScoreListSortByScore(UDM_URLSCORELIST *ScoreList);
extern void UdmURLScoreListSortByScoreThenURLTop(UDM_URLSCORELIST *ScoreList,
                                                 size_t topcount);
extern void UdmURLScoreListSortByScoreThenURL(UDM_URLSCORELIST *ScoreList);

extern void UdmGroupByURL2(UDM_AGENT *Agent,
                           UDM_DB *db,
                           UDM_RESULT *Res,
                           UDM_SECTIONLIST *CoordList,
                           UDM_URLSCORELIST *ScoreList);

extern int  UdmPrepare(UDM_AGENT *query,UDM_RESULT *res);
extern __C_LINK int __UDMCALL UdmParseQueryString(UDM_AGENT * Agent,UDM_VARLIST * vars, const char * query_string);

/* Functions from wordinfo.c */
int UdmResWordInfo(UDM_ENV *Env, UDM_RESULT *Res);


/* Functions from fuzzy.c */
int UdmAllForms(UDM_AGENT *Indexer,
                UDM_WIDEWORDLIST *result,
                UDM_WIDEWORD *uword);

int UdmComplexSynonyms(UDM_AGENT *Indexer, UDM_WIDEWORDLIST *result);

/* Functions from highlight.c */
extern char *UdmHlConvert(UDM_WIDEWORDLIST *L,const char * src, UDM_CHARSET * lcs, UDM_CHARSET * bcs);
extern size_t UdmHlConvertExt(UDM_AGENT *A, char *dst, size_t dstlen,
                              UDM_WIDEWORDLIST *L, UDM_CHARSET *wcs,
                              const char * src, size_t length,
                              UDM_CHARSET * lcs, UDM_CHARSET * bcs,
                              int hilight_stopwords, int segmenter);
extern size_t
UdmHlConvertExtWithConv(UDM_AGENT *A, char *dst, size_t dstmaxlen,
                        UDM_WIDEWORDLIST *List,
                        const char *src, size_t srclen,
                        UDM_CONV *uni_wcs, UDM_CONV *lc_uni, UDM_CONV *uni_bc,
                        int hilight_stopwords, int segmenter);
                                                                                               
extern int  UdmConvert(UDM_ENV *Conf, UDM_RESULT *Res,UDM_CHARSET *lcs,UDM_CHARSET *bcs);
extern char* UdmRemoveHiLightDup(const char *s);

/* Functions from segment.c */
int *UdmUniSegment(UDM_AGENT *Indexer, int *s, const char *lang, const char *seg);
int *UdmUniSegmentByType(UDM_AGENT *Indexer, int *s, int type, int ch);
int UdmUniSegmenterFind(UDM_AGENT *Indexer, const char *lang, const char *seg);

/* Functions from section.c */
void UdmSectionListPrint(UDM_SECTIONLIST *SectionList);
int UdmSectionListAlloc(UDM_SECTIONLIST *List, size_t ncoords, size_t nsections);
void UdmSectionListFree(UDM_SECTIONLIST *List);
int UdmSectionListListAdd(UDM_SECTIONLISTLIST *List, UDM_SECTIONLIST *Item);
void UdmSectionListListInit(UDM_SECTIONLISTLIST *List);
void UdmSectionListListFree(UDM_SECTIONLISTLIST *List);
int UdmSectionListListMergeSorted(UDM_SECTIONLISTLIST *SrcList,
                                  UDM_SECTIONLIST *Dst, int opt);

/* Functions from urlidlist.c */
int UdmCmpURLID(urlid_t *s1, urlid_t *s2); /* for qsort */
int UdmURLIdListJoin(UDM_URLID_LIST *urls, UDM_URLID_LIST *fl_urls);
int UdmURLIdListUnion(UDM_URLID_LIST *a, UDM_URLID_LIST *b);
int UdmURLIdListCopy(UDM_URLID_LIST *a, UDM_URLID_LIST *b);
int UdmURLIdListMerge(UDM_URLID_LIST *a, UDM_URLID_LIST *b);
int UdmURLIdListSort(UDM_URLID_LIST *a);

/* Functions from coords.c */
void
UdmURLCRDListSortByURLThenSecnoThenPos(UDM_URLCRDLIST *L);


/* Functions from sql.c */
int
UdmUserSiteScoreListLoadAndApplyToURLDataList(UDM_AGENT *Agent,
                                              UDM_URLDATALIST *List,
                                              UDM_DB *db);
#endif
