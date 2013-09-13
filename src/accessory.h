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

#ifndef ACCESSORY_H_
#define ACCESSORY_H_

#include <libusb-1.0/libusb.h>

#define USE_AUDIO

#define AOA_PROTOCOL_MIN	1
#define AOA_PROTOCOL_MAX	2

#define VID_GOOGLE			0x18D1
#define	PID_AOA_ACC			0x2D00
#define	PID_AOA_ACC_ADB		0x2D01
#define	PID_AOA_AU			0x2D02
#define	PID_AOA_AU_ADB		0x2D03
#define	PID_AOA_ACC_AU		0x2D04
#define	PID_AOA_ACC_AU_ADB	0x2D05

typedef void (*fnusb_iso_cb)(uint8_t *buf, int len);

typedef struct {
	struct libusb_transfer **xfers;
	uint8_t *buffer;
	fnusb_iso_cb cb;
	int num_xfers;
	int pkts;
	int len;
	int dead;
	int dead_xfers;
} fnusb_isoc_stream;

typedef struct t_accessory_droid {
	libusb_device_handle *usbHandle;
	unsigned char inendp;
	unsigned char outendp;
	unsigned char audioendp;

	int inpacketsize;
	int outpacketsize;
	int audiopacketsize;

	unsigned char bulkInterface;
	unsigned char audioInterface;
	unsigned char audioAlternateSetting;

	fnusb_isoc_stream isocStream;
} accessory_droid;

int isDroidInAcc(libusb_device *dev);
void switchDroidToAcc(libusb_device *dev, int force, int audio);
int setupDroid(libusb_device *usbDevice, accessory_droid *device);
int shutdownUSBDroid(libusb_device *usbDevice, accessory_droid *device);

#endif /* ACCESSORY_H_ */
