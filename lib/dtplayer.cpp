#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
 
#include "dtp_assemb.h"
#include "dtplayer.h"
#include "dtp_internal.h"

 
 
//write buf into block with priority and deadline
//and add it to block_send_queue
//ssize_t dtp_write_block(dtp_layers_ctx* dtp_ctx,uint64_t priority,uint64_t deadline,unit64_t ){

//}





// int dtp_tc_config_load_cert_chain_from_pem_file(dtp_tc_ctx * tc_ctx,const char *path){

//     return quiche_config_load_cert_chain_from_pem_file(tc_ctx->quic_config, path);
// }

 
// int dtp_tc_config_enable_dgram(dtp_tc_ctx * tc_ctx,bool enabled,size_t recv_queue_len,size_t send_queue_len){
    
//     quiche_config_enable_dgram(tc_ctx->quic_config, enabled, recv_queue_len,send_queue_len);

//     return 1;
// }

// bool dtp_version_is_supported(uint32_t version){
//     return  quiche_version_is_supported( version);
// }

// ssize_t dtp_negotiate_version(const uint8_t *scid, size_t scid_len,
//                                  const uint8_t *dcid, size_t dcid_len,
//                                  uint8_t *out, size_t out_len){
                                    
//     ssize_t quiche_negotiate_version(scid,  scid_len,
//                                   dcid,  dcid_len,
//                                  out,  out_len);
// }

// int dtp_accept(dtp_layers_ctx * dtp_ctx,const uint8_t *scid, size_t scid_len,
//                            const uint8_t *odcid, size_t odcid_len,
//                            const struct sockaddr *from, size_t from_len,
//                            quiche_config *config){

//     if(dtp_ctx==NULL)
//         return -1;
    
//     dtp_ctx->quic_conn=quiche_accept(scid,scid_len,odcid, odcid_len,from,  from_len,config);
//     dtp_ctx->tc_ctx->quic_conn=dtp_ctx->quic_conn;

//     return 1;
// }

int dtp_conn_check(const uint8_t *buf, size_t buf_len, size_t dcil,
                       uint32_t *version, uint8_t *type,
                       uint8_t *scid, size_t *scid_len,
                       uint8_t *dcid, size_t *dcid_len,
                       uint8_t *token, size_t *token_len) {
                        
    return quiche_header_info(buf, buf_len, dcil, version, type, scid,
                           scid_len, dcid, dcid_len, token, token_len);
}
 
int dtp_tc_config_set(dtp_tc_ctx * tc_ctx){

    quiche_config_set_max_idle_timeout(tc_ctx->quic_config, 5000);
    quiche_config_set_max_recv_udp_payload_size(tc_ctx->quic_config, MAX_DATAGRAM_SIZE);
    quiche_config_set_max_send_udp_payload_size(tc_ctx->quic_config, MAX_DATAGRAM_SIZE);
    quiche_config_set_initial_max_data(tc_ctx->quic_config, 1000000000);
    quiche_config_set_initial_max_stream_data_uni(tc_ctx->quic_config, 10000000);
    quiche_config_set_initial_max_streams_uni(tc_ctx->quic_config, 40000);
    quiche_config_set_initial_max_stream_data_bidi_local(tc_ctx->quic_config, 10000000);
    quiche_config_set_initial_max_stream_data_bidi_remote(tc_ctx->quic_config, 10000000);
    quiche_config_set_initial_max_streams_bidi(tc_ctx->quic_config, 40000);
    quiche_config_set_cc_algorithm(tc_ctx->quic_config, QUICHE_CC_RENO);

    return 1;
}

int dtp_enable_debug_logging(void (*cb)(const char *line, void *argp),
                             void *argp) {
    return quiche_enable_debug_logging(cb, argp);
}

 

void dtp_conn_stats(dtp_layers_ctx *dtp_ctx, dtp_stats *out){

    quiche_conn_stats(dtp_ctx->quic_conn,&(out->quiche_stat));
}

void dtp_conn_on_timeout(dtp_tc_ctx * tc_ctx){
    quiche_conn_on_timeout(tc_ctx->quic_conn);
}

uint64_t dtp_conn_get_feedback(dtp_tc_ctx * tc_ctx,uint8_t * feedback){
    memcpy(&(tc_ctx->peer_RTT),feedback,sizeof(tc_ctx->peer_RTT));

    return tc_ctx->peer_RTT;
}

dtp_layers_ctx* dtp_layers_initnew_cli(uint32_t version){
    //dtp layers init
    dtp_layers_ctx * dtp_ctx=(dtp_layers_ctx * )malloc(sizeof(dtp_layers_ctx));
    if (dtp_ctx == NULL) {
        return NULL;
    }
    memset(dtp_ctx, 0, sizeof(dtp_layers_ctx));
    assert(dtp_assembler_init(dtp_ctx, DTP_BLOCK_INFO_AUTO) == 0);    
    assert(dtp_scheduler_init(dtp_ctx, (dtp_trans_mode) 0) == 0);      //only implement dgram sending.blockinque left NULL
    assert(dtp_tcontroler_init(dtp_ctx) == 0);

    //quiche conf

//     const int dgramRecvQueueLen=20;
//     const int dgramSendQueueLen=20;
//     quiche_config * config=quiche_config_new(version);
//     dtp_ctx->tc_ctx->quic_config=config;
  
//     quiche_config_load_cert_chain_from_pem_file(config, "./cert.crt");
//     quiche_config_load_priv_key_from_pem_file(config, "./cert.key");

//     quiche_config_set_application_protos(config,(uint8_t *)"\x0ahq-interop\x05hq-29\x05hq-28\x05hq-27\x08http/0.9", 38);

//     quiche_config_set_max_idle_timeout(config, 5000);
//     quiche_config_set_max_recv_udp_payload_size(config, MAX_DATAGRAM_SIZE);
//     quiche_config_set_max_send_udp_payload_size(config, MAX_DATAGRAM_SIZE);
//     quiche_config_set_initial_max_data(config, 1000000000);
//     quiche_config_set_initial_max_stream_data_uni(config, 10000000);
//     quiche_config_set_initial_max_streams_uni(config, 40000);
//     quiche_config_set_initial_max_stream_data_bidi_local(config, 10000000);
//     quiche_config_set_initial_max_stream_data_bidi_remote(config, 10000000);
//     quiche_config_set_initial_max_streams_bidi(config, 40000);
//     quiche_config_set_cc_algorithm(config, QUICHE_CC_RENO);
//     //test on dgram
//     quiche_config_enable_dgram(config, true, dgramRecvQueueLen,dgramSendQueueLen);


//      if (getenv("SSLKEYLOGFILE")) {
//     quiche_config_log_keys(config);
//   }


    return dtp_ctx;
}

dtp_layers_ctx * dtp_layers_initnew_serv(uint32_t version){
    //dtp layers init
    dtp_layers_ctx * dtp_ctx=(dtp_layers_ctx *)malloc(sizeof(dtp_layers_ctx));
    if (dtp_ctx == NULL) {
        return NULL;
    }
    memset(dtp_ctx, 0, sizeof(dtp_layers_ctx));
    assert(dtp_assembler_init(dtp_ctx, DTP_BLOCK_INFO_AUTO) == 0);
    assert(dtp_scheduler_init(dtp_ctx, (dtp_trans_mode) 0) == 0);      //only implement dgram sending.blockinque left NULL
    assert(dtp_tcontroler_init(dtp_ctx) == 0);

    //quiche conf

    // const int dgramRecvQueueLen=20;
    // const int dgramSendQueueLen=20;
    // quiche_config * config=quiche_config_new(version);
    // dtp_ctx->tc_ctx->quic_config=config;
  
     
    // quiche_config_load_cert_chain_from_pem_file(config, "./cert.crt");
    // quiche_config_load_priv_key_from_pem_file(config, "./cert.key");

    // quiche_config_set_application_protos(
    //     config,
    //     (uint8_t *)"\x0ahq-interop\x05hq-29\x05hq-28\x05hq-27\x08http/0.9", 38);

    // quiche_config_set_max_idle_timeout(config, 5000);
    // quiche_config_set_max_recv_udp_payload_size(config, MAX_DATAGRAM_SIZE);
    // quiche_config_set_max_send_udp_payload_size(config, MAX_DATAGRAM_SIZE);
    // quiche_config_set_initial_max_data(config, 1000000000);
    // quiche_config_set_initial_max_stream_data_uni(config, 10000000);
    // quiche_config_set_initial_max_streams_uni(config, 40000);
    // quiche_config_set_initial_max_stream_data_bidi_local(config, 10000000);
    // quiche_config_set_initial_max_stream_data_bidi_remote(config, 10000000);
    // quiche_config_set_initial_max_streams_bidi(config, 40000);
    // quiche_config_set_cc_algorithm(config, QUICHE_CC_RENO);
    // //test on dgram
    // quiche_config_enable_dgram(config, true, 20,20);



    return dtp_ctx;
}

void dtp_layers_free(dtp_layers_ctx * dtp_ctx){
    dtp_tcontroler_free(dtp_ctx);
    dtp_scheduler_free(dtp_ctx); 
    dtp_assembler_free(dtp_ctx);    
    // quiche_config_free(dtp_ctx->tc_ctx->quic_config);
}

//send buf via scheduler
ssize_t dtpl_conn_buf_send(dtp_layers_ctx *dtp_ctx,uint8_t * buf,uint64_t size,uint64_t priority,uint64_t deadline,int is_fragment){
    // assemble block and adds it into hash map
    dtp_assemble_block(dtp_ctx, dtp_ctx->schelay_ctx, dtp_ctx->assemlay_ctx,
                        size, priority, deadline, buf, is_fragment);    
    return dtpl_tc_conn_send(dtp_ctx);
}

/*
todo:通过control stream 传递 dgram ACK？
int dtpl_handle_datagram_ack_nack(dtpl_cnx_ctx_t* cnx_ctx, //quiche_call_back_event_t quiche_event, 
    uint64_t send_time, const uint8_t* bytes, size_t length, uint64_t current_time)
{
    int ret = 0;
     
    const uint8_t* bytes_max = bytes + length;
    uint64_t _id;
    uint64_t gi;
    uint64_t obji;
    uint64_t object_offset;
    uint64_t queue_delay;
    uint8_t flags;
    uint64_t nb_objects_previous;
    int is_last_fragment;
    const uint8_t* next_bytes;

    if (bytes == NULL) {
        ret = -1;
    }
    else {
        next_bytes = dtplDgramHdrdecode(bytes, bytes_max, &_id, &gi, &obji, &object_offset, &queue_delay, &flags, &nb_objects_previous, &is_last_fragment);
        
         
        if (next_bytes == NULL) {
            ret = -1;
        }
        else {
          
            dtpl_stream_ctx_t* stream_ctx = dtpl_findStreamCordgram(cnx_ctx, _id, 1);
          
           //pass ack through control stream?
          
        }
    }

    return ret;
}
*/
 
/*
int dtplRecvST(dtpl_stream_ctx_t* stream_ctx, uint8_t* bytes, size_t length, int is_fin)
{
    int ret = 0;

    while (ret == 0 && length > 0) {
      
        if (stream_ctx->receive_state == dtplRecv_done) {
         
            ret = -1;
            break;
        }
        else {
        
            int is_finished = 0;
            uint8_t* next_bytes = dtplMsgBufstore(bytes, length, &stream_ctx->message_receive, &is_finished);
            if (next_bytes == NULL) {
        
                ret = -1;
            }
            else
            {
                length = (bytes + length) - next_bytes;
                bytes = next_bytes;
                if (is_finished) {
                   
                    dtplMsgT incoming = { 0 };
                    const uint8_t* r_bytes = dtpl_msg_decode(stream_ctx->message_receive.buffer, stream_ctx->message_receive.buffer + stream_ctx->message_receive.msg_s, &incoming);

                    if (r_bytes == NULL) {
                         
                        ret = -1;
                    }
                  
                        if (stream_ctx->receive_state != dtplRecv_notify) {
  
                            ret = -1;
                        }
                        if (stream_ctx->_notify_fn != NULL) {
                            stream_ctx->_notify_fn(stream_ctx->notify_ctx, incoming.url, incoming.url_length);
                        }
                  
                    case DTPL_ACTION_CACHE_POLICY:
                        if (stream_ctx->receive_state != dtplRecv_fragment || stream_ctx->is_cache_real_time) {
                           
                            ret = -1;
                        }
 
  
                    }
              
                    dtplMsgBufreset(&stream_ctx->message_receive);
                }
            }
        }
    

    if (is_fin) {
       
        stream_ctx->is_peer_finished = 1;
        if (stream_ctx->is_local_finished) {
            dtpl_cnx_ctx_t* cnx_ctx = stream_ctx->cnx_ctx;
  
        }
       
    }

    return ret;
}
*/
   