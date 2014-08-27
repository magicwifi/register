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




int register_conf(const char *dev_id){

    struct sockaddr_in servaddr;
    int sock;
    int tmpres;
    int result =-1;
    char            *str = NULL;

    char			request[MAX_BUF];
    unsigned int	        totalbytes;
    int			nfds, done;
    fd_set			readfds;
    unsigned int		numbytes;
    cJSON *json;
    char *safe_dev_id = httpdUrlEncode(dev_id);
	FILE * fh;

    sock = connect_auth_server();
    if (sock == -1) {
	debug(LOG_DEBUG, "unable connect");
	    return -1;
    }

	debug(LOG_DEBUG, "begin connect");
    char *hostname = safe_malloc(100);	
if ((fh = fopen("/proc/sys/kernel/hostname", "r"))) {
		if(fscanf(fh, "%s", hostname) != 1){
			debug(LOG_CRIT, "Failed to read loadhostname");
				return -1;
			}
		fclose(fh);
	}


    snprintf(request, sizeof(request) - 1,
		    "GET %s?dev_id=%s&stage=%s&mac=%s HTTP/1.1\r\n"
		    "User-Agent: WiFiDog %s\r\n"
		    "Host: %s\r\n"
		    "\r\n",
		    "http://test-auth1.51iwifi.com:8002/api10/register",
			safe_dev_id,
		     "active",
		config_get_config()->gw_id,
		    "1.1",
		    "test-auth1.51wifi.com");

	debug(LOG_DEBUG, "connect %s \n" ,request);
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
    request[totalbytes] = '\0';

    str = strstr(request, "{");
    if (str != 0) {
	debug(LOG_DEBUG, "connect back %s" ,str);
    	json=cJSON_Parse(str);
if (!json) {
	debug(LOG_DEBUG, "json error %s \n" ,cJSON_GetErrorPtr());
	}
	else{
		if (cJSON_GetObjectItem(json,"result")  && 
			strcmp(cJSON_GetObjectItem(json,"result")->valuestring,"OK")==0){

			char    content[MAX_BUF] = {0};
			char    tmp[256] = {0};
			cJSON *service = cJSON_GetObjectItem(json,"services");

			sprintf(tmp, "DevID %s\n",cJSON_GetObjectItem(service,"device_id")->valuestring);
			strcat(content,tmp);
			sprintf(tmp, "GatewayID %s\n", hostname);
			strcat(content, tmp);
			strcat(content, "GatewayInterface br-lan\n\n");

			cJSON *servers = cJSON_GetObjectItem(service,"servers");
			cJSON *auths = cJSON_GetObjectItem(servers,"auths")->child;
			for(;auths!=NULL;auths = auths->next){	
				strcat(content, "AuthServer {\n");
				sprintf(tmp, "Hostname %s\n", cJSON_GetObjectItem(auths,"hostname")->valuestring);
				strcat(content,tmp);
				sprintf(tmp, "Path %s\n", cJSON_GetObjectItem(auths,"path")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "HttpPort %s\n", cJSON_GetObjectItem(auths,"http_port")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "SSLPort %s\n", cJSON_GetObjectItem(auths,"ssl_port")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "SSLAvailable %s\n", cJSON_GetObjectItem(auths,"ssl_available")->valuestring);
				strcat(content, tmp);
				strcat(content, "}\n\n");
			}
			cJSON *platforms = cJSON_GetObjectItem(servers,"platforms")->child;
			for(;platforms!=NULL;platforms = platforms->next){	
				strcat(content, "PlatformServer {\n");
				sprintf(tmp, "Hostname %s\n", cJSON_GetObjectItem(platforms,"hostname")->valuestring);
				strcat(content,tmp);
				sprintf(tmp, "Path %s\n", cJSON_GetObjectItem(platforms,"path")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "HttpPort %s\n", cJSON_GetObjectItem(platforms,"http_port")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "SSLPort %s\n", cJSON_GetObjectItem(platforms,"ssl_port")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "SSLAvailable %s\n", cJSON_GetObjectItem(platforms,"ssl_available")->valuestring);
				strcat(content, tmp);
				strcat(content, "}\n\n");
			}

			cJSON *portals = cJSON_GetObjectItem(servers,"portals")->child;
			for(;portals!=NULL;portals = portals->next){	
				strcat(content, "PortalServer {\n");
				sprintf(tmp, "Hostname %s\n", cJSON_GetObjectItem(portals,"hostname")->valuestring);
				strcat(content,tmp);
				sprintf(tmp, "Path %s\n", cJSON_GetObjectItem(portals,"path")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "HttpPort %s\n", cJSON_GetObjectItem(portals,"http_port")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "SSLPort %s\n", cJSON_GetObjectItem(portals,"ssl_port")->valuestring);
				strcat(content, tmp);
				sprintf(tmp, "SSLAvailable %s\n", cJSON_GetObjectItem(portals,"ssl_available")->valuestring);
				strcat(content, tmp);
				strcat(content, "}\n\n");
			}

			strcat(content, "FirewallRuleSet global {\n");
			strcat(content, "}\n\n");
			strcat(content, "FirewallRuleSet validating-users {\n"
					"    FirewallRule allow to 0.0.0.0/0\n"
					"}\n\n");

			strcat(content, "FirewallRuleSet known-users {\n"
					"     FirewallRule allow to 0.0.0.0/0\n"
					"}\n\n");

			strcat(content, "FirewallRuleSet unknown-users {\n"
					"    FirewallRule allow udp port 53\n"
					"    FirewallRule allow tcp port 53\n"
					"    FirewallRule allow udp port 67\n"
					"    FirewallRule allow tcp port 67\n"
					"}\n\n");

			strcat(content, "FirewallRuleSet locked-users {\n"
					"     FirewallRule block to 0.0.0.0/0\n"
					"}\n\n");

			FILE *fp;
			fp = fopen( "/etc/wifidog.conf" , "w" );
			fwrite(content , 1 , sizeof(content) , fp );

			fclose(fp);
			result =1;
		}

		//cJSON_Delete(json);
	}
    }

    free(safe_dev_id);
    free(hostname);

    close(sock);
    return result;
}


