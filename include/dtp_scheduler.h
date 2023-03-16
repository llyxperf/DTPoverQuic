/*  DTP schedluing layer.Blocks are scheduled before being sent   */
 
#ifndef DTP_SCHEDULER_H
#define DTP_SCHEDULER_H

#include <deque>

#include <time.h>
#include "dtp_block.h"
#include "dtp_block_map.h"
#include "dtp_block_record.h"

//for compile?
// struct dtp_global_ctx;
 
// typedef struct dtp_global_ctx dtp_layers_ctx;
///
/*
typedef struct block {
    char buf[10000];
         time_t t;
    long int ID;u_int16_t size;
 uint8_t priority;
 
    uint32_t deadline;
} block;
*/
#if defined(__cplusplus)
extern "C" {
#endif
typedef struct dtp_schedule_layer {
    uint64_t nxtID; // to send
    // block_tlinkPtr blockinque; //link.record the info of the time of the blocks. always points to the header node,cam be blak
    // rbinfoptr bhistory;
    // struct sockaddr_storage peer_addr;
    dtp_trans_mode transport_mode;
    // RR scheduler struct
    std::deque<int> *block_queue;
}  dtp_sctx;
//record the timestamp the data comes in at the sender.

//listnode

  
//last k informations of blocks





//left for extension
// typedef struct dtplayerSchechGis{
//     dtp_layers_ctx* dtp_ctx;
// } dtpl_Cons_context_t;

// Initialize the scheduler layer.
int dtp_scheduler_init(dtp_layers_ctx* dtp_ctx, dtp_trans_mode transport_mode);

// TODO: consider blockinqueue
int dtp_scheduler_free(dtp_layers_ctx* dtp_ctx);

//Test no scheduling
void dtplq_disable_sche(dtp_layers_ctx* dtp_ctx);

//Calculate the real priority in scheduler
// TODO: implement this function
int64_t dtpl_sche_cal_real_pri(dtp_sctx* sche_ctx, bmap_element* block_pool, 
                          double bandwidth, double avrRTT, uint64_t id);

//Calculate all the priority of blocks and choose the id of the most uurgent  one.
// TODO: implement this function
uint64_t dtp_schedule_block(dtp_sctx* sche_ctx, bmap_element* block_pool, double bandwidth, double avrRTT);//todo:考虑sche binfo queue使用hash

#if defined(__cplusplus)
}
#endif
#endif // DTP_SCHEDULER_H