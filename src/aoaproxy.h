/*
 * aoaproxy.h
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
