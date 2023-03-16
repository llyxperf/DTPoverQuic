#ifndef DTPLAYER_H
#define DTPLAYER_H
#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include "quiche.h"
#include "uthash.h"
#include "dtp_block_map.h"
#include "dtp_internal.h"
#include "dtp_structure.h"
#include "dtp_tc.h"
 
#define DTPL_PORT 853

#define DTPL_ERROR_NO_ERROR 0x00
#define DTPL_ERROR_INTERNAL 0x01
#define DTPL_ERROR_PROTOCOL 0x02
 
typedef enum {
   dropExpiredMode=2,
    resendmode=3,


} dtplayerScheMo;


typedef struct{
    quiche_stats quiche_stat;
} dtp_stats;
/* Dtpl context */


 
typedef struct dtplStreamctx_t dtpl_stream_ctx_t;
 
//setter:
/*
set the config
set the conn 

*/
/*
quiche connector
call to construct quiche,to sink in dtp_ctx_initializer

after this,app can run into recieving times.

how to deal with ev?
when ev comes,u can read stream or read dgram.
*/

uint64_t dtp_conn_get_feedback(dtp_tc_ctx * tc_ctx,uint8_t * feedback);

int dtp_tc_config_load_cert_chain_from_pem_file(dtp_tc_ctx * tc_ctx,const char *path);
 
int dtp_tc_config_enable_dgram(dtp_tc_ctx * tc_ctx,bool enabled,size_t recv_queue_len,size_t send_queue_len);

//check if quiche conn is surpported
bool dtp_version_is_supported(uint32_t version);

ssize_t dtp_negotiate_version(const uint8_t *scid, size_t scid_len,
                                 const uint8_t *dcid, size_t dcid_len,
                                 uint8_t *out, size_t out_len);

int dtp_accept(dtp_layers_ctx * dtp_ctx,const uint8_t *scid, size_t scid_len,
                           const uint8_t *odcid, size_t odcid_len,
                           const struct sockaddr *from, size_t from_len,
                           quiche_config *config);

//tc_ctx->QUICHE_LOCAL_CONN_ID_LEN
int dtp_conn_check(const uint8_t *buf, size_t buf_len, size_t dcil,
                       uint32_t *version, uint8_t *type,
                       uint8_t *scid, size_t *scid_len,
                       uint8_t *dcid, size_t *dcid_len,
                       uint8_t *token, size_t *token_len);

//set quiche config by default
int dtp_tc_config_set(dtp_tc_ctx * tc_ctx);

// TODO: no implementation
// void dtpl_set_quic(dtp_layers_ctx* dtp_ctx, quiche_conn * quic);
 
// TODO: no implementation
// quiche_conn * dtpl_get_quic_ctx(dtp_layers_ctx* ctx);
 
typedef enum {
    dtplTransportMode_unspecified = 0,
    dtplTransportMode_single_stream = 1,
    dtplTransportMode_unreliable_stream = 2, //for extension
    dtplTransportMode_datagram = 4,
 
} dtplTransportMode_enum;

 
int dtp_enable_debug_logging(void (*cb)(const char *line, void *argp),
                             void *argp);
// void dtpl_set_cache_duration(dtp_layers_ctx* dtp_ctx, uint64_t cache_duration_max);


// uint64_t dtplTCheck(dtp_layers_ctx* dtp_ctx, uint64_t current_time);
 
// uint8_t * dtplMsgBufstore(uint8_t* bytes, size_t length, dtplMsgbuf* msg_buffer, int* is_finished);

void dtplMsgBufreset(dtplMsgbuf* msg_buffer);

void dtplMsgBufrelease(dtplMsgbuf* msg_buffer);

// int dtp_connect(dtp_layers_ctx * dtp_ctx,const char *server_name,
//                             const uint8_t *scid, size_t scid_len,
//                             const struct sockaddr *to, size_t to_len,
//                             quiche_config *config);
void dtp_conn_stats(dtp_layers_ctx *dtp_ctx, dtp_stats *out);

void dtp_conn_on_timeout(dtp_tc_ctx * tc_ctx);

//Parse the feedback recieved from peer buf
uint64_t dtp_conn_get_feedback(dtp_tc_ctx * tc_ctx,uint8_t * feedback);

//Initialize the layers.Include setting up quiche config
dtp_layers_ctx* dtp_layers_initnew_cli(uint32_t version);
dtp_layers_ctx* dtp_layers_initnew_serv(uint32_t version);

void dtp_layers_free(dtp_layers_ctx * dtp_ctx);

ssize_t dtpl_conn_buf_send(dtp_layers_ctx *dtp_ctx,uint8_t * buf,uint64_t size,uint64_t priority,uint64_t deadline,int is_fragment);


#if defined(__cplusplus)
}
#endif

#endif 
