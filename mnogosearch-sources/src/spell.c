#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h> 
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>

/*
#define UDM_OK     0
#define UDM_ERROR  1
#define UDM_BINARY 0
#define UdmMalloc  malloc
#define UdmRealloc realloc
#define UdmFree    free
#define UdmStrdup  strdup
#define udm_snprintf snprintf
*/

#include "udm_common.h"
#include "udm_unidata.h"
#include "udm_spell.h"
#include "udm_utils.h"
#include "udm_hash.h"
#include "udm_crc32.h"

static void
UdmSpellListInit(UDM_SPELLLIST *Lst)
{
  bzero((void*)Lst, sizeof(Lst[0]));
}


static void
stUdmSpellListFree(UDM_SPELLLIST *Lst)
{
  UDM_FREE(Lst->fbody);
  UDM_FREE(Lst->Item);
}


static int
cmpspell(const void *a, const void *b)
{
  return strcmp(((const UDM_SPELL *)a)->word, ((const UDM_SPELL *)b)->word);
}


static int
cmpspell_word_and_flag(const void *a, const void *b)
{
  int res= strcmp(((const UDM_SPELL *)a)->word, ((const UDM_SPELL *)b)->word);
  if (!res)
    res= strcmp(((const UDM_SPELL *)a)->flags, ((const UDM_SPELL *)b)->flags);
  return res;
}


static int
detect_hashed_ispell_dict(UDM_SPELLLIST *L, int fd)
{
  char buf[1024*8];
  size_t i, nbytes, nwords;
  
  /* QQ: Why is that? */
  if (strlen(L->fname) + 4 >= UDM_SPELL_FILELEN)
    return 0;
  
  if (!(nbytes= read(fd, buf, sizeof(buf))))
    return 0;
  
  for (i= 0; i < nbytes; i++)
  {
    if (buf[i]== '\n')
    {
      L->wordlen= i + 1;
      break;
    }
  }

  if (L->wordlen <= 0 || L->wordlen >= 64)
    return 0;

  /* Number of words in the buffer */
  nwords= nbytes / (L->wordlen + 1) - 1;

  for (i= 1; i < nwords; i++)
  {
    char ch= buf[i * L->wordlen - 1];
    if (ch != '\n')
      return 0;
  }

  return 1;
}


static int
UdmSpellListLoad(UDM_SPELLLIST *L, char *err, size_t errlen)
{
  int rc= UDM_OK;
  struct stat sb;
  int fd;
  ssize_t nbytes;
  char *tok;
  const char *filename= L->fname;
  static char noflag[]="";
  char tolowermap[512];
  size_t i;
  
  if (L->fbody)
    return UDM_OK; /* Already loaded */
  
  L->cs= UdmGetCharSet(L->cset);
  if (!L->cs)
  {
    rc= UDM_ERROR;
    udm_snprintf(err, errlen, "Unknown charset '%s'", L->cset);
    goto ex;
  }
  
  if (stat(filename, &sb))
  {
    rc= UDM_ERROR;
    udm_snprintf(err, errlen, "Can't stat '%s'", filename);
    goto ex;
  }
  
  if((fd= open(filename,O_RDONLY|UDM_BINARY)) <= 0)
  {
    rc= UDM_ERROR;
    udm_snprintf(err, errlen, "Can't open '%s'", filename);
    goto ex;
  }

  /* Detect *.msp hash file */
  if (detect_hashed_ispell_dict(L, fd))
  {
    L->fmt= UDM_SPELL_FMT_HASH;
    L->nitems= sb.st_size / L->wordlen;
    close(fd);
    return UDM_OK;
  }


  lseek(fd, 0, SEEK_SET);

  if (!(L->fbody = (char*)UdmMalloc(sb.st_size+1)))
  {
    rc= UDM_ERROR;
    udm_snprintf(err, errlen, "Can't open '%s'", filename);
    goto ex;
  }
  
  if ((nbytes= read(fd, L->fbody, sb.st_size)) != sb.st_size)
  {
    rc= UDM_ERROR;
    udm_snprintf(err, errlen, "Read error");
    goto ex; 
  }
  
  L->fbody[nbytes]= '\0';

  for (i=0 ; i < 256; i++)
    tolowermap[i]= i;
  L->cs->lcase(udm_unidata_default, L->cs, tolowermap, 256);

  for (tok= L->fbody; *tok; )
  {
    UDM_SPELL *S;
    /*size_t wordlen;*/
    
    if (L->mitems <= L->nitems)
    {
      L->mitems+= 32*1024;
      L->Item= (UDM_SPELL*)UdmRealloc(L->Item, L->mitems*sizeof(L->Item[0]));
    }
    
    S= &L->Item[L->nitems];
    S->word= tok;
    S->flags= NULL;
    
    for (; *tok; tok++)
    {
      /* 
        This kind of tolower conversion is valid for 8bit charsets
        only, but I haven't seen non-8bit ispell files yet.
      */
      *tok= tolowermap[(unsigned char) *tok];
      if (*tok == '/')
      {
        /*wordlen= tok - S->word;*/
        *tok++= '\0';
        S->flags= tok;
        for ( ; *tok && *tok != '\r' && *tok != '\n' ; tok++);
        break;
      }
      else if (*tok == '\r' || *tok == '\n')
      {
        /*wordlen= tok - S->word;*/
        break;
      }  
    }
    for ( ; *tok == '\r' || *tok == '\n' ; *tok++= '\0');

    /*
      We skip the words without flags.
      If we need these words in the future,
      we need to combine the same words with
      and withot flags together. For example,
      'Cook' presents as a name as a separate
      word in the dictionary. We need to 
      combine it with 'cook'.
    */

    if (!S->flags)
      S->flags= noflag;

#if 0
    /* This is slow for 8bit character sets */
    L->cs->lcase(L->cs, S->word, wordlen);
#endif

    if (S->word[0] != '#')
      L->nitems++;
  }

  if (!strstr(filename, "sorted"))
    UdmSort((void*)L->Item, L->nitems, sizeof(L->Item[0]), cmpspell_word_and_flag);

ex:

  close(fd);
  return rc;
}


/*
  Write a spell list in fixed length format.
  The list can be prepared either using sort or hash.
*/
static int
UdmSpellListWriteFixed(UDM_SPELLLIST *L, char *err, size_t errlen)
{
  int rc= UDM_OK, f;
  size_t i, len, nbytes, wbytes;
  char *buf= NULL, fname[UDM_SPELL_FILELEN];
  
  if (!L->nitems)
  {
    udm_snprintf(err, errlen, "Nothing to convert: no words were loaded");
    rc= UDM_ERROR;
    goto ret;
  }
  for (len=0, i= 0; i < L->nitems; i++)
  {
    UDM_SPELL *Sp= &L->Item[i];
    size_t curlen= Sp->word ? strlen(Sp->word) + strlen(Sp->flags) : 0;
    if (len < curlen)
      len= curlen;
  }
  if (!len)
  {
    udm_snprintf(err, errlen, "Nothing to convert: all loaded words were empty");
    rc= UDM_ERROR;
    goto ret;
  }
  len+= 2;
  nbytes= len * L->nitems;
  if (!(buf= UdmMalloc(nbytes)))
  {
    udm_snprintf(err, errlen, "Failed to alloc %d bytes", (int) nbytes);
    rc= UDM_ERROR;
    goto ret;
  }
  memset((void*) buf, 0, nbytes);
  for (i= 0; i < L->nitems; i++)
  {
    size_t offs= i * len;
    UDM_SPELL *S= &L->Item[i];
    if (S->word)
    {
      size_t wrdlen= strlen(S->word);
      size_t flaglen= strlen(S->flags);
      memcpy(buf + offs, S->word, wrdlen);
      if (flaglen)
      {
        buf[offs + wrdlen]= '/';
        memcpy(buf + offs + wrdlen + 1, S->flags, flaglen);
      }
    }
    buf[offs + len - 1]= '\n';
  }
  udm_snprintf(fname, sizeof(fname), "%s.msp", L->fname);
  if ((f= open(fname, O_WRONLY|O_CREAT|O_TRUNC|UDM_BINARY,UDM_IWRITE)) < 0)
  {
    rc= UDM_ERROR;
    udm_snprintf(err, errlen, "Can't open file for writing: '%s'", fname);
    goto ret;
  }
  if ((wbytes= write(f, buf, nbytes)) != nbytes)
  {
    udm_snprintf(err, errlen, "Wrote only %d out of %d bytes into '%s'",
                               (int) wbytes, (int) nbytes, fname);
    rc= UDM_ERROR;
    goto ret;
  }
ret:
  UDM_FREE(buf);
  return rc;
}


static int
UdmSpellListHash(UDM_SPELLLIST *dst, UDM_SPELLLIST *src, char *err, size_t errlen)
{
  size_t nbytes, i, ncoll;
  *dst= *src;
  dst->fbody= NULL;
  dst->nitems= (src->nitems + 1) * 123 / 100;
  dst->mitems= dst->nitems;
  nbytes= dst->nitems * sizeof(UDM_SPELL);
  if (!(dst->Item= (UDM_SPELL*) UdmMalloc(nbytes)))
  {
    udm_snprintf(err, errlen, "Failed to alloc %d bytes", (int) nbytes);
    return UDM_ERROR;
  }
  bzero((void*) dst->Item, nbytes);
  for (ncoll= 0, i= 0; i < src->nitems; i++)
  {
    UDM_SPELL *S= &src->Item[i];
    size_t pos= (UdmStrCRC32(S->word) & 0x7FFFFFF) % dst->nitems;
      
    while (dst->Item[pos].word)
    {
      pos= (pos + 1) % dst->nitems;
      ncoll++;
    }
    dst->Item[pos]= *S;
  }
  return UDM_OK;
}


static int
UdmSpellListWriteHash(UDM_SPELLLIST *L, char *err, size_t errlen)
{
  int rc;
  UDM_SPELLLIST Hashed;
  if (UDM_OK == (rc= UdmSpellListHash(&Hashed, L, err, errlen)))
    rc= UdmSpellListWriteFixed(&Hashed, err, errlen);
  UDM_FREE(Hashed.Item);
  return rc;
}


int
UdmSpellListListWriteHash(UDM_SPELLLISTLIST *L, char *err, size_t errlen)
{
  int rc= UDM_OK;
  size_t i;
  if (!L->nitems)
  {
    udm_snprintf(err, errlen, "No ispell word lists were loaded, nothing to hash");
    return UDM_ERROR;
  }
  for (i= 0; i < L->nitems; i++)
  {
    if (UDM_OK != (rc= UdmSpellListWriteHash(&L->Item[i], err, errlen)))
      break;
  }
  return rc;
}

static size_t
UdmSpellAllForms(UDM_SPELLLISTLIST *SLL, UDM_AFFIXLISTLIST *ALL,
                 const char *word,  char **Res, size_t mres);

int
UdmSpellDump(UDM_SPELLLISTLIST *SLL, UDM_AFFIXLISTLIST *ALL,
             char *err, size_t errlen)
{
  char *forms[128];
  size_t sll;
  
  for (sll= 0; sll < SLL->nitems; sll++)
  {
    UDM_SPELLLIST *SL= &SLL->Item[sll];
    size_t sl;
    
    for (sl= 0 ; sl < SL->nitems; sl++)
    {
      UDM_SPELL *Spell= &SL->Item[sl];
      size_t i, n;
      char *s= Spell->word;
      n= UdmSpellAllForms(SLL, ALL, s, forms, 128);
      for (i=0 ; i < n; i++)
      {
        printf("%s/%s\n", s, forms[i]);
        UdmFree(forms[i]);
      }
    }
  }
  return UDM_OK;
}


void
UdmSpellListListInit(UDM_SPELLLISTLIST *L)
{
  bzero((void*)L, sizeof(*L));
}

void
UdmSpellListListFree(UDM_SPELLLISTLIST *L)
{
  size_t i;
  for (i=0 ; i < L->nitems; i++)
    stUdmSpellListFree(&L->Item[i]);
  if (L->Item)
    UdmFree(L->Item);
  /*
    For PHP extension - it can call UdmSpellListListFree()
    multiple times through udm_free_ispell_data()
  */
  UdmSpellListListInit(L);
}

int
UdmSpellListListAdd(UDM_SPELLLISTLIST *L,
                    const char *lang, const char *cset, const char *name)
{
  UDM_SPELLLIST *I;
  if (L->mitems <= L->nitems)
  {
    L->mitems+= 16;
    L->Item= (UDM_SPELLLIST*)UdmRealloc((void*)L->Item,
                                        L->mitems * sizeof(L->Item[0]));
    if (!L->Item)
      return UDM_ERROR;
  }
  I= &L->Item[L->nitems++];
  UdmSpellListInit(I);
  strcpy(I->lang, lang);
  strcpy(I->cset, cset);
  strcpy(I->fname, name);
  return UDM_OK;
}


int
UdmSpellListListLoad(UDM_SPELLLISTLIST *L, char *err, size_t errlen)
{
  size_t i;
  for (i=0 ; i < L->nitems; i++)
  {
    if (UdmSpellListLoad(&L->Item[i], err, errlen))
      return UDM_ERROR;
  }
  return UDM_OK;
}



static void
UdmAffixListInit(UDM_AFFIXLIST *L)
{
  bzero((void*)L, sizeof(*L));
}


static void
stUdmAffixListFree(UDM_AFFIXLIST *L)
{
  size_t i;
  for (i=0 ; i < L->nitems; i++)
  {
    UDM_AFFIX *A= &L->Item[i];
    UdmFree(A->find);
    UdmFree(A->repl);
    UdmFree(A->mask);
    regfree(&A->regex);
  }
  if (L->Item)
    UdmFree(L->Item);
}


static void rmsp(char *s)
{
  char *d;
  for (d= s; *s; s++)
  {
    if (*s != ' ' && *s != '-' && *s != '\t')
      *d++= *s;
  }
  *d= '\0';
}


static
int UdmAffixListLoad(UDM_AFFIXLIST *L, int flags, char *err, size_t errlen)
{
  char strbuf[128];
  char flag= 0;
  char mask[1024]= "";
  char find[1024]= "";
  char repl[1024]= "";
  char *s;
  int i, rc= UDM_OK;
  int suffixes= 0;
  int prefixes= 0;
  FILE *affix;
  char *filename= L->fname;

  if (L->Item)
    return UDM_OK; /* Already loaded */

  L->cs= UdmGetCharSet(L->cset);
  if (!L->cs)
  {
    rc= UDM_ERROR;
    udm_snprintf(err, errlen, "Unknown charset '%s'", L->cset);
    goto ex;
  }

  if(!(affix=fopen(filename,"r")))
  {
    udm_snprintf(err, errlen, "Can't open file '%s'", filename);
    return UDM_ERROR;
  }

  while(fgets(strbuf, sizeof(strbuf), affix))
  {
    UDM_AFFIX *A;
    char *str= UdmTrim(strbuf, "\t ");

    if(!strncasecmp(str,"suffixes",8))
    {
      suffixes=1;
      prefixes=0;
      continue;
    }
    if(!strncasecmp(str,"prefixes",8))
    {
      suffixes=0;
      prefixes=1;
      continue;
    }
    if(!strncasecmp(str,"flag ",5))
    {
      s=str+5;
      while(strchr("* ",*s))s++;
      flag= *s;
      continue;
    }
    if((!suffixes)&&(!prefixes))continue;
    if((prefixes)&&(flags & UDM_SPELL_NOPREFIX)) continue;

    if((s=strchr(str,'#')))*s=0;
    if(!*str)continue;

    mask[0]= '\0';
    find[0]= '\0';
    repl[0]= '\0';

    i= sscanf(str,"%[^>\n]>%[^,\n],%[^\n]",mask,find,repl);

    rmsp(repl);
    UdmStrToLower(udm_unidata_default, L->cs, repl, strlen(repl));
    
    rmsp(find);
    UdmStrToLower(udm_unidata_default, L->cs, find, strlen(find));
    
    rmsp(mask);
    UdmStrToLower(udm_unidata_default, L->cs, mask, strlen(mask));

    switch(i)
    {
      case 3:break;
      case 2:
        if(*find != '\0')
        {
          strcpy(repl,find);
          find[0]= '\0';
        }
        break;
      default:
        continue;
    }

    if (suffixes)
      sprintf(str, "%s$", mask);
    else
      sprintf(str, "^%s", mask);
    strcpy(mask, str);

/*
printf("'%s' '%s' '%s' '%s' '%c'\n", mask, find, repl, suffixes ? "s" : "p", flag);
*/
    if (L->mitems <= L->nitems)
    {
      L->mitems+= 256;
      L->Item= (UDM_AFFIX*) UdmRealloc(L->Item, L->mitems * sizeof(L->Item[0]));
    }
    A= &L->Item[L->nitems++];
    A->find= UdmStrdup(find);
    A->findlen= strlen(find);
    A->repl= UdmStrdup(repl);
    A->replen= strlen(repl);
    A->mask= UdmStrdup(mask);
    A->type= suffixes ? 's' : 'p';
    A->flag= flag;
    if (regcomp(&A->regex, A->mask, REG_EXTENDED|REG_NOSUB))
    {
      regfree(&A->regex);
      udm_snprintf(err, errlen, "Can't regcomp: '%s'", A->mask);
      rc= UDM_ERROR;
      goto ex;
    }
  }

ex:
  fclose(affix);
  return rc;
}


void
UdmAffixListListInit(UDM_AFFIXLISTLIST *L)
{
  bzero((void*) L, sizeof(*L)); 
}


void
UdmAffixListListFree(UDM_AFFIXLISTLIST *L)
{
  size_t i;
  for (i=0 ; i < L->nitems; i++)
    stUdmAffixListFree(&L->Item[i]);
  UDM_FREE(L->Item);
  /*
    For PHP extension - it can call UdmSpellListListFree()
    multiple times through udm_free_ispell_data()
  */
  UdmAffixListListInit(L);
}

int
UdmAffixListListAdd(UDM_AFFIXLISTLIST *L,
                    const char *lang, const char *cset, const char *name)
{
  UDM_AFFIXLIST *I;
  if (L->mitems <= L->nitems)
  {
    L->mitems+= 16;
    L->Item= (UDM_AFFIXLIST*)UdmRealloc((void*)L->Item,
                                        L->mitems * sizeof(L->Item[0]));
    if (!L->Item)
      return UDM_ERROR;
  }
  I= &L->Item[L->nitems++];
  UdmAffixListInit(I);
  strcpy(I->lang, lang);
  strcpy(I->cset, cset);
  strcpy(I->fname, name);
  return UDM_OK;
}


int
UdmAffixListListLoad(UDM_AFFIXLISTLIST *L, int flags, char *err, size_t errlen)
{
  size_t i;
  for (i=0 ; i < L->nitems; i++)
  {
    if (UdmAffixListLoad(&L->Item[i], flags, err, errlen))
      return UDM_ERROR;
  }
  return UDM_OK;
}


size_t
UdmSpellDenormalize(UDM_SPELLLIST *Sl,
                    UDM_AFFIXLIST *Al,
                    UDM_SPELL *S,
                    char **Res, size_t mres)
{
  UDM_AFFIX *Ab, *Ae;
  size_t nres= 0;
  size_t len= strlen(S->word);

  if (!S->flags)
    return 0;

  for (Ab= &Al->Item[0], Ae= &Al->Item[Al->nitems]; Ab < Ae; Ab ++)
  {
    char wrd[128];
    if (Ab->type == 's' && strchr(S->flags, Ab->flag) &&
        !strcmp(S->word + len - Ab->findlen, Ab->find) &&
        !regexec(&Ab->regex, S->word, 0, NULL, 0))
    {
      memcpy(wrd, S->word, len - Ab->findlen);
      strcpy(wrd + len - Ab->findlen, Ab->repl);
    } else if (Ab->type == 'p' && strchr(S->flags, Ab->flag) &&
               !memcmp(S->word, Ab->find, Ab->findlen) &&
               !regexec(&Ab->regex, S->word, 0, NULL, 0))
    {
      memcpy(wrd, Ab->repl, Ab->replen);
      strcpy(wrd + Ab->replen, S->word + Ab->findlen);
    }
    else
      continue;
    
    if (nres < mres)
    {
      Res[nres++]= UdmStrdup(wrd);
    }
    /* 
    printf("Found: '%s/%s/%s/%c' '%s'\n",
           Ab->mask, Ab->find, Ab->repl, Ab->flag, wrd);
    */
  }
  return nres;
}


typedef struct udm_spell_search_info_st
{
  char buf[128];
  long pos;
  UDM_SPELL Spell;
} UDM_SPSEARCH_INFO;

static UDM_SPELL *
UdmSpellListFindFirst(UDM_SPELLLIST *Sl, UDM_SPELL *Find, UDM_SPSEARCH_INFO *Si)
{
  UDM_SPELL *Beg= (UDM_SPELL*) UdmBSearch((const void *) Find,
                                          (const void *) Sl->Item, Sl->nitems,
                                           sizeof(Sl->Item[0]), cmpspell);
  if (Beg)
  {
    for ( ; Beg > Sl->Item && !strcmp(Find->word, Beg[-1].word); Beg--);
    Si->pos= Beg - Sl->Item;
  }
  return Beg;
}


static UDM_SPELL *
UdmSpellListFindNext(UDM_SPELLLIST *Sl, UDM_SPELL *Prev, UDM_SPSEARCH_INFO *Si)
{
  Si->pos++;
  if (Si->pos < Sl->nitems && !strcmp(Prev->word, Sl->Item[Si->pos].word))
    return &Sl->Item[Si->pos];
  return NULL;
}


static UDM_SPELL *
LoadSpellAtOffset(UDM_SPELLLIST *Sl, UDM_SPELL *Find, UDM_SPSEARCH_INFO *Si)
{
  off_t offs;
  ssize_t rbytes;
  long pos= Si->pos * Sl->wordlen;
  if (pos != (offs= lseek(Sl->fd, pos, SEEK_SET)) ||
      Sl->wordlen != (rbytes= read(Sl->fd, Si->buf, Sl->wordlen)))
    return NULL;
  
  Si->Spell.word= Si->buf;
  if ((Si->Spell.flags= strchr(Si->buf, '/')))
  {
    *Si->Spell.flags= '\0';
    Si->Spell.flags++;
  }
  else
    Si->Spell.flags= Si->Spell.word + strlen(Si->Spell.word);
  return &Si->Spell;
}


static UDM_SPELL *
FindSpellAtOffset(UDM_SPELLLIST *Sl, UDM_SPELL *Find, UDM_SPSEARCH_INFO *Si)
{
  UDM_SPELL *Sp;
  while ((Sp= LoadSpellAtOffset(Sl, Find, Si)) && Sp->word[0])
  {
    if (!strcmp(Sp->word, Find->word))
    {
      return Sp;
    }
    Si->pos= (Si->pos + 1) % Sl->nitems;
  }
  return NULL;
}


static UDM_SPELL *
UdmSpellListFindFirstHash(UDM_SPELLLIST *Sl, UDM_SPELL *Find, UDM_SPSEARCH_INFO *Si)
{
  Si->pos= (UdmStrCRC32(Find->word) & 0x7FFFFFF) % Sl->nitems;
  return FindSpellAtOffset(Sl, Find, Si);
}


static UDM_SPELL *
UdmSpellListFindNextHash(UDM_SPELLLIST *Sl, UDM_SPELL *Prev, UDM_SPSEARCH_INFO *Si)
{
  Si->pos= (Si->pos + 1) % Sl->nitems;
  return FindSpellAtOffset(Sl, Prev, Si);
}


size_t
UdmSpellNormalize(UDM_SPELLLIST *Sl, UDM_AFFIXLIST *Al,
                  const char *word, UDM_SPELL *Res, size_t nres)
{
  UDM_AFFIX *Ab, *Ae;
  UDM_SPELL *N, Find;
  char wrd[128];
  char noflag[]= "";
  size_t len= strlen(word);
  size_t cres= 0;
  UDM_SPELL *(*FindFirst)(UDM_SPELLLIST *Sl_arg, UDM_SPELL *Find_arg, UDM_SPSEARCH_INFO *si);
  UDM_SPELL *(*FindNext)(UDM_SPELLLIST *Sl_arg, UDM_SPELL *Prev_arg, UDM_SPSEARCH_INFO *si);
  UDM_SPSEARCH_INFO Si;

  if (len + 1 > sizeof(wrd))
    return 0;
  
  if (Sl->fmt == UDM_SPELL_FMT_HASH)
  {
    FindFirst= UdmSpellListFindFirstHash;
    FindNext= UdmSpellListFindNextHash;
    if ((Sl->fd= open(Sl->fname, O_RDONLY|UDM_BINARY)) < 0)
      return 0;
  }
  else
  {
    FindFirst= UdmSpellListFindFirst;
    FindNext= UdmSpellListFindNext;
  }
  
  for (Ab= &Al->Item[0], Ae= &Al->Item[Al->nitems]; Ab < Ae; Ab ++)
  {
    size_t rootlen;

    if (len < Ab->replen)
      continue;

    rootlen= len - Ab->replen;

    if (len + Ab->replen + Ab->findlen + 1 > sizeof(wrd))
      continue;

    if (Ab->type == 's' && !memcmp(word + rootlen, Ab->repl, Ab->replen))
    {
      memcpy(wrd, word, rootlen);
      strcpy(wrd + rootlen, Ab->find);
    }
    else if (Ab->type == 'p' && !memcmp(word, Ab->repl, Ab->replen))
    {
      memcpy(wrd, Ab->find, Ab->findlen);
      strcpy(wrd + Ab->findlen, word + Ab->replen);
    }
    else
      continue;

    /*
    printf("HERE0 '%s'\n", wrd);
    printf("HERE1 '%s' '%s' '%s' '%c' \n", Ab->find, Ab->repl, wrd, Ab->flag);
    */

    Find.word= wrd;
    Find.flags= noflag;
    for (N= FindFirst(Sl, &Find, &Si) ; N; N= FindNext(Sl, &Find, &Si))
    {
      if (N->flags[0] && strchr(N->flags, Ab->flag) &&
          !regexec(&Ab->regex, wrd, 0, NULL, 0))
      {
        if (cres < nres)
        {
          Res[cres].word= UdmStrdup(N->word);
          Res[cres].flags= UdmStrdup(N->flags);
          cres++;
        }
      }
    }
  }

  /* Check that the word itself is a normal form */

  strcpy(wrd, word);
  Find.word= wrd;
  Find.flags= noflag;
  for (N= FindFirst(Sl, &Find, &Si) ; N; N= FindNext(Sl, &Find, &Si))
  {
    if (cres < nres)
    {
      Res[cres].word= UdmStrdup(N->word);
      Res[cres].flags= UdmStrdup(N->flags);
      cres++;
    }
  }

  if (Sl->fmt == UDM_SPELL_FMT_HASH)
  {
    close(Sl->fd);
  }

  return cres;
}


static size_t
UdmSpellAllForms(UDM_SPELLLISTLIST *SLL, UDM_AFFIXLISTLIST *ALL,
                 const char *word,  char **Res, size_t mres)
{
  UDM_AFFIXLIST *Al;
  size_t nres= 0;
  for (Al= ALL->Item; Al < &ALL->Item[ALL->nitems]; Al++)
  {
    UDM_SPELLLIST *Sl;
    for (Sl= SLL->Item; Sl < &SLL->Item[SLL->nitems]; Sl++)
    {
      if (!strcmp(Al->lang, Sl->lang) && !strcmp(Al->cset, Sl->cset))
      {
        UDM_SPELL Norm[128], *N;
        size_t nnorm= UdmSpellNormalize(Sl, Al, word, Norm, 128);
        for (N= Norm ; N < Norm + nnorm; N++)
        {
          size_t cres;
          if (mres)
          {
            *Res++= UdmStrdup(N->word);
            nres++;
            mres--;
          }
          cres= UdmSpellDenormalize(Sl, Al, N, Res, mres);
          nres+= cres;
          mres-= cres;
          Res+= cres;
        }
      }
    }
  }
  return nres;
}


#ifdef UDM_SPELL_DEMO

#include <locale.h>
int main(void)
{
  UDM_AFFIXLISTLIST ALL;
  UDM_SPELLLISTLIST SLL;
  size_t n;
  setlocale(LC_ALL, "ru_RU.KOI8-R");
  char str[128];
  char err[128]= "";
  char *forms[128];
  int flags= UDM_SPELL_NOPREFIX;

  UdmSpellListListInit(&SLL);
  UdmSpellListListAdd(&SLL, "ru", "koi8-r", "russian.dict");
  UdmSpellListListAdd(&SLL, "en", "latin1", "british.xlg");
  UdmSpellListListAdd(&SLL, "en", "latin1", "american.xlg");

  UdmAffixListListInit(&ALL);
  UdmAffixListListAdd(&ALL, "ru", "koi8-r", "russian.aff");
  UdmAffixListListAdd(&ALL, "en", "latin1", "english.aff");


  if (UdmSpellListListLoad(&SLL, err, sizeof(err)) ||  
      UdmAffixListListLoad(&ALL, flags, err, sizeof(err)))
  {
    printf("error: %s\n", err);
    goto ex;
  }
  while (fgets(str, sizeof(str), stdin))
  {
    size_t i;
    char *s;
    for (s= str; *s; s++)
    {
      if (*s == '\r' || *s == '\n')
      {
        *s= '\0';
        break;
      }
    }
    UdmTolower(str, strlen(str));
    n= UdmSpellAllForms(&SLL, &ALL, str, forms, 128);
    printf("total: %d word: '%s'\n", n, str);
    for (i=0 ; i < n; i++)
    {
      printf("[%d] %s\n", i, forms[i]);
      UdmFree(forms[i]);
    }
  }

ex:

  UdmAffixListListFree(&ALL);
  UdmSpellListListFree(&SLL);

  return 0;
}

#endif


#ifdef HAVE_MYSQL_FULLTEXT_PLUGIN
#define MYSQL_DYNAMIC_PLUGIN 1
#undef PACKAGE
#undef VERSION

#include <my_global.h>
#include <m_string.h>
#include <m_ctype.h>
#include <plugin.h>

#include "udm_env.h"
#include "udm_agent.h"
#include "udm_conf.h"
#include "udm_vars.h"

static UDM_ENV Env;


/*
  Initialize the parser plugin at server start or plugin installation.
  Load stemming.conf from MySQL's datadir, and load the given affix
  and spell files.

  SYNOPSIS
    udm_parser_init()

  DESCRIPTION
    Open and load ispell affix and dictionary files.

  CONF FILE FORMAT
    A typical stemming.conf looks like:
    
      MinWordLength 1
      Spell en latin1 american.xlg
      Affix en latin1 english.aff

   File names are relative to MySQL datadir.
   
  RETURN VALUE
    0              success
    1              one of the files was not found,
                   can't be opened, or out of memory
*/

static int
udm_parser_init(void *param __attribute__((unused)))
{
  const char *conf= "stemming.conf";
  char err[128]= "";
  char cwd[256]= "";
  int flags= UDM_SPELL_NOPREFIX, rc= UDM_OK;
  UDM_AGENT Agent;

  UdmEnvInit(&Env);
  UdmAgentInit(&Agent, &Env, 0);
  getcwd(cwd, sizeof(cwd));
  UdmVarListReplaceStr(&Env.Vars, "ConfDir", cwd);
  if (UDM_OK != (rc= UdmEnvLoad(&Agent, conf, UDM_FLAG_SPELL)))
  {
    fprintf(stderr, "Cannot load conf file '%s': %s\n",
            conf, UdmEnvErrMsg(&Env));
    goto ex;
  }

  if (Env.Spells.nitems != 1 || Env.Affixes.nitems != 1)
  {
    fprintf(stderr, "udm_spell.conf must have exactly one Spell and one Affix command\n");
    rc= UDM_ERROR;
    goto ex;
  }

  if (UdmSpellListListLoad(&Env.Spells, err, sizeof(err)) ||  
      UdmAffixListListLoad(&Env.Affixes, flags, err, sizeof(err)))
  {
    fprintf(stderr, "error: %s\n", err);
    rc= UDM_ERROR;
    goto ex;
  }
ex:
  UdmAgentFree(&Agent);
  return rc;
}


/*
  Free the memory and resources of the parser plugin at server shutdown
  or plugin deinstallation.

  SYNOPSIS
    udm_parser_deinit()

  DESCRIPTION
    Frees allocated UDM_ENV data.

  RETURN VALUE
    0                    never fails
*/

static int
udm_parser_deinit(void *param __attribute__((unused)))
{
  UdmEnvFree(&Env);
  return 0;
}


/*
  Add normalized forms of the word.

  SYNOPSIS
    add_base_form()
     param   - parsing context
     word    - a word
     length  - the word length

  DESCRIPTION
    Search for normalized word forms.
    Add the normalized forms if they found,
    or the word itself otherwise.

  NOTES
    Some words can have several normalized forms.
*/


static void
add_base_form(MYSQL_FTPARSER_PARAM *param, char *word, size_t length,
              MYSQL_FTPARSER_BOOLEAN_INFO *boolean_info)
{
  char tmpword[128];
  size_t nforms;
  UDM_SPELL Norm[128];

  if (length >= sizeof(tmpword))
  {
    /* Too long word, don't try to normalize */
    param->mysql_add_word(param, word, length, boolean_info);
    return;
  }
  
  memcpy(tmpword, word, length);
  tmpword[length]= '\0';

  /* Fetch normalized form of the word */
  if ((nforms= UdmSpellNormalize(&Env.Spells.Item[0],
                                 &Env.Affixes.Item[0],
                                 tmpword, Norm, 128)))
  {
    UDM_SPELL *N;
    if (param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO)
    {
      boolean_info->type= FT_TOKEN_LEFT_PAREN;
      param->mysql_add_word(param, 0, 0, boolean_info);
      boolean_info->type= FT_TOKEN_WORD;
      boolean_info->yesno= 0;
    }
    for (N= Norm ; N < Norm + nforms; N++)
    {
      size_t nlength= strlen(N->word);
      if (nlength >= Env.WordParam.min_word_len)
        param->mysql_add_word(param, N->word, nlength, boolean_info);
    }
    if (param->mode == MYSQL_FTPARSER_FULL_BOOLEAN_INFO)
    {
      boolean_info->type= FT_TOKEN_RIGHT_PAREN;
      param->mysql_add_word(param, 0, 0, boolean_info);
      boolean_info->type= FT_TOKEN_WORD;
      boolean_info->yesno= 1;
    }
  }
  else
  {
    /* No forms found, add the word itself */
    param->mysql_add_word(param, word, length, boolean_info);
  }

#if 0
  /*
    Maybe sometimes it's usefull to store all forms,
    rather than just the normal form, but in this case
    the index can be huge.
  */
  if ((nforms= UdmSpellAllForms(&Env.Spells, &Env.Affixes,
                                tmpword, forms, 128)))
  {
    printf("total: %d word: '%s'\n", nforms, tmpword);
    for (i=0 ; i < nforms; i++)
    {
      size_t len= strlen(forms[i]);
      printf("[%d] %s len=%d\n", i, forms[i], len);
      if (len >= Env.WordParam.min_word_len)
        add_word(param, forms[i], len);
      UdmFree(forms[i]);
    }
  }
#endif
}


static int udm_parser_boolean_parse(MYSQL_FTPARSER_PARAM *param)
{
  char *end, *start, *docend= param->doc + param->length;
  MYSQL_FTPARSER_BOOLEAN_INFO boolean_info=
    { FT_TOKEN_WORD, 1, 0, 0, 0, ' ', 0 };
  for (end= start= param->doc;; end++)
  {
    if (end == docend)
    {
      if (end - start >= Env.WordParam.min_word_len)
        add_base_form(param, start, end - start, &boolean_info);
      break;
    }
    else if (!my_isalnum(param->cs, *end) && *end != '_')
    {
      if (end - start >= Env.WordParam.min_word_len)
        add_base_form(param, start, end - start, &boolean_info);
      start= end + 1;
    }
  }
  return 0;
}


static int udm_parser_simple_parse(MYSQL_FTPARSER_PARAM *param)
{
  char *end, *start, *docend= param->doc + param->length;
  for (end= start= param->doc;; end++)
  {
    if (end == docend)
    {
      if (end - start >= Env.WordParam.min_word_len ||
          (param->mode == MYSQL_FTPARSER_WITH_STOPWORDS && end - start))
        add_base_form(param, start, end - start, 0);
      break;
    }
    else if (!my_isalnum(param->cs, *end) && *end != '_')
    {
      if (end - start >= Env.WordParam.min_word_len ||
          (param->mode == MYSQL_FTPARSER_WITH_STOPWORDS && end - start))
        add_base_form(param, start, end - start, 0);
      start= end + 1;
    }
  }
  return 0;
}

/*
  Parse a document or a search query.

  SYNOPSIS
    udm_parser_parse()
      param              parsing context

  DESCRIPTION
    This is the main plugin function which is called to parse
    a document or a search query. The call mode is set in param->flags.
    When parsing a document, it simply splits the text into words,
    then normalized the word using ispell data,
    and passes the normalized forms to MySQL fultext indexing engine.
*/

static int
udm_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  switch (param->mode) {
    case MYSQL_FTPARSER_SIMPLE_MODE:
    case MYSQL_FTPARSER_WITH_STOPWORDS:
      return udm_parser_simple_parse(param);
    case MYSQL_FTPARSER_FULL_BOOLEAN_INFO:
      return udm_parser_boolean_parse(param);
  }
  return 1;
}


static struct st_mysql_ftparser udm_parser_declaration=
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  udm_parser_parse,                 /* parsing function       */
  0,                                /* parser init function   */
  0                                 /* parser deinit function */
};


mysql_declare_plugin(stemming)
{
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &udm_parser_declaration,    /* descriptor                      */
  "stemming",                 /* name                            */
  "mnoGoSearch team",         /* author                          */
  "ispell-based stemming",    /* description                     */
  PLUGIN_LICENSE_GPL,
  udm_parser_init,            /* init function (when loaded)     */
  udm_parser_deinit,          /* deinit function (when unloaded) */
  0x0001,                     /* version                         */
  NULL,                       /* status variables                */
  NULL,                       /* system variables                */
  NULL                        /* config options                  */
}
mysql_declare_plugin_end;

#endif /* HAVE_MYSQL_FULLTEXT_PLUGIN */
