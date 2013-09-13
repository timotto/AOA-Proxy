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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include "a2spipe.h"
#include "log.h"

#define FN_SPEW printf
#define FN_ERROR printf
#define FN_WARNING printf
#define FN_FLOOD printf

static void a2s_usbrx_cb(struct libusb_transfer *transfer) {
	usbXferThread *t = (usbXferThread*)transfer->user_data;
	tickleUsbXferThread(t);
}

void tickleUsbXferThread(usbXferThread *t) {
//	logDebug("tickle, lock\n");
	pthread_mutex_lock( &t->mutex );
	if(t->usbActive) {
//		logDebug("tickle, signal\n");
		t->usbActive = 0;
		pthread_cond_signal( &t->condition );
//	} else {
//		logDebug("tickle, no signal\n");
	}
//	logDebug("tickle, unlock\n");
	pthread_mutex_unlock( &t->mutex );
}

void *a2s_usbRxThread( void *d ) {
	logDebug("a2s_usbRxThread started\n");

	struct listentry *device = (struct listentry*)d;

	unsigned char buffer[device->droid.inpacketsize];
	int rxBytes = 0;
	int txBytes;
	int sent;
	int r;

	libusb_fill_bulk_transfer(device->usbRxThread.xfr, device->droid.usbHandle, device->droid.inendp,
			buffer, sizeof(buffer),
			(libusb_transfer_cb_fn)&a2s_usbrx_cb, (void*)&device->usbRxThread, 0);

	while(!device->usbRxThread.stop && !device->usbDead && !device->socketDead) {

		pthread_mutex_lock( &device->usbRxThread.mutex );
		device->usbRxThread.usbActive = 1;

//		logDebug("a2s_usbRxThread reading...\n");
		r = libusb_submit_transfer(device->usbRxThread.xfr);
		if (r < 0) {
			logError("a2s usbrx submit transfer failed\n");
			device->usbDead = 1;
			device->usbRxThread.usbActive = 0;
			pthread_mutex_unlock( &device->usbRxThread.mutex );
			break;
		}

//		waitUsbXferThread(&device->usbRxThread);

//		logDebug("a2s_usbRxThread waiting...\n");
		pthread_cond_wait( &device->usbRxThread.condition, &device->usbRxThread.mutex);
//		logDebug("a2s_usbRxThread wait over\n");
		if (device->usbRxThread.usbActive) {
			logError("wait, unlock but usbActive!\n");
		}
		pthread_mutex_unlock( &device->usbRxThread.mutex );

		if (device->usbRxThread.stop || device->usbDead || device->socketDead)
			break;

		switch(device->usbRxThread.xfr->status) {
		case LIBUSB_TRANSFER_COMPLETED:
//			logDebug("a2s_usbRxThread writing...\n");
			rxBytes = device->usbRxThread.xfr->actual_length;
			sent = 0;
			txBytes = 0;
			while(sent < rxBytes && !device->usbRxThread.stop) {
				txBytes = write(device->sockfd, buffer + sent, rxBytes - sent);
				if (txBytes <= 0) {
					logError("a2s usbrx socket tx failed\n");
					device->socketDead = 1;
					device->usbRxThread.stop = 1;
				} else {
					sent += txBytes;
				}
			}
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			device->usbDead = 1;
			device->usbRxThread.stop = 1;
			break;
		default:
//			logDebug("a2s_usbRxThread usb error %d, ignoring\n", device->usbRxThread.xfr->status);
			break;
		}
	}

	device->usbRxThread.stopped = 1;
	logDebug("a2s_usbRxThread finished\n");
	pthread_exit(0);
	return NULL;
}

void *a2s_socketRxThread( void *d ) {
	logDebug("a2s_socketRxThread started\n");

	struct listentry *device = (struct listentry*)d;

	unsigned char buffer[device->droid.outpacketsize];
	int rxBytes = 0;
	int r;

	libusb_fill_bulk_transfer(device->socketRxThread.xfr, device->droid.usbHandle, device->droid.outendp,
			buffer, sizeof(buffer),
			(libusb_transfer_cb_fn)a2s_usbrx_cb, (void*)&device->socketRxThread, 0);

	device->socketRxThread.xfr->status = LIBUSB_TRANSFER_COMPLETED;

	while(!device->socketRxThread.stop && !device->usbDead && !device->socketDead) {

		if (device->socketRxThread.xfr->status == LIBUSB_TRANSFER_COMPLETED) {
//			logDebug("a2s_socketRxThread reading...\n");

			rxBytes = read(device->sockfd, buffer, sizeof(buffer));
			if (rxBytes <= 0) {
				logError("a2s usbtx socket rx failed\n");
				device->socketDead = 1;
				break;
			}
//			logDebug("socket RX %d bytes\n", rxBytes);
		}

		if (device->usbRxThread.stop || device->usbDead || device->socketDead)
			break;

//		logDebug("a2s_socketRxThread writing...\n");
		pthread_mutex_lock( &device->socketRxThread.mutex );
		device->socketRxThread.usbActive = 1;
		device->socketRxThread.xfr->length = rxBytes;

//		logDebug("USB TX %d bytes\n", device->socketRxThread.xfr->length);
		r = libusb_submit_transfer(device->socketRxThread.xfr);
		if (r < 0) {
			logError("a2s usbtx submit transfer failed\n");
			device->usbDead = 1;
			device->socketRxThread.usbActive = 0;
			pthread_mutex_unlock( &device->socketRxThread.mutex );
			break;
		}

//		waitUsbXferThread(&device->socketRxThread);

		pthread_cond_wait( &device->socketRxThread.condition, &device->socketRxThread.mutex);
		if (device->socketRxThread.usbActive) {
			logError("wait, unlock but usbActive!\n");
		}
		pthread_mutex_unlock( &device->socketRxThread.mutex );

		switch(device->socketRxThread.xfr->status) {
		case LIBUSB_TRANSFER_COMPLETED:
//			logDebug("USB TX %d/%d bytes DONE\n", device->socketRxThread.xfr->actual_length, rxBytes);
			break;
		case LIBUSB_TRANSFER_NO_DEVICE:
			device->usbDead = 1;
			device->usbRxThread.stop = 1;
			break;
		default:
//			logDebug("a2s_socketRxThread usb error %d, ignoring\n", device->socketRxThread.xfr->status);
			break;
		}
	}

	device->socketRxThread.stopped = 1;
	logDebug("a2s_socketRxThread finished\n");
	pthread_exit(0);
	return NULL;
}

static void iso_callback(struct libusb_transfer *xfer) {
	struct listentry *device = (struct listentry *)xfer->user_data;
	fnusb_isoc_stream *strm = &device->droid.isocStream;
	int i;

	switch(xfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: // Normal operation.
		{
			uint8_t *buf = (uint8_t*)xfer->buffer;
			for (i=0; i<strm->pkts; i++) {
				strm->cb(buf, xfer->iso_packet_desc[i].actual_length);
				buf += strm->len;
			}
			int res;
			res = libusb_submit_transfer(xfer);
			if (res != 0) {
				FN_ERROR("iso_callback(): failed to resubmit transfer after successful completion: %d\n", res);
				strm->dead_xfers++;
				if (res == LIBUSB_ERROR_NO_DEVICE) {
					device->usbDead = 1;
				}
			}
			break;
		}
		case LIBUSB_TRANSFER_NO_DEVICE:
		{
			// We lost the device we were talking to.  This is a large problem,
			// and one that we should eventually come up with a way to
			// properly propagate up to the caller.
			if(!device->usbDead) {
				FN_ERROR("USB device disappeared, cancelling stream %02x :(\n", xfer->endpoint);
			}
			strm->dead_xfers++;
			device->usbDead = 1;
			break;
		}
		case LIBUSB_TRANSFER_CANCELLED:
		{
			if(strm->dead) {
				FN_SPEW("EP %02x transfer cancelled\n", xfer->endpoint);
			} else {
				// This seems to be a libusb bug on OSX - instead of completing
				// the transfer with LIBUSB_TRANSFER_NO_DEVICE, the transfers
				// simply come back cancelled by the OS.  We can detect this,
				// though - the stream should be marked dead if we're
				// intentionally cancelling transfers.
				if(!device->usbDead) {
					FN_ERROR("Got cancelled transfer, but we didn't request it - device disconnected?\n");
				}
				device->usbDead = 1;
			}
			strm->dead_xfers++;
			break;
		}
		default:
		{
			// On other errors, resubmit the transfer - in particular, libusb
			// on OSX tends to hit random errors a lot.  If we don't resubmit
			// the transfers, eventually all of them die and then we don't get
			// any more data from the Kinect.
			FN_WARNING("Isochronous transfer error: %d\n", xfer->status);
			int res;
			res = libusb_submit_transfer(xfer);
			if (res != 0) {
				FN_ERROR("Isochronous transfer resubmission failed after unknown error: %d\n", res);
				strm->dead_xfers++;
				if (res == LIBUSB_ERROR_NO_DEVICE) {
					device->usbDead = 1;
				}
			}
			break;
		}
	}
}

int fnusb_start_iso(struct listentry *device, fnusb_iso_cb cb, int ep, int xfers, int pkts, int len)
{

	logDebug("fnusb_start_iso(device,cb,ep=0x%02x,xfers=%d,pkts=%d,len=%d)\n",
			ep, xfers, pkts, len);

	device->audio.stop = 0;

	fnusb_isoc_stream *strm = &device->droid.isocStream;
	int ret, i;

	strm->cb = cb;
//	strm->cb = iso_in_callback;
	strm->num_xfers = xfers;
	strm->pkts = pkts;
	strm->len = len;
	strm->buffer = (uint8_t*)malloc(xfers * pkts * len);
	strm->xfers = (struct libusb_transfer**)malloc(sizeof(struct libusb_transfer*) * xfers);
	strm->dead = 0;
	strm->dead_xfers = 0;

	uint8_t *bufp = strm->buffer;

	for (i=0; i<xfers; i++) {
		FN_SPEW("Creating EP %02x transfer #%d len %d\n", ep, i, pkts * len);
		strm->xfers[i] = libusb_alloc_transfer(pkts);

		libusb_fill_iso_transfer(strm->xfers[i],
				device->droid.usbHandle,
				ep,
				bufp,
				pkts * len,
				pkts,
				(libusb_transfer_cb_fn)iso_callback,
				device,
				0);

		libusb_set_iso_packet_lengths(strm->xfers[i], len);

		ret = libusb_submit_transfer(strm->xfers[i]);
		if (ret < 0) {
			FN_WARNING("Failed to submit isochronous transfer %d: %d\n", i, ret);
			strm->dead_xfers++;
		}

		bufp += pkts*len;
	}

	return 0;

}

int fnusb_stop_iso(struct listentry *device, libusb_context *ctx)
{
//	freenect_context *ctx = dev->parent->parent;
//	libusb_context *ctx = dev->parent->ctx;
	fnusb_isoc_stream *strm = &device->droid.isocStream;

	int i;

	FN_FLOOD("fnusb_stop_iso() called\n");

	device->audio.stop = 1;
	strm->dead = 1;

	for (i=0; i<strm->num_xfers; i++)
		libusb_cancel_transfer(strm->xfers[i]);
	FN_FLOOD("fnusb_stop_iso() cancelled all transfers\n");

	if(strm->dead_xfers < strm->num_xfers) {
		FN_FLOOD("fnusb_stop_iso() dead = %d\tnum = %d\n", strm->dead_xfers, strm->num_xfers);
	}
//	while (strm->dead_xfers < strm->num_xfers) {
//		FN_FLOOD("fnusb_stop_iso() dead = %d\tnum = %d\n", strm->dead_xfers, strm->num_xfers);
//		libusb_handle_events(ctx);
//	}

	for (i=0; i<strm->num_xfers; i++)
		libusb_free_transfer(strm->xfers[i]);
	FN_FLOOD("fnusb_stop_iso() freed all transfers\n");

	free(strm->buffer);
	free(strm->xfers);

	FN_FLOOD("fnusb_stop_iso() freed buffers and stream\n");
	memset(strm, 0, sizeof(*strm));
	FN_FLOOD("fnusb_stop_iso() done\n");
	return 0;
}


