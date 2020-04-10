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

#ifndef _COMMUNICATE_H
#define _COMMUNICATE_H

#include "Datastructures.h"
#include "Websocket.h"
#define CHUNK_FRAMENUM 5
#define SEG_FRAMENUM 25
#define HEADER_LEN 4
// #define CHUNK_IN_SEG 5
// #include "cJSON.h"

extern pthread_mutex_t *lock_list;

extern pthread_mutex_t lock_0; 
extern pthread_mutex_t lock_1; 
extern pthread_mutex_t lock_2;


ws_connection_close encodeMessage(ws_message *m, int option);
ws_connection_close communicate(ws_client *n, char *next, uint64_t next_len, ws_json_request* request);
// ws_connection_close encodeJsonMessage(uint8_t *video_chunk);
int parseRequest (uint8_t* message, ws_json_request* request);
ws_connection_close init_reply(ws_client* n);
ws_connection_close cJSON_reply(char** jstring);
ws_connection_close seg_reply(ws_client* n, ws_json_request* request);
ws_connection_close estab_seg_json(int seg_idx, int br_idx, int data_size, char **jstring);
void estab_header(uint8_t *seg_header, uint8_t type, uint8_t br_idx, uint16_t seg_idx, uint8_t chunk_start, uint8_t chunk_num);

#endif
