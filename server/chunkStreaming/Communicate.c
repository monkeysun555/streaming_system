/******************************************************************************
  Copyright (c) 2013 Morten Houmøller Nygaard - www.mortz.dk - admin@mortz.dk
 
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#include "Communicate.h"
// #include "cJSON.h"

/** 
 * Converts the unsigned 64 bit integer from host byte order to network byte 
 * order.
 */

pthread_mutex_t *lock_list = new pthread_mutex_t[NUM_RATE];
// pthread_mutex_t lock_0; 
// pthread_mutex_t lock_1; 
// pthread_mutex_t lock_2;

pthread_mutex_t lock_reply; 

uint64_t ntohl64(uint64_t value) {
	static const int num = 42;

	/**
	 * If these check is true, the system is using the little endian 
	 * convention. Else the system is using the big endian convention, which
	 * means that we do not have to represent our integers in another way.
	 */
	if (*(char *)&num == 42) {
		const uint32_t high = (uint32_t)(value >> 32);
		const uint32_t low = (uint32_t)(value & 0xFFFFFFFF);

		return (((uint64_t)(htonl(low))) << 32) | htonl(high);
	} else {
		return value;
	}	
}


/**
 * This function is suppose to get the remaining part of the message, if
 * the message from the client is too big to be contained in the buffer.
 * And we are dealing with the RFC6455 convention.
 */
uint64_t getRemainingMessage(ws_client *n, uint64_t msg_length) {
	int buffer_length = 0; 
	uint64_t remaining_length = 0, final_length = 0;
	char buffer[BUFFERSIZE];
	ws_message *m = n->message;

	do {
		memset(buffer, '\0', BUFFERSIZE);
	
		/**
		 * Receive new chunk of the message.
		 */	
		if ((buffer_length = recv(n->socket_id, buffer, BUFFERSIZE, 0)) <= 0) {
			printf("Didn't receive anything from remaining part of message. %d"
					"\n\n", buffer_length);
			fflush(stdout);
			return 0;	
		}

		/**
		 * The overall length of the message received. Because the recv call
		 * eventually will merge messages together we have to have a check
		 * whether the overall length we received is greater than the expected
		 * length of the message.
		 */ 
		final_length = (msg_length+remaining_length+buffer_length);	

		/**
		 * If the overall message is longer than the expected length of the
		 * message, we know that this chunk most contain the last part of the
		 * original message, and the first chunk of a new message.
		 */
		if ( final_length > m->len ) {
			uint64_t next_len = final_length-m->len;
			m->next = (char *) malloc(sizeof(char)*next_len);
			if (m->next == NULL) {
				printf("1: Couldn't allocate memory.\n\n");
				fflush(stdout);
				return 0;
			}
			memset(m->next, '\0', next_len);
			memcpy(m->next, buffer + (buffer_length - next_len), next_len);
			m->next_len = next_len;
			buffer_length = buffer_length - next_len;
		}

		remaining_length += buffer_length;

		memcpy(m->msg + (msg_length+(remaining_length-buffer_length)), buffer, 
				buffer_length);
		printf("Receiving\n");
	} while( (msg_length + remaining_length) < m->len );

	return remaining_length;
}

ws_connection_close parseMessage(char *buffer, uint64_t buffer_length, ws_client *n) {
	ws_message *m = n->message;
	int length, has_mask, skip, j;
	uint64_t message_length = m->len, i, remaining_length = 0, buf_len;

	/**
	 * Extracting information from frame
	 */
	has_mask = buffer[1] & 0x80 ? 1 : 0;
	length = buffer[1] & 0x7f;

	if (!has_mask) {
		printf("Message didn't have masked data, received: 0x%x\n\n", 
				buffer[1]);
		fflush(stdout);
		return CLOSE_PROTOCOL;
	}

	/**
	 * We need to handle the received frame differently according to which
	 * length that the frame has set.
	 *
	 * length <= 125: We know that length is the actual length of the message,
	 * 				  and that the maskin data must be placed 2 bytes further 
	 * 				  ahead.
	 * length == 126: We know that the length is an unsigned 16 bit integer,
	 * 				  which is placed at the 2 next bytes, and that the masking
	 * 				  data must be further 2 bytes away.
	 * length == 127: We know that the length is an unsigned 64 bit integer,
	 * 				  which is placed at the 8 next bytes, and that the masking
	 * 				  data must be further 2 bytes away.
	 */
	if (length <= 125) {
		m->len += length;	
		skip = 6;
		memcpy(&m->mask, buffer + 2, sizeof(m->mask));
	} else if (length == 126) {
		uint16_t sz16;
		memcpy(&sz16, buffer + 2, sizeof(uint16_t));

		m->len += ntohs(sz16);

		skip = 8;
		memcpy(&m->mask, buffer + 4, sizeof(m->mask));
	} else if (length == 127) {
		uint64_t sz64;
		memcpy(&sz64, buffer + 2, sizeof(uint64_t));

		m->len += ntohl64(sz64);

		skip = 14;
		memcpy(&m->mask, buffer + 10, sizeof(m->mask));
	} else {
		printf("Obscure length received from client: %d\n\n", length);
		fflush(stdout);
		return CLOSE_BIG;	
	}

	/**
	 * If the message length is greater that our MAXMESSAGE constant, we
	 * skip the message and close the connection.
	 */
	if (m->len > MAXMESSAGE) {
		printf("Message received was bigger than MAXMESSAGE.");
		fflush(stdout);
		return CLOSE_BIG;
	}
	
	/**
	 * Allocating memory to hold the message sent from the client.
	 * We can do this because we now know the actual length ofr the message.
	 */ 
	m->msg = (char *) malloc(sizeof(char) * (m->len + 1));
	if (m->msg == NULL) {
		printf("2: Couldn't allocate memory.\n\n");
		fflush(stdout);
		return CLOSE_UNEXPECTED;
	}
	memset(m->msg, '\0', (m->len + 1));

	buf_len = (buffer_length-skip);

	/**
	 * The message read from recv is larger than the message we are supposed
	 * to receive. This means that we have received the first part of the next
	 * message as well.
	 */
	if (buf_len > m->len) {
		uint64_t next_len = buf_len - m->len;
		m->next = (char *) malloc(next_len);
		if (m->next == NULL) {
			printf("3: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		memset(m->next, '\0', next_len);
		memcpy(m->next, buffer + (m->len+skip), next_len);
		m->next_len = next_len;
		buf_len = m->len;	
	}

	memcpy(m->msg+message_length, buffer+skip, buf_len);

	message_length += buf_len;

	/**
	 * We have not yet received the whole message, and must continue reading
	 * new data from the client.
	 */
	if (message_length < m->len) {
		if ((remaining_length = getRemainingMessage(n, message_length)) == 0) {
			return CLOSE_POLICY;
		}
	}

	message_length += remaining_length;

	/**
	 * If this is true, our receival of the message has gone wrong, and we 
	 * have no other choice than closing the connection.
	 */
	if (message_length != m->len) {
		printf("Message does not fit. Expected: %d but got %d\n\n", 
				(int) m->len, (int) message_length);
		fflush(stdout);
		return CLOSE_POLICY;
	}

	/**
	 * If everything went well, we have to remove the masking from the data.
	 */
	for (i = 0, j = 0; i < message_length; i++, j++){
		m->msg[j] = m->msg[i] ^ m->mask[j % 4];
	}

	// printf("Parse websocket successfully!%d\n", m->len);
	return CONTINUE;
}

/**
 * This function is used to get the whole message when using the Hybi-00
 * standard.
 */
ws_connection_close getWholeMessage(char *buffer, uint64_t buffer_length, 
		ws_client *n) {
	uint64_t msg_length = buffer_length, i, j;
	int buf_length;
	char buf[BUFFERSIZE];
	char *temp = NULL;

	/**
	 * Allocate what's received so far
	 */
	n->message->msg = malloc(buffer_length);
	if (n->message->msg == NULL) {
		printf("4: Couldn't allocate memory.\n\n");
		fflush(stdout);
		return CLOSE_UNEXPECTED;
	}
	memset(n->message->msg, '\0', buffer_length);

	/**
	 * If a byte is equal to zero, we know that we have reached the end of
	 * the message.
	 */
	for (i = 0; i < buffer_length; i++) {	
		if (buffer[i] != '\xFF') {
			n->message->msg[i] = buffer[i];
		} else {
			n->message->len = strlen(n->message->msg);
			return CONTINUE;
		}
	}

	/**
	 * While we still haven't seen the end of the message, continue reading
	 * data. The things done in the loop are basicly equivalent to what was
	 * done above.
	 */
	do {	
		memset(buf, '\0', BUFFERSIZE);
		if ((buf_length = recv(n->socket_id, buf, BUFFERSIZE, 0)) <= 0) {
			printf("Didn't receive any message from client.\n\n");
			fflush(stdout);
			return CLOSE_PROTOCOL;	
		}
		msg_length += buf_length;
	
		temp = realloc(n->message->msg, msg_length);
		if (temp == NULL) {
			printf("5: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		n->message->msg = temp;
		memset(n->message->msg+(msg_length-buf_length), '\0', buf_length);
		temp = NULL;

		for (j = 0, i = (msg_length-buf_length); i < msg_length; i++, j++) {	
			if (buf[j] != '\xFF') {
				n->message->msg[i] = buf[j];
			} else {
				n->message->len = strlen(n->message->msg);
				return CONTINUE;
			}
		}
	} while( msg_length < MAXMESSAGE );

	return CLOSE_UNEXPECTED;
}

/**
 * This function is split into 2. We would like to support different websocket
 * standards and therefore we encode the message as both RFC6455 and Hybi-00.
 */


															/**
															  0                   1                   2                   3
																  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
																 +-+-+-+-+-------+-+-------------+-------------------------------+
																 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
																 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
																 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
																 | |1|2|3|       |K|             |                               |
																 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
																 |     Extended payload length continued, if payload len == 127  |
																 + - - - - - - - - - - - - - - - +-------------------------------+
																 |                               |Masking-key, if MASK set to 1  |
																 +-------------------------------+-------------------------------+
																 | Masking-key (continued)       |          Payload Data         |
																 +-------------------------------- - - - - - - - - - - - - - - - +
																 :                     Payload Data continued ...                :
																 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
																 |                     Payload Data continued ...                |
																 +---------------------------------------------------------------+

															   FIN:  1 bit

																  Indicates that this is the final fragment in a message.  The first
																  fragment MAY also be the final fragment.

															   RSV1, RSV2, RSV3:  1 bit each

																  MUST be 0 unless an extension is negotiated that defines meanings
																  for non-zero values.  If a nonzero value is received and none of
																  the negotiated extensions defines the meaning of such a nonzero
																  value, the receiving endpoint MUST _Fail the WebSocket
																  Connection_.




															Fette & Melnikov             Standards Track                   [Page 28]
															 
															RFC 6455                 The WebSocket Protocol            December 2011


															   Opcode:  4 bits

																  Defines the interpretation of the "Payload data".  If an unknown
																  opcode is received, the receiving endpoint MUST _Fail the
																  WebSocket Connection_.  The following values are defined.

																  *  %x0 denotes a continuation frame

																  *  %x1 denotes a text frame

																  *  %x2 denotes a binary frame

																  *  %x3-7 are reserved for further non-control frames

																  *  %x8 denotes a connection close

																  *  %x9 denotes a ping

																  *  %xA denotes a pong

																  *  %xB-F are reserved for further control frames

															   Mask:  1 bit

																  Defines whether the "Payload data" is masked.  If set to 1, a
																  masking key is present in masking-key, and this is used to unmask
																  the "Payload data" as per Section 5.3.  All frames sent from
																  client to server have this bit set to 1.

															   Payload length:  7 bits, 7+16 bits, or 7+64 bits
															*/
ws_connection_close encodeMessage(ws_message *m, int option) {	// option 0: binary, 1: text
	uint64_t length = m->len;

	/**
	 * RFC6455 message encoding
	 */
	char op;
	if (option == 0){	// binary
		op = '\x82';
	} else{				// text
		op = '\x81';
	}
	if (m->len <= 125) {
		// printf("arrive here encode inner 1, length: %d\n", m->len);
		length += 2;
		m->enc = (uint8_t *) malloc(sizeof(uint8_t) * length);
		if (m->enc == NULL) {
			printf("6: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		m->enc[0] = op;
		m->enc[1] = m->len;
		memcpy(m->enc + 2, m->msg, m->len);
	} else if (m->len <= 65535) {
		// printf("arrive here encode inner 2, length: %d\n", m->len);
		uint16_t sz16;
		length += 4;
		// printf("arrive here encode inner 2 1, length: %d\n", m->len);
		m->enc = (uint8_t *) malloc(sizeof(uint8_t) * length);
		// printf("arrive here encode inner 2 2, length: %d\n", m->len);
		if (m->enc == NULL) {
			printf("7: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		m->enc[0] = op;
		m->enc[1] = 126;
		sz16 = htons(m->len);
		// printf("arrive here encode inner 2 3, length: %d\n", m->len);
		// printf("arrive here encode inner 2, sz16: %d\n", sz16);
		memcpy(m->enc + 2, &sz16, sizeof(uint16_t));
		memcpy(m->enc + 4, m->msg, m->len);
	} else {
		// printf("arrive here encode inner 3, length: %d\n", m->len);
		uint64_t sz64;
		length += 10;
		m->enc = (uint8_t *) malloc(sizeof(uint8_t) * length);
		if (m->enc == NULL) {
			printf("8: Couldn't allocate memory.\n\n");
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
		m->enc[0] = op;
		m->enc[1] = 127;
		sz64 = ntohl64(m->len);
		memcpy(m->enc + 2, &sz64, sizeof(uint64_t));
		memcpy(m->enc + 10, m->msg, m->len);
	}
	m->enc_len = length;

	// FILE *msgenc = fopen("msg_enc", "w");
	// for (int k = 0; k < m->enc_len; k++){
	// 	fprintf(msgenc, "%2X ", m->enc[k]);
	// }
	/**
	 * Hybi-00 message encoding
	 */
	// m->hybi00 = malloc(m->len+2);
	// if (m->hybi00 == NULL) {
	// 	printf("9: Couldn't allocate memory.\n\n");
	// 	fflush(stdout);
	// 	return CLOSE_UNEXPECTED;
	// }
	// memset(m->hybi00, '\0', m->len+2);
	// m->hybi00[0] = 0;
	// m->hybi00[m->len+1] = '\xFF';
	// memcpy(m->hybi00+1, m->msg, m->len);

	return CONTINUE;
}

ws_connection_close communicate(ws_client *n, char *next, uint64_t next_len, ws_json_request* request) {
	int buffer_length = 0;
	uint64_t buf_len;
	char buffer[BUFFERSIZE];
	ws_connection_close status;
	n->message = message_new();

	if (n == NULL) {
		printf("The client was not available anymore.");
		fflush(stdout);
		return CLOSE_PROTOCOL;	
	}

	if (n->headers == NULL) {
		printf("The header was not available anymore.");
		fflush(stdout);
		return CLOSE_PROTOCOL;	
	}

	/**
	 * If we are dealing with a Hypi-00 connection, we have to handle the
	 * message receiving differently than the RFC6455 standard.
	 **/	
	if ( n->headers->type == HYBI00 ) {
		// printf("header type is HYBI00 \n");
		memset(buffer, '\0', BUFFERSIZE);

		/**
		 * Receive new message.
		 */
		if ((buffer_length = recv(n->socket_id, buffer, BUFFERSIZE, 0)) <= 0) {
			printf("Didn't receive any message from client.\n\n");
			fflush(stdout);
			return CLOSE_PROTOCOL;
		}

		buf_len = buffer_length;

		/**
		 * If the first byte is equal to zero, the client wished to shut down.
		 * Else we keep on reading until the whole message is received.
		 */
		if (buffer[0] == '\xFF') {
			printf("Client:\n"
				  "\tSocket: %d\n"
				  "\tAddress: %s\n"
				  "reports that he is shutting down.\n\n", n->socket_id, 
				  (char *) n->client_ip);
			fflush(stdout);

			return CLOSE_NORMAL;	
		} else if (buffer[0] == '\x00') {
			/**
			 * Receive rest of the message.
			 */
			if ( (status = getWholeMessage(buffer+1, buf_len-1, n)) != 
					CONTINUE ) {
				return status; 
			}
			printf("header type is not HYBI00 \n");

			// Get rid of this function, there is no multicast in group		<==================================== Liyang
			/**	
			 * Encode the message to make it ready to be send to all others.
			 */
			// if ( (status = encodeMessage(n->message, 0)) != CONTINUE) {
			// 	return status;
			// }
		}
	} else if ( n->headers->type == HYBI07 || n->headers->type == RFC6455 
			|| n->headers->type == HYBI10 ) {
			// printf("header type is RFC6455 \n");
		/*
		 * Receiving and decoding the message.
		 */
		do {
			memset(buffer, '\0', BUFFERSIZE);
				
			memcpy(buffer, next, next_len);

			/**
			 * If we end in this case, we have not got enough of the frame to
			 * do something useful to it. Therefore, do yet another read 
			 * operation.
			 */
			// printf("%d\n", next_len);
			if (next_len <= 6 || ((next[1] & 0x7f) == 126 && next_len <= 8) ||
					((next[1] & 0x7f) == 127 && next_len <= 14)) {
				if ((buffer_length = recv(n->socket_id, (buffer+next_len), 
								(BUFFERSIZE-next_len), 0)) <= 0) {
					printf("Didn't receive any message from client.\n\n");
					fflush(stdout);
					return CLOSE_PROTOCOL;	
				}
			}
			// printf("%d\n", buffer_length);

			buf_len = (uint64_t)(buffer_length + next_len);

			/**
			 * We need the opcode to conclude which type of message we 
			 * received.
			 */

			if (n->message->opcode[0] == '\0') {
				memcpy(n->message->opcode, buffer, sizeof(n->message->opcode));
			}

			/**
			 * Get the full message and remove the masking from it.
			 */
			if ( (status = parseMessage(buffer, buf_len, n)) != CONTINUE) {
				printf("%d\n", status);
				return status;
			}

			next_len = 0;
			// if (!(buffer[0] & 0x80)){
			// 	printf("it is zero\n");
			// } else{
			// 	printf("it is one\n");
			// }
		} while( !(buffer[0] & 0x80) );	

		// printf("%s\n", n->message->opcode[0]);
		/**
		 * Checking which type of frame the client has sent.
		 */
		if (n->message->opcode[0] == '\x88' || n->message->opcode[0] == '\x08') {
			/**
			 * CLOSE: client wants to close connection, so we do.
			 **/
			printf("Client:\n"
				  "\tSocket: %d\n"
				  "\tAddress: %s\n"
				  "reports that he is shutting down.\n\n", n->socket_id, 
				  (char *) n->client_ip);
			fflush(stdout);
			
			return CLOSE_NORMAL;
		} else if (n->message->opcode[0] == '\x8A' || n->message->opcode[0] == '\x0A') {
			/**
			 * PONG: Client is still alive
			 **/
			printf("Pong arrived\n\n");
			fflush(stdout);	
			return CLOSE_TYPE;
		} else if (n->message->opcode[0] == '\x89' || n->message->opcode[0] == '\x09') {
			/** 
			 * PING: I am still alive
			 **/
			printf("Ping arrived\n\n");
			fflush(stdout);
			return CLOSE_TYPE;
		} else if (n->message->opcode[0] == '\x02' || n->message->opcode[0] == '\x82') {
			/** 
			 * BINARY: data. 
			 * TODO: find out what to do here!
			 **/
			
			fflush(stdout);
			return CLOSE_TYPE;
		} else if (n->message->opcode[0] == '\x01' || n->message->opcode[0] == '\x81') {
			/** 
			 * TEXT: encode the message to make it ready to be send to all 
			 * 		 others.
			 **/
			// if ( (status = encodeMessage(n->message, 0)) != CONTINUE) {
			// 	return status;
			// }

			// printf("Binary data arrived\n");
			// Parse JSON

			if ((status=parseRequest(n->message->msg, request)) <= REQUEST){
				// printf("Json is PARSED! %d\n", status);
				return status;
			}
			// Json parse failed, might be origin text
			return CONTINUE;
		} else {
			printf("Something very strange happened, received opcode: 0x%x\n\n", 
					n->message->opcode[0]);
			fflush(stdout);
			return CLOSE_UNEXPECTED;
		}
	}

	return CONTINUE;
}


// Parse the request in JSON
int parseRequest (uint8_t* message, ws_json_request* request)
{
	const cJSON *type = NULL;

	ws_connection_close status = CLOSE_JSON_UNKNOWN;
	// printf("%s\n", message);
    pthread_mutex_lock(&lock_reply);
	cJSON *monitor_recv = cJSON_Parse(message); 	//<=========================Error
    pthread_mutex_unlock(&lock_reply);

	if (monitor_recv == NULL)
	{
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL)
		{
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		status = CLOSE_JSON_FAIL;
		goto end;
	}

	type = cJSON_GetObjectItemCaseSensitive(monitor_recv, "type");
	strcpy(request->type, type->valuestring);
	if (cJSON_IsString(type) && (type->valuestring != NULL))
	{
		// printf("Checking monitor \"%s\"\n", request->type->valuestring);
		//Check the type of request!
		if (!strcmp(type->valuestring, "INIT")){
			status = INIT;
		} else if (!strcmp(type->valuestring,"SEG_REQ")){
			status = REQUEST;
			// printf("%s\n", request->type);
			const cJSON *seg_idx = NULL;
			const cJSON *br_idx = NULL;
			seg_idx = cJSON_GetObjectItemCaseSensitive(monitor_recv, "seg_idx");
			br_idx = cJSON_GetObjectItemCaseSensitive(monitor_recv, "br_idx");
			request->seg_idx = seg_idx->valuedouble;
			request->br_idx = br_idx->valuedouble;
			// printf("received seg_idx is: %d, and assign to req: %d\n", seg_idx->valuedouble, request->seg_idx);
		}
	}
	
end:
	cJSON_Delete(monitor_recv);
	return status;
}

// Reply the init request from client
ws_connection_close init_reply(ws_client* n){
	ws_connection_close status;

	char *string = new char [200];
    pthread_mutex_lock(&lock_reply);
	status = cJSON_reply(&string);
    pthread_mutex_unlock(&lock_reply);
	if (string == NULL) {
		raise(SIGINT);		
		return ENCODING_FAIL;
	}

	if(status != CONTINUE){
		return status;
	}

	ws_message *m = message_new();
	m->len = strlen(string);
		
	// char *temp = malloc( sizeof(char)*(m->len+1) );
	// if (temp == NULL) {
	// 	raise(SIGINT);		
	// 	return ENCODING_FAIL;
	// }
	// memset(temp, '\0', (m->len+1));
	// memcpy(temp, string, m->len);
	// m->msg = temp;
	m->msg = string;
	printf("in 00000000000000000000\n");
	if ( (status = encodeMessage(m, 1)) != CONTINUE) {
		message_free(m);
		free(m);
		raise(SIGINT);
		return ENCODING_FAIL;
	}

	ws_send(n, m);
	// temp = NULL;
	// free(temp);
	message_free(m);
	free(m);
	// delete[] string;	
	return CONTINUE;

}

// Establish JSON object for replying
ws_connection_close cJSON_reply(char** jstring)
{
	cJSON *type = NULL;
	cJSON *bitrates = NULL;
	cJSON *bitrate = NULL;
	cJSON *curr = NULL;
	int index = 0;

	cJSON *monitor = cJSON_CreateObject();

	if (monitor == NULL)
	{
		return JSON_CREATE_FAIL;
	}

	type = cJSON_CreateString("INIT REPLY");
	if (type == NULL)
	{
		return JSON_CREATE_STR_FAIL;
	}
	/* after creation was successful, immediately add it to the monitor,
	 * thereby transfering ownership of the pointer to it */
	cJSON_AddItemToObject(monitor, "type", type);

	bitrates = cJSON_CreateArray();
	if (bitrates == NULL)
	{
		return JSON_CREATE_ARR_FAIL;
	}
	cJSON_AddItemToObject(monitor, "bitrates", bitrates);

	for (index = 0; index < NUM_RATE; ++index)
	{
		bitrate = cJSON_CreateNumber(BITRATES[index]);
		if (bitrate == NULL)
		{
			return JSON_CREATE_NUM_FAIL;
		}
		cJSON_AddItemToArray(bitrates, bitrate);
	}

	// Always give the index of BITRATE[0]
	curr = cJSON_CreateNumber(seg_index_list[0]);
	if (curr == NULL)
	{
		return JSON_CREATE_NUM_FAIL;
	}
	cJSON_AddItemToObject(monitor, "curr_idx", curr);

	*jstring = cJSON_Print(monitor);
	if (jstring == NULL)
	{
		fprintf(stderr, "Failed to print monitor.\n");
		return JSON_EMPTY_STR;
	}
	// printf("%s\n", *string);  
	// Assign this string to n->message->msg
	// And send to client
	cJSON_Delete(monitor);

	return CONTINUE;

	// *string = "test";
}

// Triggered when a request is received, and reply corresponding seg/chunk
ws_connection_close seg_reply(ws_client* n, ws_json_request* request){
	// Check br idx
	printf("Enter seg reply, br_idx: %d, seg_idx: %d\n", request->br_idx, request->seg_idx);
	char filename[50] = {};
	int *requested_br_seg_idx;
	int *requested_br_seg_in_chunk;
	int *requested_br_seg_frameNum;
	// int *temp_saving_signal;
	int *requested_br_seg_in_ending_signal;
	int *requested_br_seg_in_ending_len;
	uint8_t *requested_seg;
	uint8_t *requested_seg_chunk;
	pthread_mutex_t* lock;

	int *requested_br_seg_chunk_offset;
	int *temp_chunk_his_offset;
	int *current_offset_ptr;

	requested_br_seg_idx = &seg_index_list[request->br_idx];
	requested_br_seg_in_chunk = &seg_in_chunk_list[request->br_idx];
	requested_br_seg_frameNum = &frame_num_list[request->br_idx];
	requested_br_seg_in_ending_signal = &seg_in_ending_signal_list[request->br_idx];
	requested_br_seg_in_ending_len = &seg_in_ending_len_list[request->br_idx];
	// temp_saving_signal = &saving_signal_list[request->br_idx];
	requested_seg = seg_temp_list[request->br_idx];
	requested_seg_chunk = seg_chunk_list[request->br_idx];
	lock = &lock_list[request->br_idx];

	// Get offset

	current_offset_ptr = &curr_offset_ptrs[request->br_idx];
	requested_br_seg_chunk_offset = chunk_byte_offset_list[request->br_idx];		//Modified
	temp_chunk_his_offset = seg_chunk_new_offsets[request->br_idx];	// To be modified

	for(int ccc = 0; ccc < 25; ++ccc) {
					//printf("%d\t", seg_chunk_new_offsets[i][ccc]);
					printf("%d\t", seg_chunk_new_offsets[request->br_idx][ccc]);
				}
				printf("\n");


	// To be modified: request previous segment,
	// Check offset history and deliver in chunks
	// printf("assignment done\n");
	if (request->seg_idx < *requested_br_seg_idx) {
		printf("%d  -  %d  = to multiple %d\n",*requested_br_seg_idx, request->seg_idx, (*requested_br_seg_idx-request->seg_idx-1)*SEG_FRAMENUM/CHUNK_FRAMENUM);

		// Get seg from hd
		printf("Request for previous segment!\n");
		// printf("%d and %d\n", request->seg_idx, request->br_idx);
		sprintf(filename, "./segs/seg%d_br%d.m4s", request->seg_idx, request->br_idx);
		FILE *segInput;
		if(!(segInput = fopen(filename, "rb"))){
			printf("open failed\n");
		}
		 // For send raw data!
		fseek(segInput, 0, SEEK_END);
		int fsize = ftell(segInput);
		fseek(segInput, 0, SEEK_SET); 

		// Above 
		uint8_t *seg_data = (uint8_t *)malloc(sizeof(uint8_t)* (fsize + 1));
		fread(seg_data, 1, fsize, segInput);
		fclose(segInput);

		// printf("%d\n", seg_data[0]);
		// printf("%d\n", seg_data[1]);
		// printf("%d\n", seg_data[2]);
		// printf("Read from HD done!\n");
		// Get offsets for the segs
		int *seg_offsets = new int[SEG_FRAMENUM/CHUNK_FRAMENUM];
		memcpy(seg_offsets, temp_chunk_his_offset + (*requested_br_seg_idx-request->seg_idx-1)*SEG_FRAMENUM/CHUNK_FRAMENUM, sizeof(int)*SEG_FRAMENUM/CHUNK_FRAMENUM);
		// printf("Get offset history done!\n");
		int pre_offset = 0;

		//for (int chunk_idx=0; chunk_idx<SEG_FRAMENUM/CHUNK_FRAMENUM; ++chunk_idx) {
		//	printf("chunk idx : %d\t seg_offsets : %d\n", chunk_idx, seg_offsets[chunk_idx]);
		//}

		for (int chunk_idx=0; chunk_idx<SEG_FRAMENUM/CHUNK_FRAMENUM; ++chunk_idx) {
			// Send chunk according to the offset
			// printf("Offset is %d\n", seg_offsets[chunk_idx]);
			printf("data size : %d \t pre_offset : %d  \tseg_offsets : %d\n", (seg_offsets[chunk_idx] - pre_offset + HEADER_LEN + 1), pre_offset, seg_offsets[chunk_idx]);

			uint8_t *chunk_data;


			if((seg_offsets[chunk_idx] - pre_offset + HEADER_LEN + 1) > 0) {
				chunk_data = (uint8_t *)malloc(sizeof(uint8_t)* (seg_offsets[chunk_idx] - pre_offset + HEADER_LEN + 1));
				printf("chunk_start memcpy\n");
				memcpy(chunk_data + HEADER_LEN, seg_data + pre_offset, seg_offsets[chunk_idx] - pre_offset);
				printf("end memcpy\n");
			} else {
				printf("error\n");
				chunk_data = (uint8_t *)malloc(sizeof(uint8_t)* (HEADER_LEN + 1));
				printf("error end!!!!!!!!!!!!\n");
			}
			
			// establish header
			uint8_t *seg_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
			estab_header(seg_header, 2, request->br_idx, request->seg_idx, chunk_idx, 1);	//type 2: 
			memcpy(chunk_data, seg_header, HEADER_LEN);

			ws_message *m = message_new();
			

			if(seg_offsets[chunk_idx] - pre_offset + HEADER_LEN > 0)
				m->len = seg_offsets[chunk_idx] - pre_offset + HEADER_LEN;
			else
				m->len = HEADER_LEN;

			m->msg = chunk_data;

			if ( encodeMessage(m, 0) != CONTINUE) {

				message_free(m);
				free(m);
				raise(SIGINT);
				return ENCODING_FAIL;
			}

			// before send out data, insert a PING TYPE packet
			ws_message *ping = message_new();
			ping->len = HEADER_LEN;
			uint8_t *ping_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
			estab_header(ping_header, 4, request->br_idx, request->seg_idx, chunk_idx, 0);	//type 4: ping header
			ping->msg = ping_header;
			if ( encodeMessage(ping, 0) != CONTINUE) {
				message_free(ping);
				free(ping);
				raise(SIGINT);
				return ENCODING_FAIL;
			}
			ws_send(n, ping);		// continuously send out two packets
			ws_send(n, m);

			free(seg_header);
			message_free(ping);
			message_free(m);
			free(ping);
			free(m);
			pre_offset = seg_offsets[chunk_idx];
		}
		///////////////////////////////////////////////////////////////////////////////////////////////
		/// Deliver entire segment
		//  // For send raw data!
		// fseek(segInput, 0, SEEK_END);
		// int fsize = ftell(segInput);
		// fseek(segInput, 0, SEEK_SET); 
		// // Above 
		// uint8_t *seg_data = (uint8_t *)malloc(sizeof(uint8_t)* (fsize + HEADER_LEN + 1));
		// fread(seg_data + HEADER_LEN, 1, fsize, segInput);
		// fclose(segInput);

		// uint8_t *seg_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
		// estab_header(seg_header, 1, request->br_idx, request->seg_idx, 0, 0);	//type 1: entire seg
		// memcpy(seg_data, seg_header, HEADER_LEN);

		// ws_message *m = message_new();
		// m->len = fsize + HEADER_LEN;
		// m->msg = seg_data;

		// if ( encodeMessage(m, 0) != CONTINUE) {
		// 	message_free(m);
		// 	free(m);
		// 	raise(SIGINT);
		// 	return ENCODING_FAIL;
		// }

		// // before send out data, insert a PING TYPE packet
		// ws_message *ping = message_new();
		// ping->len = HEADER_LEN;
		// uint8_t *ping_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
		// estab_header(ping_header, 4, request->br_idx, request->seg_idx, 0, 0);	//type 4: ping header
		// ping->msg = ping_header;
		// if ( encodeMessage(ping, 0) != CONTINUE) {
		// 	message_free(ping);
		// 	free(ping);
		// 	raise(SIGINT);
		// 	return ENCODING_FAIL;
		// }
		// ws_send(n, ping);		// continuously send out two packets
		// ws_send(n, m);

		// free(seg_header);
		// message_free(ping);
		// message_free(m);
		// free(ping);
		// free(m);
		///////////////////////////////////////////////////////////////////////////////////////////////
		return CONTINUE;
	} 
	// To be modified, if request current seg, and several chunks are ready,
	// Delivery them in chunks, not in multiple chunks together
	else if (request->seg_idx == *requested_br_seg_idx) { 
		// Enter chunk delivery mode
		// printf("Request for current segment!\n");
		*requested_br_seg_in_chunk = 1;
		int ending_chunk = 0;
		int pre_offset = 0;
		int enter_chunk_ptr = *current_offset_ptr;
		int *p = (int*)malloc(sizeof(int) * enter_chunk_ptr);
		memcpy(p, chunk_byte_offset_list[request -> br_idx], sizeof(int) * enter_chunk_ptr);
		
 		printf("chunk offset :\t");
		for(int i =0; i < enter_chunk_ptr;++i) {
			printf("%d\t%d\t", p[i], chunk_byte_offset_list[request -> br_idx][i]);
		}
		printf("\n");
		printf("enter_chunk_ptr is %d\n", enter_chunk_ptr);
		// There is encoded chunks 
		if (*current_offset_ptr > 0) {
			for (int chunk_idx = 0; chunk_idx < enter_chunk_ptr; ++chunk_idx) { // modift by siquan  it was < *current_offset_ptr

				printf("sent in 1111111111  and now pointer is %d\n", chunk_idx);
			printf("data size : %d \t pre_offset : %d  \t requested_br_seg_chunk_offset : %d\n", p[chunk_idx] - pre_offset + HEADER_LEN, pre_offset, p[chunk_idx]);
				

				uint8_t *chunk_data = (uint8_t *)malloc(sizeof(uint8_t)* (p[chunk_idx] - pre_offset + HEADER_LEN + 1));
				memcpy(chunk_data + HEADER_LEN, requested_seg + pre_offset, p[chunk_idx] - pre_offset);

				// establish header
				uint8_t *seg_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
				estab_header(seg_header, 2, request->br_idx, request->seg_idx, chunk_idx, 1);	//type 2: 
				memcpy(chunk_data, seg_header, HEADER_LEN);

				ws_message *m = message_new();
				m->len = p[chunk_idx] - pre_offset + HEADER_LEN;
				m->msg = chunk_data;

				if ( encodeMessage(m, 0) != CONTINUE) {
					message_free(m);
					free(m);
					raise(SIGINT);
					return ENCODING_FAIL;
				}

				// before send out data, insert a PING TYPE packet
				ws_message *ping = message_new();
				ping->len = HEADER_LEN;
				uint8_t *ping_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
				estab_header(ping_header, 4, request->br_idx, request->seg_idx, chunk_idx, 0);	//type 4: ping header
				ping->msg = ping_header;
				if ( encodeMessage(ping, 0) != CONTINUE) {
					message_free(ping);
					free(ping);
					raise(SIGINT);
					return ENCODING_FAIL;
				}
				ws_send(n, ping);		// continuously send out two packets
				ws_send(n, m);

				free(seg_header);
				message_free(ping);
				message_free(m);
				free(ping);
				free(m);
				pre_offset = p[chunk_idx];
			}
		}
		free(p);

		/*///////////////////////////////////////////////////////////////////////////////////////////////////
		// int pre_offset = 0;
		// int pre_chunk = 0;
		// int ending_chunk = 0;
		// if (*requested_br_seg_chunk_offset > 0){
		// 	// printf("There is encoded chunks!\n");
		// 	// There is encoded chunk ready for delivery
		// 	pre_offset = *requested_br_seg_chunk_offset;
		// 	ws_message *m = message_new();
		// 	// printf("multiple chunk length is: %d\n", *requested_br_seg_chunk_offset);
		// 	m->len = *requested_br_seg_chunk_offset + HEADER_LEN;
		// 	uint8_t *seg_data = (uint8_t *)malloc(sizeof(uint8_t)* (m->len + 1));
		// 	pthread_mutex_lock(lock);
		// 	memcpy(seg_data + HEADER_LEN, requested_seg, *requested_br_seg_chunk_offset);
		// 	pthread_mutex_unlock(lock);

		// 	uint8_t *seg_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
		// 	estab_header(seg_header, 2, request->br_idx, request->seg_idx, pre_chunk, (*requested_br_seg_frameNum+1)/CHUNK_FRAMENUM);
		// 	memcpy(seg_data, seg_header, HEADER_LEN);
		// 	pre_chunk = (*requested_br_seg_frameNum+1)/CHUNK_FRAMENUM;
		// 	m->msg = seg_data;
		// 	if ( encodeMessage(m, 0) != CONTINUE) {
		// 		message_free(m);
		// 		free(m);
		// 		raise(SIGINT);
		// 		return ENCODING_FAIL;
		// 	}

		// 	// Send ping first 
		// 	ws_message *ping = message_new();
		// 	ping->len = HEADER_LEN;
		// 	uint8_t *ping_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
		// 	estab_header(ping_header, 4, request->br_idx, request->seg_idx, 0, 0);	//type 4: ping header
		// 	ping->msg = ping_header;
		// 	if ( encodeMessage(ping, 0) != CONTINUE) {
		// 		message_free(ping);
		// 		free(ping);
		// 		raise(SIGINT);
		// 		return ENCODING_FAIL;
		// 	}
		// 	ws_send(n, ping);		// continuously send out two packets

		// 	// Then send out data message
		// 	ws_send(n, m);
		// 	// temp = NULL;
		// 	free(seg_header);
		// 	message_free(m);
		// 	free(m);
		// }
		///////////////////////////////////////////////////////////////////////////////////////////////////
		// printf("pre offset is: %d\n", pre_offset);
		// printf("current frame number is: %d\n", *requested_br_seg_frameNum);
		// printf("Catch current encoding!\n");*/
		while (1){
			// printf("Wait chunk ready, current frame: %d\n", *requested_br_seg_frameNum);
			// if ((*requested_br_seg_frameNum+1)%CHUNK_FRAMENUM == 0 && *requested_br_seg_chunk_offset > pre_offset){

			if ( (*requested_br_seg_idx == request->seg_idx && *current_offset_ptr > enter_chunk_ptr && *current_offset_ptr < SEG_FRAMENUM/CHUNK_FRAMENUM) ||
				(*requested_br_seg_in_ending_signal && *requested_br_seg_idx > request->seg_idx)){
				printf("frame number is: %d\n", *requested_br_seg_frameNum);
				ws_message *m = message_new();
				uint8_t *seg_data;
				uint8_t *seg_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
				pthread_mutex_lock(lock);
				// To be modified, insert for loop and implement pro chunk mode

				// printf("Encoding seg: %d\n", *requested_br_seg_idx);
				// printf("Seg ending signal: %d\n", *requested_br_seg_in_ending_signal);
				if (*requested_br_seg_in_ending_signal && *requested_br_seg_idx > request->seg_idx) {
					// printf("Segment ends!\n");
					printf("sent in 222222222\n");
			//printf("data size : %d \t pre_offset : %d  \t requested_br_seg_chunk_offset : %d\n", requested_br_seg_chunk_offset[enter_chunk_ptr] - pre_offset + HEADER_LEN, pre_offset, requested_br_seg_chunk_offset[enter_chunk_ptr]);

					
					m->len = *requested_br_seg_in_ending_len - pre_offset + HEADER_LEN;

					printf("data len is : %d\n", m -> len);
					printf("now is %d and len is %d\n", enter_chunk_ptr, SEG_FRAMENUM/CHUNK_FRAMENUM - enter_chunk_ptr);
					seg_data = (uint8_t *)malloc(sizeof(uint8_t)* (m->len + 1));
					// printf("enter final chunk, len is %d \n", m->len);
					memcpy(seg_data + HEADER_LEN, requested_seg_chunk + pre_offset, m->len - HEADER_LEN);
					memset(requested_seg_chunk, 0, *requested_br_seg_in_ending_len);
					
					estab_header(seg_header, 2, request->br_idx, request->seg_idx, enter_chunk_ptr, SEG_FRAMENUM/CHUNK_FRAMENUM - enter_chunk_ptr);
					// printf("after estab_header\n");
					memcpy(seg_data, seg_header, HEADER_LEN);
					*requested_br_seg_in_ending_signal = 0;
					ending_chunk = 1;
					// printf("%d %d %d %d %d %d %d %d\n", seg_data[4], seg_data[5], seg_data[6], seg_data[7], seg_data[8], seg_data[9], seg_data[10], seg_data[11]);
				} else {
				// To be modified, insert for loop and implement pro chunk mode
					// printf("THere is another chunk ready!\n");
					// printf("Data length is: %d\n", requested_br_seg_chunk_offset[enter_chunk_ptr] - pre_offset + HEADER_LEN);
					printf("sent in 33333333333\n");
			printf("data size : %d \t pre_offset : %d  \t requested_br_seg_chunk_offset : %d\n", requested_br_seg_chunk_offset[enter_chunk_ptr] - pre_offset + HEADER_LEN, pre_offset, requested_br_seg_chunk_offset[enter_chunk_ptr]);

					m->len = requested_br_seg_chunk_offset[enter_chunk_ptr] - pre_offset + HEADER_LEN;
					
					seg_data = (uint8_t *)malloc(sizeof(uint8_t)* (m->len + 1));
					
					memcpy(seg_data + HEADER_LEN, requested_seg + pre_offset, m->len - HEADER_LEN);

					estab_header(seg_header, 2, request->br_idx, request->seg_idx, enter_chunk_ptr, *current_offset_ptr - enter_chunk_ptr);
					// printf("after estab_header\n");
					memcpy(seg_data, seg_header, HEADER_LEN);
					pre_offset = requested_br_seg_chunk_offset[enter_chunk_ptr];
					enter_chunk_ptr = *current_offset_ptr;
				}
				pthread_mutex_unlock(lock);
				// printf("single chunk length is: %d\n", m->len);
				// printf("copy header to seg data\n");
				m->msg = seg_data;
				if ( encodeMessage(m, 0) != CONTINUE) {
					message_free(m);
					free(m);
					raise(SIGINT);
					return ENCODING_FAIL;
				}
				// printf("before sending\n");
				ws_message *ping = message_new();
				ping->len = HEADER_LEN;
				uint8_t *ping_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
				estab_header(ping_header, 4, request->br_idx, request->seg_idx, *current_offset_ptr - 1, 0);	//type 4: ping header
				ping->msg = ping_header;
				if ( encodeMessage(ping, 0) != CONTINUE) {
					message_free(ping);
					free(ping);
					raise(SIGINT);
					return ENCODING_FAIL;
				}
				ws_send(n, ping);		// continuously send out two packets
				//Then send out data
				ws_send(n, m);
				// temp = NULL;
				free(seg_header);
				message_free(m);
				free(m);
				// printf("one chunk is send out, current offset is %d, and frame num is: %d\n", pre_offset, *requested_br_seg_frameNum);
				// check whether it reach the final frame in a segment
				// printf("IN chunk mode, the frame number is %d\n", *requested_br_seg_frameNum);
				if (ending_chunk) {
					*requested_br_seg_in_chunk = 0;
					*requested_br_seg_in_ending_signal = 0;
					*requested_br_seg_in_ending_len = 0;
					// memset(requested_seg_chunk, 0, *requested_br_seg_in_ending_len);
					// printf("will break\n");
					break;
				}
			}
		} 
		return CONTINUE;
	} else {
		int pre_offset = 0;
		int enter_chunk_ptr = 0;
		int ending_chunk = 0;
		// Wait and send out chunk
		printf("Should send request slower to wait server!!\n");
		printf("request seg idx: %d and current seg idx: %d\n", request->seg_idx, *requested_br_seg_idx);
		while (1){
			if ((request->seg_idx == *requested_br_seg_idx && *current_offset_ptr > enter_chunk_ptr && *current_offset_ptr < SEG_FRAMENUM/CHUNK_FRAMENUM) ||
				(*requested_br_seg_in_ending_signal && *requested_br_seg_idx > request->seg_idx)){
				if (*requested_br_seg_in_chunk == 0)
					*requested_br_seg_in_chunk = 1;
				ws_message *m = message_new();
				uint8_t *seg_data;
				uint8_t *seg_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
				pthread_mutex_lock(lock);
				if (*requested_br_seg_in_ending_signal && *requested_br_seg_idx > request->seg_idx) {
					// Rarely happen
					// To be modified, insert for loop and implement pro chunk mode
					// assert(0 == 1);
					m->len = *requested_br_seg_in_ending_len - pre_offset + HEADER_LEN;
					seg_data = (uint8_t *)malloc(sizeof(uint8_t)* (m->len + 1));
					memcpy(seg_data + HEADER_LEN, requested_seg_chunk + pre_offset, m->len - HEADER_LEN);
					printf("sent in 55555555555\n");

					estab_header(seg_header, 2, request->br_idx, request->seg_idx, enter_chunk_ptr, SEG_FRAMENUM/CHUNK_FRAMENUM - enter_chunk_ptr);
					*requested_br_seg_in_ending_signal = 0;
					ending_chunk = 1;
				} else {
					// To be modified, insert for loop and implement pro chunk mode
					printf("sent in 666666666666\n");

					m->len = requested_br_seg_chunk_offset[enter_chunk_ptr] - pre_offset + HEADER_LEN;
					seg_data = (uint8_t *)malloc(sizeof(uint8_t)* (m->len + 1));
					memcpy(seg_data + HEADER_LEN, requested_seg + pre_offset, m->len - HEADER_LEN);
					estab_header(seg_header, 2, request->br_idx, request->seg_idx, enter_chunk_ptr, *current_offset_ptr - enter_chunk_ptr);
					pre_offset = requested_br_seg_chunk_offset[enter_chunk_ptr];
					enter_chunk_ptr = *current_offset_ptr;
				}
				memcpy(seg_data, seg_header, HEADER_LEN);
				pthread_mutex_unlock(lock);

				m->msg = seg_data;
				if ( encodeMessage(m, 0) != CONTINUE) {
					message_free(m);
					free(m);
					raise(SIGINT);
					return ENCODING_FAIL;
				}
				// printf("before sending\n");
				ws_message *ping = message_new();
				ping->len = HEADER_LEN;
				uint8_t *ping_header = (uint8_t *)malloc(sizeof(uint8_t)*HEADER_LEN);
				estab_header(ping_header, 4, request->br_idx, request->seg_idx, *current_offset_ptr - 1, 0);	//type 4: ping header
				ping->msg = ping_header;
				if ( encodeMessage(ping, 0) != CONTINUE) {
					message_free(ping);
					free(ping);
					raise(SIGINT);
					return ENCODING_FAIL;
				}
				ws_send(n, ping);		// continuously send out two packets
				//Then send out data;
				ws_send(n, m);
				// temp = NULL;
				free(seg_header);
				message_free(m);
				free(m);
				pre_offset = *requested_br_seg_chunk_offset;
				// printf("one chunk is send out, current offset is %d, and frame num is: %d\n", pre_offset, *requested_br_seg_frameNum);
				// check whether it reach the final frame in a segment
				// printf("IN chunk mode, the frame number is %d\n", *requested_br_seg_frameNum);
				if (ending_chunk) {
					*requested_br_seg_in_chunk = 0;
					*requested_br_seg_in_ending_signal = 0;
					*requested_br_seg_in_ending_len = 0;
					// memset(requested_seg_chunk, 0, *requested_br_seg_in_ending_len);
					// printf("will break\n");
					break;
				}
			}
		} 
		return CONTINUE;
	}

}


void estab_header(uint8_t *seg_header, uint8_t type, uint8_t br_idx, uint16_t seg_idx, uint8_t chunk_start, uint8_t chunk_num){
	 //printf("Before esatblish header\n");
	// printf("seg idx: %d\n", seg_idx);
	// printf("chunk_start: %d\n", chunk_start);
	// printf("chunk num: %d\n", chunk_num);

	// For type
	seg_header[0] = (type << 4);

	// For bitrate
	seg_header[0] = seg_header[0] | br_idx;


	// printf("Type and bitrate %d\n", seg_header[0]);
	// For seg_idx

	memcpy(seg_header + 1, &seg_idx, 2);
	// printf("Segment index: %d\n", seg_header[1]);

	// For chunk start and chunk num
	if (type == 1) {
		seg_header[3] = 0xFF;
	} else if (type == 2) {
		// Mulitple chunks
		seg_header[3] = chunk_start << 4;
		seg_header[3] += chunk_num;
	} else if (type == 3) {
		// Single chunks
		seg_header[3] = (chunk_start << 4) + 1;
	} else if (type == 4) {
		printf("%d\n", chunk_start);
		if (chunk_start == 255)
			chunk_start = 5 - chunk_num;
		seg_header[3] = (chunk_start << 4);
	}
	// printf("chunk start and num: %d\n", seg_header[3]);

	return seg_header;

}

ws_connection_close estab_seg_json(int seg_idx, int br_idx, int data_size, char **jstring){

	cJSON *type = NULL;
	cJSON *json_seg_idx = NULL;
	cJSON *json_br_idx = NULL;
	cJSON *json_data_len = NULL;

	cJSON *monitor = cJSON_CreateObject();

	if (monitor == NULL)
	{
		return JSON_CREATE_FAIL;
	}

	type = cJSON_CreateString("SEG REPLY");
	if (type == NULL)
	{
		return JSON_CREATE_STR_FAIL;
	}
	cJSON_AddItemToObject(monitor, "type", type);

	json_seg_idx = cJSON_CreateNumber(seg_idx);
	if (json_seg_idx == NULL)
	{
		return JSON_CREATE_NUM_FAIL;
	}
	cJSON_AddItemToObject(monitor, "seg_idx", json_seg_idx);

	json_br_idx = cJSON_CreateNumber(br_idx);
	if (json_br_idx == NULL)
	{
		return JSON_CREATE_NUM_FAIL;
	}
	cJSON_AddItemToObject(monitor, "br_idx", json_br_idx);



	json_data_len = cJSON_CreateNumber(data_size);
	if (json_data_len == NULL)
	{
		return JSON_CREATE_NUM_FAIL;
	}
	cJSON_AddItemToObject(monitor, "data_length", json_data_len);

	// json_data = cJSON_CreateString(seg_data);
	// if (json_data == NULL)
	// {
	// 	return JSON_CREATE_STR_FAIL;
	// }
	// cJSON_AddItemToObject(monitor, "data", json_data);


	// json_data = cJSON_CreateArray();
	// if (json_data == NULL)
	// {
	// 	return JSON_CREATE_ARR_FAIL;
	// }
	// cJSON_AddItemToObject(monitor, "data", json_data);

	// for (int index = 0; index < data_size; ++index)
	// {
	// 	printf("arrive here 111\n");
	// 	temp_jdata = cJSON_CreateNumber(seg_data[index]);
	// 	if (temp_jdata == NULL)
	// 	{
	// 		return JSON_CREATE_NUM_FAIL;
	// 	}
	// 	cJSON_AddItemToArray(json_data, temp_jdata);
	// }

	*jstring = cJSON_Print(monitor);
	if (jstring == NULL)
	{
		fprintf(stderr, "Failed to print monitor.\n");
		return JSON_EMPTY_STR;
	}
	// printf("%s\n", *string);  
	// Assign this string to n->message->msg
	// And send to client
	cJSON_Delete(monitor);

	return CONTINUE;

	// *string = "test";

}