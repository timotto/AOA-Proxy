/*
 * audio.c
 *
 *  Created on: Oct 22, 2012
 *      Author: Tim
 */

#include "audio.h"
#include "log.h"

#ifdef AUDIO_NULL
int initAudio(audioStruct *audio)
{
	return 0;
}

int deinitAudio(audioStruct *audio)
{
	return 0;
}

int requestAudio(audioStruct *audio)
{
	return 0;
}

int releaseAudio(audioStruct *audio)
{
	return 0;
}

int playAudio(audioStruct *audio, char *buf, int len)
{
	return 0;
}

#else
#include <string.h>
#include <ao/ao.h>
#include <pthread.h>

static void initAudioBuffers(audioStruct *audio, int num, int bytesPerBuffer);
static void deinitAudioBuffers(audioStruct *audio);
static struct s_audiobufferList* shiftList(audioStruct *audio, struct s_audiobufferList** list, pthread_mutex_t* mutex, pthread_cond_t* cond);
static void appendList(struct s_audiobufferList** list, pthread_mutex_t* mutex, pthread_cond_t* cond, struct s_audiobufferList* add);

//static void* audioThreadFunction(void *d);

static volatile int audioInUse = 0;

int initAudio(audioStruct *audio)
{
	audio->aoFormat.bits = 16;
	audio->aoFormat.rate = 44100;
	audio->aoFormat.channels = 2;
	audio->aoFormat.byte_format = AO_FMT_LITTLE;
	audio->aoFormat.matrix = NULL;

	ao_initialize();

	audio->aoDevice = ao_open_live(ao_default_driver_id(), &audio->aoFormat, NULL);
	if (audio->aoDevice == NULL) {
		logError("failed to open audio device\n");
		ao_shutdown();
		audio->run = 0;
		return -1;
//		return NULL;
	}

	initAudioBuffers(audio, 3, 1764*2);
	audio->run = 1;
//	pthread_create(&audio->thread, NULL, (void*)&audioThreadFunction, (void*)audio);
	return 0;
}

int deinitAudio(audioStruct *audio)
{

	audio->run = 0;
	pthread_cond_signal(&audio->emptyBuffersCond);
	pthread_cond_signal(&audio->fullBuffersCond);
	pthread_join(audio->thread, NULL);
	ao_close(audio->aoDevice);
	ao_shutdown();

	deinitAudioBuffers(audio);
	logDebug("audio.c: deinitAudio done");
	return 0;
}

int requestAudio(audioStruct *audio)
{
	if(audioInUse != 0)
		return 1;
	audioInUse = 1;
	return 0;
}

int releaseAudio(audioStruct *audio)
{
	audioInUse = 0;
	return 0;
}

static struct s_audiobufferList *currentWriteBuffer = NULL;
static int droppedAudioBytes = 0;

/**
 * TODO: this method could be called from different Androids
 */
int playAudio(audioStruct *audio, char *buf, int len)
{
	if (!(audio->run && len))
		return 0;

	if (currentWriteBuffer == NULL) {
		currentWriteBuffer = shiftList(audio, &audio->emptyBuffers, &audio->emptyBuffersMutex, NULL);
	} else if(currentWriteBuffer->buffer->len+len > currentWriteBuffer->buffer->size) {
		appendList(&audio->fullBuffers, &audio->fullBuffersMutex, &audio->fullBuffersCond, currentWriteBuffer);
		currentWriteBuffer = shiftList(audio, &audio->emptyBuffers, &audio->emptyBuffersMutex, NULL);
	}

	if (currentWriteBuffer == NULL) {
		if(!droppedAudioBytes)
			logError("no free buffers - dropping %d bytes of audio", len);
		droppedAudioBytes += len;
		return 0;
	} else if(droppedAudioBytes != 0) {
		logDebug("resuming after %d bytes dropped from audio", droppedAudioBytes);
		droppedAudioBytes = 0;
	}

	//TODO check if len <= buffer->size!
	memcpy(currentWriteBuffer->buffer->buffer + currentWriteBuffer->buffer->len, buf, len);
	currentWriteBuffer->buffer->len += len;
	return len;
}

void* audioThreadFunction(void *d)
{
	logDebug("audio thread started");
	audioStruct *audio = (audioStruct*)d;

	struct s_audiobufferList *currentReadBuffer = NULL;
	while(audio->run) {
		currentReadBuffer = shiftList(audio, &audio->fullBuffers, &audio->fullBuffersMutex, &audio->fullBuffersCond);
		if (currentReadBuffer == NULL) {
			if(audio->run){
				logError("this should never happen #1");
			}
			logDebug("audio thread finished");
			return NULL;
		}

		ao_play(audio->aoDevice, currentReadBuffer->buffer->buffer, currentReadBuffer->buffer->len);
		currentReadBuffer->buffer->len = 0;
		appendList(&audio->emptyBuffers, &audio->emptyBuffersMutex, &audio->emptyBuffersCond, currentReadBuffer);
	}
	logDebug("audio thread finished");
	return NULL;
}

static void initAudioBuffers(audioStruct *audio, int num, int bytesPerBuffer)
{
	int i;
	struct s_audiobufferList *last = NULL;
	struct s_audiobufferList *np = NULL;
	audio->fullBuffers = NULL;

	for(i=0;i<num;i++) {
		np = (struct s_audiobufferList*)malloc(sizeof(struct s_audiobufferList));
		if(np == NULL) {
			logError("not enough RAM");
			exit(1);
		}
		np->buffer = (t_audiobuffer*)malloc(sizeof(t_audiobuffer));
		if (np->buffer == NULL) {
			logError("not enough RAM");
			exit(1);
		}
		np->buffer->buffer = (char*)malloc(sizeof(char) * bytesPerBuffer);
		if (np->buffer->buffer == NULL) {
			logError("not enough RAM");
			exit(1);
		}
		np->buffer->size = bytesPerBuffer;
		np->buffer->len = 0;
		np->next = NULL;

		if (last == NULL) {
			audio->emptyBuffers = np;
		} else {
			last->next = np;
		}
		last = np;
	}
	pthread_mutex_init(&audio->emptyBuffersMutex, NULL);
	pthread_cond_init(&audio->emptyBuffersCond, NULL);
	pthread_mutex_init(&audio->fullBuffersMutex, NULL);
	pthread_cond_init(&audio->fullBuffersCond, NULL);

}

static void deinitAudioBuffers(audioStruct *audio)
{
	struct s_audiobufferList *np = audio->emptyBuffers;
	while(np != NULL) {
		struct s_audiobufferList *next = np->next;
		free(np->buffer->buffer);
		free(np->buffer);
		free(np);
		np = next;
	}

	np = audio->fullBuffers;
	while(np != NULL) {
		struct s_audiobufferList *next = np->next;
		free(np->buffer->buffer);
		free(np->buffer);
		free(np);
		np = next;
	}

	pthread_mutex_destroy(&audio->emptyBuffersMutex);
	pthread_mutex_destroy(&audio->fullBuffersMutex);
	pthread_cond_destroy(&audio->emptyBuffersCond);
	pthread_cond_destroy(&audio->fullBuffersCond);
}

static struct s_audiobufferList* shiftList(audioStruct *audio, struct s_audiobufferList** list, pthread_mutex_t* mutex, pthread_cond_t* cond)
{

	pthread_mutex_lock(mutex);
	struct s_audiobufferList* result = *list;

	if(cond == NULL) {
		if (result != NULL) {
			*list = result->next;
		}
	} else {
		while(*list == NULL) {
			pthread_cond_wait(cond, mutex);
			if (!audio->run){
				pthread_mutex_unlock(mutex);
				return result;
			}

		}
		result = *list;
		*list = result->next;
	}
	pthread_mutex_unlock(mutex);
	return result;
}

static void appendList(struct s_audiobufferList** list, pthread_mutex_t* mutex, pthread_cond_t* cond, struct s_audiobufferList* add)
{
	pthread_mutex_lock(mutex);
	struct s_audiobufferList* last;
	add->next = NULL;
	if (*list == NULL) {
		*list = add;
	} else {
		last = *list;
		while(last->next != NULL) {
			last = last->next;
		}
		last->next = add;
	}
	if(cond != NULL) {
		pthread_cond_signal(cond);
	}
	pthread_mutex_unlock(mutex);
}

#endif
