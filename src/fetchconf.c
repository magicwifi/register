/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
 \********************************************************************/

/* $Id: conf.c 1373 2008-09-30 09:27:40Z wichert $ */
/** @file conf.c
  @brief Config file parsing
  @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
  @author Copyright (C) 2007 Benoit Grégoire, Technologies Coeus inc.
 */

#define _GNU_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

#include "../config.h"
#include "safe.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "util.h"
#include "httpd.h"
#include "fetchconf.h"
#include "cJSON.h"




int register_conf(t_auth_conf *authserver,cJSON *auth_json,char *dev_id){

    struct sockaddr_in servaddr;
    int sock;
    int tmpres;
    int result =-1;
    char            *str = NULL;
    char            *str2 = NULL;
    char            *host = NULL;
    char            *path = NULL;


    char			request[MAX_BUF];
    unsigned int	        totalbytes;
    int			nfds, done;
    fd_set			readfds;
    unsigned int		numbytes;
    cJSON *json = auth_json;
	t_auth_conf	*new=authserver;
    char *safe_dev_id = httpdUrlEncode(dev_id);
	

    sock = connect_auth_server();
    if (sock == -1) {
	    return -1;
    }



    snprintf(request, sizeof(request) - 1,
		    "GET %s?gw_id=%s&dev_id=%s HTTP/1.1\r\n"
		    "User-Agent: WiFiDog %s\r\n"
		    "Host: %s\r\n"
		    "\r\n",
		    "http://124.127.116.177/authserver.json",
		    "A8574EFCCBE0",
		     safe_dev_id,
		    "1.1",
		    "124.127.116.177");

    printf(" %s \n", request);    
    send(sock, request, strlen(request), 0);

	numbytes = totalbytes = 0;
	done = 0;
	do {
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		nfds = sock + 1;

		nfds = select(nfds, &readfds, NULL, NULL, NULL);

		if (nfds > 0) {
			numbytes = read(sock, request + totalbytes, MAX_BUF - (totalbytes + 1));
			if (numbytes < 0) {
				close(sock);
				return;
			}
			else if (numbytes == 0) {
				done = 1;
			}
			else {
				totalbytes += numbytes;
				done = 1;
			}
		}
		else if (nfds == 0) {
			close(sock);
			return;
		}
		else if (nfds < 0) {
			close(sock);
			return;
		}
	} while (!done);


    //str2 = strstr(request, "}");
    //int num = str2-request+2;
    request[totalbytes-3] = '\0';

    str = strstr(request, "{");
    if (str != 0) {
	printf("%s \n",str);
    	json=cJSON_Parse(str);
if (!json) {printf("Error before: [%s]\n",cJSON_GetErrorPtr());
	}
	else{
		if (cJSON_GetObjectItem(json,"result")  && 
			strcmp(cJSON_GetObjectItem(json,"result")->valuestring,"OK")==0){
		cJSON * format = cJSON_GetObjectItem(json,"authservers")->child;
		host = cJSON_GetObjectItem(format,"hostname")->valuestring;
		path = cJSON_GetObjectItem(format,"path")->valuestring;
		int ssl_port = cJSON_GetObjectItem(format,"sslport")->valueint; 
		int http_port = cJSON_GetObjectItem(format,"httpport")->valueint; 
		int ssl_available = cJSON_GetObjectItem(format,"usessl")->valueint; 
    		

		new->authserv_hostname = host;
		new->authserv_use_ssl = ssl_available;
		new->authserv_path = path;
		new->authserv_http_port = http_port;
		new->authserv_ssl_port = ssl_port;

		printf(" %s \n", new->authserv_hostname);    
			result =1;
		}

		//cJSON_Delete(json);
	}
    }

    free(safe_dev_id);

    close(sock);
    return result;
}


