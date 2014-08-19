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

/* $Id$ */
/** @file http.c
  @brief HTTP IO functions
  @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
  @author Copyright (C) 2007 Benoit Grégoire
  @author Copyright (C) 2007 David Bird <david@coova.com>

 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "httpd.h"

#include "safe.h"
#include "debug.h"
#include "http.h"
#include "common.h"
#include "httpd.h"
#include "conf.h"
#include "cJSON.h"
#include "fetchconf.h"

#include "util.h"

#include "../config.h"


void 
http_callback_wifidog(httpd *webserver, request *r)
{
	send_http_page(r, "云WiFi", "<p>欢迎使用北研院智慧企业研发的云WiFi产品，有问题请及时反馈</p>");
}

//用于生产wifidog.conf,请替换成自己的写文件操作 
void 
http_callback_write(httpd *webserver, request *r)
{

	t_auth_conf	*authserver;
    	cJSON *json ;
	httpVar * dev_id = httpdGetVariableByName(r, "dev_id");
	if(dev_id ){
		authserver =(t_auth_conf *)malloc(sizeof(t_auth_conf));
		int result = register_conf(authserver,json,dev_id->value);
	
		if (result ==1 ) {	
			char    content[4096] = {0};
			char    tmp[256] = {0};
			strcat(content, "GatewayID pubinfo\n\n");
			strcat(content, "GatewayInterface br-lan\n\n");
			sprintf(tmp, "DevID %s\n", dev_id);
			strcat(content, tmp);

			strcat(content, "AuthServer {\n");
			sprintf(tmp, "Hostname %s\n", "auth.51iwifi.com");
			strcat(content, tmp);
			if(authserver->authserv_use_ssl){
				strcat(content, "SSLAvailable Yes  \n");
			}else{
				strcat(content, "SSLAvailable No \n");
			}
			sprintf(tmp, "Path %s\n", authserver->authserv_path);
			strcat(content, tmp);
			strcat(content, "}\n\n");

			strcat(content, "PlatformServer {\n");
			sprintf(tmp, "Hostname %s\n", "www.51iwifi.com");
			strcat(content, tmp);
			sprintf(tmp, "Path %s\n", "/");
			strcat(content, tmp);
			sprintf(tmp, "HttpPort %d\n", 80);
			strcat(content, tmp);
			strcat(content, "SSLAvailable No \n");
			sprintf(tmp, "SSLPort %d\n", 443);
			strcat(content, tmp);
			strcat(content, "}\n\n");

			strcat(content, "PortalServer {\n");
			sprintf(tmp, "Hostname %s\n", "portal.51iwifi.com");
			strcat(content, tmp);
			sprintf(tmp, "Path %s\n", "/site/");
			strcat(content, tmp);
			sprintf(tmp, "HttpPort %d\n", 80);
			strcat(content, tmp);
			strcat(content, "SSLAvailable No \n");
			sprintf(tmp, "SSLPort %d\n", 443);
			strcat(content, tmp);
			strcat(content, "}\n\n");

			strcat(content, "FirewallRuleSet global {\n"
					"     FirewallRule allow to 117.34.78.204\n"
					"     FirewallRule allow to 124.127.116.177\n"
					"     FirewallRule allow to 122.229.30.31\n"
					"}\n\n");
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

			free(authserver);
			send_http_page(r, "云WiFi", "<p>注册成功</p>");
			exit(1);
		}else{
		free(authserver);
		send_http_page(r, "WiFiDog error", "Invalid token");
		} 
	}
	else{
		send_http_page(r, "WiFiDog error", "Invalid token");
	}
}


void send_http_page(request *r, const char *title, const char* message)
{
    s_config	*config = config_get_config();
    char *buffer;
    struct stat stat_info;
    int fd;
    ssize_t written;

    fd=open(config->htmlmsgfile, O_RDONLY);
    if (fd==-1) {
        debug(LOG_CRIT, "Failed to open HTML message file %s: %s", config->htmlmsgfile, strerror(errno));
        return;
    }

    if (fstat(fd, &stat_info)==-1) {
        debug(LOG_CRIT, "Failed to stat HTML message file: %s", strerror(errno));
        close(fd);
        return;
    }

    buffer=(char*)safe_malloc(stat_info.st_size+1);
    written=read(fd, buffer, stat_info.st_size);
    if (written==-1) {
        debug(LOG_CRIT, "Failed to read HTML message file: %s", strerror(errno));
        free(buffer);
        close(fd);
        return;
    }
    close(fd);

    buffer[written]=0;
    httpdAddVariable(r, "title", title);
    httpdAddVariable(r, "message", message);
    httpdAddVariable(r, "nodeID", config->gw_id);
    httpdOutput(r, buffer);
    free(buffer);
}

