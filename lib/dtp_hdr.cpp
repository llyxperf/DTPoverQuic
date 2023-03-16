#include "dtp_hdr.h"
#include <string.h>

ssize_t encode_dgram_hdr(uint8_t *out, const dgram_hdr* hdr) {
    if(out == NULL || hdr == NULL) {
        return 0;
    }
    memcpy(out, hdr, DGRAM_HDR_LEN);
    return DGRAM_HDR_LEN;
}

ssize_t decode_dgram_hdr(uint8_t *in, dgram_hdr *hdr) {
    if(in == NULL || hdr == NULL) {
        return 0;
    }
    memcpy(hdr, in, DGRAM_HDR_LEN);
    return DGRAM_HDR_LEN;
}

ssize_t encode_metadata_hdr(uint8_t *out, const metadata_hdr *hdr) {
    if(out == NULL || hdr == NULL) {
        return 0;
    }
    memcpy(out, hdr, METADATA_HDR_LEN);
    return METADATA_HDR_LEN;
}

ssize_t decode_metadata_hdr(uint8_t *in, metadata_hdr *hdr) {
    if(in == NULL || hdr == NULL) {
        return 0;
    }
    memcpy(hdr, in, METADATA_HDR_LEN);
    return METADATA_HDR_LEN;
}