#include "server264.h"
#include <time.h>
clock_t start, end;
double cpu_time_used;


unsigned int *g_uiPTSFactor_list = new unsigned int[NUM_RATE];
int *iNal_list = new int[NUM_RATE];
x264_nal_t **pNals_list = (x264_nal_t **)malloc(sizeof(x264_nal_t *)*NUM_RATE);


static struct itimerval oldtv;
struct timeval tv_start, tv_end;
//struct itimercal ntv;
bool loopflag = true;

void set_timer() {
	struct itimerval nt;
	nt.it_interval.tv_sec = 0;
	nt.it_interval.tv_usec = 40000;
	nt.it_value.tv_sec = 0;
	nt.it_value.tv_usec = 40000;
	setitimer(ITIMER_REAL, &nt, &oldtv);
	printf("done\n");
}

void signal_handler(int m) {
	gettimeofday(&tv_end, NULL);
	double d =  (double) (tv_end.tv_usec - tv_start.tv_usec)/1000 + (double) (tv_end.tv_sec - tv_start.tv_sec)*1000;
	// printf("xxx%f\n", d);
	// if(d> 41) printf("xxxxxxxxxxxxxxxxxxxxxxxx%f\n", d);
	gettimeofday(&tv_start, NULL);	
	loopflag = true;
}

int serverEncoder(int br_idx) {
	FILE *pInput = fopen("/home/liyang/Documents/low_latency_live/video_source/720x480.yuv", "rb");
	// FILE *pInput = fopen("/home/liyang/Downloads/test.yuv", "rb");
	FILE * encoding_log_fp;
	char filep[] = "/home/liyang/Documents/low_latency_live/streaming_testbed/logs/encoderk";
	filep[sizeof(filep) - 2] = '0' + br_idx;
	encoding_log_fp = fopen (filep,"w"); // to log

	int *temp_seg_idx = &seg_index_list[br_idx];
	int *temp_len = &seg_len_list[br_idx];
	int *temp_saving_signal = &saving_signal_list[br_idx];
	int *temp_seg_in_chunk = &seg_in_chunk_list[br_idx];
	int *temp_frameNum = &frame_num_list[br_idx];
	int *iNal = &iNal_list[br_idx];

	unsigned int *g_uiPTSFactor = &g_uiPTSFactor_list[br_idx];
	x264_nal_t **pNals = &pNals_list[br_idx];
	uint8_t* ptr_seg = seg_temp_list[br_idx];
	pthread_mutex_t* lock = &lock_list[br_idx];
	int *nals_info = new int[3];
	int iResult = 0;

	// PRo chunk
	int *current_offset_ptr = &curr_offset_ptrs[br_idx];
	int *temp_chunk_offset = chunk_byte_offset_list[br_idx];	// To be modified
	int *temp_chunk_his_offset = seg_chunk_new_offsets[br_idx];	// To be modified
	// Time related variables
	
	struct tm * timeinfo;
	double time_budget = 0, encoding_sleep;
	//struct timeval tv_start, tv_end;

	x264_t* pX264Handle = NULL;
	x264_param_t* pX264Param = new x264_param_t;
	assert(pX264Param);
	// x264_param_default(pX264Param);
	x264_param_default_preset(pX264Param, "veryfast", "zerolatency");

	//* cpuFlags
	pX264Param->i_threads = X264_SYNC_LOOKAHEAD_AUTO;
	//* video Properties
	pX264Param->i_frame_reference = 1;
	pX264Param->i_width = 720;
	pX264Param->i_height = 480; 
	pX264Param->i_frame_total = 0; 
	pX264Param->i_keyint_max = video_fps;
	pX264Param->i_keyint_min = video_fps;
	//* bitstream parameters
	pX264Param->i_bframe = 0;	// Could be int, e.g. 3
	pX264Param->b_open_gop = 0;	// Infer-frame prediction is within a gop
	pX264Param->i_bframe_pyramid = 0;	// Use B frame as reference
	// pX264Param->i_bframe_adaptive = X264_B_ADAPT_TRELLIS;
	pX264Param->i_bframe_adaptive = X264_B_ADAPT_NONE;

	//pX264Param->vui.i_sar_width = 1080;
	//pX264Param->vui.i_sar_height = 720;

	//* Log
	// pX264Param->i_log_level = X264_LOG_DEBUG;
	//* Rate control Parameters
	pX264Param->rc.i_rc_method = X264_RC_ABR;
	// pX264Param->rc.i_rc_method = X264_RC_CRF;
	// pX264Param->rc.i_bitrate = 1024 * 5; 	//* muxing parameters
	// printf("bitrate is: %d \n", BITRATES[bitrate]);
	pX264Param->rc.i_bitrate = BITRATES[br_idx]; 	//* muxing parameters

	pX264Param->rc.i_lookahead = 0;
	pX264Param->i_fps_den = 1; 
	pX264Param->i_fps_num = video_fps; 
	pX264Param->i_timebase_den = pX264Param->i_fps_num;
	pX264Param->i_timebase_num = pX264Param->i_fps_den;

	// x264_param_apply_profile(pX264Param, x264_profile_names[1]);
	x264_param_apply_profile(pX264Param, "baseline");

	pX264Handle = x264_encoder_open(pX264Param);
	assert(pX264Handle);

	iResult = x264_encoder_headers(pX264Handle, pNals, iNal);
	// printf("Returned frame size is: %d\n", iResult);
	assert(iResult >= 0);

	x264_encoder_maximum_delayed_frames(pX264Handle);
	// int iMaxFrames = x264_encoder_maximum_delayed_frames(pX264Handle);
	*iNal = 0;
	*pNals = NULL;
	x264_picture_t* pPicIn = new x264_picture_t;
	x264_picture_t* pPicOut = new x264_picture_t;

	x264_picture_init(pPicOut);
	x264_picture_alloc(pPicIn, X264_CSP_I420, pX264Param->i_width, pX264Param->i_height);
	pPicIn->img.i_csp = X264_CSP_I420;
	pPicIn->img.i_plane = 3;

	// FILE *pInput = fopen("/home/liyang/Documents/360_live/video_source/rollercoaster.yuv", "rb");
	// FILE *ptest = fopen("agent.nal", "wb+");

	int iDataLen = pX264Param->i_width * pX264Param->i_height;
	int newiDataLen = (1.5) * iDataLen;
	uint8_t* data = new uint8_t[newiDataLen];

	// std::cout << "rtn" << std::endl;
	uint8_t uiComponent = 0;
	memset(data, uiComponent, newiDataLen);

	// Get initial time
	gettimeofday(&tv_start, NULL);

	// while (++uiComponent <= 1000) {
	// int present_f = 0;
	signal(SIGALRM, signal_handler);
	set_timer();

	while(1){
		start = clock();
		loopflag = false; // signal
		int rtn = fread(data, sizeof(uint8_t), newiDataLen, pInput);
		if (rtn < newiDataLen){
			// The data read from file does not equal to what is needed.
			// INitial the indicator of pinput
			// printf("Not equal!!!!!!!!!!!!!!!!!!\n");
			// printf("%d\n", rtn);
			fseek(pInput, 0, SEEK_SET);
			int rtn = fread(data, sizeof(uint8_t), newiDataLen, pInput);
		}
		// printf("%u\n", pInput);
		memcpy(pPicIn->img.plane[0], data, iDataLen);
		memcpy(pPicIn->img.plane[1], data + iDataLen, iDataLen / 4);
		memcpy(pPicIn->img.plane[2], data + iDataLen + iDataLen / 4, iDataLen / 4);

		if (uiComponent <= 1000) {
			pPicIn->i_pts = uiComponent + *g_uiPTSFactor * 1000;
			nals_info = encode(pX264Handle, pPicIn, pPicOut, iNal, pNals);
		} else {
			nals_info = encode(pX264Handle, NULL, pPicOut, iNal, pNals);
			if (0 == nals_info[0]) {
				uiComponent = 0;
				++(*g_uiPTSFactor);
				// x264_encoder_reconfig(pX264Handle, pX264Param);
				// x264_encoder_intra_refresh(pX264Handle);
			}
		}	
		uiComponent++;
		// printf("%d\n", uiComponent);
		// // Move chunk delivery out of encoder thread
		// int dc_rtn = deliver_chunk(**pNals, chunk_temp, uiComponent, &entire_length, client);
		// if (dc_rtn){
		// 	// Chunk delivered
		// 	chunk_temp = new uint8_t[MAX_CHUNK_LEN];
		// }
		// Get nals belonging to one frames
		// printf("iNal value is: %d\n", iNal);
		if (nals_info[1]%SEG_FRAMENUM == 0){
			// Add timestamp at the head of data, assuming it is in the header
			// gettimeofday(&g_tv, NULL);
			// int seg_gap = *requested_br_seg_idx - request->seg_idx;
			uint32_t u_sec = tv_start.tv_usec/1000;
			memcpy(ptr_seg + 2, &u_sec, 4);
			// printf("arrive herer\n");
			timeinfo = localtime( &(tv_start.tv_sec));
			ptr_seg[0] = (uint8_t)timeinfo->tm_min;
			ptr_seg[1] = (uint8_t)timeinfo->tm_sec;
			// printf("time is: %d %lu\n", ptr_seg[1], u_sec);
			*temp_len += TIME_LEN;
		}

		for (int i = 0; i < *iNal; ++i) {
			// printf("pnal length at %d is: %d, address of pnal is: %d\n", bitrate, (*pNals)[i].i_payload, &((*pNals)[i].p_payload));
			pthread_mutex_lock(lock);
			memcpy(ptr_seg + *temp_len, (*pNals)[i].p_payload, (*pNals)[i].i_payload);
			pthread_mutex_unlock(lock); 
			*temp_len += (*pNals)[i].i_payload;
		}

		pthread_mutex_lock(lock);
		*temp_frameNum = nals_info[1]%SEG_FRAMENUM;
		// Save offset of a chunk
		if ((*temp_frameNum + 1)% CHUNK_FRAMENUM == 0) {
			temp_chunk_offset[*current_offset_ptr] = *temp_len;
			//if(br_idx == 3)
			//printf("offset  : %d\n", *temp_len);
			// printf("offset saved: %d\n", *temp_len);
			// printf("pointer is: %d\n", *current_offset_ptr);
			(*current_offset_ptr) ++;
			// printf("pointer is: %d\n", *current_offset_ptr);
		}
		// *temp_chunk_offset = (*temp_frameNum + 1)% CHUNK_FRAMENUM == 0 ? *temp_len : *temp_chunk_offset;
		pthread_mutex_unlock(lock); 
		// printf("temp frame number is: %d, and offset is set to: %d\n", *temp_frameNum, *temp_chunk_offset);
		//printf("i_frame: %d\n", pX264Handle->i_frame);
		//printf("uiComponent: %d\n", uiComponent);

		if ((nals_info[1] + 1) && (nals_info[1] + 1) % SEG_FRAMENUM == 0 && *temp_len > 0){	// Adjust chunk length & delay frame number
			// Trigger local saving func
			//if (*temp_seg_in_chunk == 0){
			//	*temp_saving_signal = 1;
			//}
			*temp_saving_signal = 1;
		}
		end = clock();
		cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
		// printf("%d bit rate encode use %f ms\n", BITRATES[br_idx], cpu_time_used * 1000); // record use time of encode
		// fprintf(encoding_log_fp,"%d bit rate encode use %f ms\n", BITRATES[br_idx], cpu_time_used * 1000);
		
		//gettimeofday(&tv_end, NULL);
		// double gap = (double) (tv_end.tv_usec - tv_start.tv_usec)/1000 + (double) (tv_end.tv_sec - tv_start.tv_sec)*1000;
		// while(gap < 39.7) {
		// 	usleep(100);
		// 	gettimeofday(&tv_end, NULL);
		// 	gap = (double) (tv_end.tv_usec - tv_start.tv_usec)/1000 + (double) (tv_end.tv_sec - tv_start.tv_sec)*1000;
		// 	//printf("gap %f", gap);
		// 	//if(gap > 30) printf("gap %f \t frame%d\n", gap, present_f);
		// }
		//printf("gap %f \t frame%d\n", gap, present_f++);
		while(!loopflag); // wait
	}
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

int* encode(x264_t* pX264Handle, x264_picture_t* pPicIn, x264_picture_t* pPicOut, int* iNal, x264_nal_t** pNals) {
	// int iResult = 0;
	int *threeResult = (int*)malloc(sizeof(int)*3);
	threeResult = x264_encoder_encode(pX264Handle, pNals, iNal, pPicIn, pPicOut);
	// iResult = nals_info[0];
	// *temp_frameNum = nals_info[1];
	// printf("iResult: %d\n", iResult);
	// printf("frameNum: %d\n", *temp_frameNum);
	// if (0 == nals_info[0]) {
	// 	std::cout << "Encoded successfully, cached." << std::endl;
	// } else if (nals_info[0] < 0) {
	// 	std::cout << "Error Encoding." << std::endl;
	// } else if (nals_info[0] > 0) {
	// 	std::cout << "Encoded data obtained" << std::endl;
	// }
	// Check the delayed buffer, and flash the delayed frame to chunk
	threeResult[2] = x264_encoder_delayed_frames(pX264Handle);
	// std::cout << "Cached frame numbers: " << iFrames << " frames\n";
	return threeResult;
}

int Savefile(){
	char filename[50] = {};
	while(1){
		// printf("SAVE FILE %d\n", seg_len_0);
		for (int i = 0; i < NUM_RATE; ++i){
			if (saving_signal_list[i] == 1) {
				sprintf(filename, "./segs/seg%d_br%d.m4s", seg_index_list[i], i);
				FILE *pFile = fopen(filename, "wb+");
				// printf("SAVE FILE BR %d %d\n", i, seg_len_list[i]);

				pthread_mutex_lock(&lock_list[i]);
				fwrite(seg_temp_list[i], 1, seg_len_list[i], pFile);
				saving_signal_list[i] = 0;
				// If current chunk is under chunking mode
				// printf("seg_in_chunk_list: %d\n", seg_in_chunk_list[i]);
				if (seg_in_chunk_list[i] == 1) {
					memcpy(seg_chunk_list[i], seg_temp_list[i], seg_len_list[i]);
					seg_in_ending_len_list[i] = seg_len_list[i];
					seg_in_ending_signal_list[i] = 1;
					// printf("copy to ending chunk, len is: %d\n", seg_len_list[i]);
				}
				memset(seg_temp_list[i], 0, MAX_CHUNK_LEN);
				seg_len_list[i] = 0;
				seg_index_list[i] ++;
				frame_num_list[i] = -1;
				
				///////////////////////////////////////////////////////////////////////		//Modified
				// Copy offsets to his_array		//Modified
				// Then clear the current offset, and reset the ptr  //Modified
				// printf("offset pointer is: %d\n", curr_offset_ptrs[i]);
				assert(curr_offset_ptrs[i] == SEG_FRAMENUM/CHUNK_FRAMENUM);
				// Shift offsets by 5
				for (int shift_idx = SEG_FRAMENUM/CHUNK_FRAMENUM*(OFFSET_HIS_LEN-1); shift_idx >=SEG_FRAMENUM/CHUNK_FRAMENUM; shift_idx -= SEG_FRAMENUM/CHUNK_FRAMENUM) {
					memcpy(seg_chunk_new_offsets[i] + shift_idx, seg_chunk_new_offsets[i] + shift_idx-SEG_FRAMENUM/CHUNK_FRAMENUM, sizeof(int)*SEG_FRAMENUM/CHUNK_FRAMENUM);
					// printf("%d \n", seg_chunk_new_offsets[i][shift_idx-SEG_FRAMENUM/CHUNK_FRAMENUM]);
					// printf("%d \n", seg_chunk_new_offsets[i][shift_idx-SEG_FRAMENUM/CHUNK_FRAMENUM + 1]);
					// printf("%d \n", seg_chunk_new_offsets[i][shift_idx-SEG_FRAMENUM/CHUNK_FRAMENUM + 2]);
					// printf("%d \n", seg_chunk_new_offsets[i][shift_idx-SEG_FRAMENUM/CHUNK_FRAMENUM + 3]);
					// printf("%d \n", seg_chunk_new_offsets[i][shift_idx-SEG_FRAMENUM/CHUNK_FRAMENUM + 4]);
				}
				memcpy(seg_chunk_new_offsets[i], chunk_byte_offset_list[i], sizeof(int)*SEG_FRAMENUM/CHUNK_FRAMENUM);
				curr_offset_ptrs[i] = 0;
				
				memset(chunk_byte_offset_list[i], 0, sizeof(int)*SEG_FRAMENUM/CHUNK_FRAMENUM);
				///////////////////////////////////////////////////////////////////////		//Modified
				pthread_mutex_unlock(&lock_list[i]); 
				// seg_temp_0 = new uint8_t[MAX_CHUNK_LEN];
				fclose(pFile);

				if (seg_index_list[i] >= 20 && seg_index_list[i]%10 == 0){
					clear_cache_list[i] = 1;
				}
			}
		}
	}
}

int clear_cache(){
	char filename[50] = {};
	int status;
	while(1){
		for (int i=0; i<NUM_RATE;++i) {
			if (clear_cache_list[i] == 1) {
				for (int j=seg_index_list[i]-20;j<seg_index_list[i]-10;++j) {
					sprintf(filename, "./segs/seg%d_br%d.m4s", j, i);
					if (access(filename, F_OK) != -1) {
						status = remove(filename);
						if (status != 0) {
							perror("Following Error Occurred!");
						}
					} else {
						printf("Cannot Access %s\n", filename);
					}
				}
				clear_cache_list[i] = 0;
			}
		}
	}
}