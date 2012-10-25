/*
 * log.h
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
