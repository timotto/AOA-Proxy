/*
 * bluetooth.c
 *
 *  Created on: Oct 22, 2012
 *      Author: Tim
 */

#include "bluetooth.h"
#include "log.h"
#include "tcp.h"

//#define BTlogDebug(x...) logDebug(x)
#define BTlogDebug(x...)


#ifdef BLUETOOTH_NULL
int initBluetooth(bluetoothtoken_t *bt)
{
	return 0;
}
int deinitBluetooth(bluetoothtoken_t *bt)
{
	return 0;
}

void *bluetoothThreadFunction( void *d )
{

}
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

// 598a65d9-9659-4251-9dd5-3619c946a572

static int add_service(sdp_session_t *session, uint32_t *handle, uint8_t rfcomm_channel);
static int del_service(sdp_session_t *session, uint32_t handle);


typedef struct s_socketPair {
	int btFd;
	int tcpFd;
	struct s_socketPair *next;
} t_sockatPair;
static struct sockaddr_rc loc_addr = { 0 };
static const char *hostname;
static int portno;
static t_sockatPair *pairsList = NULL;
static sdp_session_t *sdpSession;
static uint32_t recordHandle;

int testfunc() {
	return 2;
}

bluetoothtoken_t * initBluetooth(const char *h, const int p)
{
	hostname = h;
	portno = p;
	int s;
    uuid_t svc_uuid;
    struct sockaddr_rc rem_addr = { 0 };
    socklen_t opt = sizeof(rem_addr);

    // allocate socket
    if((s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) <= 0) {
    	logError("failed to bind bluetooth socket: %d", s);
    	return NULL;
    }

    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 10;
    if((bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr))) == -1) {
    	logError("failed to bind bluetooth socket");
    	close(s);
    	return NULL;
    }
    logDebug("BT bound to channel %d", loc_addr.rc_channel);

	// put socket into listening mode
	if(listen(s, 1) == -1) {
		logError("failed to setup listening mode");
    	close(s);
    	return NULL;
	}

	sdpSession = sdp_connect( BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY );
	if(add_service(sdpSession, &recordHandle, loc_addr.rc_channel) < 0) {
		logError("failed to register service @ SDP");
		recordHandle = 0;
	}

//	sdpSession = register_service(loc_addr.rc_channel);

	bluetoothtoken_t* btp = (bluetoothtoken_t*)malloc(sizeof(bluetoothtoken_t));
	btp->s = s;
//not used yet:
//	pthread_mutex_init(&btp->mutex, NULL);
//	pthread_cond_init(&btp->condition, NULL);

	return btp;
}

int deinitBluetooth(bluetoothtoken_t *bt)
{
	if (recordHandle != 0) {
		del_service(sdpSession, recordHandle);
	}
	del_service(sdpSession, recordHandle);
	sdp_close(sdpSession);

    close(bt->s);
//not used yet:
//    pthread_mutex_destroy(&bt->mutex);
//    pthread_cond_destroy(&bt->condition);
	return 0;
}

static int peerWrite(int firstFd, const char *buf, int len)
{
	t_sockatPair * head = pairsList;
	while(head != NULL) {
		int s;
		if (head->btFd == firstFd) {
			s = head->tcpFd;
		} else if (head->tcpFd == firstFd) {
			s = head->btFd;
		} else {
			head = head->next;
			continue;
		}
		int r;
		BTlogDebug("found peer, sending %d bytes, ", len);
		r = write(s, buf, len);
		return r;
	}
	logError("peer not found");
	return -1;
}

static t_sockatPair* addPair(int btfd, int tcpfd) {
	t_sockatPair *n = (t_sockatPair*)malloc(sizeof(t_sockatPair));
	if (n == NULL) {
		logError("not enough RAM");
		return NULL;
	}

	n->btFd = btfd;
	n->tcpFd = tcpfd;

	if (pairsList == NULL) {
		pairsList = n;
	} else {
		t_sockatPair *p = pairsList;
		while (p->next != NULL) {
			p = p->next;
		}
		p->next = n;
	}
	return n;
}

/**
 * also free the pair
 * one of fds may be zero
 */
static void closePair(int btFd, int tcpFd, fd_set *master)
{
	t_sockatPair * head = pairsList;
	t_sockatPair *prev = NULL;
	t_sockatPair *next = NULL;

	while(head != NULL) {
		if (
				(btFd == 0 || head->btFd == btFd)
				&&
				(tcpFd == 0 || head->tcpFd == tcpFd)) {
			next = head->next;
			if(head->btFd != 0){
				close(head->btFd);
        		FD_CLR(head->btFd, master);
			}
			if(head->tcpFd!=0){
				close(head->tcpFd);
        		FD_CLR(head->tcpFd, master);
			}

			if (prev == NULL) {
				pairsList = next;
			} else {
				prev->next = next;
			}
			free(head);
			return;
		}
		prev = head;
		head = head->next;
	}
	logDebug("closePair(%d, %d) - not found", btFd, tcpFd);
	if (btFd != 0) {
		close(btFd);
		FD_CLR(btFd, master);
	}
	if (tcpFd != 0) {
		close(tcpFd);
		FD_CLR(tcpFd, master);
	}
}

void *bluetoothThreadFunction( void *d ) {

    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[1024] = { 0 };
    int client, bytes_read, status;
    unsigned int opt = sizeof(rem_addr);
    fd_set readfds;
    fd_set master;
    int maxfd, sock_flags, i;
    struct timeval timeout;

	bluetoothtoken_t *bt = (bluetoothtoken_t*)d;

	FD_ZERO(&master);
	FD_ZERO(&readfds);

    // put server socket into nonblocking mode
    sock_flags = fcntl( bt->s, F_GETFL, 0 );
    fcntl( bt->s, F_SETFL, sock_flags | O_NONBLOCK );

    FD_SET(bt->s, &master);
    maxfd = bt->s;

    while(1) {
    	readfds = master;
		if(select(maxfd+1, &readfds, NULL, NULL, NULL) == -1) {
		    logError("bt-server select failed, thread dies");
		    //TODO close all pairs
		    pthread_exit(NULL);
		    return NULL;
		}

		for(i=0;i<=maxfd;i++) {
			if(FD_ISSET(i, &readfds)) {
				if (i == bt->s) {
		            client = accept( bt->s, (struct sockaddr*)&rem_addr, &opt );
		            if( client >= 0 ){
		                FD_SET(client, &master); /* add to master set */
		                if(client > maxfd) {
		                    maxfd = client;
		                }

		                logDebug("new bt connection! tcp...");
		                int tcpfd = connectTcpSocket(hostname, portno);
		                if (tcpfd >= 0) {
		                	FD_SET(tcpfd, &master);
		                	if (tcpfd > maxfd) {
		                		maxfd = tcpfd;
		                	}
		                	addPair(client, tcpfd);
		                	logDebug("new bt <-> tcp connection!");
		                } else {
		                	logError("tcp socket for bt failed");
		                	close(i);
		                	FD_CLR(i, &master);
		                }
		            } else {
		            	logError("bt accept failed");
		            }
				} else {
					bytes_read = read(i, buf, sizeof(buf));
					if (bytes_read <= 0) {
						logError("rx error, closing both");
						// don't know if it was bt or tcp socket...
						closePair(i, 0, &master);
						closePair(0, i, &master);
					} else {
						if(peerWrite(i, buf, bytes_read) != bytes_read) {
							logError("tx error, closing both");
							closePair(i, 0, &master);
							closePair(0, i, &master);
						}
					}
				}
			}
		}
    }
    logDebug("bluetooth thread finished");
	pthread_exit(NULL);
	return NULL;
}

static int add_service(sdp_session_t *session, uint32_t *handle, uint8_t rfcomm_channel)
{
	int ret = 0;
	unsigned char service_uuid_int[] = CARBOT_BLUETOOTH_SDP_UUID;
	const char *service_name = "Carbot Soul";
	const char *service_dsc = "General Purpose Android-Auto Interface";
	const char *service_prov = "Ubergrund.com";

	uuid_t root_uuid;
	uuid_t rfcomm_uuid, l2cap_uuid, svc_uuid;
	sdp_list_t *root_list;
	sdp_list_t *rfcomm_list = 0, *l2cap_list = 0, *proto_list = 0, *access_proto_list = 0, *service_list = 0;

	sdp_data_t *channel = 0;
	sdp_record_t *rec;
	// connect to the local SDP server, register the service record, and
	// disconnect

	if (!session) {
		logDebug("Bad local SDP session\n");
		return -1;
	}
	rec = sdp_record_alloc();

	// set the general service ID
	sdp_uuid128_create(&svc_uuid, &service_uuid_int);
	service_list = sdp_list_append(0, &svc_uuid);
	sdp_set_service_classes(rec, service_list);
	sdp_set_service_id(rec, svc_uuid);

	// make the service record publicly browsable
	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root_list = sdp_list_append(0, &root_uuid);
	sdp_set_browse_groups(rec, root_list);

	// set l2cap information
	sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
	l2cap_list = sdp_list_append(0, &l2cap_uuid);
	proto_list = sdp_list_append(0, l2cap_list);

	// set rfcomm information
	sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
	rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
	channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
	sdp_list_append(rfcomm_list, channel);
	sdp_list_append(proto_list, rfcomm_list);

	// attach protocol information to service record
	access_proto_list = sdp_list_append(0, proto_list);
	sdp_set_access_protos(rec, access_proto_list);

	// set the name, provider, and description
	sdp_set_info_attr(rec, service_name, service_prov, service_dsc);

	ret = sdp_record_register(session, rec, 0);

	if (ret < 0) {
		logError("Service registration failed\n");
	} else {
		*handle = rec->handle;
	}

	// cleanup
	sdp_data_free(channel);
	sdp_list_free(l2cap_list, 0);
	sdp_list_free(rfcomm_list, 0);
	sdp_list_free(root_list, 0);
	sdp_list_free(proto_list, 0);
	sdp_list_free(access_proto_list, 0);
	sdp_list_free(service_list, 0);

	sdp_record_free(rec);

	return ret;
}

static int del_service(sdp_session_t *session, uint32_t handle)
{
	sdp_record_t *rec;

	logDebug("Deleting Service Record.\n");

	if (!session) {
		logDebug("Bad local SDP session!\n");
		return -1;
	}

	rec = sdp_record_alloc();

	if (rec == NULL) {
		return -1;
	}

	rec->handle = handle;

	if (sdp_device_record_unregister(session, &loc_addr.rc_bdaddr, rec) != 0) {
		/*
		 If Bluetooth is shut off, the sdp daemon will not be running and it is
		 therefore common that this function will fail in that case. This is fine
		 since the record is removed when the daemon shuts down. We only have
		 to free our record handle here then....
		 */
		//CM_DBG("Failed to unregister service record: %s\n", strerror(errno));
		sdp_record_free(rec);
		return -1;
	}

	logDebug("Service Record deleted.");

	return 0;
}

