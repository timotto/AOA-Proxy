/*
 ============================================================================
 Name        : aoaproxy-r1.c
 Author      : Tim Otto
 Version     :
 Copyright   : This is totally mine
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#define	PIDFILE "/var/run/aoaproxy.pid"
#define SOCKET_RETRY	1

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "aoaproxy.h"
#include "accessory.h"
#include "a2spipe.h"
#include "audio.h"
#include "tcp.h"
#include "bluetooth.h"
#include "log.h"

static int do_exit = 0;
static libusb_context *ctx;
struct listentry *connectedDevices;
pthread_t usbInventoryThread;
int doUpdateUsbInventory = 0;

//static int timeIsUp(struct timeval *start, unsigned int timeoutMs);
static void shutdownEverything();
static void initSigHandler();
static int initUsb();
static void cleanupDeadDevices();
static int updateUsbInventory(libusb_device **devs);
static int connectDevice(libusb_device *device);
static void disconnectDevice(libusb_device *dev);
static void disconnectSocket(struct listentry *device);
static int startUSBPipe(struct listentry *device);
static void stopUSBPipe(struct listentry *device);
static int startAudio(struct listentry *device);
static int stopAudio(struct listentry *device);
static void sig_hdlr(int signum);
static void tickleUsbInventoryThread();
int do_fork_foo();

audioStruct audio;

static int haveAudio = 0;
static int haveBluetooth = 0;
static bluetoothtoken_t *bt = NULL;

static const char *hostname = "localhost";
static struct t_excludeList *exclude = NULL;
static int portno = 8721;
static int autoscan = 1;
static int force = 0;
static int do_fork = 0;

int main(int argc, char** argv) {
	int r;
	int opt;

	while ((opt = getopt(argc, argv, "dfp:x:")) != -1) {
	  switch (opt) {
		case 'd':
			do_fork = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'p':
			portno = atoi(optarg);
			break;
		case 'x':
		{
			if(strlen(optarg)  != 9 || optarg[4] != ':') {
				fprintf(stderr, "invalid argument for -x: %s\n", optarg);
				exit(EXIT_FAILURE);
			}
			struct t_excludeList *e;
			int vid, pid;

			e = (struct t_excludeList*)malloc(sizeof(struct t_excludeList));
			e->next = NULL;

			e->vid = strtol(optarg, NULL, 16);
			e->pid = strtol(optarg+5, NULL, 16);

			if (exclude == NULL) {
				exclude = e;
			} else {
				exclude->next = e;
			}

			break;
		}
		default: /* '?' */
			fprintf(stderr, "Usage: %s [-p port] [-f] [-x vid1:pid1] [-x vidn:pidn]\n",
					argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	logDebug("using [%s:%d]\n", hostname, portno);

	ctx = NULL;
	connectedDevices = NULL;

	if (0 > initUsb()) {
		logError("Failed to initialize USB\n");
		return 1;
	}

	if(initAudio(&audio) != 0) {
		logError("failed to open audio device - starting without");
		haveAudio = 0;
	} else {
		haveAudio = 1;
	}

//	if ((bt = initBluetooth(hostname, portno)) != NULL) {
//		haveBluetooth = 1;
//		pthread_create(&bt->thread, NULL, (void*)&bluetoothThreadFunction, (void*)bt);
//	} else {
//		haveBluetooth = 0;
//		logError("Failed to initialize bluetooth - starting without");
//	}

	if (do_fork)
		do_fork_foo();

	if(haveAudio) {
		// do after fork?
		pthread_create(&audio.thread, NULL, (void*)&audioThreadFunction, (void*)&audio);
	}

	initSigHandler();

	if(autoscan) {
		struct itimerval timer;
		timer.it_value.tv_sec = 1;
		timer.it_value.tv_usec = 0;
		timer.it_interval.tv_sec = 1;
		timer.it_interval.tv_usec = 0;
		setitimer (ITIMER_REAL, &timer, NULL);
	}


	libusb_device **devs = NULL;
	while(!do_exit) {

		if (doUpdateUsbInventory == 1) {
			doUpdateUsbInventory = 0;
			cleanupDeadDevices();
			updateUsbInventory(devs);
		}

		r = libusb_handle_events(ctx);
		if (r) {
			if (r == LIBUSB_ERROR_INTERRUPTED) {
				// ignore
			} else {
				if(!do_exit)
					logDebug("libusb_handle_events_timeout: %d\n", r);

				break;
			}
		}
	}

	if (devs != NULL)
		libusb_free_device_list(devs, 1);

	if(autoscan) {
		struct itimerval timer;
		memset (&timer, 0, sizeof(timer));
		setitimer (ITIMER_REAL, &timer, NULL);
	}
	shutdownEverything();
	return EXIT_SUCCESS;
}

static void shutdownEverything() {
	logDebug("shutdownEverything\n");
	do_exit = 1;

	struct itimerval timer;
	memset (&timer, 0, sizeof(timer));
	setitimer (ITIMER_REAL, &timer, NULL);

	while(connectedDevices != NULL)
		disconnectDevice(connectedDevices->usbDevice);

	if (ctx != NULL)
		libusb_exit(ctx); //close the session

	if (haveAudio) {
		deinitAudio(&audio);
	}

	if (bt != NULL) {
		deinitBluetooth(bt);
		logDebug("waiting for bluetooth thread...\n");
		if(0 != pthread_join(bt->thread, NULL)) {
			logError("failed to join bluetooth thread\n");
		}
		free(bt);
	}

}

static void tickleUsbInventoryThread() {
	doUpdateUsbInventory = 1;
}

static int updateUsbInventory(libusb_device **devs) {
	static ssize_t cnt = 0;
	static ssize_t lastCnt = 0;
//	static libusb_device **devs;
	static libusb_device **lastDevs = NULL;
	cnt = libusb_get_device_list(ctx, &devs);
	if(cnt < 0) {
		logError("Failed to list devices\n");
		return -1;
	}

	ssize_t i, j;
	int foundBefore;
	for(i = 0; i < cnt; i++) {
		foundBefore = 0;
		if ( lastDevs != NULL) {
			for(j=0;j < lastCnt; j++) {
				if (devs[i] == lastDevs[j]) {
					foundBefore = 1;
					break;
				}
			}
		}
		if (!foundBefore) {
			if(connectDevice(devs[i]) >= 0)
				libusb_ref_device(devs[i]);
		}
	}

	if (lastDevs != NULL) {
//		if (cnt != lastCnt)
//			fprintf(LOG_DEB, "number of USB devices changed from %d to %d\n", lastCnt, cnt);

		for (i=0;i<lastCnt;i++) {
			foundBefore = 0;
			for(j=0;j<cnt;j++) {
				if (devs[j] == lastDevs[i]) {
					foundBefore = 1;
					break;
				}
			}
			if(!foundBefore) {
				struct listentry *hit = connectedDevices;

				while(hit != NULL) {
					if ( hit->usbDevice == lastDevs[i]) {
						disconnectDevice(lastDevs[i]);
						libusb_unref_device(lastDevs[i]);
						break;
					}
					hit = hit->next;
				}
			}
		}
		libusb_free_device_list(lastDevs, 1);
	}
	lastDevs = devs;
	lastCnt = cnt;

	return 0;
}

static int connectDevice(libusb_device *device) {

	struct libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(device, &desc);
	if (r < 0) {
		logError("failed to get device descriptor: %d", r);
		return -1;
	}

	switch(desc.bDeviceClass) {
	case 0x09:
//		logDebug("device 0x%04X:%04X has wrong deviceClass: 0x%02x",
//				desc.idVendor, desc.idProduct,
//				desc.bDeviceClass);
		return -1;
	}


	struct t_excludeList *e = exclude;
	while(e != NULL) {
//		logDebug("comparing device [%04x:%04x] and [%04x:%04x]",
//				desc.idVendor, desc.idProduct, e->vid, e->pid);
		if(e->vid == desc.idVendor && e->pid == desc.idProduct) {
//			logDebug("device is on exclude list", desc.idVendor, desc.idProduct);
			return -1;
		}
		e = e->next;
	}

	if(!isDroidInAcc(device)) {
		logDebug("attempting AOA on device 0x%04X:%04X",
				desc.idVendor, desc.idProduct);
		switchDroidToAcc(device, 1, haveAudio);
		return -1;
	}

	struct listentry *entry = malloc(sizeof(struct listentry));
	if (entry == NULL) {
		logError("Not enough RAM");
		return -2;
	}
	bzero(entry, sizeof(struct listentry));

	entry->usbDevice = device;

#ifdef SOCKET_RETRY
	int s = connectTcpSocket(hostname, portno);
	while((r = connectTcpSocket(hostname, portno)) <= 0) {
		logError("failed to setup socket: %d, retrying\n", r);
		sleep(SOCKET_RETRY);
	}
	entry->sockfd = r;
	entry->socketDead = 0;
#else
	r = connectTcpSocket(hostname, portno);
	if (r < 0) {
		fprintf(LOG_ERR, "failed to setup socket: %d\n", r);
		free(entry);
		return -4;
	}
	entry->sockfd = r;
	entry->socketDead = 0;
#endif

	r = setupDroid(device, &entry->droid);
	if (r < 0) {
		logError("failed to setup droid: %d\n", r);
		free(entry);
		return -3;
	}

	entry->next = NULL;
	if (connectedDevices == NULL) {
		entry->prev = NULL;
		connectedDevices = entry;
	} else {
		struct listentry *last = connectedDevices;
		while(last->next != NULL)
			last = last->next;
		entry->prev = last;
		last->next = entry;
	}

	r = startUSBPipe(entry);
	if (r < 0) {
		logError("failed to start pipe: %d", r);
		disconnectDevice(device);
		return -5;
	}

	if (haveAudio && entry->droid.audioendp) {
		startAudio(entry);
	}

	logDebug("new Android connected");
	return 0;
}

static void disconnectDevice(libusb_device *dev) {
	struct listentry *device = connectedDevices;

	while(device != NULL) {
		if (device->usbDevice == dev) {
			if (device->prev == NULL) {
				connectedDevices = device->next;
			} else {
				device->prev->next = device->next;
			}
			if (device->next != NULL) {
				device->next->prev = device->prev;
			}

			if (device->droid.audioendp) {
				stopAudio(device);
			}
			stopUSBPipe(device);
			disconnectSocket(device);
			shutdownUSBDroid(device->usbDevice, &device->droid);

			free(device);
			logDebug("Android disconnected");
			return;
		}
		device = device->next;
	}
}

static void cleanupDeadDevices() {
	struct listentry *device = connectedDevices;

	while(device != NULL) {
		if (device->socketDead) {
			logDebug("found device with dead socket\n");
		} else if (device->usbDead) {
			logDebug("found device with dead USB\n");
		} else {
			device = device->next;
			continue;
		}
		disconnectDevice(device->usbDevice);
		cleanupDeadDevices();
		break;
	}
}

static void disconnectSocket(struct listentry *device) {
	if (device->sockfd > 0) {
		close(device->sockfd);
		device->sockfd = 0;
	}
}

static int initUsbXferThread(usbXferThread *t) {
//	t->dead = 0;
	t->xfr = libusb_alloc_transfer(0);
	if (t->xfr == NULL) {
		return -1;
	}
	t->stop = 0;
	t->stopped = 0;
	t->usbActive = 0;
	t->tickle = 0;
	pthread_mutex_init(&t->mutex, NULL);
	pthread_cond_init(&t->condition, NULL);
	return 0;
}

static void destroyUsbXferThread(usbXferThread *t) {
	pthread_mutex_destroy(&t->mutex);
	pthread_cond_destroy(&t->condition);
	libusb_free_transfer(t->xfr);
}

static int startUSBPipe(struct listentry *device) {
	int r;
	if(initUsbXferThread(&device->usbRxThread) < 0) {
		logError("failed to allocate usb rx transfer\n");
		return -1;
	}
	if(initUsbXferThread(&device->socketRxThread) < 0) {
		logError("failed to allocate usb tx transfer\n");
		destroyUsbXferThread(&device->usbRxThread);
		return -1;
	}

	r = pthread_create(&device->usbRxThread.thread, NULL, (void*)&a2s_usbRxThread, (void*)device);
	if (r < 0) {
		logError("failed to start usb rx thread\n");
		return -1;
	}

	r = pthread_create(&device->socketRxThread.thread, NULL, (void*)&a2s_socketRxThread, (void*)device);
	if (r < 0) {
		// other thread is stopped in disconnectDevice method
		logError("failed to start socket rx thread\n");
		return -1;
	}

	return 0;
}

static void stopUSBPipe(struct listentry *device) {
//	device->do_exit = 1;

	device->usbRxThread.stop = 1;
	if(device->usbRxThread.usbActive) {
//		logDebug("canceling usb rx\n");
		libusb_cancel_transfer(device->usbRxThread.xfr);
//		logDebug("usb rx thread tickle\n");
	}
//	logDebug("usb rx thread tickle\n");
	tickleUsbXferThread(&device->usbRxThread);
//	logDebug("usb rx thread kill usr1\n");
	pthread_kill(device->usbRxThread.thread, SIGUSR1);
	logDebug("waiting for usb rx thread...\n");
	if(0 != pthread_join(device->usbRxThread.thread, NULL)) {
		logError("failed to join usb rx thread\n");
	}

	device->socketRxThread.stop = 1;
	if(device->socketRxThread.usbActive) {
//		logDebug("canceling usb tx\n");
		libusb_cancel_transfer(device->socketRxThread.xfr);
	}
//	logDebug("socket rx thread tickle\n");
	tickleUsbXferThread(&device->socketRxThread);
//	logDebug("socket rx thread kill usr1\n");
	pthread_kill(device->socketRxThread.thread, SIGUSR1);
	logDebug("waiting for socket rx thread...\n");
	if(0 != pthread_join(device->socketRxThread.thread, NULL)) {
		logError("failed to join socket rx thread\n");
	}

//	logDebug("usb rx thread destroy!\n");
	destroyUsbXferThread(&device->usbRxThread);
//	logDebug("socket rx thread destroy!\n");
	destroyUsbXferThread(&device->socketRxThread);
	close(device->sockfd);

	logDebug("threads stopped\n");
}

int audioError = 1;

static inline void audio_callback(uint8_t *buf, int len) {
	if(len != 0 && 	playAudio(&audio, (char*)buf, len) == 0) {
		if (audioError) {
			audioError = 0;
			logError("first audio error\n");
		}
	}
}

static int startAudio(struct listentry *device) {
	requestAudio(&audio);
	fnusb_start_iso(device, &audio_callback, device->droid.audioendp, 8, 16, device->droid.audiopacketsize);
	return 0;
}

static int stopAudio(struct listentry *device) {
	fnusb_stop_iso(device, ctx);
	releaseAudio(&audio);
	return 0;
}

static void initSigHandler() {
	struct sigaction sigact;
	sigact.sa_handler = sig_hdlr;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGUSR1, &sigact, NULL);
	sigaction(SIGUSR2, &sigact, NULL);
	sigaction(SIGVTALRM, &sigact, NULL);
	sigaction(SIGALRM, &sigact, NULL);
}

static void sig_hdlr(int signum)
{
//	fprintf(LOG_DEB, "received signal [%d]\n", signum);
	switch (signum) {
	case SIGINT:
		logDebug("received SIGINT\n");
		do_exit = 1;
		break;
	case SIGUSR1:
		// USR1 is used to stop usb/socket rx threads.
		// when this signal arrives here, the thread was
		// already dead when the signal was sent.
		logDebug("received SIGUSR1\n");
		// ignore it.
		break;
	case SIGUSR2:
		tickleUsbInventoryThread();
		break;
	case SIGALRM:
	case SIGVTALRM:
		tickleUsbInventoryThread();
		break;
	default:
		break;
	}
}

static int initUsb() {

	int r;
	r = libusb_init(&ctx);
	if(r < 0) {
		return r;
	}
	libusb_set_debug(ctx, 3);
	return 0;
}

int do_fork_foo(){
	pid_t thisPid = fork();
	if (thisPid < 0) {
		printf("Failed to fork\n");
		return 1;
	}

	if (thisPid > 0) {
		FILE *pidfile = fopen(PIDFILE, "w");
		if (pidfile != NULL) {
			fprintf(pidfile,"%d\n", thisPid);
			fclose(pidfile);
		} else {
			perror("failed to create pidfile");
		}
		// this is parent.
		exit(0);
	}

	pid_t sid = 0;
	sid = setsid();
	if(sid < 0)
	{
		logError("setsid() failed: %d", sid);
		return 2;
	}
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	return 0;
}

