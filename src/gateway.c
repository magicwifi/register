/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free:Software Foundation; either version 2 of   *
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
/** @internal
  @file gateway.c
  @brief Main loop
  @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
  @author Copyright (C) 2004 Alexandre Carmel-Veilleux <acv@miniguru.ca>
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

/* for strerror() */
#include <string.h>

/* for wait() */
#include <sys/wait.h>

/* for unix socket communication*/
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "httpd.h"
#include "safe.h"
#include "debug.h"
#include "conf.h"
#include "gateway.h"
#include "commandline.h"
#include "http.h"
#include "fetchconf.h"
#include "httpd_thread.h"
#include "util.h"

httpd * webserver = NULL;

time_t started_time = 0;


void
sigchld_handler(int s)
{
	int	status;
	pid_t rc;
	
	debug(LOG_DEBUG, "Handler for SIGCHLD called. Trying to reap a child");

	rc = waitpid(-1, &status, WNOHANG);

	debug(LOG_DEBUG, "Handler for SIGCHLD reaped child PID %d", rc);
}

/** Exits cleanly after cleaning up the firewall.  
 *  Use this function anytime you need to exit after firewall initialization */
void
termination_handler(int s)
{
	static	pthread_mutex_t	sigterm_mutex = PTHREAD_MUTEX_INITIALIZER;

	debug(LOG_INFO, "Handler for termination caught signal %d", s);

	/* Makes sure we only call fw_destroy() once. */
	if (pthread_mutex_trylock(&sigterm_mutex)) {
		debug(LOG_INFO, "Another thread already began global termination handler. I'm exiting");
		pthread_exit(NULL);
	}
	else {
		debug(LOG_INFO, "Cleaning up and exiting");
	}

	debug(LOG_INFO, "Flushing firewall rules...");

	debug(LOG_NOTICE, "Exiting...");
	exit(s == 0 ? 1 : 0);
}

/** @internal 
 * Registers all the signal handlers
 */
static void
init_signals(void)
{
	struct sigaction sa;

	debug(LOG_DEBUG, "Initializing signal handlers");
	
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		debug(LOG_ERR, "sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* Trap SIGPIPE */
	/* This is done so that when libhttpd does a socket operation on
	 * a disconnected socket (i.e.: Broken Pipes) we catch the signal
	 * and do nothing. The alternative is to exit. SIGPIPE are harmless
	 * if not desirable.
	 */
	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1) {
		debug(LOG_ERR, "sigaction(): %s", strerror(errno));
		exit(1);
	}

	sa.sa_handler = termination_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	/* Trap SIGTERM */
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		debug(LOG_ERR, "sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* Trap SIGQUIT */
	if (sigaction(SIGQUIT, &sa, NULL) == -1) {
		debug(LOG_ERR, "sigaction(): %s", strerror(errno));
		exit(1);
	}

	/* Trap SIGINT */
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		debug(LOG_ERR, "sigaction(): %s", strerror(errno));
		exit(1);
	}
}

/**@internal
 * Main execution loop 
 */
static void
main_loop(void)
{
	int result;
	pthread_t	tid;
	request *r;
	void **params;
	s_config *config = config_get_config();

if (!config->gw_address) {
		debug(LOG_DEBUG, "Finding IP address of %s", config->gw_interface);
		if ((config->gw_address = get_iface_ip(config->gw_interface)) == NULL) {
			debug(LOG_ERR, "Could not get IP address information of %s, exiting...", config->gw_interface);
			exit(1);
		}
		debug(LOG_DEBUG, "%s = %s", config->gw_interface, config->gw_address);
	}

	/* If we don't have the Gateway ID, construct it from the internal MAC address.
	 * "Can't fail" so exit() if the impossible happens. */
	if (!config->gw_id) {
    	debug(LOG_DEBUG, "Finding MAC address of %s", config->gw_interface);
    	if ((config->gw_id = get_iface_mac(config->gw_interface)) == NULL) {
			debug(LOG_ERR, "Could not get MAC address information of %s, exiting...", config->gw_interface);
			exit(1);
		}
		debug(LOG_DEBUG, "%s = %s", config->gw_interface, config->gw_id);
	}

	if ((webserver = httpdCreate(config->gw_address, config->gw_port)) == NULL) {
		debug(LOG_ERR, "Could not create web server: %s", strerror(errno));
		exit(1);
	}

	debug(LOG_DEBUG, "Assigning callbacks to web server");
	httpdAddCContent(webserver, "/smartwifi", "", 0, NULL, http_callback_wifidog);
	httpdAddCContent(webserver, "/smartwifi", "active", 0, NULL, http_callback_write);


	
	
	while(1) {
		r = httpdGetConnection(webserver, NULL);

		if (webserver->lastError == -1) {
			/* Interrupted system call */
			continue; /* restart loop */
		}
		else if (webserver->lastError < -1) {

			debug(LOG_ERR, "FATAL: httpdGetConnection returned unexpected value %d, exiting.", webserver->lastError);
			termination_handler(0);
		}
		else if (r != NULL) {
			/*
			 * We got a connection
			 *
			 * We should create another thread
			 */
			debug(LOG_INFO, "Received connection from %s, spawning worker thread", r->clientAddr);
			/* The void**'s are a simulation of the normal C
			 * function calling sequence. */
			params = safe_malloc(2 * sizeof(void *));
			*params = webserver;
			*(params + 1) = r;

			result = pthread_create(&tid, NULL, (void *)thread_httpd, (void *)params);
			if (result != 0) {
				debug(LOG_ERR, "FATAL: Failed to create a new thread (httpd) - exiting");
				termination_handler(0);
			}
			pthread_detach(tid);
		}
		else {
			/* webserver->lastError should be 2 */
			/* XXX We failed an ACL.... No handling because
			 * we don't set any... */
		}
	}

	/* never reached */
}

/** Reads the configuration file and then starts the main loop */
int main(int argc, char **argv) {

	
	s_config *config = config_get_config();
	config_init();

	parse_commandline(argc, argv);

	/* Initialize the config */
	//config_read(config->configfile);
//	config_validate();
	init_signals();
	//append_x_restartargv();
if (config->daemon) {

		debug(LOG_INFO, "Forking into background");

		switch(safe_fork()) {
			case 0: /* child */
				setsid();
				main_loop();
				break;

			default: /* parent */
				exit(0);
				break;
		}
	}
	else {
		main_loop();
	}


	return(0); /* never reached */
}
