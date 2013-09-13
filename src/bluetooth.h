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
 *  Created on: Oct 22, 2012
 *      Author: Tim
 */

#ifndef BLUETOOTH_H_
#define BLUETOOTH_H_

// edfe0000-005b-0000-ebb1-0000e5ab0000
#define CARBOT_BLUETOOTH_SDP_UUID \
		{ \
			0xed, 0xfe, 0x00, 0x00,	\
			0x00, 0x5b, 0x00, 0x00,	\
			0xeb, 0xb1, 0x00, 0x00,	\
			0xe5, 0xab, 0x00, 0x00,	\
		}

#define BT_BUFFER_SIZE			16384
#define RFCOMM_DEFAULT_CHANNEL 10

#ifdef BLUETOOTH_NULL
typedef struct {
	int token;
} bluetoothtoken_t;
#else

#include <bluetooth/bluetooth.h>
#include <pthread.h>

typedef struct {
	int token;
	int s;
	pthread_t thread;
//	pthread_mutex_t mutex;
//	pthread_cond_t condition;
} bluetoothtoken_t;
#endif

bluetoothtoken_t* initBluetooth(const char *h, const int p);
int deinitBluetooth(bluetoothtoken_t *bt);
void *bluetoothThreadFunction( void *d );

#endif /* BLUETOOTH_H_ */
