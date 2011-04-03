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
/* Based on this code:
--------------------------------------------------------------------
lookup8.c, by Bob Jenkins, January 4 1997, Public Domain.
hash(), hash2(), hash3, and mix() are externally useful functions.
Routines to test the hash are included if SELF_TEST is defined.
You can use this free for any purpose.  It has no warranty.
--------------------------------------------------------------------
*/

#include "udm_config.h"

#include <sys/types.h>

#include "udm_hash.h"

#define hashsize64(n) ((udmhash64_t)1 << (n))
#define hashmask64(n) (hashsize64(n) - 1)
#define hashsize32(n) ((udmhash32_t)1 << (n))
#define hashmask32(n) (hashsize32(n) - 1)

/*
--------------------------------------------------------------------
mix -- mix 3 64-bit values reversibly.
mix() takes 48 machine instructions, but only 24 cycles on a superscalar
  machine (like Intel's new MMX architecture).  It requires 4 64-bit
  registers for 4::2 parallelism.
All 1-bit deltas, all 2-bit deltas, all deltas composed of top bits of
  (a,b,c), and all deltas of bottom bits were tested.  All deltas were
  tested both on random keys and on keys that were nearly all zero.
  These deltas all cause every bit of c to change between 1/3 and 2/3
  of the time (well, only 113/400 to 287/400 of the time for some
  2-bit delta).  These deltas all cause at least 80 bits to change
  among (a,b,c) when the mix is run either forward or backward (yes it
  is reversible).
This implies that a hash using mix64 has no funnels.  There may be
  characteristics with 3-bit deltas or bigger, I didn't test for
  those.
--------------------------------------------------------------------
*/
#define mix64(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>43); \
  b -= c; b -= a; b ^= (a<<9); \
  c -= a; c -= b; c ^= (b>>8); \
  a -= b; a -= c; a ^= (c>>38); \
  b -= c; b -= a; b ^= (a<<23); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>35); \
  b -= c; b -= a; b ^= (a<<49); \
  c -= a; c -= b; c ^= (b>>11); \
  a -= b; a -= c; a ^= (c>>12); \
  b -= c; b -= a; b ^= (a<<18); \
  c -= a; c -= b; c ^= (b>>22); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 64-bit value
  k     : the key (the unaligned variable-length array of bytes)
  len   : the length of the key, counting by bytes
  level : can be any 8-byte value
Returns a 64-bit value.  Every bit of the key affects every bit of
the return value.  No funnels.  Every 1-bit and 2-bit delta achieves
avalanche.  About 41+5len instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 64 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, Jan 4 1997.  bob_jenkins@burtleburtle.net.  You may
use this code any way you wish, private, educational, or commercial,
as long as this whole comment accompanies it.

See http://burtleburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^^64
is acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

static udmhash64_t hash64( register const char *k, register size_t length, udmhash64_t level) 
/* register ub1 *k;        *//* the key */
/* register ub8  length;   *//* the length of the key */
/* register ub8  level;    *//* the previous hash, or an arbitrary value */
{
  register udmhash64_t a, b, c;
  register size_t len;

  /* Set up the internal state */
  len = length;
  a = b = level;                         /* the previous hash value */
#ifndef WIN32
  c = 0x9e3779b97f4a7c13LL; /* the golden ratio; an arbitrary value */
#else
  c = 0x9e3779b97f4a7c13; /* the golden ratio; an arbitrary value */
#endif

  /*---------------------------------------- handle most of the key */
  while (len >= 24)
  {
    a += (k[0]        +((udmhash64_t)k[ 1]<< 8)+((udmhash64_t)k[ 2]<<16)+((udmhash64_t)k[ 3]<<24)
     +((udmhash64_t)k[4 ]<<32)+((udmhash64_t)k[ 5]<<40)+((udmhash64_t)k[ 6]<<48)+((udmhash64_t)k[ 7]<<56));
    b += (k[8]        +((udmhash64_t)k[ 9]<< 8)+((udmhash64_t)k[10]<<16)+((udmhash64_t)k[11]<<24)
     +((udmhash64_t)k[12]<<32)+((udmhash64_t)k[13]<<40)+((udmhash64_t)k[14]<<48)+((udmhash64_t)k[15]<<56));
    c += (k[16]       +((udmhash64_t)k[17]<< 8)+((udmhash64_t)k[18]<<16)+((udmhash64_t)k[19]<<24)
     +((udmhash64_t)k[20]<<32)+((udmhash64_t)k[21]<<40)+((udmhash64_t)k[22]<<48)+((udmhash64_t)k[23]<<56));
    mix64(a,b,c);
    k += 24; len -= 24;
  }

  /*------------------------------------- handle the last 23 bytes */
  c += length;
  switch(len)              /* all the case statements fall through */
  {
  case 23: c+=((udmhash64_t)k[22]<<56);
  case 22: c+=((udmhash64_t)k[21]<<48);
  case 21: c+=((udmhash64_t)k[20]<<40);
  case 20: c+=((udmhash64_t)k[19]<<32);
  case 19: c+=((udmhash64_t)k[18]<<24);
  case 18: c+=((udmhash64_t)k[17]<<16);
  case 17: c+=((udmhash64_t)k[16]<<8);
    /* the first byte of c is reserved for the length */
  case 16: b+=((udmhash64_t)k[15]<<56);
  case 15: b+=((udmhash64_t)k[14]<<48);
  case 14: b+=((udmhash64_t)k[13]<<40);
  case 13: b+=((udmhash64_t)k[12]<<32);
  case 12: b+=((udmhash64_t)k[11]<<24);
  case 11: b+=((udmhash64_t)k[10]<<16);
  case 10: b+=((udmhash64_t)k[ 9]<<8);
  case  9: b+=((udmhash64_t)k[ 8]);
  case  8: a+=((udmhash64_t)k[ 7]<<56);
  case  7: a+=((udmhash64_t)k[ 6]<<48);
  case  6: a+=((udmhash64_t)k[ 5]<<40);
  case  5: a+=((udmhash64_t)k[ 4]<<32);
  case  4: a+=((udmhash64_t)k[ 3]<<24);
  case  3: a+=((udmhash64_t)k[ 2]<<16);
  case  2: a+=((udmhash64_t)k[ 1]<<8);
  case  1: a+=((udmhash64_t)k[ 0]);
    /* case 0: nothing left to add */
  }
  mix64(a,b,c);
  /*-------------------------------------------- report the result */
  return c;
}

udmhash64_t UdmHash64(const char * buf, size_t size) {
#ifndef WIN32
  return hash64(buf, size, 0xb7e151628aed2a6bLL);
#else
  return hash64(buf, size, 0xb7e151628aed2a6b);
#endif
}

/*********************************************************************************************/

/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bit set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a 
  structure that could supported 2x parallelism, like so:
      a -= b; 
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/* same, but slower, works on systems that might have 8 byte ub4's */
#define mix2(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<< 8); \
  c -= a; c -= b; c ^= ((b&0xffffffff)>>13); \
  a -= b; a -= c; a ^= ((c&0xffffffff)>>12); \
  b -= c; b -= a; b = (b ^ (a<<16)) & 0xffffffff; \
  c -= a; c -= b; c = (c ^ (b>> 5)) & 0xffffffff; \
  a -= b; a -= c; a = (a ^ (c>> 3)) & 0xffffffff; \
  b -= c; b -= a; b = (b ^ (a<<10)) & 0xffffffff; \
  c -= a; c -= b; c = (c ^ (b>>15)) & 0xffffffff; \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k     : the key (the unaligned variable-length array of bytes)
  len   : the length of the key, counting by bytes
  level : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 36+6len instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burlteburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

static udmhash32_t __UDMCALL hash32( register const char *k, size_t length, udmhash32_t initval)
/*register ub1 *k;        *//* the key */
/*register ub4  length;   *//* the length of the key */
/*register ub4  initval;  *//* the previous hash, or an arbitrary value */
{
   register udmhash32_t a, b, c;
   register size_t len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;           /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((udmhash32_t)k[1]<<8) +((udmhash32_t)k[2]<<16) +((udmhash32_t)k[3]<<24));
      b += (k[4] +((udmhash32_t)k[5]<<8) +((udmhash32_t)k[6]<<16) +((udmhash32_t)k[7]<<24));
      c += (k[8] +((udmhash32_t)k[9]<<8) +((udmhash32_t)k[10]<<16)+((udmhash32_t)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((udmhash32_t)k[10]<<24);
   case 10: c+=((udmhash32_t)k[9]<<16);
   case 9 : c+=((udmhash32_t)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((udmhash32_t)k[7]<<24);
   case 7 : b+=((udmhash32_t)k[6]<<16);
   case 6 : b+=((udmhash32_t)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((udmhash32_t)k[3]<<24);
   case 3 : a+=((udmhash32_t)k[2]<<16);
   case 2 : a+=((udmhash32_t)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return c;
}

udmhash32_t UdmHash32(const char * buf, size_t size) {
  return hash32(buf, size, 0x0);
}


/*********************************************************/

#include <string.h>

void
UdmHashInit(UDM_HASH *hash, void* base, size_t nmemb, size_t size,
            size_t(*key)(const void*),
            int(*join)(void *, void *))
{
  hash->base= base;
  hash->nmemb= nmemb;
  hash->size= size;
  hash->key= key;
  hash->join= join;
}


size_t
UdmHashSize(size_t n)
{
  return (n + n/10 + 10);
}


void
UdmHashPut(UDM_HASH *hash, void *ptr)
{
  size_t key= hash->key(ptr);
  size_t pos= key % hash->nmemb;
  size_t ncoll;

  for (ncoll= 0 ; ncoll < hash->nmemb ; pos= (pos + 1) % hash->nmemb, ncoll++)
  {
    void *addr= ((char*)hash->base) + pos * hash->size;
    size_t hkey= hash->key(addr);
    if (hkey == 0)
    {
      memcpy(addr, ptr, hash->size);
      return;
    }
    if (hkey == key)
    {
      hash->join(addr, ptr);
      return;
    }
  }
}
