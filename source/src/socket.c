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
#include <errno.h>
#include <time.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "udm_common.h"
#include "udm_socket.h"
#include "udm_utils.h"
#include "udm_proto.h"
#include "udm_log.h"
#include "udm_xmalloc.h"
#include "udm_proto.h"


void socket_close( UDM_CONN *connp ){
	if (!connp)
		return;

	if (connp->conn_fd > 0){
		closesocket(connp->conn_fd);
		connp->conn_fd = 0;
	}    
	return;
}


int socket_open( UDM_CONN *connp ) {
	int op, len;
	
	op=1;
	len = sizeof(struct sockaddr_in);
	
	/* Create cocket */
	connp->conn_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connp->conn_fd == -1){
		connp->err = UDM_NET_ERROR;
		return -1;
	}

	if (setsockopt(connp->conn_fd, SOL_SOCKET, SO_REUSEADDR,
					(char *)&op,sizeof(op))==-1) {
		connp->err = UDM_NET_ERROR;
		return -1;
	}

	connp->sin.sin_family = AF_INET;
	return 0;
}

int socket_connect( UDM_CONN *connp){
        socklen_t len;

	if (connect(connp->conn_fd, (struct sockaddr *)&connp->sin,
	    				    sizeof(connp->sin)) == -1) {
		connp->err = UDM_NET_CANT_CONNECT;
		return -1;
	}
	
	len = sizeof(struct sockaddr_in);
	if (getsockname(connp->conn_fd, (struct sockaddr *)&connp->sin, &len) == -1){
		connp->err = UDM_NET_ERROR;
                return -1;
	}
	connp->connected = UDM_NET_CONNECTED;
	return 0;
}


int socket_select( UDM_CONN *connp, int timeout, int mode) {
        fd_set fds;
        struct timeval tv;
	int rfd;

        FD_ZERO(&fds);

        tv.tv_sec = timeout;
        tv.tv_usec = 0;
    do{
        FD_ZERO(&fds);
	FD_SET(connp->conn_fd, &fds);
	if (mode == 'r')
    		rfd = select(connp->conn_fd+1, &fds, 0, 0, &tv);
	else
    		rfd = select(connp->conn_fd+1, 0, &fds, 0, &tv);
	
	if (rfd == 0 ) {
		/* timeout */
		if (timeout)
			connp->err = UDM_NET_TIMEOUT;
		return -1;
	
	}
    }while( rfd == -1 && errno == EINTR);
	return 0;	
}

int socket_read( UDM_CONN *connp, size_t maxsize){
        int num_read;
	size_t num_read_total;
	time_t t;

	num_read_total = 0;

	if (connp->buf)
		UDM_FREE(connp->buf);

	connp->buf_len_total = 0;
	connp->buf_len = 0;
	connp->err = 0;
	
	t = time(NULL);
	do {
		if (socket_select(connp, connp->timeout, 'r') == -1){
			return -1;
		}

		if (connp->buf_len_total <= num_read_total+UDM_NET_BUF_SIZE){
			connp->buf_len_total += UDM_NET_BUF_SIZE;
			connp->buf = UdmXrealloc(connp->buf, (size_t)(connp->buf_len_total+1));
		}
		
		num_read = recv(connp->conn_fd, connp->buf+num_read_total, 
				(UDM_NET_BUF_SIZE <= maxsize - num_read_total) ? UDM_NET_BUF_SIZE : (maxsize - num_read_total),
				0);
		num_read_total += num_read;		

		if (num_read < 0){
			connp->err = UDM_NET_ERROR;
			return -1;
		}else if (num_read == 0) {
			if (time(NULL) - t > connp->timeout) break;
		} else t = time(NULL);
		if (num_read_total >=maxsize ){
			connp->err = UDM_NET_FILE_TL;
			break;
		}
	}while (num_read!=0);
	connp->buf_len = num_read_total;
	return num_read_total;	
}


int socket_read_line( UDM_CONN *connp ){
	size_t num_read_total, buf_size;
	
        buf_size = UDM_NET_BUF_SIZE;
	num_read_total = 0;

	if (connp->buf)
		UDM_FREE(connp->buf);
	
	connp->buf_len_total = 0;
	connp->buf_len = 0;

	while(1){
		if (connp->buf_len_total <= num_read_total+UDM_NET_BUF_SIZE) {
			connp->buf_len_total += UDM_NET_BUF_SIZE;
			connp->buf = UdmXrealloc(connp->buf, (size_t)(connp->buf_len_total +1)); 
		}
		if(!recv(connp->conn_fd,&(connp->buf[num_read_total]),1,0))
			return -1;
		
		if (connp->buf[num_read_total] == '\n')
			break;
		else if (connp->buf[num_read_total] == 0)
			break;
		num_read_total++;
        }
	connp->buf_len = strlen(connp->buf);
	return num_read_total;	
}

int socket_write(UDM_CONN *connp, const char *buf){
	
	if (socket_select(connp, UDM_NET_READ_TIMEOUT, 'w') == -1)
		return -1;

	if (UdmSend(connp->conn_fd, buf, strlen(buf), 0) == -1){
		connp->err = UDM_NET_ERROR;
		return -1;
	}
	return 0;
}

int socket_accept(UDM_CONN *connp){
	struct sockaddr sa;
	int sfd;
	socklen_t len;
        if (socket_select(connp, UDM_NET_ACC_TIMEOUT, 'r') == -1)
		return -1;

	len = sizeof(struct sockaddr);
						
	sfd = accept(connp->conn_fd, &sa, &len);
	socket_close(connp);

	if (sfd == -1){
		connp->err = UDM_NET_ERROR;
		return -1;
	}
	connp->conn_fd = sfd;
	
	memcpy(&connp->sin, &sa, sizeof(connp->sin));
	return 0;
}

int socket_listen(UDM_CONN *connp){

        connp->sin.sin_port = 0; 

        if (bind(connp->conn_fd, (struct sockaddr *)&connp->sin, 
				    sizeof(struct sockaddr_in)) == -1){
		connp->err = UDM_NET_ERROR;
	        return -1;
	}
	if (socket_getname(connp, &connp->sin) == -1)
	    return -1;	
								
        if (listen(connp->conn_fd, 1) == -1){
		connp->err = UDM_NET_ERROR;
	        return -1;
	}
        return 0;
}

int socket_getname(UDM_CONN *connp, struct sockaddr_in *sa_in){
        socklen_t len;
	
	len = sizeof(struct sockaddr_in);
        if (getsockname(connp->conn_fd, (struct sockaddr *)sa_in, &len) == -1){
		connp->err = UDM_NET_ERROR;
                return -1;
	}
	return 0;
}

void socket_buf_clear(UDM_CONN *connp){
	char buf[1024];
	int len;
	do {
    		if (socket_select(connp, 0, 'r')==-1)
			return;
		len = recv(connp->conn_fd, buf, 1024,0);
	}while(len > 0);
}

