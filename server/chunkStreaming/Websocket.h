#ifndef WEBSOCKET_H_
#define WEBSOCKET_H_

#include "Handshake.h"
#include "Communicate.h"
#include "Errors.h"
#include "server264.h"

#define PORT 34343
#define NUM_RATE 4
#define FPS 25
#define OFFSET_HIS_LEN 100

extern uint8_t **seg_temp_list;
extern uint8_t **seg_chunk_list;
// extern int *seg_chunk_len_list;
extern int *seg_in_ending_len_list;
extern int *seg_in_ending_signal_list;
extern int *seg_len_list;
extern int *saving_signal_list;
extern int *seg_index_list;
extern int *clear_cache_list;
extern int *seg_in_chunk_list;
extern int *frame_num_list;
extern int video_fps;

// Modify to pro chunk 
extern int **seg_chunk_new_offsets;
extern int **chunk_byte_offset_list;
extern int *curr_offset_ptrs;

extern const int BITRATES[NUM_RATE];

#endif
