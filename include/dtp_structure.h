#ifndef DTP_STRUCTURE_H
#define DTP_STRUCTURE_H


#include <stdint.h>
#include <stdbool.h> 

#include "quiche.h"
#include "uthash.h"
#include "dtp_block_map.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define offsetsize_t int32_t

#define MAX_DGRAM_SIZE 1350

struct dtp_assemble_layer_ctx;
struct dtp_schedule_layer;
struct dtp_traffic_control_ctx;

struct dtp_global_ctx {
    const char* sni;
    
    quiche_conn * quic_conn;
    struct dtp_schedule_layer* schelay_ctx;     
    struct dtp_traffic_control_ctx* tc_ctx;
    
    struct dtp_assemble_layer_ctx* assemlay_ctx;
    
    uint64_t newid; //todo:先前释放的id？grow?
 
    uint64_t avrRTT;    //todo:move to tc_ctx?
    uint64_t bandwidth;  //? 单位?
    uint64_t avrddl;    //dgram level.todo:block level
    bmap_element* block_pool; //block hash
 
 
    unsigned int do_congestion_control : 1;
};
 
typedef struct dtp_global_ctx dtp_layers_ctx;


#if defined(__cplusplus)
}
#endif

#endif // DTP_STRUCTURE_H