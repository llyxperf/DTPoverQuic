#ifndef DTP_BLOCK_MAP_H
#define DTP_BLOCK_MAP_H

#include <stdint.h>
#include <stdbool.h>
#include "dtp_block.h"
#include "log_helper.h"

#if defined(__cplusplus)
extern "C" {
#endif
//todo multiple connection situation
//{conn:bmap}
typedef struct blockmap_element {
    uint64_t id;
    bool     is_read;
    Block *block;

    UT_hash_handle hh;    
} bmap_element;

bmap_element * bmap_find(bmap_element * head,uint64_t id);

// TODO: what if we add duplicate id but different block?
int bmap_add(bmap_element ** head, uint64_t id, Block * blk);

int bmap_delete(bmap_element ** head, uint64_t id);

// delete block pointer and mark deleted
// remain the id
int bmap_lazy_delete(bmap_element **head, uint64_t id);

#if defined(__cplusplus)
}
#endif
#endif // DTP_BLOCK_MAP_H