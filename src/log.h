/*
    AOA Proxy - a general purpose Android Open Accessory Protocol host implementation
    Copyright (C) 2012 Tim Otto

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Created on: Oct 21, 2012
 *      Author: Tim
 */

#ifndef LOG_H_
#define LOG_H_

#include <syslog.h>
#include <stdio.h>

//#define	LOG_ERR	stderr
//#define	LOG_DEB stdout
//#define logDebug(x...) fprintf(LOG_DEB, x)
//#define logError(x...) fprintf(LOG_ERR, x)

#define logDebug(x...) syslog(LOG_DEBUG, x)
#define logError(x...) syslog(LOG_ERR, x)

#endif /* LOG_H_ */
