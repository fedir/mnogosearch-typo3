/* Copyright (C) 2000-2013 Lavtech.com corp. All rights reserved.

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


#ifndef UDM_DIRENT_H
#define UDM_DIRENT_H

#ifdef WIN32
/*
 * Structures and types used to implement opendir/readdir/closedir
 * on Windows 95/NT.
 */

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

struct dirent {
        long d_ino;                         /* inode (always 1 in WIN32) */
        off_t d_off;                        /* offset to this dirent */
        unsigned short d_reclen;            /* length of d_name */
        char d_name[_MAX_FNAME + 1];        /* filename (null terminated) */
};

/* typedef DIR - not the same as Unix */
typedef struct {
        long handle;                        /* _findfirst/_findnext handle */
        short offset;                       /* offset into directory */
        short finished;                     /* 1 if there are not more files */
        struct _finddata_t fileinfo;        /* from _findfirst/_findnext */
        char *dir;                          /* the dir we are reading */
        struct dirent dent;                 /* the dirent to return */
} DIR;

/* Function prototypes */
extern DIR *opendir(const char *);
extern struct dirent *readdir(DIR *);
extern int closedir(DIR *);

extern void rewinddir(DIR *);
#else
#include <dirent.h>
#endif /* WIN32 */

#endif /* UDM_DIRENT_H */
