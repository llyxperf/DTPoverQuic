#ifndef CONFIG_DTP_H
#define CONFIG_DTP_H
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
__uint64_t get_current_usec();

typedef struct dtp_config {
  int deadline;
  int priority;
  int size;
  float send_time_gap;
} dtp_config;

struct dtp_config *parse_dtp_config(const char *filename, int *number);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif