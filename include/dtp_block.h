#ifndef DTP_BLOCK_H
#define DTP_BLOCK_H
#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <time.h>
#include <uthash.h>

typedef enum {
    stream = 1,
    gram = 2,
} dtp_trans_mode;

typedef struct {
    uint8_t * buf;
    uint64_t t; // timestamp when the block is generated
    uint64_t id;
    uint64_t size;
    uint64_t priority;
    uint64_t deadline;
//tmode =1 ,drop the blocks which exceeds the deadline.
//tmode =0 ,transmit as possible as it can.
    uint8_t tmode;
    // already sent information
    size_t counter;
    size_t offset;
    size_t dgram_num;
    // recv information
    size_t total_recv;
    // statistic information
    uint64_t begin_to_send; // timestamp when the block is sent to the transport layer
} Block;

typedef struct quiche_block {
    uint64_t block_id;
    /// The DTP block size.
    uint64_t size;
    /// The DTP block priority.
    uint64_t priority;
    /// The DTP block deadline.
    uint64_t deadline;
    /// timestamp when the block is created
    uint64_t start_at;
    /// timestamp when the block is sent to the transport protocol
    uint64_t send_at;
} quiche_block;

#if defined(__cplusplus)
}
#endif
#endif 