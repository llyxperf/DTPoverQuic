/* Handling of a _ */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

 
#include "dtp_assemb.h"
#include "dtp_internal.h"
#include "dtp_scheduler.h"

/* init scheduler
peer_addr blockinque left blank
 */
int dtp_scheduler_init(dtp_layers_ctx* dtp_ctx, dtp_trans_mode transport_mode) {
    int ret = 0;

    if (dtp_ctx->schelay_ctx != NULL) {
        ret = -1;
    }
    else {
        dtp_sctx* schelay_ctx = (dtp_sctx*)calloc(1, sizeof(dtp_sctx));
        if (schelay_ctx == NULL) {
            ret = -1;
        }
        else {
            // memset(schelay_ctx, 0, sizeof(dtp_sctx)); 
            schelay_ctx->nxtID = 0;
            schelay_ctx->transport_mode = transport_mode;
            // schelay_ctx->dtpctx=dtp_ctx;
            // block in que init
            // block_t_link * head=(block_t_link *)malloc(sizeof(block_t_link));
            // head->next = head;
            // head->last = head;
             
            // schelay_ctx->blockinque=head;
            log_debug("Here");
            schelay_ctx->block_queue = new std::deque<int>();
            log_debug("Not here");

            dtp_ctx->schelay_ctx = schelay_ctx;
        }
    }
    return ret;
}

int dtp_scheduler_free(dtp_layers_ctx* dtp_ctx) {
    int ret = 0;
    if(dtp_ctx == NULL || dtp_ctx->schelay_ctx == NULL) {
        ret = -1;
    } else {
        delete(dtp_ctx->schelay_ctx->block_queue);
        free(dtp_ctx->schelay_ctx);
    }
    return ret;
}


//test no scheduling
void dtplq_disable_sche(dtp_layers_ctx* dtp_ctx)
{
    if (dtp_ctx->schelay_ctx != NULL) {
        free(dtp_ctx->schelay_ctx);
        dtp_ctx->schelay_ctx = NULL;
    }

    printf("ScheOFF\n");
}
int64_t dtpl_sche_cal_real_pri(dtp_sctx* sche_ctx, bmap_element* block_pool, double bandwidth, double avrRTT, uint64_t id) {

    // bmap* cur = bmap_find(block_pool,id);
    // block_tlinkPtr iter=sche_ctx->blockinque;
    // uint64_t time_queue=0;
    
    // while(iter!=NULL){
    //     //HASH_FIND()
    //     if ( iter->data.id==id){
    //         time_queue=iter->data.in;
    //         break;
    //     }

    //     iter=iter->next;
    // }

    // if(time_queue==0)
    //     return -1;

    // uint64_t rem=getCurrentUsec()-time_queue;
    // iter->data.left=rem;

    // int64_t ret=rem+(cur->block.priority)/(cur->block.deadline)+(float)cur->block.size/bandwidth+avrRTT;
    
    // return ret;
    /*
todo:use hash
 
*/
    return 0;
}


//todo:考虑sche binfo queue使用hash
uint64_t dtp_schedule_block(dtp_sctx* sche_ctx, bmap_element* block_pool, double bandwidth, double avrRTT){
    bmap_element * blocks = block_pool;
    bmap_element * iter,*tmp;
    uint64_t ret=0;
    uint64_t nxtP=0;
    uint64_t nextID_priority;
    // TODO: implement scheduler
    // int pri = dtpl_sche_cal_real_pri(sche_ctx, block_pool, bandwidth, avrRTT, sche_ctx->nxtID);
    // if(pri<0)
    //     nextID_priority=0;
    // else
    //     nextID_priority=pri;

    // HASH_ITER(hh, blocks, iter, tmp) {
    //     int real = dtpl_sche_cal_real_pri(sche_ctx, block_pool, bandwidth, avrRTT, iter->block.id);
    //     if(real!=-1&&real>nextID_priority){
    //         ret=iter->block.id;
    //         sche_ctx->nxtID=ret;
    //         nextID_priority=real;
    //     }
    // }
    // RR scheduler
    ret = sche_ctx->block_queue->front();
    sche_ctx->block_queue->pop_front();
    sche_ctx->block_queue->push_back(ret);
    return ret; // return the first element
}  
 
  
