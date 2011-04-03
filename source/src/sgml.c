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

#include <stdlib.h>
#include <string.h>

#include "udm_sgml.h"
#include "udm_unicode.h"


static const struct udm_sgml_chars
{
     const char     *sgml;
     int       unicode;
} SGMLChars[] = {

{    "lt",          '<'  },
{    "gt",          '>'  },
{    "amp",         '&'  },
{    "quot",        '"'  },

{    "nbsp",        0xA0 },   /* non breaking space */

/* ISO-8859-1 entities */
     
{    "iexcl",  161  },   /* inverted exclamation mark */
{    "cent",        162  },   /* cent sign */
{    "pound",  163  },   /* pound sign */
{    "curren", 164  },   /* currency sign */
{    "yen",         165  },   /* yen sign */
{    "brvbar", 166  },   /* broken vertical bar, (brkbar) */
{    "sect",        167  },   /* section sign */
{    "uml",         168  },   /* spacing diaresis */
{    "copy",        169  },   /* copyright sign */
{    "ordf",        170  },   /* feminine ordinal indicator */
{    "laquo",  171  },   /* angle quotation mark, left */
{    "not",         172  },   /* negation sign */
{    "shy",         173  },   /* soft hyphen */
{    "reg",         174  },   /* circled R registered sign */
{    "hibar",  175  },   /* spacing macron */
{    "deg",         176  },   /* degree sign */
{    "plusmn", 177  },   /* plus-or-minus sign */
{    "sup2",        178  },   /* superscript 2 */
{    "sup3",        179  },   /* superscript 3 */
{    "acute",  180  },   /* spacing acute (96) */
{    "micro",  181  },   /* micro sign */
{    "para",        182  },   /* paragraph sign */
{    "middot", 183  },   /* middle dot */
{    "cedil",  184  },   /* spacing cedilla */
{    "sup1",        185  },   /* superscript 1 */
{    "ordm",        186  },   /* masculine ordinal indicator */
{    "raquo",  187  },   /* angle quotation mark, right */
{    "frac14", 188  },   /* fraction 1/4 */
{    "frac12", 189  },   /* fraction 1/2 */
{    "frac34", 190  },   /* fraction 3/4 */
{    "iquest", 191  },   /* inverted question mark */
{    "Agrave", 192  },   /* capital A, grave accent */ 
{    "Aacute", 193  },   /* capital A, acute accent */ 
{    "Acirc",  194  },   /* capital A, circumflex accent */ 
{    "Atilde", 195  },   /* capital A, tilde */ 
{    "Auml",        196  },   /* capital A, dieresis or umlaut mark */ 
{    "Aring",  197  },   /* capital A, ring */ 
{    "AElig",  198  },   /* capital AE diphthong (ligature) */ 
{    "Ccedil", 199  },   /* capital C, cedilla */ 
{    "Egrave", 200  },   /* capital E, grave accent */ 
{    "Eacute", 201  },   /* capital E, acute accent */ 
{    "Ecirc",  202  },   /* capital E, circumflex accent */ 
{    "Euml",        203  },   /* capital E, dieresis or umlaut mark */ 
{    "Igrave", 205  },   /* capital I, grave accent */ 
{    "Iacute", 204  },   /* capital I, acute accent */ 
{    "Icirc",  206  },   /* capital I, circumflex accent */ 
{    "Iuml",        207  },   /* capital I, dieresis or umlaut mark */ 
{    "ETH",         208  },   /* capital Eth, Icelandic (Dstrok) */ 
{    "Ntilde", 209  },   /* capital N, tilde */ 
{    "Ograve", 210  },   /* capital O, grave accent */ 
{    "Oacute", 211  },   /* capital O, acute accent */ 
{    "Ocirc",  212  },   /* capital O, circumflex accent */ 
{    "Otilde", 213  },   /* capital O, tilde */ 
{    "Ouml",        214  },   /* capital O, dieresis or umlaut mark */ 
{    "times",  215  },   /* multiplication sign */ 
{    "Oslash", 216  },   /* capital O, slash */ 
{    "Ugrave", 217  },   /* capital U, grave accent */ 
{    "Uacute", 218  },   /* capital U, acute accent */ 
{    "Ucirc",  219  },   /* capital U, circumflex accent */ 
{    "Uuml",        220  },   /* capital U, dieresis or umlaut mark */ 
{    "Yacute", 221  },   /* capital Y, acute accent */ 
{    "THORN",  222  },   /* capital THORN, Icelandic */ 
{    "szlig",  223  },   /* small sharp s, German (sz ligature) */ 
{    "agrave", 224  },   /* small a, grave accent */ 
{    "aacute", 225  },   /* small a, acute accent */ 
{    "acirc",  226  },   /* small a, circumflex accent */ 
{    "atilde", 227  },   /* small a, tilde */
{    "auml",        228  },   /* small a, dieresis or umlaut mark */ 
{    "aring",  229  },   /* small a, ring */
{    "aelig",  230  },   /* small ae diphthong (ligature) */ 
{    "ccedil", 231  },   /* small c, cedilla */ 
{    "egrave", 232  },   /* small e, grave accent */ 
{    "eacute", 233  },   /* small e, acute accent */ 
{    "ecirc",  234  },   /* small e, circumflex accent */ 
{    "euml",        235  },   /* small e, dieresis or umlaut mark */ 
{    "igrave", 236  },   /* small i, grave accent */ 
{    "iacute", 237  },   /* small i, acute accent */ 
{    "icirc",  238  },   /* small i, circumflex accent */ 
{    "iuml",        239  },   /* small i, dieresis or umlaut mark */ 
{    "eth",         240  },   /* small eth, Icelandic */ 
{    "ntilde", 241  },   /* small n, tilde */ 
{    "ograve", 242  },   /* small o, grave accent */ 
{    "oacute", 243  },   /* small o, acute accent */ 
{    "ocirc",  244  },   /* small o, circumflex accent */ 
{    "otilde", 245  },   /* small o, tilde */ 
{    "ouml",        246  },   /* small o, dieresis or umlaut mark */ 
{    "divide", 247  },   /* division sign */
{    "oslash", 248  },   /* small o, slash */ 
{    "ugrave", 249  },   /* small u, grave accent */ 
{    "uacute", 250  },   /* small u, acute accent */ 
{    "ucirc",  251  },   /* small u, circumflex accent */ 
{    "uuml",        252  },   /* small u, dieresis or umlaut mark */ 
{    "yacute", 253  },   /* small y, acute accent */ 
{    "thorn",  254  },   /* small thorn, Icelandic */ 
{    "yuml",        255  },   /* small y, dieresis or umlaut mark */

/* Latin Extended-A */ 

{    "OElig",  338  }, 
{    "oelig",  339  }, 
{    "Scaron", 352  }, 
{    "scaron", 353  }, 
{    "Yuml",        376  }, 

/* Latin-Extended-B */

{    "fnof",        402  }, 


/* Accents */

{    "circ",        710  }, 
{    "tilde",  732  }, 

/* Greek */

{    "Alpha",  913  }, 
{    "Beta",        914  }, 
{    "Gamma",  915  },
{    "Delta",  916  }, 
{    "Epsilon",     917  }, 
{    "Zeta",        918  }, 
{    "Eta",         919  }, 
{    "Theta",  920  }, 
{    "Iota",        921  }, 
{    "Kappa",  922  }, 
{    "Lambda", 923  }, 
{    "Mu",          924  }, 
{    "Nu",          925  }, 
{    "Xi",          926  }, 
{    "Omicron",     927  }, 
{    "Pi",          928  }, 
{    "Rho",         929  }, 
{    "Sigma",  931  }, 
{    "Tau",         932  }, 
{    "Upsilon",     933  }, 
{    "Phi",         934  }, 
{    "Chi",         935  }, 
{    "Psi",         936  }, 
{    "Omega",  937  }, 
{    "alpha",  945  }, 
{    "beta",        946  }, 
{    "gamma",  947  }, 
{    "delta",  948  }, 
{    "epsilon",     949  }, 
{    "zeta",        950  }, 
{    "eta",         951  }, 
{    "theta",  952  }, 
{    "iota",        953  }, 
{    "kappa",  954  }, 
{    "lambda", 955  }, 
{    "mu",          956  }, 
{    "nu",          957  }, 
{    "xi",          958  }, 
{    "omicron",     959  }, 
{    "pi",          960  }, 
{    "rho",         961  }, 
{    "sigmaf", 962  }, 
{    "sigma",  963  }, 
{    "tau",         964  }, 
{    "upsilon",     965  }, 
{    "phi",         966  }, 
{    "chi",         967  }, 
{    "psi",         968  }, 
{    "omega",  969  }, 
{    "thetasym",    977  }, 
{    "upsih",  978  }, 
{    "piv",         982  }, 


/* Punctuation */

{    "ensp",        8194 },   /* en space */
{    "emsp",        8195 },   /* em space */
{    "thinsp", 8201 },
{    "zwnj",        8204 },   /* zero width non-joiner */
{    "zwj",         8205 },   /* zero width joiner */
{    "lrm",         8206 },   /* left-to-right mark */
{    "rlm",         8207 },   /* right-to-left mark */
{    "ndash",  8211 },   /* en dash */
{    "mdash",  8212 },   /* em dash */
{    "lsquo",  8216 }, 
{    "rsquo",  8217 }, 
{    "sbquo",  8218 }, 
{    "ldquo",  8220 }, 
{    "rdquo",  8221 }, 
{    "bdquo",  8222 }, 
{    "dagger", 8224 }, 
{    "Dagger", 8225 }, 
{    "bull",        8226 }, 
{    "hellip", 8230 }, 
{    "permil", 8240 }, 
{    "prime",  8242 }, 
{    "Prime",  8243 }, 
{    "lsaquo", 8249 }, 
{    "rsaquo", 8250 }, 
{    "oline",  8254 }, 
{    "frasl",  8260 }, 
{    "euro",        8364 }, 


/* Letter type characters */ 

{    "weierp", 8472 }, 
{    "image",  8465 }, 
{    "real",        8476 }, 
{    "trade",  8482 }, 
{    "alefsym",     8501 }, 

/* Arrows */

{    "larr",        8592 }, 
{    "uarr",        8593 }, 
{    "rarr",        8594 }, 
{    "darr",        8595 }, 
{    "harr",        8596 }, 
{    "crarr",  8629 }, 
{    "lArr",        8656 }, 
{    "uArr",        8657 }, 
{    "rArr",        8658 }, 
{    "dArr",        8659 }, 
{    "hArr",        8660 }, 

/* Math characters */

{    "forall", 8704 }, 
{    "part",        8706 }, 
{    "exist",  8707 }, 
{    "empty",  8709 }, 
{    "nabla",  8711 }, 
{    "isin",        8712 }, 
{    "notin",  8713 }, 
{    "ni",          8715 }, 
{    "prod",        8719 }, 
{    "sum",         8721 }, 
{    "minus",  8722 }, 
{    "lowast", 8727 }, 
{    "radic",  8730 }, 
{    "prop",        8733 }, 
{    "infin",  8734 }, 
{    "ang",         8736 }, 
{    "and",         8743 }, 
{    "or",          8744 }, 
{    "cap",         8745 }, 
{    "cup",         8746 }, 
{    "int",         8747 }, 
{    "there4", 8756 }, 
{    "sim",         8764 }, 
{    "cong",        8773 }, 
{    "asymp",  8776 }, 
{    "ne",          8800 }, 
{    "equiv",  8801 }, 
{    "le",          8804 }, 
{    "ge",          8805 }, 
{    "sub",         8834 }, 
{    "sup",         8835 }, 
{    "nsub",        8836 }, 
{    "sube",        8838 }, 
{    "supe",        8839 }, 
{    "oplus",  8853 }, 
{    "otimes", 8855 }, 
{    "perp",        8869 }, 
{    "sdot",        8901 }, 

/* Misc tech characters */

{    "lceil",  8968 }, 
{    "rceil",  8969 }, 
{    "lfloor", 8970 }, 
{    "rfloor", 8971 }, 
{    "lang",        9001 }, 
{    "rang",        9002 }, 
{    "loz",         9674 }, 
{    "spades", 9824 }, 
{    "clubs",  9827 }, 
{    "hearts", 9829 }, 
{    "diams",  9830 },

/* END Marker */

{    "",       0    }};  


int UdmSgmlToUni(const char *sgml)
{
  const struct udm_sgml_chars *p;
  for(p= SGMLChars; p->unicode; p++)
  {
    const char *s, *t;
    for (s= sgml, t= p->sgml; *s == *t ; s++, t++);
    if (!*t)
      return p->unicode;
  }
  return 0;
}


/*
  Scan an entity. Return number of bytes scanned.
*/
int UdmSGMLScan(int *wc, const unsigned char *str, const unsigned char *end)
{
  const unsigned char *p, *end10= str + 10;
  if (end10 > end)
    end10= end;
  for (p= str + 2; p < end10; p++)
  {
    if (*p == ';')
    {
      if (str[1] == '#')
      {
        if (str[2] == 'x' || str[2] == 'X')
          *wc= strtol((const char*)str + 3, NULL, 16);
        else
          *wc= strtol((const char*)str + 2, NULL, 10);
      }
      else
      {
        *wc= UdmSgmlToUni((const char*) str + 1);
      }
      if (*wc)
        return p - str + 1;
    }
  }
  *wc= '&';
  return 1;
}


/** This function replaces SGML entities
    With their character equivalents     
*/

#define UDM_MAX_SGML_LEN 20

__C_LINK char * __UDMCALL UdmSGMLUnescape(char * str){
     char * s=str,*e,c;
     
     while(*s){
          if(*s=='&'){
               if(*(s+1)=='#'){
                    for(e=s+2;(e-s<UDM_MAX_SGML_LEN)&&(*e<='9')&&(*e>='0');e++);
                    if(*e==';'){
                         int v=atoi(s+2);
                         if(v>=0&&v<=255) {
                           *s=(char)v;
                         } else {
                           *s = ' ';
                         }
                         memmove(s+1, e+1, strlen(e+1)+1);
                    }
               }else{
                    for(e=s+1;(e-s<UDM_MAX_SGML_LEN)&&(((*e<='z')&&(*e>='a'))||((*e<='Z')&&(*e>='A')));e++);
                    if((*e==';')&&(c=(char)UdmSgmlToUni(s+1))){
                         *s=c;
                         memmove(s+1,e+1,strlen(e+1)+1);
                         
                    }
               }
          }
          s++;
     }
     return(str);
}

/** This function replaces SGML entities
    With their UNICODE   equivalents     
*/
void UdmSGMLUniUnescape(int * ustr) {
  int *s = ustr, *e, c;

  while (*s){
          if(*s=='&'){
                  char sgml[UDM_MAX_SGML_LEN+1];
               int i = 0;
               if(*(s+1)=='#'){
                    for(e = s + 2; (e - s < UDM_MAX_SGML_LEN) && (*e <= '9') && (*e >= '0'); e++);
                    if(*e==';'){
                         for(i = 2; s + i < e; i++)
                              sgml[i-2]=s[i];
                         sgml[i-2] = '\0';
                         *s = atoi(sgml);
                         memmove(s + 1, e + 1, sizeof(int) * (UdmUniLen(e + 1) + 1));
                    }
               }else{
                       for(e=s+1;(e-s<UDM_MAX_SGML_LEN)&&(((*e<='z')&&(*e>='a'))||((*e<='Z')&&(*e>='A')));e++){
                      sgml[i] = (char)*e;
                      i++;
                    };
                    if(/* (*e==';')&& */(c = UdmSgmlToUni(sgml)) ) {
                         *s=c;
                         memmove(s + 1, e + 1, sizeof(int) * (UdmUniLen(e + 1) + 1));
                         
                    }
               }
          }
          s++;
  }
}


size_t UdmHTMLEncode(char *dst, size_t dstlen, const char *src, size_t srclen)
{
  char *dst0= dst;
  
  for ( ; srclen ; srclen--, src++)
  {
    const char *ch;
    size_t chlen;
    switch (*src)
    {
      case '&':
        ch= "&amp;";
        chlen= 5;
        break;
      case '"':
        ch= "&quot;";
        chlen= 6;
        break;
      case '<':
        ch= "&lt;";
        chlen= 4;
        break;
      case '>':
        ch= "&gt;";
        chlen= 4;
        break;
      default:
        ch= src;
        chlen= 1;
    }
    if (chlen > dstlen)
      break;
    if (chlen == 1)
      *dst= *ch;
    else
      memcpy(dst, ch, chlen);
    dst+= chlen;
    dstlen-= chlen;
  }
  return dst - dst0;
}
