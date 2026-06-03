
/* 
 * Argyll Color Correction System
 * ChromCast test harness
 *
 * Author: Graeme W. Gill
 * Date:   28/8/2014
 *
 * Copyright 2014 Graeme W. Gill
 * All rights reserved.
 *
 * This material is licenced under the GNU AFFERO GENERAL PUBLIC LICENSE Version 3 :-
 * see the License2.txt file for licencing details.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <time.h>
#include "copyright.h"
#include "aconfig.h"
#ifndef SALONEINSTLIB
#include "numlib.h"
#else
#include "numsup.h"
#endif
#include "yajl.h"
#include "conv.h"
#include "ccmdns.h"
#include "ccpacket.h"
#include "ccmes.h"

/* Check if JSON is invalid */
/* Exits if invalid */
static void check_json(char *mesbuf) {
	yajl_val node;
	char errbuf[1024];

	if ((node = yajl_tree_parse(mesbuf, errbuf, sizeof(errbuf))) == NULL) {
		printf("yajl_tree_parse of send message failed with '%s'\n",errbuf);
		printf("JSON = '%s'\n",mesbuf);
		exit(1);
	}
	yajl_tree_free(node);
}

/* Return nz on error */
static int get_a_reply(ccmessv *mesv, ORD8 **pdata) {
	ccmessv_err merr;
	ccmes mes;
	char *source_id;
	char *destination_id;
	char *namespace;
	int binary;					/* Flag */
	ORD8 *data;					/* String or binary */
	ORD32 bin_len;				/* Binary data length */

	if ((merr = mesv->receive(mesv, &mes)) != ccmessv_OK) {
		printf("mesv->receive failed with '%s'\n",ccmessv_emes(merr));
		return -1;
	} else {
		printf("Got message:\n");
		printf("  source_id = '%s'\n",mes.source_id);
		printf("  destination_id = '%s'\n",mes.destination_id);
		printf("  namespace = '%s'\n",mes.namespace);
		printf("  binary = %d\n",mes.binary);
		if (binary) {
			printf("  payload =\n");
			adump_bytes(g_log, "  ", data, 0, mes.bin_len);
		} else {
			printf("  payload = '%s'\n",mes.data);
		}
		if (pdata != NULL)
			*pdata = mes.data;
		else
			free(data);
	}
	return 0;
}

/* Get replies until we get one with the matching Id, then return */
static int get_a_reply_id(ccmessv *mesv, int tid, ORD8 **pdata) {
	ORD8 *data;
	int id, rv;
	yajl_val node, v;
	char errbuf[1024];

//printf("~~~ looking for id %d\n",tid);
	for (;;) {
		int id = -1;

		if ((rv = get_a_reply(mesv, &data)) != 0)
			return rv;

//		printf("\nGot JSON data to parse: '%s'\n",data);
		if ((node = yajl_tree_parse(data, errbuf, sizeof(errbuf))) == NULL)
			error("yajl_tree_parse failed with '%s'",errbuf);

		if ((v = yajl_tree_get_first(node, "requestId", yajl_t_number)) != NULL) {
			printf("~~~~ Got id %d\n",id);
			id = YAJL_GET_INTEGER(v);
		}
		yajl_tree_free(node);

		if (id == tid) {
//printf("~~~ got our message\n");
			break;
		}
		free(data);
//printf("~~~ round we go again\n");
	}
	if (pdata != NULL)
		*pdata = data;
	else
		free(data);
	return 0; 
}

/* Do the authentication sequence */
static void authenticate(ccmessv *mesv) {

	/* (Not needed) */
}

int
main(
	int argc,
	char *argv[]
) {
	ccast_id **ids;
	int i;
	ccpacket *pk;
	ccpacket_err perr;
	ccmessv *mesv;
	ccmessv_err merr;
	char *sessionId = NULL;
	char *transportId = NULL;
	char mesbuf[1024];

	if ((ids = get_ccids()) == NULL) {
		error("get_ccids() failed");
	}

	if (ids[0] == NULL) {
		error("Found no ChromCasts");
	}

	for (i = 0; ids[i] != NULL; i++) {
		printf("Entry %d:\n",i);
		printf(" Name: %s\n",ids[i]->name);
		printf(" IP:   %s\n",ids[i]->ip);
	}

	if ((pk = new_ccpacket()) == NULL) {
		error("new_ccpacket() failed");
	}
	
	if ((perr = pk->connect(pk, ids[0]->ip, 8009)) != ccpacket_OK) {
		error("ccpacket connect failed with '%s",ccpacket_emes(perr));
	} 
	printf("Got TLS connection to '%s\n'",ids[0]->name);

	if ((mesv = new_ccmessv(pk)) == NULL) {
		error("new_ccmessv() failed");
	}

	/* Attempt a connection */
	if ((merr = mesv->send(mesv, "sender-0", "receiver-0",
		"urn:x-cast:com.google.cast.tp.connection", 0,
		"{ \"type\": \"CONNECT\" }", 0)) != ccmessv_OK) {
		error("mesv->send CONNECT failed with '%s'",ccmessv_emes(merr));
	}
	
	/* Send a ping */
	if ((merr = mesv->send(mesv, "sender-0", "receiver-0",
		"urn:x-cast:com.google.cast.tp.heartbeat", 0,
		"{ \"type\": \"PING\" }", 0)) != ccmessv_OK) {
		error("mesv->send PING failed with '%s'",ccmessv_emes(merr));
	}
	
	/* Wait for a PONG */
	get_a_reply(mesv, NULL);

	authenticate(mesv);

	/* Launch the default application */
	/* (Presumably we would use the com.google.cast.receiver channel */
	/*  for monitoring and controlling the reciever) */
	if ((merr = mesv->send(mesv, "sender-0", "receiver-0",
		"urn:x-cast:com.google.cast.receiver", 0,
		"{ \"requestId\": 1, \"type\": \"LAUNCH\", \"appId\": \"CC1AD845\" }", 0)) != ccmessv_OK) {
		error("mesv->send LAUNCH failed with '%s'",ccmessv_emes(merr));
	}

	/* Receive the RECEIVER_STATUS status messages until it is ready to cast */
	/* and get the sessionId and transportId */
	/* We get periodic notification messages (requestId=0) as well as */
	/* a response messages (requestId=1) */
	for (;sessionId == NULL || transportId == NULL;) {
		ORD8 *data;
		yajl_val node, v;
		char errbuf[1024];

		get_a_reply(mesv, &data);

//		printf("\nGot JSON data to parse: '%s'\n",data);
		if ((node = yajl_tree_parse(data, errbuf, sizeof(errbuf))) == NULL)
			error("yajl_tree_parse failed with '%s'",errbuf);

		if ((v = yajl_tree_get_first(node, "sessionId", yajl_t_string)) != NULL) {
			sessionId = strdup(YAJL_GET_STRING(v));
//			printf("~1 got sessionId = '%s'\n",sessionId);
		}

		if ((v = yajl_tree_get_first(node, "transportId", yajl_t_string)) != NULL) {
			transportId = strdup(YAJL_GET_STRING(v));
//			printf("~1 got transportId = '%s'\n",transportId);
		}
		free(data);
		yajl_tree_free(node);
	}

	printf("\ngot sessionId = '%s', transportId = '%s'\n",sessionId, transportId);

	printf("\nAbout to send URL\n");

	/* Connect up to the reciever media channels */
	if ((merr = mesv->send(mesv, "med_send", transportId,
		"urn:x-cast:com.google.cast.tp.connection", 0,
		"{ \"type\": \"CONNECT\" }", 0)) != ccmessv_OK) {
		error("mesv->send CONNECT failed with '%s'",ccmessv_emes(merr));
	}
	
	// "urn:x-cast:com.google.cast.player.message"
	// "urn:x-cast:com.google.cast.media"}

	sprintf(mesbuf, "{ \"requestId\": 2, \"type\": \"LOAD\", \"media\": "
	"{ \"contentId\": \"http://www.argyllcms.com/ArgyllCMSLogo.gif\",\"streamType\": \"LIVE\","
	"\"contentType\": \"image/gif\" } }");

//	"\"contentType\": \"image/gif\", \"duration\" : 0.1 }, \"autplay\": \"true\" }");

	check_json(mesbuf);

	/* Send the LOAD command */
	if ((merr = mesv->send(mesv, "med_send", transportId,
		"urn:x-cast:com.google.cast.media", 0,
	    mesbuf, 0
		)) != ccmessv_OK) {
		error("mesv->send LOAD failed with '%s'",ccmessv_emes(merr));
	}

	if (get_a_reply_id(mesv, 2, NULL) != 0)
		exit(1);

	msec_sleep(2000);

#ifdef NEVER
	/* Check the media status */
	if ((merr = mesv->send(mesv, "med_send", transportId,
		"urn:x-cast:com.google.cast.media", 0,
		"{ \"requestId\": 3, \"type\": \"GET_STATUS\" }", 0)) != ccmessv_OK) {
		error("mesv->send GET_STATUS failed with '%s'",ccmessv_emes(merr));
	}
	
	if (get_a_reply_id(mesv, 3, NULL) != 0)
		exit(1);
#endif

	sprintf(mesbuf, "{ \"requestId\": 4, \"type\": \"LOAD\", \"media\": { \"contentId\": \"http://www.argyllcms.com/testing.png\",\"streamType\": \"LIVE\",\"contentType\": \"image/png\" } }");

//	sprintf(mesbuf, "{ \"requestId\": 4, \"type\": \"LOAD\", \"media\": { \"contentId\": \"http://www.argyllcms.com/testing.tif\",\"streamType\": \"LIVE\",\"contentType\": \"image/tiff\" } }");
//	sprintf(mesbuf, "{ \"requestId\": 4, \"type\": \"LOAD\", \"media\": { \"contentId\": \"http://www.argyllcms.com/doc/sl.jpg\",\"streamType\": \"LIVE\",\"contentType\": \"image/jpeg\" } }");
	check_json(mesbuf);

	/* Send the LOAD command */
	if ((merr = mesv->send(mesv, "med_send", transportId,
		"urn:x-cast:com.google.cast.media", 0,
	    mesbuf, 0
		)) != ccmessv_OK) {
		error("mesv->send LOAD failed with '%s'",ccmessv_emes(merr));
	}

	if (get_a_reply_id(mesv, 4, NULL) != 0)
		exit(1);

	msec_sleep(2000);

#ifdef NEVER
	/* Check the media status */
	if ((merr = mesv->send(mesv, "med_send", transportId,
		"urn:x-cast:com.google.cast.media", 0,
		"{ \"requestId\": 5, \"type\": \"GET_STATUS\" }", 0)) != ccmessv_OK) {
		error("mesv->send GET_STATUS failed with '%s'",ccmessv_emes(merr));
	}
	
	if (get_a_reply_id(mesv, 5, NULL) != 0)
		exit(1);
#endif

#ifdef NEVER
	/* Try and send it an image URL */
	if ((merr = mesv->send(mesv, "med_send", transportId,
		"urn:x-cast:com.google.cast.media", 0,
//		"{ \"imageUrl\": \"http://www.argyllcms.com/ArgyllCMSLogo.gif\", \"requestId\": 6 }", 0)) != ccmessv_OK) {
		"{ \"url\": \"http://www.argyllcms.com/ArgyllCMSLogo.gif\", \"requestId\": 2 }", 0)) != ccmessv_OK) {
		error("mesv->send imageUrl failed with '%s'",ccmessv_emes(merr));
	}

#endif
	/* Wait for any reply */
	for (;;) {
		if (get_a_reply(mesv, NULL) != 0)
			break;
	}
	
	// ~~999

	mesv->del(mesv);
	pk->del(pk);

	printf("Disconnected from '%s'\n",ids[0]->name);

	free_ccids(ids);

	return 0;
}

