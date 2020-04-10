// #include <cassert>
// #include <iostream>
// #include <string>
#include "stdint.h"
#include <string.h>
#include <stdio.h>
extern "C" {
#include "x264.h"
}
;



unsigned int g_uiPTSFactor = 0;
int iNal = 0;
x264_nal_t* pNals = NULL;
int encode(x264_t* p264, x264_picture_t* pIn, x264_picture_t* pOut);

// uint8_t charToBin(char c)
// {
//     switch(c)
//     {
//         case '0': return 0;
//         case '1': return 1;
//     }
//     return 0;
// }

// uint8_t CstringToU8(char * ptr)
// {
//     uint8_t value = 0;
//     for(int i = 0; (ptr[i] != '\0') && (i<8); ++i)
//     {
//         value = (value<<1) | charToBin(ptr[i]);
//     }
//     return value;
// }

int main(int argc, char** argv) {
	int iResult = 0;
	x264_t* pX264Handle = NULL;


	x264_param_t* pX264Param = new x264_param_t;
	assert(pX264Param);
//* 配置参数
//* 使用默认参数
	x264_param_default(pX264Param);
//* cpuFlags
pX264Param->i_threads = X264_SYNC_LOOKAHEAD_AUTO; //* 取空缓冲区继续使用不死锁的保证.
//* video Properties
pX264Param->i_width = 1280; //* 宽度.
pX264Param->i_height = 720; //* 高度
pX264Param->i_frame_total = 0; //* 编码总帧数.不知道用0.
pX264Param->i_keyint_max = 10;
//* bitstream parameters
pX264Param->i_bframe = 0;
pX264Param->b_open_gop = 0;
pX264Param->i_bframe_pyramid = 0;
pX264Param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;


//* 宽高比,有效果,但不是想要的.
//pX264Param->vui.i_sar_width = 1080;
//pX264Param->vui.i_sar_height = 720;


//* Log
pX264Param->i_log_level = X264_LOG_DEBUG;
//* Rate control Parameters
pX264Param->rc.i_bitrate = 1024 * 10; //* 码率(比特率,单位Kbps)
//* muxing parameters
pX264Param->i_fps_den = 1; //* 帧率分母
pX264Param->i_fps_num = 25; //* 帧率分子
pX264Param->i_timebase_den = pX264Param->i_fps_num;
pX264Param->i_timebase_num = pX264Param->i_fps_den;


//* 设置Profile.使用MainProfile
x264_param_apply_profile(pX264Param, x264_profile_names[1]);


//* 打开编码器句柄,通过x264_encoder_parameters得到设置给X264
//* 的参数.通过x264_encoder_reconfig更新X264的参数
pX264Handle = x264_encoder_open(pX264Param);
assert(pX264Handle);


//* 获取整个流的PPS和SPS,不需要可以不调用.
iResult = x264_encoder_headers(pX264Handle, &pNals, &iNal);
assert(iResult >= 0);
//* PPS SPS 总共只有36B,如何解析出来呢?
for (int i = 0; i < iNal; ++i) {
	switch (pNals[i].i_type) {
		case NAL_SPS:
		break;
		case NAL_PPS:
		break;
		default:
		break;
	}
}


//* 获取允许缓存的最大帧数.
int iMaxFrames = x264_encoder_maximum_delayed_frames(pX264Handle);


//* 编码需要的参数.
iNal = 0;
pNals = NULL;
x264_picture_t* pPicIn = new x264_picture_t;
x264_picture_t* pPicOut = new x264_picture_t;


x264_picture_init(pPicOut);
x264_picture_alloc(pPicIn, X264_CSP_I420, pX264Param->i_width,
	pX264Param->i_height);
pPicIn->img.i_csp = X264_CSP_I420;
pPicIn->img.i_plane = 3;


//* 创建文件,用于存储编码数据
FILE* pFile = fopen("agnt.264", "wb");
assert(pFile);

FILE *pInput = fopen(argv[1], "rb");


//* 示例用编码数据.
int iDataLen = pX264Param->i_width * pX264Param->i_height;
uint8_t* data = new uint8_t[iDataLen];




int rtn = fread(data, sizeof(uint8_t), iDataLen, pInput);
// std::cout << rtn << std::endl;
printf("%d\n", rtn);
unsigned int uiComponent = 0;
while (++uiComponent <= 25) {
//* 构建需要编码的源数据(YUV420色彩格式)
	memset(data, uiComponent, iDataLen);
	memcpy(pPicIn->img.plane[0], data, iDataLen);
	memcpy(pPicIn->img.plane[1], data, iDataLen / 4);
	memcpy(pPicIn->img.plane[2], data, iDataLen / 4);


	if (uiComponent <= 1000) {
		pPicIn->i_pts = uiComponent + g_uiPTSFactor * 1000;
		encode(pX264Handle, pPicIn, pPicOut);
	} else {
//* 将缓存的数据取出
		int iResult = encode(pX264Handle, NULL, pPicOut);
		if (0 == iResult) {
//break; //* 取空,跳出
			uiComponent = 0;
			++g_uiPTSFactor;


/* {{ 这个解决不了取空缓冲区,再压缩无B帧的问题
x264_encoder_reconfig(pX264Handle, pX264Param);
x264_encoder_intra_refresh(pX264Handle);
//* }} */
		}
	}


//* 将编码数据写入文件.
	for (int i = 0; i < iNal; ++i) {
		fwrite(pNals[i].p_payload, 1, pNals[i].i_payload, pFile);
	}
}
//* 清除图像区域
x264_picture_clean(pPicIn);
x264_picture_clean(pPicOut);
//* 关闭编码器句柄
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


int encode(x264_t* pX264Handle, x264_picture_t* pPicIn,
	x264_picture_t* pPicOut) {


	int iResult = 0;
	iResult = x264_encoder_encode(pX264Handle, &pNals, &iNal, pPicIn, pPicOut);
	if (0 == iResult) {
		printf("successfully encoded, but in the cache.\n");
	} else if (iResult < 0) {
		printf("error encoded.\n");
	} else if (iResult > 0) {
		printf("get encoded data\n");
	}


/* {{ 作用不明
unsigned char* pNal = NULL;
for (int i = 0;i < iNal; ++i)
{
int iData = 1024 * 32;
x264_nal_encode(pX264Handle, pNal,&pNals[i]);
}
//* }} */


//* 获取X264中缓冲帧数.
	int iFrames = x264_encoder_delayed_frames(pX264Handle);
	printf("frameNum in cache: %d\n", iFrames);
	return iFrames;
}

