#ifndef DTP_BLOCK_RECORD_H
#define DTP_BLOCK_RECORD_H

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct block_record {
    
    uint64_t id;
   
    uint64_t in; //time in queue ,also used for the arriving timestamps from peer's control STREAM
    uint64_t left;//left time ,compared with ddl

} block_info;


typedef struct block_t_link{
    block_info data;
    struct block_t_link * next;
    struct block_t_link * last;
   
} block_t_link,* block_tlinkPtr;

#if defined(__cplusplus)
}
#endif
#endif // DTP_BLOCK_RECORD_H