/*
 * a2spipe.h
 *
 *  Created on: Oct 21, 2012
 *      Author: Tim
 */

#ifndef A2SPIPE_H_
#define A2SPIPE_H_

#include "aoaproxy.h"
#include "accessory.h"

void tickleUsbXferThread(usbXferThread *t);
void* a2s_usbRxThread( void *d );
void* a2s_socketRxThread( void *d );

int fnusb_stop_iso(struct listentry *device, libusb_context *ctx);
int fnusb_start_iso(struct listentry *device, fnusb_iso_cb cb, int ep, int xfers, int pkts, int len);

#endif /* A2SPIPE_H_ */
