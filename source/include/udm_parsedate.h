#ifndef _UDM_PARSE_DATE_H
#define _UDM_PARSE_DATE_H

/* GPL
 parse_date
 author: Heiko Stoermer, innominate AG, stoermer@innominate.de
 20000316

 UdmParseDate returns allocated pointer to string containing 
 the SQL-conformant datetime type.
 processes nntp datestrings of the following types:
 
 1: 17 Jun 1999 11:05:42 GMT
 2: Wed, 08 Mar 2000 13:52:43 +0100
 
*/

extern char * UdmDateParse(const char * datestring);

#endif
