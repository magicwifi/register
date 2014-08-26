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

	httpVar * dev_id = httpdGetVariableByName(r, "dev_id");
	if(dev_id ){
		debug(LOG_DEBUG, "get device id");
		int result = register_conf(dev_id->value);
		if (result ==1 ) {	
			send_http_page(r, "云WiFi", "<p>注册成功</p>");
			exit(1);
		}else{
			debug(LOG_DEBUG, "unable register");
			send_http_page(r, "WiFiDog error", "Invalid token");
		} 
	}
	else{
		debug(LOG_DEBUG, "fail get device id");
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

