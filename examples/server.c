#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <ev.h>
#include <uthash.h>

#include "dtp_config.h"
#include "log_helper.h"
#include <quiche.h>
#include <assert.h>

#include <argp.h>
#include <string.h>
#include <time.h>

#include "dtplayer.h"
#include "dtp_structure.h"
#include "block_util.h"
/***** Argp configs *****/

const char *argp_program_version = "dtptest-server 0.1";
static char doc[] = "dtptest-server -- a simple DTP test server";
static char args_doc[] = "LOCAL_IP LOCAL_PORT DTP_TRACE_FILE";

static struct argp_option options[] = {
    {"log", 'l', "FILE", 0, "Log to FILE instead of stderr"},
    {"out", 'o', "FILE", 0, "Write received data to FILE"},
    {"verbose", 'v', "LEVEL", 0, "Print verbose debug messages"},
    {"color", 'c', 0, 0, "Colorize log messages"},
    {"diffserv", 'd', 0, 0, "Enable DiffServ"},
    {"quic", 'q', 0, 0, "Use QUIC instead of DTP"},
    {0}
    
    };

struct arguments {
  FILE *log_file;
  FILE *out_file;
  char *local_ip;
  char *local_port;
  char *dtp_trace_file;
  char * gm_on;
};

static bool DIFFSERV_ENABLE = false;
static bool QUIC_ENABLE = false;

static struct arguments args;

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;
  switch (key) {
  case 'l':
    arguments->log_file = fopen(arg, "w+");
    break;
  case 'o':
    arguments->out_file = fopen(arg, "w+");
    break;
  case 'v':
    LOG_LEVEL = arg ? atoi(arg) : 3;
    break;
  case 'c':
    LOG_COLOR = 1;
    break;
  case 'd':
    DIFFSERV_ENABLE = true;
    break;
  case 'q':
    QUIC_ENABLE = true;
    break;
  case ARGP_KEY_ARG:
    switch (state->arg_num) {
    case 0: {
      arguments->local_ip = arg;
      break;
    }
    case 1: {
      arguments->local_port = arg;
    }
    case 2:
      arguments->dtp_trace_file = arg;
      break;
    case 3:
      arguments->gm_on = arg;
      break;
    default:
      argp_usage(state);
      break;
    }
    break;
  case ARGP_KEY_END:
    if (state->arg_num < 3)
      argp_usage(state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};

#undef HELPER_LOG
#undef HELPER_OUT
#define HELPER_LOG args.log_file
#define HELPER_OUT args.out_file

/***** DTP QUIC configs *****/

#define LOCAL_CONN_ID_LEN 16

#define MAX_DATAGRAM_SIZE 1350

#define MAX_BLOCK_SIZE 10000000

#define MAX_TOKEN_LEN                                                          \
  sizeof("quiche") - 1 + sizeof(struct sockaddr_storage) +                     \
      QUICHE_MAX_CONN_ID_LEN

struct connections {
  int sock;
  int ai_family;

  struct sockaddr_storage *local_addr;
  socklen_t local_addr_len;

  struct conn_io *h;
};

struct conn_io {
  ev_timer timer;
  ev_timer sender;
  ev_timer pacer;
  int send_round;

  int sock;

  uint8_t cid[LOCAL_CONN_ID_LEN];

  quiche_conn *conn;

  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len;

  dtp_config *cfgs;
  int cfg_len;

  dtp_layers_ctx * dtp_ctx;

  UT_hash_handle hh;

  sender_block *blocks;
};

static quiche_config *config = NULL;

static struct connections *conns = NULL;

uint64_t start_timestamp = 0;
uint64_t end_timestamp = 0;

uint64_t send_bytes = 0;
uint64_t complete_bytes = 0;
uint64_t good_bytes = 0;

/***** utilites *****/

static void mint_token(const uint8_t *dcid, size_t dcid_len,
                       struct sockaddr_storage *addr, socklen_t addr_len,
                       uint8_t *token, size_t *token_len) {
  memcpy(token, "quiche", sizeof("quiche") - 1);
  memcpy(token + sizeof("quiche") - 1, addr, addr_len);
  memcpy(token + sizeof("quiche") - 1 + addr_len, dcid, dcid_len);

  *token_len = sizeof("quiche") - 1 + addr_len + dcid_len;
}

static bool validate_token(const uint8_t *token, size_t token_len,
                           struct sockaddr_storage *addr, socklen_t addr_len,
                           uint8_t *odcid, size_t *odcid_len) {
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
  int rng = open("/dev/urandom", O_RDONLY);
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

static void debug_log(const char *line, void *argp) { log_trace("%s", line); }

void set_tos(int ai_family, int sock, int tos) {
  if (!DIFFSERV_ENABLE)
    return;

  switch (ai_family) {
  case AF_INET:
    if (setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
      log_error("failed to set TOS %s", strerror(errno));
    }
    break;
  case AF_INET6:
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_TCLASS, &tos, sizeof(tos)) < 0) {
      log_error("failed to set TOS %s", strerror(errno));
    }
    break;

  default:
    break;
  }
}



/***** callbacks *****/

static void timeout_cb(struct ev_loop *loop, ev_timer *w, int revents);
static void sender_cb(struct ev_loop *loop, ev_timer *w, int revents);
static void pacer_cb(struct ev_loop *loop, ev_timer *pacer, int revents);

static struct conn_io *create_conn(uint8_t *scid, size_t scid_len,
                                   uint8_t *odcid, size_t odcid_len,
                                   struct sockaddr_storage *peer_addr,
                                   socklen_t peer_addr_len) {
  log_debug("enter create_conn");
  struct conn_io *conn_io = calloc(1, sizeof(*conn_io));
  if (conn_io == NULL) {
    log_error("failed to allocate connection IO");
    return NULL;
  }
  // int rng = open("/dev/urandom", O_RDONLY);
  // if (rng < 0) {
  //     perror("failed to open /dev/urandom");
  //     return NULL;
  // }

  // ssize_t rand_len = read(rng, conn_io->cid, LOCAL_CONN_ID_LEN);
  // if (rand_len < 0) {
  //     perror("failed to create connection ID");
  //     return NULL;
  // }
  // log_debug("before accept");
  // log_debug("conn_io->cid: ");
  // for(int i = 0; i < LOCAL_CONN_ID_LEN; i++) {
  //   fprintf(stderr, "%x", conn_io->cid[i]);
  // }
  // fprintf(stderr, "\n");
  // log_debug("scid: ");
  // for(int i = 0; i < scid_len; i++) {
  //   fprintf(stderr, "%x", scid[i]);
  // }
  // fprintf(stderr, "\n");
  // log_debug("odcid: ");
  // for(int i = 0; i < odcid_len; i++) {
  //   fprintf(stderr, "%x", odcid[i]);
  // }
  // fprintf(stderr, "\n");
  memcpy(conn_io->cid, scid, scid_len);
  quiche_conn *conn = quiche_accept(scid, scid_len, 
                                    odcid, odcid_len,
                                    conns->local_addr, conns->local_addr_len,
                                    peer_addr, peer_addr_len,
                                    config);
  if (conn == NULL) {
      fprintf(stderr, "failed to create connection\n");
      return NULL;
  }
  log_debug("before dtp_layer");
  // init dtp_layer_ctx
  dtp_layers_ctx * dtp_ctx = dtp_layers_initnew_serv(QUICHE_PROTOCOL_VERSION);
  if (dtp_ctx == NULL) {
    log_error("failed to create dtp_context");
    return -1;
  }
  log_debug("after dtp layer");
  conn_io->dtp_ctx = dtp_ctx;
  conn_io->sock = conns->sock;
  conn_io->conn = conn;
  // * we move some dtp_ctx init outside the function
  conn_io->dtp_ctx->quic_conn = conn;
  conn_io->dtp_ctx->tc_ctx->quic_conn = conn;
  conn_io->dtp_ctx->tc_ctx->quic_config = config;
  log_debug("before copying");

  memcpy(&conn_io->peer_addr, peer_addr, peer_addr_len);
  conn_io->peer_addr_len = peer_addr_len;
  log_debug("after copying");

  dtp_config *cfgs = parse_dtp_config(args.dtp_trace_file, &conn_io->cfg_len);
  if (!cfgs || conn_io->cfg_len <= 0) {
    log_error("failed to parse dtp config");
    return NULL;
  }
  log_debug("after parse config");
  conn_io->cfgs = cfgs;
  conn_io->send_round = -1;

  conn_io->blocks = calloc(conn_io->cfg_len, sizeof(sender_block));
  for(int i = 0; i < conn_io->cfg_len; ++i) {
    conn_io->blocks[i].block_hdr.block_id = i;
    conn_io->blocks[i].block_hdr.priority = conn_io->cfgs[i].priority;
    conn_io->blocks[i].block_hdr.deadline = conn_io->cfgs[i].deadline;
    conn_io->blocks[i].block_hdr.size     = conn_io->cfgs[i].size;
    conn_io->blocks[i].block_hdr.start_at = 0;
    conn_io->blocks[i].block_hdr.send_at  = 0;
  }

  ev_init(&conn_io->timer, timeout_cb);
  conn_io->timer.data = conn_io;

  ev_init(&conn_io->sender, sender_cb);
  conn_io->sender.data = conn_io;

  ev_init(&conn_io->pacer, pacer_cb);
  conn_io->pacer.data = conn_io;

  HASH_ADD(hh, conns->h, cid, LOCAL_CONN_ID_LEN, conn_io);

  log_info("new connection");

  return conn_io;
}

static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io) {
  static uint8_t out[MAX_DATAGRAM_SIZE];

  quiche_send_info send_info;

  while (1) {
    log_debug("enter flush");
    ssize_t written =
        quiche_conn_send(conn_io->conn, out, sizeof(out), &send_info);

    if (written == DTP_ERR_DONE) {
      log_debug("done writing");
      break;
    }

    if (written < 0) {
      log_error("failed to create packet: %zd", written);
      return;
    }

    // set_tos(conn_io->peer_addr.ss_family, conn_io->sock,
    //         send_info.diffserv << 2);
    ssize_t sent = sendto(conn_io->sock, out, written, 0,
                          (struct sockaddr *)&send_info.to, send_info.to_len);

    if (sent != written) {
      log_error("failed to send %s", strerror(errno));
      return;
    }

    log_debug("sent %zd bytes", sent);
  }

  double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
  conn_io->timer.repeat = t;
  ev_timer_again(loop, &conn_io->timer);

  struct timespec now = {0, 0};
  clock_gettime(CLOCK_REALTIME, &now);

  double repeat = (send_info.at.tv_sec - now.tv_sec) +
                          (send_info.at.tv_nsec - now.tv_nsec) / 1e9f;
  conn_io->pacer.repeat = repeat > 0 ? repeat : 0.001;
  // conn_io->pacer.repeat = 0.0001;
  ev_timer_again(loop, &conn_io->pacer);
  log_debug("leave flush");
}

static void pacer_cb(struct ev_loop *loop, ev_timer *pacer, int revents) {
  log_debug("flush egress pace triggered");
  struct conn_io *conn_io = pacer->data;
  flush_egress(loop, conn_io);
}

// use quiche to send data from the queue conn_io->blocks
// try to send data if possible
static void data_sender(struct ev_loop *loop, struct conn_io *conn_io) {
  static uint8_t buf[MAX_BLOCK_SIZE];
  static size_t send_offset = 0;
  static size_t sending_block = 0;

  ssize_t sent = 0;

  if(QUIC_ENABLE) {
    for(;sending_block < conn_io->send_round; ++sending_block) {

      int stream_id = 4 * (sending_block + 1) + 1;

      if(send_offset == 0) {
        conn_io->blocks[sending_block].block_hdr.send_at = get_current_usec(); //? what if sent < 0?
      }

      quiche_block block = conn_io->blocks[sending_block].block_hdr;

      int hdr_len = encode_block_hdr(buf, MAX_BLOCK_SIZE, block);
      if(hdr_len <= 0) {
        log_error("hdr_len <= 0: %d", hdr_len);
        perror("hdr_len <= 0");
      }
      log_debug("sending stream %d", stream_id);

      log_debug("block %d info: size: %lu, priority: %lu, deadline: %lu, started_at: %lu, send_at: %lu", 
                sending_block,
                block.size,
                block.priority,
                block.deadline,
                block.start_at,
                block.send_at
                );
      sent = quiche_conn_stream_send(conn_io->conn, 
                                     stream_id, 
                                     buf + send_offset,
                                     hdr_len + block.size - send_offset, 
                                     true);
                                        
      if(sent > 0 && sent + send_offset < block.size + hdr_len) {
        log_debug("failed to send block %d completely: sent %zd",
                sending_block, sent);
        log_debug("expect: %ld, sent: %ld, offset+sent: %ld", block.size + hdr_len, sent, sent+send_offset);
        send_offset += sent;
        break;
      } else if(sent == QUICHE_ERR_DONE) {
        log_debug("failed to send block %d, done", sending_block);
        break;
      } else if(sent < 0) {
        log_error("failed to send block %d : sent %zd",
                sending_block, sent);
        break;
      } else {
        // finish sending
        conn_io->blocks[sending_block].send_finish = get_current_usec();
        log_debug("finish sending block %d, begin: %lu, end: %lu, duration: %lu",
          sending_block,
          conn_io->blocks[sending_block].send_begin,
          conn_io->blocks[sending_block].send_finish
        );
        send_offset = 0;
      }
    }
  } else {
    size_t size = dtpl_tc_conn_send(conn_io->dtp_ctx);
    log_debug("dtpl_tc_conn_send, send: %d", size);
  }
  flush_egress(loop, conn_io);
}
// emulate application data generator
// push data into sending queue to wait `quiche` to send them
static void sender_cb(struct ev_loop *loop, ev_timer *w, int revents) {
  struct conn_io *conn_io = w->data;
  log_debug("enter sender_cb");
  if (quiche_conn_is_established(conn_io->conn)) {
    log_debug("established");
    for(;conn_io->send_round < conn_io->cfg_len; conn_io->send_round++) {
      float send_time_gap = conn_io->cfgs[conn_io->send_round].send_time_gap;

      if(conn_io->blocks[conn_io->send_round].block_hdr.start_at == 0) {
        conn_io->blocks[conn_io->send_round].block_hdr.start_at = get_current_usec();
        conn_io->blocks[conn_io->send_round].send_begin = get_current_usec();
      }

      log_debug(
        "block %d is forming, timestamp: %lu", 
        conn_io->send_round, 
        conn_io->blocks[conn_io->send_round].send_begin
      );

      if (QUIC_ENABLE) {
        // pass
      } else {
        // sent = quiche_conn_block_send(conn_io->conn, stream_id, buf,block.size, true, &block);
        quiche_block block = conn_io->blocks[conn_io->send_round].block_hdr;
        static uint8_t buf[MAX_BLOCK_SIZE];
        int hdr_len = encode_block_hdr(buf, MAX_BLOCK_SIZE, block);
        assert(hdr_len > 0);
        log_debug("block info: size: %ld, priority: %ld, deadline: %ld, started_at: %ld", 
                  block.size,
                  block.priority,
                  block.deadline,
                  block.start_at
                  );
        // create block
        int r = dtp_assemble_block(
          conn_io->dtp_ctx, 
          conn_io->dtp_ctx->schelay_ctx, 
          conn_io->dtp_ctx->assemlay_ctx,
          block.size, 
          block.priority,
          block.deadline, 
          buf, 
          true);

        if(r != 1) {
          log_error("failed to create block, r: %d", r);
          break;
        }
      }
      
      if(send_time_gap < 0.000010) {
        continue;
      } else {
        conn_io->sender.repeat = send_time_gap;
        ev_timer_again(loop, &conn_io->sender);
        conn_io->send_round += 1;
        break;
      }
    }
    if (conn_io->send_round >= conn_io->cfg_len) {
      ev_timer_stop(loop, &conn_io->sender);
    }
  }
  
  data_sender(loop, conn_io);
  // flush_egress(loop, conn_io);
}

static void recv_cb(struct ev_loop *loop, ev_io *w, int revents) {
  log_debug("enter recv cb");
  struct conn_io *tmp, *conn_io = NULL;

  static uint8_t buf[MAX_BLOCK_SIZE];
  static uint8_t out[MAX_DATAGRAM_SIZE];

  while (1) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, peer_addr_len);

    ssize_t read = recvfrom(conns->sock, buf, sizeof(buf), 0,(struct sockaddr *)&peer_addr, &peer_addr_len);

    log_debug("read %ld bytes", read);

    if (read < 0) {
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
        log_debug("recv would block");
        break;
      }

      log_error("failed to read %s", strerror(errno));
      return;
    }

    uint8_t type;
    uint32_t version;

    uint8_t scid[QUICHE_MAX_CONN_ID_LEN];
    size_t scid_len = sizeof(scid);

    uint8_t dcid[QUICHE_MAX_CONN_ID_LEN];
    size_t dcid_len = sizeof(dcid);

    uint8_t odcid[QUICHE_MAX_CONN_ID_LEN];
    size_t odcid_len = sizeof(odcid);

    uint8_t token[MAX_TOKEN_LEN];
    size_t token_len = sizeof(token);

    int rc = quiche_header_info(buf, read, LOCAL_CONN_ID_LEN, &version,
                                    &type, scid, &scid_len, dcid, &dcid_len,
                                    token, &token_len);
    if (rc < 0) {
      log_error("failed to parse header: %d", rc);
      continue;
    }

    HASH_FIND(hh, conns->h, dcid, dcid_len, conn_io);

    if (conn_io == NULL) {
      if (!quiche_version_is_supported(version)) {
        log_debug("version negotiation");
        ssize_t written = quiche_negotiate_version(
                    scid, scid_len, dcid, dcid_len, out, sizeof(out));

        if (written < 0) {
          log_error("failed to create vneg packet: %zd", written);
          continue;
        }

        // set_tos(conns->ai_family, conns->sock, 5 << 5);
        ssize_t sent =
                    sendto(conns->sock, out, written, 0,
                           (struct sockaddr *)&peer_addr, peer_addr_len);
        if (sent != written) {
          log_error("failed to send %s", strerror(errno));
          continue;
        }

        send_bytes += sent;

        log_debug("sent %zd bytes", sent);
        continue;
      }

      if (token_len == 0) {
        log_debug("stateless retry");

        mint_token(dcid, dcid_len, &peer_addr, peer_addr_len, token,
                   &token_len);

        uint8_t new_cid[LOCAL_CONN_ID_LEN];

        if (gen_cid(new_cid, LOCAL_CONN_ID_LEN) == NULL) {
          continue;
        }

        ssize_t written = quiche_retry(scid, scid_len, dcid, dcid_len, new_cid, //省略
                                       LOCAL_CONN_ID_LEN, token, token_len,
                                       version, out, sizeof(out));

        if (written < 0) {
          log_error("failed to create retry packet: %zd", written);
          continue;
        }

        // set_tos(conns->ai_family, conns->sock, 5 << 5);
        ssize_t sent = sendto(conns->sock, out, written, 0,
                              (struct sockaddr *)&peer_addr, peer_addr_len);
        if (sent != written) {
          log_error("failed to send %s", strerror(errno));
          continue;
        }

        log_debug("sent %zd bytes", sent);
        continue;
      }

      if (!validate_token(token, token_len, &peer_addr, peer_addr_len, odcid,
                          &odcid_len)) {
        log_error("invalid address validation token");
        continue;
      }

      conn_io = create_conn(dcid, dcid_len, odcid, odcid_len, &peer_addr,
                            peer_addr_len);

      if (conn_io == NULL) {
        continue;
      }
    }

    log_debug("have conn_io");  
    quiche_recv_info recv_info = {
      .from=(struct sockaddr *)&peer_addr,
      .from_len=peer_addr_len,
      .to=conns->local_addr,
      .to_len=conns->local_addr_len,
    };

    ssize_t done = quiche_conn_recv(conn_io->conn, buf, read, &recv_info);

    log_debug("after recv");

    if (done < 0) {
      log_error("failed to process packet: %zd", done);
      continue;
    }

    log_debug("recv %zd bytes", done);
    //Some stream or dgrams or conn might be set up.and check if there are any.
    
    if(quiche_conn_is_established(conn_io->conn)) {
      log_debug("established");
      if (conn_io->send_round == -1) {
        conn_io->send_round = 0;
        conn_io->sender.repeat = conn_io->cfgs[0].send_time_gap;
        ev_timer_again(loop, &conn_io->sender);
        if(QUIC_ENABLE) {
          memcpy(out, &conn_io->cfg_len, sizeof(int));
          quiche_conn_stream_send(conn_io->conn, QUIC_INFO_STREAM, out, sizeof(int), false);
        } else {
          memcpy(out, &conn_io->cfg_len, sizeof(int));
          dtp_tc_control_flow_send(conn_io->dtp_ctx->tc_ctx,
            out, sizeof(int), false);
        }
      }

      uint64_t s = 0;

      quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);

      if(!QUIC_ENABLE) {
        // static uint8_t block_buf[MAX_BLOCK_SIZE];
        // dtp_tc_conn_block_recv(conn_io->dtp_ctx, block_buf);


        // for(uint64_t offset=0; offset<(conn_io->dtp_ctx->tc_ctx->off_array_num); offset++) {
        //   uint64_t offstart=conn_io->dtp_ctx->tc_ctx->offset_arrived[offset];
        //   log_debug("Dgram offset = %lu",offstart);
        //   for(int i=0;i<MAX_DATAGRAM_SIZE;i++) {
        //     printf("%c",block_buf[offstart+i]);
        //   }
        // }

        // TODO: add control flow send
        // dtp_tc_control_flow_check(conn_io->dtp_ctx->tc_ctx);
        // dtp_tc_control_flow_send(conn_io->dtp_ctx,block_buf,sizeof(block_buf),false);
      }

      while (quiche_stream_iter_next(readable, &s)) {
        log_debug("stream %" PRIu64 " is readable", s);

        bool fin = false;
        ssize_t recv_len =
          quiche_conn_stream_recv(conn_io->conn, s, buf, sizeof(buf), &fin);
        if (recv_len < 0) {
          break;
        }
      }

      quiche_stream_iter_free(readable);

      //! send data
      data_sender(loop, conn_io);
    }
  }

  log_debug("outside loop");
  HASH_ITER(hh, conns->h, conn_io, tmp) {
    log_debug("inside hash");
    flush_egress(loop, conn_io);

    if (quiche_conn_is_closed(conn_io->conn)) {
      quiche_stats stats;

      quiche_conn_stats(conn_io->conn, &stats);
      dump_info("connection closed, recv=%zu sent=%zu lost=%zu rtt=?ns cwnd=??\n",
                stats.recv, stats.sent, stats.lost);
      fflush(NULL);

      HASH_DELETE(hh, conns->h, conn_io);

      ev_timer_stop(loop, &conn_io->timer);
      ev_timer_stop(loop, &conn_io->sender);
      ev_timer_stop(loop, &conn_io->pacer);
      quiche_conn_free(conn_io->conn);
      dtp_layers_free(conn_io->dtp_ctx);
      free(conn_io->cfgs);
      free(conn_io->blocks);
      free(conn_io);
    }
  }
}

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
  struct conn_io *conn_io = w->data;
  quiche_conn_on_timeout(conn_io->conn);

  log_debug("timeout");  

  flush_egress(loop, conn_io);

  if (quiche_conn_is_closed(conn_io->conn)) {
    quiche_stats stats;

    quiche_conn_stats(conn_io->conn,&stats);
    dump_info("connection closed, recv=%zu sent=%zu lost=%zu rtt=??ns cwnd=??\n",
              stats.recv, stats.sent, stats.lost);
    fflush(NULL); 

    HASH_DELETE(hh, conns->h, conn_io);

    ev_timer_stop(loop, &conn_io->timer);
    ev_timer_stop(loop, &conn_io->sender);
    ev_timer_stop(loop, &conn_io->pacer);
    quiche_conn_free(conn_io->conn);
    dtp_layers_free(conn_io->dtp_ctx);
    free(conn_io->cfgs);
    free(conn_io->blocks);
    free(conn_io);
    return;
  }
}

int main(int argc, char *argv[]) {
  args.out_file = stdout;
  args.log_file = stdout;
  argp_parse(&argp, argc, argv, 0, 0, &args);
  log_info("SERVER_IP:PORT %s:%s DTP_TRACE_FILE %s", args.local_ip,
           args.local_port, args.dtp_trace_file);

   struct addrinfo hints = {.ai_family = AF_UNSPEC,
                                 .ai_socktype = SOCK_DGRAM,
                                 .ai_protocol = IPPROTO_UDP};

  dtp_enable_debug_logging(debug_log, NULL);

  struct addrinfo *local;
  int err = getaddrinfo(args.local_ip, args.local_port, &hints, &local);
  if (err != 0) {
    log_error("getaddrinfo: %s", gai_strerror(err));
    return -1;
  }

  int sock =socket(local->ai_family, local->ai_socktype, local->ai_protocol);
  if (sock < 0) {
    log_error("create socket");
    return -1;
  }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
    log_error("fcntl");
    return -1;
  }

  if (bind(sock, local->ai_addr, local->ai_addrlen) < 0) {
    log_error("bind %s", strerror(errno));
    return -1;
  }

  config = quiche_config_new(QUICHE_PROTOCOL_VERSION);
  if (config == NULL) {
    // fprintf(stderr, "failed to create config\n");
    return -1;
  }
  quiche_config_load_cert_chain_from_pem_file(config, "./cert.crt");
  quiche_config_load_priv_key_from_pem_file(config, "./cert.key");
  quiche_config_set_application_protos(
    config,
    (uint8_t *)"\x0ahq-interop\x05hq-29\x05hq-28\x05hq-27\x08http/0.9", 38);

  const int dgramRecvQueueLen=MAX_BLOCK_SIZE / 1250;
  const int dgramSendQueueLen=MAX_BLOCK_SIZE;

  quiche_config_set_max_idle_timeout(config, 10000);
  quiche_config_set_max_recv_udp_payload_size(config, MAX_DATAGRAM_SIZE);
  quiche_config_set_max_send_udp_payload_size(config, MAX_DATAGRAM_SIZE);
  quiche_config_set_initial_max_data(config, 1000000000);
  quiche_config_set_max_connection_window(config, 1000000000);
  quiche_config_set_initial_max_stream_data_uni(config, 1000000000);
  quiche_config_set_initial_max_streams_uni(config, 1000000000);
  quiche_config_set_initial_max_stream_data_bidi_local(config, 1000000000);
  quiche_config_set_initial_max_stream_data_bidi_remote(config, 1000000000);
  quiche_config_set_initial_max_streams_bidi(config, 1000000000);
  quiche_config_set_cc_algorithm(config, QUICHE_CC_CUBIC);
  // enable dgram
  quiche_config_enable_dgram(config, true, dgramRecvQueueLen, dgramSendQueueLen); 

  struct connections c;
  c.sock = sock;
  c.ai_family = local->ai_family;
  c.local_addr = local->ai_addr;
  c.local_addr_len = local->ai_addrlen;
  c.h = NULL;

  conns = &c;

  
  ev_io watcher;

  struct ev_loop *loop = ev_default_loop(0);

  ev_io_init(&watcher, recv_cb, sock, EV_READ);
  
  ev_io_start(loop, &watcher);
  watcher.data = &c;

  ev_loop(loop, 0);

  freeaddrinfo(local);

  quiche_config_free(config);

  struct conn_io *tmp, *conn_io = NULL;
  HASH_ITER(hh, conns->h, conn_io, tmp) {
    if (true) {
      HASH_DELETE(hh, conns->h, conn_io);

      ev_timer_stop(loop, &conn_io->timer);
      ev_timer_stop(loop, &conn_io->sender);
      ev_timer_stop(loop, &conn_io->pacer);
      quiche_conn_free(conn_io->conn);
      dtp_layers_free(conn_io->dtp_ctx);
      free(conn_io->cfgs);
      free(conn_io->blocks);
      free(conn_io);
    }
  }
  return 0;
}