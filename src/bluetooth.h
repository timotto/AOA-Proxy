/*
 * bluetooth.h
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
