#ifndef _SERVER264_H
#define _SERVER264_H

#include <cassert>
#include <iostream>
#include <string>
#include "stdint.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h> // for usleep

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include "Communicate.h"
#include <sys/time.h>
#include <signal.h>
// #include "Websocket.h"
#include "cJSON.h"

extern "C" {
#include "x264.h"
}
;

#define MAX_CHUNK_LEN 10000000
#define TIME_LEN 6

extern unsigned int *g_uiPTSFactor_list;
extern int *iNal_list;
extern x264_nal_t **pNals_list;

int* encode(x264_t* pX264Handle, x264_picture_t* pPicIn, x264_picture_t* pPicOut, int* iNal, x264_nal_t** pNals);
int serverEncoder(int br_idx);
int Savefile();
int clear_cache();

#endif