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

#include "Websocket.h"

// #include "cJSON.h"
// #include "SegSaver.c"

// uint8_t **seg_temp_list = new uint8_t[NUM_RATE][MAX_CHUNK_LEN];
uint8_t **seg_temp_list = NULL;

uint8_t **seg_chunk_list = NULL;

int *seg_in_ending_signal_list = new int[NUM_RATE];

int *seg_in_ending_len_list = new int[NUM_RATE];

// int *seg_chunk_len_list = new int[NUM_RATE];

int *seg_len_list = new int[NUM_RATE];

int *saving_signal_list = new int[NUM_RATE];

int *seg_index_list = new int[NUM_RATE];

int *clear_cache_list = new int[NUM_RATE];

int *seg_in_chunk_list = new int[NUM_RATE];

int *frame_num_list = new int[NUM_RATE];

int *curr_offset_ptrs = new int[NUM_RATE];
// Pro chunk mode
int **seg_chunk_new_offsets = NULL;
int **chunk_byte_offset_list = NULL;

int video_fps = FPS;
// struct timeval g_tv;
const int BITRATES[NUM_RATE] = {1000, 2000, 3000, 4000};
//const int BITRATES[NUM_RATE] = {300, 500, 1000, 2000, 3000, 6000};

// uint8_t* seg_temp_3 = new uint8_t[MAX_CHUNK_LEN];	// Current chunk mem
ws_list *l;
int port;
/**
 * Handler to call when CTRL+C is typed. This function shuts down the server
 * in a safe way.
 */

uint8_t ** new_2d_array(int len_1, int len_2) {
	uint8_t **seg_temp_list = new uint8_t *[len_1];
	for (int i = 0; i < len_1; i++)
	    seg_temp_list[i] = new uint8_t[len_2];
	return seg_temp_list;
}

int ** new_int_2d_array(int len_1, int len_2) {
	int **seg_temp_list = new int *[len_1];
	for (int i = 0; i < len_1; i++)
	    seg_temp_list[i] = new int[len_2];
	return seg_temp_list;
}

void sigint_handler(int sig) {
	if (sig == SIGINT || sig == SIGSEGV) {
		if (l != NULL) {
			list_free(l);
			l = NULL;
		}
		(void) signal(sig, SIG_DFL);
		exit(0);
	} else if (sig == SIGPIPE) {
		(void) signal(sig, SIG_IGN);
	}
}

/**
 * Shuts down a client in a safe way. This is only used for Hybi-00.
 */
void cleanup_client(void *args) {
	ws_client *n = args;
	if (n != NULL) {
		printf("Shutting client down..\n\n> ");
		fflush(stdout);
		list_remove(l, n);
	}
}


void *handleClient(void *args) {
	pthread_detach(pthread_self());
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
	pthread_cleanup_push(&cleanup_client, args);

	int buffer_length = 0, string_length = 1, reads = 1;

	ws_client *n = args;
	ws_json_request* request = (ws_json_request*)malloc(sizeof(ws_json_request));
	request->type = (char *)malloc(sizeof(char)*10);
	ws_connection_close status;
	// cJSON *monitor_recv;
	n->thread_id = pthread_self();

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	char buffer[BUFFERSIZE];
	n->string = (char *) malloc(sizeof(char));

	if (n->string == NULL) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		handshake_error("Couldn't allocate memory.", ERROR_INTERNAL, n);
		pthread_exit((void *) EXIT_FAILURE);
	}

	printf("Client connected with the following information:\n"
		   "\tSocket: %d\n"
		   "\tAddress: %s\n\n", n->socket_id, (char *) n->client_ip);
	printf("Checking whether client is valid ...\n\n");
	fflush(stdout);

	/**
	 * Getting headers and doing reallocation if headers is bigger than our
	 * allocated memory.
	 */
	do {
		memset(buffer, '\0', BUFFERSIZE);
		if ((buffer_length = recv(n->socket_id, buffer, BUFFERSIZE, 0)) <= 0){
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			handshake_error("Didn't receive any headers from the client.", 
					ERROR_BAD, n);
			pthread_exit((void *) EXIT_FAILURE);
		}

		if (reads == 1 && strlen(buffer) < 14) {
			handshake_error("SSL request is not supported yet.", 
					ERROR_NOT_IMPL, n);
			pthread_exit((void *) EXIT_FAILURE);
		}

		string_length += buffer_length;

		char *tmp = realloc(n->string, string_length);
		if (tmp == NULL) {
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			handshake_error("Couldn't reallocate memory.", ERROR_INTERNAL, n);
			pthread_exit((void *) EXIT_FAILURE);
		}
		n->string = tmp;
		tmp = NULL;

		memset(n->string + (string_length-buffer_length-1), '\0', 
				buffer_length+1);
		memcpy(n->string + (string_length-buffer_length-1), buffer, 
				buffer_length);
		reads++;
	} while( strncmp("\r\n\r\n", n->string + (string_length-5), 4) != 0 
			&& strncmp("\n\n", n->string + (string_length-3), 2) != 0
			&& strncmp("\r\n\r\n", n->string + (string_length-8-5), 4) != 0
			&& strncmp("\n\n", n->string + (string_length-8-3), 2) != 0 );
	
	printf("User connected with the following headers:\n%s\n\n", n->string);
	fflush(stdout);

	ws_header *h = header_new();

	if (h == NULL) {
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		handshake_error("Couldn't allocate memory.", ERROR_INTERNAL, n);
		pthread_exit((void *) EXIT_FAILURE);
	}

	n->headers = h;

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if ( parseHeaders(n->string, n, port) < 0 ) {
		pthread_exit((void *) EXIT_FAILURE);
	}

	if ( sendHandshake(n) < 0 && n->headers->type != UNKNOWN ) {
		pthread_exit((void *) EXIT_FAILURE);	
	}	

	list_add(l, n);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	// printf("Client has been validated and is now connected\n\n");
	// printf("> ");
	// fflush(stdout);

	uint64_t next_len = 0;
	char next[BUFFERSIZE];
	memset(next, '\0', BUFFERSIZE);

	// serverEncoder(l, n);	// For single thread testing

	while (1) {
		status = communicate(n, next, next_len, request); 
		if ( status >= CLOSE_NORMAL) {
			// Check the type of op/type of received websocket mesage
			// Normall, it is binary/ping/pong/text

			printf("communicate Error!\n");
			break;
		} else if (status >= INIT){
			// Receive init/request from client;
			switch (status){
				case INIT:
					// Process initial request from client
					// printf("It is %s\n", request->type);
					status = init_reply(n);
					break;

				case REQUEST:
					// printf("It is %s\n", request->type);
					// How replay corresponding seg/chunk
					// printf("%d\n", request->seg_idx);
					status = seg_reply(n, request);
					break;

				default:
					printf("communicate Error!\n");
					break;

			}

		}		
		// pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		// if (n->headers->protocol == CHAT) {
		// 	list_multicast(l, n);
		// } else if (n->headers->protocol == ECHO) {
		// 	list_multicast_one(l, n, n->message);
		// } else {
		// 	list_multicast_one(l, n, n->message);
		// }
		// pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
		//

		if (n->message != NULL) {
			memset(next, '\0', BUFFERSIZE);
			memcpy(next, n->message->next, n->message->next_len);
			next_len = n->message->next_len;
			message_free(n->message);
			free(n->message);
			n->message = NULL;	
		}
		// printf("In loop!\n");
	}
	
	printf("Shutting client down..\n\n");
	printf("> ");
	fflush(stdout);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	list_remove(l, n);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	// cJSON_Delete(monitor_send);
	pthread_cleanup_pop(0);
	pthread_exit((void *) EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

	int server_socket, client_socket, on = 1;
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_length;
	pthread_t pthread_id;
	pthread_attr_t pthread_attr;

	/**
	 * Creating new lists, l is supposed to contain the connected users.
	 */
	l = list_new();

	/**
	 * Listens for CTRL-C and Segmentation faults.
	 */ 
	(void) signal(SIGINT, &sigint_handler);
	(void) signal(SIGSEGV, &sigint_handler);
	(void) signal(SIGPIPE, &sigint_handler);

	printf("Server: \t\tStarted\n");
	fflush(stdout);

	/**
	 * Assigning port value.
	 */
	if (argc == 2) {
		port = strtol(argv[1], (char **) NULL, 10);
		
		if (port <= 1024 || port >= 65565) {
			port = PORT;
		}

	} else {
		port = PORT;	
	}

	printf("Port: \t\t\t%d\n", port);
	fflush(stdout);

	// const char filepath[256] = "test.mp4";
	/**
	 * Opening server socket.
	 */
	if ( (server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
		server_error(strerror(errno), server_socket, l);
	}

	printf("Socket: \t\tInitialized\n");
	fflush(stdout);

	/**
	 * Allow reuse of address, when the server shuts down.
	 */
	if ( (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, 
					sizeof(on))) < 0 ){
		server_error(strerror(errno), server_socket, l);
	}

	printf("Reuse Port %d: \tEnabled\n", port);
	fflush(stdout);

	memset((char *) &server_addr, '\0', sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	printf("Ip Address: \t\t%s\n", inet_ntoa(server_addr.sin_addr));
	fflush(stdout);

	/**
	 * Bind address.
	 */
	if ( (bind(server_socket, (struct sockaddr *) &server_addr, 
			sizeof(server_addr))) < 0 ) {
		server_error(strerror(errno), server_socket, l);
	}

	printf("Binding: \t\tSuccess\n");
	fflush(stdout);

	/**
	 * Listen on the server socket for connections
	 */
	if ( (listen(server_socket, 10)) < 0) {
		server_error(strerror(errno), server_socket, l);
	}

	printf("Listen: \t\tSuccess\n\n");
	fflush(stdout);

	/**
	 * Attributes for the threads we will create when a new client connects.
	 */
	pthread_attr_init(&pthread_attr);
	pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED);
	pthread_attr_setstacksize(&pthread_attr, 524288);

	printf("Server is now waiting for clients to connect ...\n\n");
	fflush(stdout);


	// /**
	//  * Create commandline, such that we can do simple commands on the server.
	//  */
	// if ( (pthread_create(&pthread_id, &pthread_attr, cmdline, NULL)) < 0 ){
	// 	server_error(strerror(errno), server_socket, l);
	// }

	// /**
	//  * Do not wait for the thread to terminate.
	//  */
	// pthread_detach(pthread_id);


	
	/* Init and start encoder thread 	<=====================================================Encoder part */
	// FILE *pInput = fopen("/home/liyang/Documents/360_live/video_source/test.yuv", "rb");
	// arg_struct args_0, args_1, args_2;
	// args_0.arg2 = pInput;
	// args_1.arg2 = pInput;
	// args_2.arg2 = pInput;

	seg_temp_list = new_2d_array(NUM_RATE, MAX_CHUNK_LEN);
	seg_chunk_list = new_2d_array(NUM_RATE, MAX_CHUNK_LEN);
	seg_chunk_new_offsets = new_int_2d_array(NUM_RATE, SEG_FRAMENUM/CHUNK_FRAMENUM*OFFSET_HIS_LEN);
	chunk_byte_offset_list = new_int_2d_array(NUM_RATE, SEG_FRAMENUM/CHUNK_FRAMENUM);

	memset(frame_num_list, -1, NUM_RATE);

	for (int i = 0; i < NUM_RATE; ++i){
		// switch (i){
			// case 0:
			// 	// args_0.arg1 = 0;
		if ( (pthread_create(&pthread_id, NULL, serverEncoder, 
						i)) < 0 ){
			printf("Thread create failed!\n");
		}
		pthread_detach(pthread_id);
		// break;
		// 	case 1:
		// 		args_1.arg1 = 1;
		// 		if ( (pthread_create(&pthread_id, NULL, serverEncoder, 
		// 						(void *)&args_1)) < 0 ){
		// 			printf("Thread create failed!\n");
		// 		}
		// 		pthread_detach(pthread_id);
		// 		break;
		// 	case 2:
		// 		args_2.arg1 = 2;
		// 		if ( (pthread_create(&pthread_id, NULL, serverEncoder, 
		// 						(void *)&args_2)) < 0 ){
		// 			printf("Thread create failed!\n");
		// 		}
		// 		pthread_detach(pthread_id);
		// 		break;
		// }
	    
	}
	
	if ( (pthread_create(&pthread_id, NULL, Savefile, 
						NULL)) < 0 ){
			printf("Thread create failed!\n");
		}
	pthread_detach(pthread_id);

	if ( (pthread_create(&pthread_id, NULL, clear_cache, 
						NULL)) < 0 ){
			printf("Thread create failed!\n");
		}
	pthread_detach(pthread_id);
	/* Init and start encoder thread 	<=====================================================Encoder part */
	

	while (1) {
		client_length = sizeof(client_addr);
		
		/**
		 * If a client connects, we observe it here.
		 */
		if ( (client_socket = accept(server_socket, 
				(struct sockaddr *) &client_addr,
				&client_length)) < 0) {
			server_error(strerror(errno), server_socket, l);
		}

		/**
		 * Save some information about the client, which we will
		 * later use to identify him with.
		 */
		char *temp = (char *) inet_ntoa(client_addr.sin_addr);
		char *addr = (char *) malloc( sizeof(char)*(strlen(temp)+1) );
		if (addr == NULL) {
			server_error(strerror(errno), server_socket, l);
			break;
		}
		memset(addr, '\0', strlen(temp)+1);
	    memcpy(addr, temp, strlen(temp));	

		ws_client *n = client_new(client_socket, addr);

		/**
		 * Create client thread, which will take care of handshake and all
		 * communication with the client.
		 */
		if ( (pthread_create(&pthread_id, &pthread_attr, handleClient, 
						(void *) n)) < 0 ){
			server_error(strerror(errno), server_socket, l);
		}
		pthread_detach(pthread_id);
		// char sendBuff[] = "received, reply";
		// send(client_socket, (const char *) sendBuff, strlen(sendBuff),  MSG_CONFIRM);

	}

	list_free(l);
	l = NULL;
	close(server_socket);
	pthread_attr_destroy(&pthread_attr);
	return EXIT_SUCCESS;
}
