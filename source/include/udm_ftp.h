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

#ifndef _UDM_FTP_H
#define _UDM_FTP_H

#define FTP_FREE	0
#define FTP_BUSY	1

extern int Udm_ftp_connect(UDM_ENV * Conf,UDM_CONN *connp, 
	const char *hostname, int port, char *user, char *passwd, int timeout);
extern int Udm_ftp_open_control_port(UDM_ENV * Conf,UDM_CONN *connp);

extern int Udm_ftp_get_reply(UDM_CONN *connp);
extern int Udm_ftp_read_line(UDM_CONN *connp);
extern int Udm_ftp_open_data_port(UDM_CONN *c, UDM_CONN *d);
extern int Udm_ftp_send_cmd(UDM_CONN *connp, const char *cmd);
extern int Udm_ftp_send_data_cmd(UDM_CONN *c, UDM_CONN *d, char *cmd, size_t max_doc_size);
extern int Udm_ftp_login(UDM_CONN *connp, char *user, char *passwd);
extern int Udm_ftp_cwd(UDM_CONN *c, const char *path);
extern int Udm_ftp_list(UDM_DOCUMENT *Doc);
extern int Udm_ftp_get(UDM_CONN *c, UDM_CONN *d, char *path,size_t max_doc_size);
extern int Udm_ftp_mdtm(UDM_CONN *c, char *path);
extern int Udm_ftp_close(UDM_CONN *connp);
extern int Udm_ftp_abort(UDM_CONN *connp);
extern int Udm_ftp_set_binary(UDM_CONN *c);
extern size_t Udm_ftp_size(UDM_CONN *c, char *path);
extern int Udm_ftp_rest(UDM_CONN *c, size_t rest);

#endif
