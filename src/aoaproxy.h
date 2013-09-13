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

#ifndef AOAPROXY_H_
#define AOAPROXY_H_

#include <libusb-1.0/libusb.h>
#include "accessory.h"

typedef struct t_excludeList {
	int vid;
	int pid;
	struct t_excludeList *next;
} excludeList_Type;

typedef struct t_usbXferThread {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	struct libusb_transfer *xfr;
	int usbActive;
//	int dead;
	int stop;
	int stopped;
	int tickle;
} usbXferThread;

typedef struct t_audioXfer {
	int stop;
} audioXfer;

typedef struct listentry {
	libusb_device *usbDevice;
	int sockfd;
	int socketDead;
	int usbDead;

	accessory_droid droid;
//	pid_t pipePid;
//	int do_exit;

	usbXferThread usbRxThread;
	usbXferThread socketRxThread;
	audioXfer audio;

	struct listentry *prev;
	struct listentry *next;
} t_listentry;



#endif /* AOAPROXY_H_ */
