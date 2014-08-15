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

/* $Id: commandline.c 935 2006-02-01 03:22:04Z benoitg $ */
/** @file commandline.c
    @brief Command line argument handling
    @author Copyright (C) 2004 Philippe April <papril777@yahoo.com>
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>

#include "debug.h"
#include "safe.h"
#include "conf.h"

#include "../config.h"

/*
 * Holds an argv that could be passed to exec*() if we restart ourselves
 */
/*
 * A flag to denote whether we were restarted via a parent wifidog, or started normally
 * 0 means normally, otherwise it will be populated by the PID of the parent
 */

/** @internal
 * @brief Print usage
 *
 * Prints usage, called when wifidog is run with -h or with an unknown option
 */
/** Uses getopt() to parse the command line and set configuration values
 * also populates restartargv
 */
void parse_commandline(int argc, char **argv) {
    int c;

    s_config *config = config_get_config();

	//MAGIC 3: Our own -x, the pid, and NULL :

    while (-1 != (c = getopt(argc, argv, "c:hfd:sw:vx:i:"))) {


		switch(c) {



			case 'f':
				config->daemon = 0;
				break;

			case 'd':
				if (optarg) {
					config->debuglevel = atoi(optarg);
				}
				break;


			default:
				exit(1);
				break;

		}


	}
}

