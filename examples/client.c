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


#include <quiche.h>
#include "dtp_config.h"
#include "log_helper.h"
#include <argp.h>
#include <string.h>
#include <assert.h>

#include <time.h>

#include "dtplayer.h"
#include "dtp_structure.h"
#include "block_util.h"

#define min(a,b) (a < b ? a : b)
/***** Argp configs *****/

const char *argp_program_version = "dtptest-client 0.1";
static char doc[] = "dtptest-client -- a simple DTP test client";
static char args_doc[] = "SERVER_IP PORT LOCAL_IP LOCAL_PORT";

static struct argp_option options[] = {
    {"log", 'l', "FILE", 0, "Log to FILE instead of stderr"},
    {"out", 'o', "FILE", 0, "Write received data to FILE"},
    {"verbose", 'v', "LEVEL", 0, "Print verbose debug messages"},
    {"color", 'c', 0, 0, "Colorize log messages"},
    {"diffserv", 'd', 0, 0, "Enable DiffServ"},
    {"quic", 'q', 0, 0, "Use QUIC instead of DTP"},
    {0}};

struct arguments {
  FILE *log_file;
  FILE *out_file;
  char *server_ip;
  char *server_port;
  char *local_ip;
  char *local_port;
  char *gm_on;
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
        arguments->server_ip = arg;
        break;
      }
      case 1: {
        arguments->server_port = arg;
        break;
      }
      case 2: {
        arguments->local_ip = arg;
        break;
      }
      case 3: {
        arguments->local_port = arg;
        break;
      }
      case 4: {
        arguments->gm_on = arg;
        break;
      }
      default:
        argp_usage(state);
        break;
    }
    break;
  case ARGP_KEY_END:
    if (state->arg_num < 2)
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

uint64_t total_bytes = 0;
uint64_t total_udp_bytes = 0;
uint64_t started_at = 0;
uint64_t ended_at = 0;

struct 
conn_io {
  ev_timer timer;
  ev_timer pacer;

  int sock;
  int ai_family;
  struct sockaddr* local_addr;
  socklen_t local_addr_len;

  quiche_conn *conn;
  dtp_layers_ctx * dtp_ctx;

  int cfg_len;
  block_stats *blocks;
  int blocks_len;
};

/***** utilites *****/

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

static void flush_egress(struct ev_loop *loop, struct conn_io *conn_io) {
  static uint8_t out[MAX_DATAGRAM_SIZE];

  quiche_send_info send_info;

  while (1) {
    ssize_t written =
        quiche_conn_send(conn_io->conn, out, MAX_DATAGRAM_SIZE, &send_info);

    if (written == DTP_ERR_DONE) {
      log_debug("done writing");
      break;
    }

    if (written < 0) {
      log_error("failed to create packet: %zd", written);
      return;
    }

    // set_tos(conn_io->ai_family, conn_io->sock, send_info.diffserv << 2);
    ssize_t sent = sendto(conn_io->sock, out, written, 0,
                          (struct sockaddr *)&send_info.to, send_info.to_len);
    
    if (sent != written) {
      log_error("failed to send %s", strerror(errno));
      return;
    }

    log_debug("sent %zd bytes", sent);
  }

  double t = quiche_conn_timeout_as_nanos(conn_io->conn) / 1e9f;
  if (t != 0) {
    conn_io->timer.repeat = t;
    ev_timer_again(loop, &conn_io->timer);
  }

  struct timespec now = {0, 0};
  clock_gettime(CLOCK_REALTIME, &now);

  double repeat = (send_info.at.tv_sec - now.tv_sec) +
                  (send_info.at.tv_nsec - now.tv_nsec) / 1e9f;
  conn_io->pacer.repeat = repeat > 0 ? repeat : 0.001;
  // conn_io->pacer.repeat = 0.0001;
  ev_timer_again(loop, &conn_io->pacer);
}

static void pacer_cb(struct ev_loop *loop, ev_timer *pacer, int revents) {
  log_debug("flush egress pace triggered");
  struct conn_io *conn_io = pacer->data;
  flush_egress(loop, conn_io);
}

static void recv_cb(EV_P_ ev_io *w, int revents) {
  // static bool req_sent = false;
  log_debug("recv_cb");

  struct conn_io *conn_io = w->data;

  static uint8_t buf[MAX_BLOCK_SIZE];

  while (1) {
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, peer_addr_len);

    ssize_t read = recvfrom(conn_io->sock, buf, sizeof(buf), 0,
                            (struct sockaddr *)&peer_addr, &peer_addr_len);
    log_debug("read %ld bytes", read);
    log_debug("recv_cb 1");
    if (read < 0) {
      if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
        log_debug("recv would block");
        break;
      }

      log_error("failed to read %s", strerror(errno));
      return;
    }

    total_udp_bytes += read;

    log_debug("recv_cb 2");

    quiche_recv_info recv_info = {
      .from=(struct sockaddr *)&peer_addr,
      .from_len=peer_addr_len,
      .to=conn_io->local_addr,
      .to_len=conn_io->local_addr_len,
    };
     
    ssize_t done = quiche_conn_recv(conn_io->conn, buf, read, &recv_info);
    //quiche_conn_recv(conn_io->conn, buf, read, &recv_info);
    log_debug("recv_cb 3");
    if (done < 0) {
      log_error("failed to process packet %ld", done);
      continue;
    }

    log_debug("recv %zd bytes", done);
  }

  log_debug("done reading");
  //todo:hide all var behind dtp?
  if(quiche_conn_is_closed(conn_io->conn)) {
    log_info("connection closed");

    quiche_stats stats;

    quiche_conn_stats(conn_io->dtp_ctx, &stats);

    log_info("connection closed, recv=%zu sent=%zu lost=%zu rtt=?\
             ns total_bytes=%zu total_udb_bytes=%zu total_time=%zu",
             stats.recv, stats.sent, stats.lost, total_bytes,
             total_udp_bytes, ended_at - started_at);
    fflush(NULL);

    ev_break(EV_A_ EVBREAK_ONE);
    return;
  }

  if (quiche_conn_is_established(conn_io->conn)) {
    if(!QUIC_ENABLE) {
      static uint8_t block_buf[MAX_BLOCK_SIZE];
      dtpl_tc_conn_recv(conn_io->dtp_ctx);

      // for(uint64_t offset=0;offset<(conn_io->dtp_ctx->tc_ctx->off_array_num);offset++) {
      //   uint64_t offstart=conn_io->dtp_ctx->tc_ctx->offset_arrived[offset];
      //   log_debug("Dgram offset = %lu",offstart);
      //   for(int i=0;i<MAX_DATAGRAM_SIZE;i++) {
      //     printf("%c",block_buf[offstart+i]);
      //   }
      // }

      // dtp_tc_control_flow_check(conn_io->dtp_ctx->tc_ctx);

      // dtp_tc_control_flow_send(conn_io->dtp_ctx, buf, sizeof(buf),true);
      log_trace("after block recv");
      // check blocks
      if(HASH_COUNT(conn_io->dtp_ctx->tc_ctx->recv_blocks)) {
        bmap_element **recv_blocks = &conn_io->dtp_ctx->tc_ctx->recv_blocks;
        bmap_element *iter, *tmp;
        // TODO: beter iter
        HASH_ITER(hh, *recv_blocks, iter, tmp) {
          // finish condition
          if(!iter->is_read && iter->block->total_recv >= iter->block->size) {
            ended_at = get_current_usec();
            dump_file("%lu, %lu, %lu, %lu, %lu, %lf, %lu, %lu, %lu\n", 
                        iter->block->id, 
                        (ended_at - iter->block->begin_to_send) / 1000,
                        (ended_at - iter->block->t) / 1000,
                        iter->block->size,
                        iter->block->size,
                        1.0,
                        iter->block->priority,
                        iter->block->deadline,
                        ended_at - started_at);
            bmap_delete(recv_blocks, iter->id);
            conn_io->blocks_len += 1;
          } 
          else if(!iter->is_read && 
            ((get_current_usec() - iter->block->t) / 1000) >= iter->block->deadline &&
            (double)iter->block->total_recv / (double) iter->block->size >= 0.9) {
            log_debug("size: %lu", iter->block->size);
            // deadline condition
            ended_at = get_current_usec();
            dump_file("%lu, %lu, %lu, %lu, %lu, %lf, %lu, %lu, %lu\n", 
                        iter->id, 
                        (ended_at - iter->block->begin_to_send) / 1000,
                        (ended_at - iter->block->t) / 1000,
                        iter->block->size,
                        iter->block->total_recv,
                        (double)iter->block->total_recv / iter->block->size,
                        iter->block->priority,
                        iter->block->deadline,
                        ended_at - started_at);
            bmap_lazy_delete(recv_blocks, iter->id);
            conn_io->blocks_len += 1;
          }
        }
        if(conn_io->cfg_len > 0 && conn_io->blocks_len >= conn_io->cfg_len) {
          quiche_conn_close(conn_io->conn, true, 0, "done", sizeof("done"));
        }
      }
    }

    uint64_t s = 0;
    quiche_stream_iter *readable = quiche_conn_readable(conn_io->conn);
    log_trace("after readable, readable: %p", readable);

    // control flow
    if(!QUIC_ENABLE) {
      if(conn_io->cfg_len == 0) {
        bool fin = false;
        ssize_t recv_len = dtp_tc_control_flow_recv(
          conn_io->dtp_ctx->tc_ctx,
          buf,
          sizeof(buf),
          &fin
        );
        if(recv_len >= (ssize_t)sizeof(int)) {
          //? what if recv_len < sizeof(int) ?
          memcpy(&conn_io->cfg_len, buf, sizeof(int));
          conn_io->blocks_len = 0;
        }
      }
    }

    while (quiche_stream_iter_next(readable, &s)) {
      log_debug("stream %" PRIu64 " is readable", s);
      bool fin = false;
      ssize_t recv_len = quiche_conn_stream_recv(conn_io->conn, s, buf, sizeof(buf), &fin);
      //dgram try
      //problem:How can I formed them into an totally shaped block?
      //if a divided block:need block id
      //if a total block:
      //
      //get feed back
      if (recv_len < 0) {
        break;
      }
      // TODO: control flow feedback?
      // if(s == conn_io->dtp_ctx->tc_ctx->control_stream_id){
      //   uint64_t rtt=dtp_conn_get_feedback(conn_io->dtp_ctx->tc_ctx,buf);
      //   printf("rtt %lu",rtt);
      // }

      total_bytes += recv_len;
      //  log_debug("收到数据(%d)->%.*s\n", (int)recv_len, buf);
      log_debug("recv %ld bytes", recv_len);

      if(QUIC_ENABLE) {
        if(s == QUIC_INFO_STREAM) {
          memcpy(&conn_io->cfg_len, buf, sizeof(int));
          log_info("recv cfg_len: %d", conn_io->cfg_len);
          conn_io->blocks = calloc(conn_io->cfg_len, sizeof(block_stats));
          conn_io->blocks_len = 0;
        } else {
          long block_id = ((s - 1) / 4) - 1;
          assert(conn_io->blocks);
          if(!conn_io->blocks[block_id].has_hdr) {
            log_debug("recv_len: %d", recv_len);
            if(conn_io->blocks[block_id].recv_size < sizeof(quiche_block)) {
              log_debug("%d recv_size: %d", block_id, conn_io->blocks[block_id].recv_size);
              memcpy(conn_io->blocks[block_id].hdr_buf + conn_io->blocks[block_id].recv_size, 
                    buf,
                    min(sizeof(quiche_block) - conn_io->blocks[block_id].recv_size, recv_len)
              );
            }
            if(conn_io->blocks[block_id].recv_size + recv_len >= sizeof(quiche_block)) {
              int hdr_len = decode_block_hdr(conn_io->blocks[block_id].hdr_buf, MAX_BLOCK_SIZE, &conn_io->blocks[block_id].block_hdr);
              // log_debug("hdr_len: %d", hdr_len);
              if(hdr_len != sizeof(quiche_block)) {
                log_error("hdr_len %d != sizeof(quiche_block) %d", hdr_len, sizeof(quiche_block));
                perror("hdr_len != sizeof(quiche_block)");
              }
              conn_io->blocks[block_id].has_hdr = true;
              log_debug("get hdr for %lu: %lu, %lu, %lu, %lu, %lu, %lu",
                block_id,
                conn_io->blocks[block_id].block_hdr.block_id,
                conn_io->blocks[block_id].block_hdr.size,
                conn_io->blocks[block_id].block_hdr.priority,
                conn_io->blocks[block_id].block_hdr.deadline,
                conn_io->blocks[block_id].block_hdr.start_at,
                conn_io->blocks[block_id].block_hdr.send_at
              );
              conn_io->blocks[block_id].recv_size += recv_len - hdr_len;
            } else {
              conn_io->blocks[block_id].recv_size += recv_len;
            }
          } else {
            conn_io->blocks[block_id].recv_size += recv_len;
          }
        }
      } else {
        // TODO
      }
      if (fin) {
        ended_at = get_current_usec();

        if(QUIC_ENABLE) {
          long block_id = ((s - 1) / 4) - 1;
          if(s != QUIC_INFO_STREAM) {
            log_debug("recv_size: %ld, block_size: %ld", 
                      conn_io->blocks[block_id].recv_size,
                      conn_io->blocks[block_id].block_hdr.size);
            if(conn_io->blocks[block_id].recv_size != conn_io->blocks[block_id].block_hdr.size) {
              log_error("recv_size: %lu, size: %lu", conn_io->blocks[block_id].recv_size, conn_io->blocks[block_id].block_hdr.size);
              perror("recv_size != block.size");
            }
            conn_io->blocks[block_id].bct = (ended_at - conn_io->blocks[block_id].block_hdr.send_at) / 1000;
            conn_io->blocks[block_id].lifetime = (ended_at - conn_io->blocks[block_id].block_hdr.start_at) / 1000;
            dump_file("%lu, %lu, %lu, %lu, %lu, %lf, %lu, %lu, %lu\n", 
                        block_id, 
                        conn_io->blocks[block_id].bct,
                        conn_io->blocks[block_id].lifetime,
                        conn_io->blocks[block_id].block_hdr.size,
                        conn_io->blocks[block_id].block_hdr.size,
                        1.0, // complete
                        conn_io->blocks[block_id].block_hdr.priority,
                        conn_io->blocks[block_id].block_hdr.deadline,
                        ended_at - started_at);
            conn_io->blocks_len += 1;
            if(conn_io->blocks_len >= conn_io->cfg_len) {
              log_info("block recv done, close the connection");
              quiche_conn_close(conn_io->conn, true, 0, "done", sizeof("done"));
            }
          }
        }
      }
    }
    quiche_stream_iter_free(readable);
  }

  flush_egress(loop, conn_io);
}

static void timeout_cb(EV_P_ ev_timer *w, int revents) {
  struct conn_io *conn_io = w->data;
  quiche_conn_on_timeout(conn_io->conn);

  log_debug("timeout");

  flush_egress(loop, conn_io);

  if (quiche_conn_is_closed(conn_io->conn)) {
    quiche_stats stats;

    quiche_conn_stats(conn_io->conn, &stats);

    log_info("connection closed, recv=%zu sent=%zu lost=%zu rtt=?\
             ns total_bytes=%zu total_udb_bytes=%zu total_time=%zu",
             stats.recv, stats.sent, stats.lost, total_bytes,
             total_udp_bytes, ended_at - started_at);
    fflush(NULL);

    ev_break(EV_A_ EVBREAK_ONE);
    return;
  }
}

int main(int argc, char *argv[]) {
  args.out_file = stdout;
  args.log_file = stdout;
  argp_parse(&argp, argc, argv, 0, 0, &args);
  log_info("SERVER_IP:PORT %s:%s", args.server_ip, args.server_port);

  const struct addrinfo hints = {.ai_family = AF_UNSPEC,
                                 .ai_socktype = SOCK_DGRAM,
                                 .ai_protocol = IPPROTO_UDP};

  quiche_enable_debug_logging(debug_log, NULL);

  struct addrinfo *peer;
    if (getaddrinfo(args.server_ip, args.server_port, &hints, &peer) != 0) {
        log_error("failed to resolve host");
        freeaddrinfo(peer);
        return -1;
    }

  struct addrinfo *local;
  if (getaddrinfo(args.local_ip, args.local_port, &hints, &local) != 0) {
      log_error("failed to resolve local");
      freeaddrinfo(local);
      return -1;
  }

  int sock =
      socket(local->ai_family, SOCK_DGRAM | SOCK_NONBLOCK, 0);
  if (sock < 0) {
    log_error("create socket");
    return -1;
  }

  if (bind(sock, local->ai_addr, local->ai_addrlen) != 0) {
    log_error("fcntl");
    return -1;
  }

  if (connect(sock, peer->ai_addr, peer->ai_addrlen) != 0) {
    perror("failed to connect socket");
    return -1;
  }

  quiche_config *config = quiche_config_new(0xbabababa);
  if (config == NULL) {
    fprintf(stderr, "failed to create config\n");
    return -1;
  }
  
  quiche_config_set_application_protos(config,(uint8_t *)"\x0ahq-interop\x05hq-29\x05hq-28\x05hq-27\x08http/0.9", 38);

  const int dgramRecvQueueLen=MAX_BLOCK_SIZE;
  const int dgramSendQueueLen=MAX_BLOCK_SIZE / 1250;

  quiche_config_set_max_idle_timeout(config, 10000);
  quiche_config_set_max_recv_udp_payload_size(config, MAX_DATAGRAM_SIZE);
  quiche_config_set_max_send_udp_payload_size(config, MAX_DATAGRAM_SIZE);
  quiche_config_set_max_connection_window(config, 1000000000);
  quiche_config_set_initial_max_data(config, 1000000000);
  quiche_config_set_initial_max_stream_data_uni(config, 1000000000);
  quiche_config_set_initial_max_streams_uni(config, 1000000000);
  quiche_config_set_initial_max_stream_data_bidi_local(config, 1000000000);
  quiche_config_set_initial_max_stream_data_bidi_remote(config, 1000000000);
  quiche_config_set_initial_max_streams_bidi(config, 1000000000);
  quiche_config_set_cc_algorithm(config, QUICHE_CC_CUBIC);
  //test on dgram
  quiche_config_enable_dgram(config, true, dgramRecvQueueLen,dgramSendQueueLen);

  if (getenv("SSLKEYLOGFILE")) {
    quiche_config_log_keys(config);
  }
  // init dtp layer
  dtp_layers_ctx * dtp_ctx = dtp_layers_initnew_cli(0xbabababa);
  if (dtp_ctx == NULL) {
    log_error("failed to create dtp_context");
    return -1;
  }
  dtp_ctx->tc_ctx->quic_config=config;
 
  uint8_t scid[LOCAL_CONN_ID_LEN];
  int rng = open("/dev/urandom", O_RDONLY);
  if (rng < 0) {
    log_error("failed to open /dev/urandom %s", strerror(errno));
    return -1;
  }

  ssize_t rand_len = read(rng, &scid, sizeof(scid));
  if (rand_len < 0) {
    log_error("failed to create connection ID %s", strerror(errno));
    return -1;
  }

  quiche_conn *conn = 
    quiche_connect(args.server_ip, 
                   scid, LOCAL_CONN_ID_LEN, 
                   local->ai_addr, local->ai_addrlen,
                   peer->ai_addr, peer->ai_addrlen, 
                   config);
  if (conn == NULL) {
    log_error("failed to create connection");
    return -1;
  }
  dtp_ctx->quic_conn = conn;
  dtp_ctx->tc_ctx->quic_conn = conn;


  dump_file("block_id,bct,lifetime,size,recv_len,complete,priority,deadline,duration\n");
  started_at = get_current_usec();

  struct conn_io *conn_io = calloc(1, sizeof(*conn_io));
  if (conn_io == NULL) {
    log_error("failed to allocate connection IO");
    return -1;
  }

  conn_io->sock = sock;
  conn_io->ai_family = peer->ai_family;
  conn_io->local_addr = local->ai_addr;
  conn_io->local_addr_len = local->ai_addrlen;
  conn_io->conn = conn;
  conn_io->dtp_ctx = dtp_ctx;

  ev_io watcher;

  struct ev_loop *loop = ev_default_loop(0);

  ev_io_init(&watcher, recv_cb, conn_io->sock, EV_READ);
  ev_io_start(loop, &watcher);
  watcher.data = conn_io;

  ev_init(&conn_io->timer, timeout_cb);
  conn_io->timer.data = conn_io;

  ev_init(&conn_io->pacer, pacer_cb);
  conn_io->pacer.data = conn_io;

  flush_egress(loop, conn_io);

  ev_loop(loop, 0);

  freeaddrinfo(peer);
  freeaddrinfo(local);

  quiche_conn_free(conn_io->conn);
  quiche_config_free(config);
  dtp_layers_free(dtp_ctx);
  free(conn_io->blocks);
  free(conn_io);

  return 0;
}