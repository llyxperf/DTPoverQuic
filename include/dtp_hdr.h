#ifndef DTP_HDR_H
#define DTP_HDR_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define offsetsize_t int64_t

typedef struct {
    uint64_t     id;        // block id
    offsetsize_t offset;    // current offset
    uint64_t     len;       // the payload length
    uint64_t     sent_time; // timestamp on the sender side
    uint64_t     size;      // total length of the block
    uint64_t     priority;
    uint64_t     deadline;
    uint64_t     t;         // timestamp when the block is pushed in sender queue
} dgram_hdr;

typedef struct {
    uint64_t size;
    uint64_t priority;
    uint64_t deadline;
    uint64_t t; // timestamp when the block is pushed in sender queue
} metadata_hdr;

static const int DGRAM_HDR_LEN = sizeof(dgram_hdr);
static const int METADATA_HDR_LEN = sizeof(metadata_hdr);

// encode the basic dgram hdr to the buffer
ssize_t encode_dgram_hdr(uint8_t *out, const dgram_hdr* hdr);
 
//parse the buf to get information from the peer.
ssize_t decode_dgram_hdr(uint8_t *in, dgram_hdr *hdr);

// encode block metadata
// return: size of hdr
ssize_t encode_metadata_hdr(uint8_t *out, const metadata_hdr* hdr);

// decode block metadata
// return: 1 for success
ssize_t decode_metadata_hdr(uint8_t *in, metadata_hdr* hdr);


#if defined(__cplusplus)
}
#endif
#endif // DTP_HDR_H