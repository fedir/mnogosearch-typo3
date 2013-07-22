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

#ifndef _UDM_SOCKET_H
#define _UDM_SOCKET_H

#define	UDM_NET_BUF_SIZE		10240
/* Net status flags */
#define	UDM_NET_NOTCONNECTED	0
#define	UDM_NET_CONNECTED		1
#define	UDM_NET_CONNECTED		1
/* Net timeouts */
#define	UDM_NET_ACC_TIMEOUT		20
#define	UDM_NET_READ_TIMEOUT		20


void socket_close(UDM_CONN *connp );
int socket_open(UDM_CONN *connp );
int socket_connect(UDM_CONN *connp );
int socket_select(UDM_CONN *connp, int timeout, int mode );
int socket_read(UDM_CONN *connp, size_t maxsize);
int socket_read_line(UDM_CONN *connp);
int socket_write(UDM_CONN *connp, const char *buf);
int socket_listen(UDM_CONN *connp );
int socket_accept(UDM_CONN *connp );
int socket_getname(UDM_CONN *connp, struct sockaddr_in *sa_in);
void socket_buf_clear(UDM_CONN *connp);

#endif
