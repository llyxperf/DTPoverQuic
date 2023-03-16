//control flow:
//passing repeat request
//timestamp informations..etc
 
 
#ifndef DTP_INTERNAL_H
#define DTP_INTERNAL_H
 
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "uthash.h"  
//#include "dtp_assemb.h"
//#include "dtp_scheduler.h"
//#include "dtp_tc.h"
#include "quiche.h"
#include "log_helper.h"
#include "dtp_structure.h"

#define offsetsize_t int32_t

#define MAX_TOKEN_LEN                                                          \
  sizeof("quiche") - 1 + sizeof(struct sockaddr_storage) +                     \
      QUICHE_MAX_CONN_ID_LEN

#define MAX_DATAGRAM_SIZE 1350
#define MAX_BLOCK_SIZE 10000000
#define DTPL_MAX_CONNECTIONS 256

//for compile?
 
/* 时钟同步 */
typedef struct _ntp_time
{
    unsigned int coarse;
    unsigned int fine;
} ntp_time;



struct ntp_packet
{
    unsigned char leap_ver_mode;
    unsigned char startum;
    char poll;
    char precision;
    int root_delay;
    int root_dispersion;
    int reference_identifier;
    ntp_time reference_timestamp;
    ntp_time originage_timestamp;
    ntp_time receive_timestamp;
    ntp_time transmit_timestamp;
};

enum dtp_error {
    // There is no more work to do.
    DTP_ERR_DONE = -1,
    DTP_ERR_BUFFER_TOO_LARGE = -2,
    DTP_ERR_NULL_PTR = -3,
   
};


typedef struct dtplMsg_buf {
    size_t bytes_rn; 
    size_t msg_s;       //msg size
    size_t buffer_alloc;
    uint8_t* buffer;
    int is_finished;
} dtplMsgbuf;

void dtplMsgBufrelease(dtplMsgbuf* msg_buffer);
 
typedef enum dtp_msgType {
    // timestamps arrival
    DTPL_MSG_TIMESTAMP=1,
    // unreliable dgram slices()
    DTPL_MSG_DGRAMACK=2,

    //request for resending the left of some block or datagram、包含offset、ID等等（todo:如何考虑ID复用情况）
    DTPL_REQ_RESNED=11,
};
// Protocol message.
//This structure is used when decoding messages

typedef struct  dtpl_msgt {
    uint64_t message_type;

    int is_last_fragment;
    size_t length;
    const uint8_t* data;
 
} dtplMsgT;


// En  and decode the header of datagram packets.  
#define DTPL_DATAGRAM_HEADER_MAX 16

 

 
   
// Dtpl stream handling.
 
 
typedef enum {
   
    dtplSendStream=1,
    
    dtplSendReq,
 
    dtplSendFin,
 
    dtplSendNoMore //end
} dtpStreamsendstate_enum;

typedef enum {
    dtplRecv_initial = 0,
    dtplRecv_stream,
    dtplRecv_confirmation,
    dtplRecv_fragment,
    dtplRecv_notify,
    dtplRecv_done
}  dtplStreamrecvstate_enum;

typedef struct dtplDgramA_stateD {
 
   
    uint8_t flags;
    int is_last_fragment;
    size_t length;
    int is_acked;
    uint64_t repeatTimes;
    uint64_t start_time;
     
    uint64_t last_sent_time;
} dtpl_DgramAck_state;

 
__uint64_t getCurrentUsec();  //usec
 
 /*
dtpl_stream_ctx_t* dtpl_find_or_create_stream(
    uint64_t stream_id,
    dtpl_cnx_ctx_t* cnx_ctx,
    int should_create);

dtpl_stream_ctx_t* dtplCstreamCtx(dtpl_cnx_ctx_t* cnx_ctx, uint64_t stream_id);

void dtplDelstream_ctx(dtpl_cnx_ctx_t* cnx_ctx, dtpl_stream_ctx_t* stream_ctx);

 
const uint8_t* dtplDecObjhdr(const uint8_t* fh, const uint8_t* fh_max, dtplMobjPHdr* hdr);
uint8_t* dtplEncObjhdr(uint8_t* fh, const uint8_t* fh_max, const dtplMobjPHdr* hdr);

 
 
int dtpl_cnx_post_accepted(dtpl_stream_ctx_t* stream_ctx, dtpl_transport_mode_enum transport_mode, uint64_t _id);

 
 

 
int dtpl_congestion_check_per_cnx(dtpl_cnx_ctx_t* cnx_ctx, uint8_t flags, int has_backlog, uint64_t current_time);
*/
#endif