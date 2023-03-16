/*  DTP assembler layer.Blocks are assembled before being scheduled   */
 
#ifndef DTP_ASSEMB_H
#define DTP_ASSEMB_H


// #include "dtplayer.h"
// #include "dtp_structure.h"
#include "dtp_block.h"
#include "dtp_block_record.h"
#include "dtp_structure.h"
#include "dtp_scheduler.h"

#if defined(__cplusplus)
extern "C" {
#endif

// the maximum size of block record(history array)
#define BLOCK_RECORD_LEN 10 

/*
int dtp_assemb_input(
    dtp_assem_ctx* reassembly_ctx,
    uint64_t current_time,
    const uint8_t* data,
   
    uint64_t b_id,
    uint64_t offset,
    uint64_t queue_delay,
    uint8_t flags,

    int is_last_fragment,
    size_t data_length,
              
              
              dtpAssMidInfoready_fn ready_fn,
    void * app__ctx);
*/
typedef enum {
    DTP_BLOCK_INFO_AUTO =1,
    DTP_BLOCK_INFO_MANNUAL =2,
} info_assemble_mode;

typedef struct rec_block_info{
    uint64_t  size;
    uint64_t  priority;
    uint64_t  deadline;
} r_binfo,* rbinfoptr;

//context
typedef struct dtp_assemble_layer_ctx {
    //average deadline.
    uint64_t avrddl;
    //Record some of the last blocks.
    uint64_t historyLen;
    rbinfoptr historyArray;
    //current array index
    uint64_t historyCurIndex;
    uint64_t hisCount;
    //auto or mannual or other ways
    info_assemble_mode mode;
} dtp_assem_ctx;

//initialize the layer
int dtp_assembler_init(dtp_layers_ctx* dtp_ctx, info_assemble_mode mode);

int dtp_assembler_free(dtp_layers_ctx* dtp_ctx);
// assemble the block automatically
// Automatically set priority and deadline.
int dtp_assemble_block_auto(dtp_assem_ctx* assemlay_ctx, uint64_t avrddl, uint64_t avrRTT, uint64_t bandwidth, Block* blk);

// assemble the block
// Create a block based on the given data,and adds it to the block pool.
// if either priority or deadline is 0, then both of them will be set automatically
int dtp_assemble_block(dtp_layers_ctx* dtp_ctx, \
    dtp_sctx* schelay_ctx, \
    dtp_assem_ctx* assemlay_ctx,\
    uint64_t size,      \
    uint64_t priority,  \
    uint64_t deadline,  \
    uint8_t* buf,       \
    int is_fragment     \
    );


int dtp_assemb_learn_start_point(dtp_assem_ctx* reassembly_ctx,uint64_t start_b_id, uint64_t current_time);


int dtp_assemb_learn_final_obji(dtp_assem_ctx* reassembly_ctx,uint64_t final_b_id);

 
//release
void dtp_assemb_release(dtp_assem_ctx* reassembly_ctx);


#if defined(__cplusplus)
}
#endif

#endif