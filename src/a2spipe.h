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
