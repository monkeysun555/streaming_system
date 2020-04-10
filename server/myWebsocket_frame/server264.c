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

extern "C" {
#include "x264.h"
}
;

#define MAX_CHUNK_LEN 10000000

unsigned int g_uiPTSFactor = 0;
int iNal = 0;
x264_nal_t* pNals = NULL;
int encode(x264_t* p264, x264_picture_t* pIn, x264_picture_t* pOut);
int findStartCode2(uint8_t*);
int findStartCode3(uint8_t*);

int serverEncoder(ws_list *l, ws_client *client) {
	int iResult = 0;
	x264_t* pX264Handle = NULL;


	x264_param_t* pX264Param = new x264_param_t;
	assert(pX264Param);

	x264_param_default(pX264Param);
	//* cpuFlags
	pX264Param->i_threads = X264_SYNC_LOOKAHEAD_AUTO;
	//* video Properties
	pX264Param->i_width = 1280;
	pX264Param->i_height = 720; 
	pX264Param->i_frame_total = 0; 
	pX264Param->i_keyint_max = 100;
	//* bitstream parameters
	pX264Param->i_bframe = 0;
	pX264Param->b_open_gop = 0;
	pX264Param->i_bframe_pyramid = 0;
	pX264Param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;

	//pX264Param->vui.i_sar_width = 1080;
	//pX264Param->vui.i_sar_height = 720;

	//* Log
	pX264Param->i_log_level = X264_LOG_DEBUG;
	//* Rate control Parameters
	pX264Param->rc.i_bitrate = 1024 * 10; 	//* muxing parameters
	pX264Param->i_fps_den = 1; 
	pX264Param->i_fps_num = 25; 
	pX264Param->i_timebase_den = pX264Param->i_fps_num;
	pX264Param->i_timebase_num = pX264Param->i_fps_den;

	// x264_param_apply_profile(pX264Param, x264_profile_names[1]);
	x264_param_apply_profile(pX264Param, "baseline");

	pX264Handle = x264_encoder_open(pX264Param);
	assert(pX264Handle);


	iResult = x264_encoder_headers(pX264Handle, &pNals, &iNal);
	// printf("Returned frame size is: %d\n", iResult);
	assert(iResult >= 0);

	for (int i = 0; i < iNal; ++i) {
		switch (pNals[i].i_type) {
			case NAL_SPS:{
				break;
			}
			case NAL_PPS:{
				break;
			}
			default:{
				break;
			}
		}
	}

	int iMaxFrames = x264_encoder_maximum_delayed_frames(pX264Handle);
	iNal = 0;
	pNals = NULL;
	x264_picture_t* pPicIn = new x264_picture_t;
	x264_picture_t* pPicOut = new x264_picture_t;

	x264_picture_init(pPicOut);
	x264_picture_alloc(pPicIn, X264_CSP_I420, pX264Param->i_width,
		pX264Param->i_height);
	pPicIn->img.i_csp = X264_CSP_I420;
	pPicIn->img.i_plane = 3;

	FILE *pInput = fopen("/home/liyang/Documents/360_live/video_source/test.yuv", "rb");
	// FILE *pInput = fopen("/home/liyang/Documents/360_live/video_source/rollercoaster.yuv", "rb");

	// FILE *ptest = fopen("agent.nal", "wb+");

	int iDataLen = pX264Param->i_width * pX264Param->i_height;
	int newiDataLen = (1.5) * iDataLen;
	uint8_t* data = new uint8_t[newiDataLen];

	// std::cout << "rtn" << std::endl;
	uint8_t uiComponent = 0;
	memset(data, uiComponent, newiDataLen);
	while (++uiComponent <= 1000) {
	// while(1){
		int rtn = fread(data, sizeof(uint8_t), newiDataLen, pInput);
		memcpy(pPicIn->img.plane[0], data, iDataLen);
		memcpy(pPicIn->img.plane[1], data + iDataLen, iDataLen / 4);
		memcpy(pPicIn->img.plane[2], data + iDataLen + iDataLen / 4, iDataLen / 4);

		if (uiComponent <= 1000) {
			pPicIn->i_pts = uiComponent + g_uiPTSFactor * 1000;
			encode(pX264Handle, pPicIn, pPicOut);
		} else {
			int iResult = encode(pX264Handle, NULL, pPicOut);
			if (0 == iResult) {
				
				uiComponent = 0;
				++g_uiPTSFactor;
				// x264_encoder_reconfig(pX264Handle, pX264Param);
				// x264_encoder_intra_refresh(pX264Handle);
			}
		}
		// printf("%d 2%d\n", uiComponent, entire_length);

		for (int i = 0; i < iNal; ++i) {
			ws_message *m = message_new();
			int start_len = 0;
			m->len = pNals[i].i_payload;	// With start code
			if (findStartCode2(pNals[i].p_payload)){
				start_len = 3;
				m->len = pNals[i].i_payload - start_len;
				
			} else if (findStartCode3(pNals[i].p_payload)){
				start_len = 4;
				m->len = pNals[i].i_payload - start_len;
			} else{
				printf("Error while finding start code!\n");
				break;
			}
			uint8_t *nal_temp = malloc(sizeof(uint8_t)*(m->len+1));

			// fwrite(pNals[i].p_payload, 1, pNals[i].i_payload, ptest);
			memset(nal_temp, '\0', (m->len+1));
			memcpy(nal_temp, pNals[i].p_payload+start_len, m->len);
			m->msg = nal_temp;
			nal_temp = NULL;
			ws_connection_close status;

			if ( (status = encodeMessage(m, 0)) != CONTINUE) {
				message_free(m);
				free(m);
				raise(SIGINT);
				break;;
			}
			ws_send(client, m);
			message_free(m);
			free(m);
		}
	}
	// fclose(pFile);
	fclose(pInput);

	x264_picture_clean(pPicIn);
	x264_picture_clean(pPicOut);
	x264_encoder_close(pX264Handle);
	pX264Handle = NULL;

	delete pPicIn;
	pPicIn = NULL;

	delete pPicOut;
	pPicOut = NULL;

	delete pX264Param;
	pX264Param = NULL;

	delete[] data;
	data = NULL;

	return 0;
}


int encode(x264_t* pX264Handle, x264_picture_t* pPicIn, x264_picture_t* pPicOut) {


	int iResult = 0;
	iResult = x264_encoder_encode(pX264Handle, &pNals, &iNal, pPicIn, pPicOut);
	if (0 == iResult) {
		std::cout << "Encoded successfully, cached." << std::endl;
	} else if (iResult < 0) {
		std::cout << "Error Encoding." << std::endl;
	} else if (iResult > 0) {
		std::cout << "Encoded data obtained" << std::endl;
	}

	int iFrames = x264_encoder_delayed_frames(pX264Handle);
	std::cout << "Cached frame numbers: " << iFrames << " frames\n";
	return iFrames;
}

int findStartCode2(uint8_t* pBuf){
	// printf("get here start code 2\n");
  if( (pBuf[0]!=0) || (pBuf[1]!=0) || (pBuf[2]!=1) ) {
    return 0;
  }
  else {
    return 1;
  }
}

int findStartCode3(uint8_t* pBuf){
	// printf("get here start code 3\n");
  if( (pBuf[0]!=0) || (pBuf[1]!=0) || (pBuf[2]!=0) || (pBuf[3]!=1) ) {
    return 0;
  }
  else {
    return 1;
  }
}

// static int getaNal(uint8_t *bsFile, uint8_t *pdstBuf, int iPayload){
//   int startCodeLen  = 3;
//   int readByteLen   = 0;
//   int  findStartCode = 0;


//   for(int i = 0; i < 4; i++){
//     pdstBuf[i-4] = bsFile[i];
//   }


//   if( findStartCode2(pdstBuf-4) ) {
//     startCodeLen = 3;
//     pdstBuf[readByteLen++] = pdstBuf[-1];
//   }
//   else if( findStartCode3(pdstBuf-4) ) {
//     startCodeLen = 4;
//   }
//   else {
//     printf("CAN NOT FIND START CODE!\n");
//     return -1;
//   }


//   for (int i = 4; i < iPayload; i++){
//     pdstBuf[readByteLen++] = bsFile[i];
//     if( readByteLen==MAXBUFFSIZE ) {
//       printf("BUFFER IS FULL!\n");
//       return -3;
//     }
    
//   }
//   return readByteLen - startCodeLen ;
// }
