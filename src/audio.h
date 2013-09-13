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

#ifndef AUDIO_H_
#define AUDIO_H_

#include <ao/ao.h>
#include <pthread.h>

typedef struct s_audiobuffer {
	char *buffer;
	int size;
	int len;
} t_audiobuffer;
typedef struct s_audiobufferList {
	t_audiobuffer *buffer;
	struct s_audiobufferList *next;
} t_audiobufferList;


typedef struct t_audioStruct {
	ao_sample_format aoFormat;
	ao_device *aoDevice;
	int run;
	pthread_t thread;
//	pthread_mutex_t mutex;
//	pthread_cond_t cond;

	struct s_audiobufferList *fullBuffers;
	pthread_mutex_t fullBuffersMutex;
	pthread_cond_t fullBuffersCond;
	struct s_audiobufferList *emptyBuffers;
	pthread_mutex_t emptyBuffersMutex;
	pthread_cond_t emptyBuffersCond;

//	int buffersize;
//	char *bufferA;
//	char *bufferB;
//	char *writeBuffer;
//	char *readBuffer;
//	int writePos;

} audioStruct;

int initAudio(audioStruct *audio);
int deinitAudio(audioStruct *audio);
int requestAudio(audioStruct *audio);
int releaseAudio(audioStruct *audio);
int playAudio(audioStruct *audio, char *buf, int len);
void* audioThreadFunction(void *d);

#endif /* AUDIO_H_ */
