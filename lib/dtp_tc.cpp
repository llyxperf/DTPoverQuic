/* Handling of the congestion control,include interacting with quic layer*/
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ev.h>
#include <uthash.h>

 

#include "quiche.h" 

#include <argp.h>
#include <string.h>
#include <time.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>

#include "dtp_block.h"
#include "dtp_internal.h"
#include "dtp_assemb.h"
#include "dtp_structure.h"
#include "dtp_tc.h"
#include "log_helper.h"

#define min(a, b) (a < b ? a : b)

//usage:
/*
    send blocks 
#include "dtplayer.h"
#include "dtp_internal.h"
#include "dtp_assemb.h"
    typedef struct block {
    char * buf;
    time_t t;
    uint64_t id;
    uint64_t size;
    uint64_t priority;
    uint64_t deadline;

    uint8_t tmode ;
} block;

used for sending blocks to peer via quiche conn

how to extract:
typedef struct dtp_dgram{
    uint64_t id;
    uint16_t offset
   
1.send
    Block is divided equally into several parts,whose size is set 1350.

2.recv
    offset 0 means dgram with block data
    offset -1 means it is the last dgram.(不一定和设定的size相等，当存在块切割交付功能)
    extracted and intergrated into a buffer and handed it to upper layer.

    
}
*/

//initialize tcontroler
//quic_conn and hash、config、block RecvQueue and blockinque left blank.



int dtp_tcontroler_init(dtp_layers_ctx* dtp_ctx)
{//dtp_tc_ctx
    int ret = 0;

    if (dtp_ctx->tc_ctx != NULL) {

        ret = -1;
    }
    else {
      
        dtp_tc_ctx* tc_ctx = ( dtp_tc_ctx*)malloc(sizeof(dtp_tc_ctx));
        if (tc_ctx == NULL) {
            ret = -1; 
        }
        else {
           
  
            memset(tc_ctx, 0, sizeof( dtp_tc_ctx)); 
   
            
            tc_ctx->quic_conn=NULL;
            tc_ctx->connhash=NULL;
            tc_ctx->quic_config=NULL;
           
            tc_ctx->dgram_hdrlen = DGRAM_HDR_LEN;//sizeof(id)+sizeof(offset)+sizeof(sent_time);
            tc_ctx->offset_arrived=(offsetsize_t *)malloc(sizeof(offsetsize_t)*tc_ctx->off_array_num);
            tc_ctx->recvQueue=NULL;
            tc_ctx->off_array_num=MAX_BLOCK_SIZE/(MAX_DATAGRAM_SIZE-tc_ctx->dgram_hdrlen)+2; //off =-2 and x.x
           
            tc_ctx->recv_dgram_num=0;
            tc_ctx->peer_RTT=0;
        
            tc_ctx->control_stream_id = 1;
            tc_ctx->transport_mode = (dtp_trans_mode) 0;

            tc_ctx->recv_blocks = NULL;
           
            dtp_ctx->tc_ctx = tc_ctx;
            
        }
    }
    return ret;
}
int dtp_tcontroler_free(dtp_layers_ctx* dtp_ctx) {
    //dtp_tc_ctx
    int ret = 0;

    if (dtp_ctx == NULL || dtp_ctx->tc_ctx == NULL) {
        ret = -1;
    } else {
        dtp_tc_ctx* tc_ctx = dtp_ctx->tc_ctx;

        if(tc_ctx->offset_arrived) {
            free(tc_ctx->offset_arrived);
        }

        if(HASH_COUNT(tc_ctx->recv_blocks)) {
            bmap_element * iter,*tmp;
            HASH_ITER(hh, tc_ctx->recv_blocks, iter, tmp) {
                bmap_delete(&tc_ctx->recv_blocks, iter->id);
            }
        }

        free(tc_ctx);
        ret = 0;
    }
    return ret;
}
static void mint_token(const uint8_t *dcid, size_t dcid_len,
                       struct sockaddr_storage *addr, socklen_t addr_len,
                       uint8_t *token, size_t *token_len) {
  memcpy(token, "quiche", sizeof("quiche") - 1);
  memcpy(token + sizeof("quiche") - 1, addr, addr_len);
  memcpy(token + sizeof("quiche") - 1 + addr_len, dcid, dcid_len);

  *token_len = sizeof("quiche") - 1 + addr_len + dcid_len;
}

static bool validate_token(const uint8_t *token, size_t token_len,
            struct sockaddr_storage *addr, socklen_t addr_len,uint8_t *odcid, size_t *odcid_len) {
  if ((token_len < sizeof("quiche") - 1) ||
      memcmp(token, "quiche", sizeof("quiche") - 1)) {
    return false;
  }

  token += sizeof("quiche") - 1;
  token_len -= sizeof("quiche") - 1;

  if ((token_len < addr_len) || memcmp(token, addr, addr_len)) {
    return false;
  }

  token += addr_len;
  token_len -= addr_len;

  if (*odcid_len < token_len) {
    return false;
  }

  memcpy(odcid, token, token_len);
  *odcid_len = token_len;

  return true;
}

static uint8_t *gen_cid(uint8_t *cid, size_t cid_len) {
  int rng ;
  if (rng < 0) {
    log_error("failed to open /dev/urandom %s", strerror(errno));
    return NULL;
  }

  ssize_t rand_len = read(rng, cid, cid_len);
  if (rand_len < 0) {
    log_error("failed to create connection ID %s", strerror(errno));
    return NULL;
  }

  return cid;
}
/**/
//todo ：如何控制粒度
//validate the connection,
//return -1 => conn doesnt exist , some work needs to be done and need to wait for further data from peer's socket
//1  => connection exists;
// int dtp_tc_conn_validate(dtp_tc_ctx * tc_ctx,uint8_t * buf,struct sockaddr_storage * peer_addr,socklen_t peer_addr_len,int socket){

//     const int LOCAL_CONN_ID_LEN=10;
//     static uint8_t out[MAX_DATAGRAM_SIZE];
//     memset(out,0,MAX_DATAGRAM_SIZE);
//     int ret=-1;
//     uint8_t type;
//     uint32_t version;

//     uint8_t scid[MAX_DATAGRAM_SIZE];
//     size_t scid_len = sizeof(scid);

//     uint8_t dcid[QUICHE_MAX_CONN_ID_LEN];
//     size_t dcid_len = sizeof(dcid);

//     uint8_t odcid[QUICHE_MAX_CONN_ID_LEN];
//     size_t odcid_len = sizeof(odcid);

//     uint8_t token[MAX_TOKEN_LEN];
//     size_t token_len = sizeof(token);

//     int rc =
//         quiche_header_info(buf, read, LOCAL_CONN_ID_LEN, &version, &type, scid,
//                            &scid_len, dcid, &dcid_len, token, &token_len);

//     if (rc < 0) { 
//       log_error("failed to parse header: %d", rc);
//     //  ret=DTP_ERR_DONE; 

//       return DTP_ERR_DONE;
//     }

//     //HASH_FIND(hh, tc_ctx->connhash, dcid, dcid_len, tc_ctx->quic_conn);


//     if (tc_ctx->quic_conn == NULL) {
//       if (!quiche_version_is_supported(version)) {
//         log_debug("version negotiation");

//         ssize_t written = quiche_negotiate_version(scid, scid_len, dcid,
//                                                    dcid_len, out, sizeof(out));

//         if (written < 0) {
//           log_error("failed to create vneg packet: %zd", written);
          
//           return DTP_ERR_DONE;
//         }

//        // set_tos(conns->ai_family, conns->sock, 5 << 5);
//         ssize_t sent = sendto(socket, out, written, 0,
//                               (struct sockaddr *)&peer_addr, peer_addr_len);
//         if (sent != written) {
//           log_error("failed to send %s", strerror(errno));
//           return DTP_ERR_DONE;
//         }

//         log_debug("sent %zd bytes", sent);
//          return DTP_ERR_DONE;
//       }

//       if (token_len == 0) {
//         log_debug("stateless retry");

//         mint_token(dcid, dcid_len, &peer_addr, peer_addr_len, token,
//                    &token_len);

//         uint8_t new_cid[LOCAL_CONN_ID_LEN];

//         if (gen_cid(new_cid, LOCAL_CONN_ID_LEN) == NULL) {
          
//           return DTP_ERR_DONE;
//         }

//         ssize_t written = quiche_retry(scid, scid_len, dcid, dcid_len, new_cid,
//                                        LOCAL_CONN_ID_LEN, token, token_len,
//                                        version, out, sizeof(out));

//         if (written < 0) {
//           log_error("failed to create retry packet: %zd", written);
//           return DTP_ERR_DONE;
//         }

//        // set_tos(conns->ai_family, conns->sock, 5 << 5);
//         ssize_t sent = sendto(socket, out, written, 0,
//                               (struct sockaddr *)peer_addr, peer_addr_len);
//         if (sent != written) {
//           log_error("failed to send %s", strerror(errno));
//           return DTP_ERR_DONE;
//         }

//         log_debug("sent %zd bytes", sent);
//          return DTP_ERR_DONE;
//       }

//       if (!validate_token(token, token_len, &peer_addr, peer_addr_len, odcid,
//                           &odcid_len)) {
//         log_error("invalid address validation token");
//         return DTP_ERR_DONE;
//       }

//       tc_ctx->quic_conn = create_conn(dcid, dcid_len, odcid, odcid_len, &peer_addr,
//                             peer_addr_len);

//       if (tc_ctx->quic_conn == NULL) {
//         return DTP_ERR_DONE;
//       }
//     }

 
//   return 1;
// } 

//send the feeback of current network
//lost pkt
ssize_t dtp_tc_control_flow_send(dtp_tc_ctx * tc_ctx,
    uint8_t * buf, size_t buflen,
    bool final_flow_data) {
    if (quiche_conn_is_established(tc_ctx->quic_conn)) {
        ssize_t sent = quiche_conn_stream_send(
            tc_ctx->quic_conn, tc_ctx->control_stream_id, 
            buf, buflen, final_flow_data);
        if (sent != buflen) {
            log_debug("Failed to send feeback_stream %d completely: sent %zd",tc_ctx->control_stream_id, sent);
        }
        return sent;
    }  
    else {
        log_debug("Failed to send feeback_stream %d,conn isn't exist",tc_ctx->control_stream_id);
        return -1;
    }
}

//Check and Read the feedback data from peer .
ssize_t dtp_tc_control_flow_recv(dtp_tc_ctx * tc_ctx, uint8_t *out, size_t buf_len, bool *final_flow_data){
    uint64_t s = tc_ctx->control_stream_id;
    if(quiche_conn_stream_readable(tc_ctx->quic_conn,s)) {
        ssize_t recv_len =
            quiche_conn_stream_recv(tc_ctx->quic_conn, s, out, buf_len, final_flow_data);
        if (recv_len > 0){
            log_debug("Control stream: recv %ld", recv_len);
            return recv_len;
        }
        else {
            log_debug("Failed to get data from control stream");
            return recv_len;
        }
    } else {
        return -1;
    }
}

uint64_t dtp_conn_timeout_as_nanos(dtp_tc_ctx *tc_ctx){
  return quiche_conn_timeout_as_nanos(tc_ctx->quic_conn);
}


//在使用者眼里，应该可以传入buf就解决掉assemble、发送的问题
//是否提供写buf的能力



//send some block
ssize_t dtpl_tc_conn_block_send(dtp_layers_ctx *dtp_ctx, Block * block){
    log_trace("inside dtpl_tc_conn_block_send");
    if (block==NULL)
        return DTP_ERR_NULL_PTR;
    uint8_t *buf_out=block->buf;
    uint64_t id=block->id;
    uint64_t size=block->size;
    log_debug("send id: %d, size: %d, counter: %d, offset: %d, dgram_num: %d, ", block->id, block->size, block->counter, block->offset, block->dgram_num);

    static uint8_t out[MAX_DGRAM_SIZE];
    memset(out, 0, MAX_DGRAM_SIZE);
    double bandwidth = 0;
    ssize_t sent = 0;
    offsetsize_t offset = block->offset;//off set of the block
    uint64_t priority   = block->priority;
    uint64_t deadline   = block->deadline;
    size_t send_size = 0;

    size_t total_data_sent = 0;

    assert(dtp_ctx);
    assert(dtp_ctx->quic_conn);
    size_t dgram_max_writable_len = quiche_conn_dgram_max_writable_len(dtp_ctx->quic_conn);
    log_debug("max writable len: %d", dgram_max_writable_len);
    size_t dgram_hdrlen = DGRAM_HDR_LEN;
    size_t dgram_payloadlen = dgram_max_writable_len-dgram_hdrlen;

    size_t dgram_num=size / dgram_payloadlen;
    log_debug("dgram_num: %d, size: %d, dgram_payloadlen: %d", dgram_num, size, dgram_payloadlen);
    // dgram_num=((size%dgram_payloadlen==0)?(dgram_num):(dgram_num+1))+1;
    dgram_num = (size%dgram_payloadlen==0)?(dgram_num):(dgram_num+1); // give up metadata dgram
    block->dgram_num = dgram_num;
    //divided into grams and send.
    //todo:dgram_size variables in ctx
    size_t dg_counter = block->counter;
    log_debug("dgram_num: %d", dgram_num);
    while(dg_counter<dgram_num){
        uint8_t* curptr = out;
        dgram_hdr dhdr = {
            .id        = id,
            .offset    = offset,
            .len       = 0,
            .sent_time = getCurrentUsec(),
            .size      = size,
            .priority  = block->priority,
            .deadline  = block->deadline,
            .t         = block->t,
        };
        log_debug("id: %x, offset: %x, sent_time: %x\n", id, offset, dhdr.sent_time);

        uint8_t* buf_out_ptr = ((uint8_t*)buf_out)+offset;
        send_size = min(size - offset, dgram_payloadlen);
        // hdr
        dhdr.len = send_size;
        curptr += encode_dgram_hdr(curptr, &dhdr);
        // payload
        memcpy(curptr, buf_out_ptr, send_size);
        curptr += send_size;
        //!todo:因队列发送失败，是否不更新，重发 ？
        sent = quiche_conn_dgram_send(dtp_ctx->quic_conn,out,curptr - out);
        if(sent != curptr - out) {
            if(sent == -1) {
                block->counter = dg_counter;
                block->offset = offset;
                return offset;
            } else {
                log_error("sent: %d, curptr - out: %d", sent, curptr - out);
                block->counter = dg_counter;
                block->offset = offset;
                return -2;
            }
        }
        log_debug("sent %d bytes", sent);
        if(dg_counter > 0) {
            offset += send_size;
        }
        dg_counter++;
        // TODO: bandwidth += getCurrentUsec()-sent_time;
    }
    log_debug("finish sending block %d: counter %d/%d, offset/size %d/%d ", block->id, dg_counter, block->dgram_num, offset, block->size);
 
    // TODO: dtp_ctx->bandwidth = (dgram_num * MAX_DATAGRAM_SIZE) / (bandwidth / 1000);//byte/ms
    //delete block from hash map and list node

    bmap_delete(&dtp_ctx->block_pool,id);

    // remove block from scheduler
    log_debug("block_queue size: %d", dtp_ctx->schelay_ctx->block_queue->size());
    for(auto iter = dtp_ctx->schelay_ctx->block_queue->begin(); 
             iter != dtp_ctx->schelay_ctx->block_queue->end(); 
             ++iter) {
        if(*iter == id) {
            dtp_ctx->schelay_ctx->block_queue->erase(iter);
            break;
        }
    }

    // TODO: implement blockinque related function
    // block_tlinkPtr linkIter=dtp_ctx->schelay_ctx->blockinque;
    // if(linkIter!=NULL){
        // while(linkIter->data.id!=id){
        //     linkIter=linkIter->next;
        // }
    // //not point to head
    
        // block_tlinkPtr last=linkIter->last;
        // block_tlinkPtr next=linkIter->next;

        // last->next=next;
        // next->last=last;

        // free(linkIter);

        // if(linkIter==dtp_ctx->schelay_ctx->blockinque) {
        //     dtp_ctx->schelay_ctx->blockinque=NULL;
        // }
    // }
  
    // dtp_ctx->schelay_ctx->blockinque

    return offset;
}

ssize_t dtpl_tc_conn_recv(dtp_layers_ctx *dtp_ctx) {
    dtp_tc_ctx * tc_ctx=dtp_ctx->tc_ctx;
    
    uint64_t total_bytes=0;
    ssize_t recv_len=0;
    static uint8_t recv_buf[MAX_DGRAM_SIZE];
    

    // static uint8_t block_buf[BLOCK_SIZE];

    memset(recv_buf,0, MAX_DGRAM_SIZE);

    dgram_hdr dhdr = {0};

    recv_len=1;
    uint32_t off_ind=0;
    // ! the follower expression causes segement fault
    // TODO: fix it
    // memset(tc_ctx->offset_arrived,0,tc_ctx->off_array_num);
 
    static uint64_t sent_time;
    static uint64_t id  = 0;
    uint64_t RTT_gap    = 0;
    offsetsize_t offset = 0;

    tc_ctx->recv_dgram_num = 0;

    for(;recv_len!=DTP_ERR_DONE; off_ind++){
        //todo :consider multiple ID situation/ using hash
        recv_len = quiche_conn_dgram_recv(tc_ctx->quic_conn, recv_buf, MAX_DATAGRAM_SIZE);
        log_debug("recv dgram， len: %d", recv_len);
        // TODO: total_bytes += recv_len;

        if(recv_len > 0){
            uint8_t *curptr = recv_buf + decode_dgram_hdr(recv_buf, &dhdr);
            id = dhdr.id;
            offset = dhdr.offset;
            sent_time = dhdr.sent_time;
            log_debug("hdr{ id: %d, offset: %d, sent_time: %ld, size: %ld, len: %ld}", 
                dhdr.id, 
                dhdr.offset, 
                dhdr.sent_time,
                dhdr.size,
                dhdr.len
            );
            //todo:collected and push to queue with correspond ID,to indicate
            //the ID and timestamps via control stream
            //when the queue is not empty,send.
            // TODO: RTT_gap = RTT_gap + getCurrentUsec() - sent_time;
            tc_ctx->recv_dgram_num++;
            // get block object
            bmap_element *e = bmap_find(tc_ctx->recv_blocks, id);
            if(e == NULL) {
                //create a new block (empty)
                Block *blk = (Block *)calloc(1, sizeof(Block));
                blk->id   = dhdr.id;
                blk->priority = dhdr.priority;
                blk->deadline = dhdr.deadline;
                blk->t        = dhdr.t;
                blk->size = dhdr.size;
                blk->buf = (uint8_t *)calloc(dhdr.size, sizeof(uint8_t));
                bmap_add(&tc_ctx->recv_blocks, id, blk);
                e = bmap_find(tc_ctx->recv_blocks, id);
            } else if(e->is_read) {
                continue;
            }
            if(dhdr.offset == 0) {
                e->block->begin_to_send = dhdr.sent_time;
            }
            // read data to block buffer
            uint8_t *block_cur_ptr = e->block->buf + dhdr.offset;
            memcpy(block_cur_ptr, curptr, dhdr.len);
            e->block->total_recv += dhdr.len;
            if(e->block->size > 0 && e->block->total_recv >= e->block->size) {
                log_debug("finish receiving block %d, size: %ld", e->block->id, e->block->size);
            }
            // TODO: (tc_ctx->offset_arrived)[off_ind]=offset;
        } else if(recv_len != DTP_ERR_DONE) {
            log_error("recv error: %d", recv_len);
            break;
        } else {
            break;
        }
    }
    uint64_t RTT = RTT_gap/(off_ind+1);
    tc_ctx->peer_RTT=RTT; //each dgram.todo:more stable standard recent RTTs?

    return 0;
}

//extract block bufdata (may be broken when using datagram) from dgram_recv_queue 
int dtp_tc_conn_block_recv(dtp_layers_ctx *dtp_ctx,uint8_t * block_buf, size_t buf_size){
    // TODO
    // if(block_buf == NULL){
    //     return -1;
    // }
    // dtp_tc_ctx * tc_ctx=dtp_ctx->tc_ctx;
    
    
    // uint64_t total_bytes=0;
    // size_t recv_len=0;
    // static uint8_t recv_buf[MAX_DGRAM_SIZE];
    

    // // static uint8_t block_buf[BLOCK_SIZE];

    // memset(recv_buf,0, MAX_DGRAM_SIZE);
    // memset(block_buf,0, buf_size);

    // uint64_t priority=0;
    // uint64_t deadline=0;
    // uint64_t size=0;

    // recv_len=1;
    // uint32_t off_ind=0;
    // // ! the follower expression causes segement fault
    // // TODO: fix it
    // // memset(tc_ctx->offset_arrived,0,tc_ctx->off_array_num);
 
    // static uint64_t sent_time;
    // static uint64_t id=0;
    // uint64_t RTT_gap=0;
    // offsetsize_t offset=0;

    // tc_ctx->recv_dgram_num=0;
 

    // for( ;recv_len!=DTP_ERR_DONE;){
    //     //todo :consider multiple ID situation/ using hash
    //     recv_len=quiche_conn_dgram_recv(tc_ctx->quic_conn, recv_buf,MAX_DATAGRAM_SIZE);
    //     log_info("recv dgram， len: %d", recv_len);
    //     // TODO: total_bytes += recv_len;

    //     if(recv_len!=DTP_ERR_DONE){
    //         void * curptr=(void *)recv_buf+tc_ctx->dgram_hdrlen;
    //         parse_ddgram_hdr(recv_buf,&id,&offset,&sent_time);
    //         log_info("hdr{id: %d, offset: %d, sent_time: %d}", id, offset, sent_time);
    //         //todo:collected and push to queue with correspond ID,to indicate
    //         //the ID and timestamps via control stream
    //         //when the queue is not empty,send.
    //         RTT_gap=RTT_gap+getCurrentUsec()-sent_time;
    //         tc_ctx->recv_dgram_num++;
    //         if(offset==-2){//metadata
    //             memcpy(&size,curptr,sizeof(size));
    //             curptr+=sizeof(size);
    //             memcpy(&priority,curptr,sizeof(priority));
    //             curptr+=sizeof(priority);
    //             memcpy(&deadline,curptr,sizeof(deadline));
    //             log_info("meta data: size: %d, priority: %d, deadline: %d", size, priority, deadline);   
    //         }
    //         else{
    //             void * block_cur_ptr=(void*)block_buf+offset;
    //             // TODO: memcpy(block_cur_ptr,curptr,MAX_DATAGRAM_SIZE-tc_ctx->dgram_hdrlen);
    //         }
    //         // TODO: (tc_ctx->offset_arrived)[off_ind]=offset;
    //    }

     
    // }
    // uint64_t RTT=RTT_gap/(off_ind+1);
    // tc_ctx->peer_RTT=RTT; //each dgram.todo:more stable standard recent RTTs?

    // log_info("Recieve block,ID :%lu size : %lu priority :%lu dgram_num:%lu RTT:%lu\n",id,size,priority,off_ind+1, RTT);
    return 0;
}

//process the imcoming socket buf;

//select a block from the pool and send via datagram
//TODO: rename
ssize_t dtpl_tc_conn_send(dtp_layers_ctx *dtp_ctx){
    if(HASH_COUNT(dtp_ctx->block_pool) == 0) {
        // no block to send
        return -1; // DONE
    }
    log_trace("enter dtpl_tc_conn_send");
    uint64_t id = dtp_schedule_block(dtp_ctx->schelay_ctx, dtp_ctx->block_pool, dtp_ctx->bandwidth, dtp_ctx->avrRTT);
    log_debug("schedule id: %lu", id);
    bmap_element *aim_bmap_element = bmap_find(dtp_ctx->block_pool, id);
    if(aim_bmap_element == NULL) {
        log_error("no target element");
        return -1;
    }
    return dtpl_tc_conn_block_send(dtp_ctx, aim_bmap_element->block);
}

//bool quiche_conn_is_closed(quiche_conn *conn);
// bool dtp_conn_is_closed(dtp_tc_ctx * tc_ctx){
//   return quiche_conn_is_closed(tc_ctx->quic_conn);
// }

//Todo :check if the ctx as well as the parameters is not null.
// bool dtp_conn_is_established(dtp_tc_ctx *tc_ctx) {
//   assert((tc_ctx != NULL && tc_ctx->quic_conn != NULL));
//   return quiche_conn_is_established(tc_ctx->quic_conn);
// }
 
//select a block from the pool and send via datagram
// ssize_t dtp_conn_send(dtp_tc_ctx *tc_ctx, uint8_t *out,size_t out_len){

//   quiche_send_info send_info;

//   return quiche_conn_send(tc_ctx->quic_conn, out, out_len,
//                           &send_info);
// }

//process the imcoming socket buf;
// ssize_t dtp_conn_recv(dtp_tc_ctx * tc_ctx, uint8_t *buf, size_t buf_len,struct sockaddr_storage * peer_addr,socklen_t peer_addr_len){
//     quiche_recv_info recv_info = {
//         (struct sockaddr *)peer_addr,

//         peer_addr_len,
//     };

//     return quiche_conn_recv(tc_ctx->quic_conn, buf, read, &recv_info);
// }

/*
used for sending buf 

*/

/*
making connection via quiche
*/