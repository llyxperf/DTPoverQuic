#include "dtp_block_map.h"

bmap_element* bmap_find(bmap_element* head, uint64_t id) {
    bmap_element *s;
    // s = (bmap *)malloc(sizeof *s);
    HASH_FIND_INT(head,&id,s);
    return s;
}

int bmap_add(bmap_element ** head,uint64_t id,Block * blk){ 
    bmap_element * news;
    
    HASH_FIND_INT(*head, &id, news); 

    if (news == NULL){
        news=(bmap_element *)malloc(sizeof(bmap_element));
        news->id=id;
        news->block = blk;
        HASH_ADD_INT(*head, id, news);
        return 0;
    } else {
        return -1;
    }
}

int bmap_delete(bmap_element ** blockhash, uint64_t id){
    bmap_element *aim = bmap_find(*blockhash, id);
    if(aim == NULL){
        return -1;
    }

    HASH_DEL(*blockhash, aim);
    log_debug("Delete block with id %lu",id);
    if(aim->block) {
        if(aim->block->buf) {
            free(aim->block->buf);
        }
        free(aim->block);
    }
    free(aim);
    
    return 1;
}

int bmap_lazy_delete(bmap_element **head, uint64_t id) {
    bmap_element *aim = bmap_find(*head, id);
    if(aim == NULL){
        return -1;
    }

    if(aim->block) {
        if(aim->block->buf) {
            free(aim->block->buf);
        }
        free(aim->block);
    }
    aim->is_read = true;
    aim->block = NULL;

    return 1;
}